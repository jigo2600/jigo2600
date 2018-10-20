#  emulator.py
#  Emulator driver

# Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
# This file is part of Jigo2600 and is made available under
# the terms of the BSD license (see the COPYING file).

import argparse
import cmd
import copy
import ctypes
import enum
import hashlib
import json
import os
import pkgutil
import re
import select
import sys

import sdl2
import jigo2600
from jigo2600 import Atari2600, Cartridge, TIA
from jigo2600.peripheral import *
import jigo2600.debug as debug

# -------------------------------------------------------------------
# CommandPrompt
# -------------------------------------------------------------------


class CommandPrompt(cmd.Cmd):
    def __init__(self, game):
        super(CommandPrompt, self).__init__()
        self.game = game

    def do_meta(self, line):
        print_cart_meta(self.game.cart_meta)

    def do_pause(self, line):
        self.game.paused = True

    def do_resume(self, line):
        self.game.paused = False

    def do_quit(self, line):
        self.game.done = True

    def do_save(self, line):
        with open(os.path.expanduser('~/Desktop/state.json'), 'w') as f:
            self.game.save_state(f)
        with open(os.path.expanduser('~/Desktop/inputs.json'), 'w') as f:
            self.game.save_input_stream(f)

    def do_load(self, line):
        with open(os.path.expanduser('~/Desktop/state.json'), 'r') as f:
            self.game.load_state(f)
        with open(os.path.expanduser('~/Desktop/inputs.json'), 'r') as f:
            self.game.load_input_stream(f)

    def do_info(self, line):
        print(f"Frame number: {self.game.atari.frame_number}")
        print(f"Color cycle number: {self.game.atari.color_cycle_number}")
        print(f"Video standard: {self.game.atari.video_standard}")
        print(f"Cartridge type: {self.game.atari.cartridge.type}" +
              f" ({self.game.atari.cartridge.size} B)")

    def do_replay(self, reply_path):
        pass

    def do_debug_info(self, line):
        print(debug.cpu_info(self.game.atari.cpu))
        print(debug.pia_info(self.game.atari.pia))

def print_cart_meta(cart_meta):
    print(f'Cartridge metadata')
    print(f'* Name:           {cart_meta["name"]}')
    print(f'* Release date:   {cart_meta["releaseDate"]}')
    print(f'* Video standard: {cart_meta["standard"]}')
    print(f'* Type:           {cart_meta["cartridgeType"]}')
    print(f'* Size:           {cart_meta["size"]} B')
    print(f'* SHA1:           {cart_meta["sha1"]}')
    print(f'* Peripheral:     {cart_meta["peripheral"]}')


# -------------------------------------------------------------------
# Emulator
# -------------------------------------------------------------------

class Emulator():
    def __init__(self, perhiperal_type=Input.Type.JOYSTICK, verbosity=0, silent=False):
        self.atari = jigo2600.Atari2600()
        self.speed = 1
        self.max_fps = 60.
        self.aspect = 1.8
        self.paused = False
        self.done = False
        self.verbosity = verbosity
        self.cart_db = None
        self.cart_sha1 = None
        self.cart_meta = None
        self.peripheral_type = Input.Type.JOYSTICK
        self.switches_interface = SwitchesInterface()
        self.joysticks_interface = JoystickInterface()
        self.paddles_interface = PaddleInterface()
        self.input_stream = InputStream()
        self.has_sdl_audio = False
        self.has_sdl_gamecontroller = False
        self.has_sdl_joystick = False

        # Load the cartridge database.
        try:
            data = pkgutil.get_data('jigo2600', 'cartridges.json').decode("utf8")
            self.cart_db = json.loads(data)
            del data
        except FileNotFoundError:
            if self.verbosity > 0:
                print(f'Warning: could not open the cartridge metadata file')

        # Setup the graphics.
        frame = self.atari.get_current_frame()
        if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO) != 0:
            print(sdl2.SDL_GetError())
            sys.exit(-1)
        self.screen = sdl2.SDL_CreateWindow(b"Atari2600",
                                            sdl2.SDL_WINDOWPOS_UNDEFINED,
                                            sdl2.SDL_WINDOWPOS_UNDEFINED,
                                            int(frame.width * self.aspect *
                                                2), int(frame.height * 2),
                                            sdl2.SDL_WINDOW_SHOWN | sdl2.SDL_WINDOW_RESIZABLE)
        if not self.screen:
            print(sdl2.SDL_GetError())
            sys.exit(-1)
        self.renderer = sdl2.SDL_CreateRenderer(
            self.screen, -1, sdl2.SDL_RENDERER_PRESENTVSYNC)
        self.texture = sdl2.SDL_CreateTexture(self.renderer,
                                              sdl2.SDL_PIXELFORMAT_BGRA32,
                                              sdl2.SDL_TEXTUREACCESS_STREAMING,
                                              frame.width, frame.height)

        # Setup the audio.
        self.has_sdl_audio = not silent and (
            sdl2.SDL_InitSubSystem(sdl2.SDL_INIT_AUDIO) == 0)
        if self.has_sdl_audio:
            self.audio_spec = sdl2.SDL_AudioSpec(
                44100,
                sdl2.AUDIO_U8,
                1,
                1024,
                sdl2.SDL_AudioCallback(self.play_audio))
            self.audio_dev = sdl2.SDL_OpenAudioDevice(
                None, 0, self.audio_spec, self.audio_spec, 0)
            self.audio_buffer_rate = self.audio_spec.freq / self.audio_spec.samples
            assert self.audio_spec.channels == 1

        # Setup the joysticks.
        self.has_sdl_joystick = (
            sdl2.SDL_InitSubSystem(sdl2.SDL_INIT_JOYSTICK) == 0)
        if self.has_sdl_joystick:
            sdl2.SDL_SetHint(
                sdl2.SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, b"1")
            sdl2.SDL_JoystickEventState(sdl2.SDL_ENABLE)

        # Setup the game controllers.
        self.has_sdl_gamecontroller = (
            sdl2.SDL_InitSubSystem(sdl2.SDL_INIT_GAMECONTROLLER) == 0)
        if self.has_sdl_gamecontroller:
            sdl2.SDL_GameControllerEventState(sdl2.SDL_ENABLE)
            data = pkgutil.get_data("jigo2600", "gamecontrollerdb.txt")
            rw = sdl2.SDL_RWFromConstMem(data, len(data))
            n = sdl2.SDL_GameControllerAddMappingsFromRW(rw, False)
            if self.verbosity > 0:
                if n == -1:
                    print(f"Warning: could not load the controller database file")
                else:
                    print(f"Loaded {n} game controller mappings")

    def __del__(self):
        del self.joysticks_interface
        if self.has_sdl_gamecontroller:
            sdl2.SDL_QuitSubSystem(sdl2.SDL_INIT_GAMECONTROLLER)
        if self.has_sdl_joystick:
            sdl2.SDL_QuitSubSystem(sdl2.SDL_INIT_JOYSTICK)
        if self.has_sdl_audio:
            sdl2.SDL_CloseAudioDevice(self.audio_dev)
            sdl2.SDL_QuitSubSystem(sdl2.SDL_INIT_AUDIO)
        sdl2.SDL_DestroyTexture(self.texture)
        sdl2.SDL_DestroyRenderer(self.renderer)
        sdl2.SDL_DestroyWindow(self.screen)
        sdl2.SDL_Quit()

    def play_audio(self, _, buffer, buffer_size):
        at = ctypes.c_uint8 * buffer_size
        array = ctypes.cast(buffer, ctypes.POINTER(at))
        rate = self.atari.color_clock_rate / self.audio_buffer_rate * self.speed
        self.atari.get_audio_samples(memoryview(array.contents), rate)

    def load_cart(self, cart_path, cart_type=Cartridge.Type.UNKNOWN,
                  video_standard=None, peripheral_type=None):
        # Load the cartridge data.
        if args.verbosity > 0:
            print(f'Cartridge file path: {cart_path}')
        with open(cart_path, "rb") as f:
            cart_bytes = f.read()

        # Identify the cartridge in the database.
        sha1 = hashlib.sha1()
        sha1.update(cart_bytes)
        self.cart_sha1 = sha1.hexdigest().upper()
        self.cart_type = cart_type
        match = [x for x in self.cart_db if x['sha1'].upper() ==
                 self.cart_sha1]
        if match:
            self.cart_meta = match[0]
            if video_standard is None:
                video_standard = TIA.VideoStandard.__members__[
                    self.cart_meta['standard']]
            if self.cart_type is Cartridge.Type.UNKNOWN:
                self.cart_type = Cartridge.Type.__members__[
                    self.cart_meta['cartridgeType'].upper()]
            if peripheral_type is None:
                peripheral_type = Input.Type.__members__[
                    self.cart_meta['peripheral'].upper()]
        else:
            self.cart_meta = None

        # Make the cartridge.
        cart = jigo2600.make_cartridge_from_bytes(cart_bytes, self.cart_type)

        # Load the cartridge in the simulator.
        if video_standard is not None:
            self.atari.video_standard = video_standard
        self.atari.cartridge = cart

        if self.verbosity > 0:
            print(f'Video standard: {self.atari.video_standard}')
            print(f'Color clock rate: {self.atari.color_clock_rate} Hz')
            print(f'Cartirdge type: {cart.type}')
            if args.silent:
                print('Audio disabled')
            if self.cart_meta is not None:
                print_cart_meta(self.cart_meta)

        # Prepare input.
        if peripheral_type is None:
            peripheral_type = Input.Type.JOYSTICK
        self.input_stream.reset(peripheral_type)

    def reset(self):
        self.atari.reset()

    def load_state(self, f):
        data = f.read()
        state = self.atari.make_state()
        state.from_json(data)
        self.atari.load_state(state)

    def save_state(self, f):
        data = self.atari.save_state().to_json()
        f.write(data)

    def load_input_stream(self, f):
        data = json.load(f)
        self.input_stream.load_json(data)

    def save_input_stream(self, f):
        json.dump(self.input_stream.to_json(), f, indent=4)

    def replay(self, replay_path):
        try:
            with open(os.path.join(replay_path, 'Header.txt'), 'r') as f:
                header = f.read()
            with open(os.path.join(replay_path, 'Input Log.txt'), 'r') as f:
                inputs = [l.rstrip('\n') for l in f.readlines()]
        except FileNotFoundError as e:
            print(f"Error: could load replay data: {e}")
            return
        sha1 = re.search('SHA1 ([0-9A-F]+)', header, re.IGNORECASE)
        sha1 = sha1.group(1).upper()
        if sha1 != self.cart_sha1:
            print(f"Warning: the replay data SHA1 ({sha1}) " +
                  f"does not match the cartidge SHA1 ({self.cart_sha1})")
        frame_number = -1
        state = -1
        self.input_stream.reset(Input.Type.JOYSTICK)
        for line in inputs:
            if state == -1 and line == '[Input]':
                state = 0
                continue
            if state == 0:
                state = 1
                continue
            if state == 1 and line == '[/Input]':
                state = -1
                continue

            def get(i):
                return line[i] != '.'
            frame_number += 1
            input = Input()
            input.frame_number = frame_number
            input.panel[Panel.Switch.RESET] = get(1)
            input.panel[Panel.Switch.SELECT] = get(2)
            input.panel[Panel.Switch.DIFFICULTY_LEFT] = not get(4)
            input.panel[Panel.Switch.DIFFICULTY_RIGHT] = not get(5)
            input.peripheral_type = Input.Type.JOYSTICK
            input.peripheral = [Joystick(), Joystick()]
            for k in range(2):
                input.peripheral[k][Joystick.Switch.UP] = get(7 + 6*k)
                input.peripheral[k][Joystick.Switch.DOWN] = get(8 + 6*k)
                input.peripheral[k][Joystick.Switch.LEFT] = get(9 + 6*k)
                input.peripheral[k][Joystick.Switch.RIGHT] = get(10 + 6*k)
                input.peripheral[k][Joystick.Switch.FIRE] = get(11 + 6*k)
            self.input_stream[frame_number] = input
        if self.verbosity > 0:
            print(f"Loaded replay data {replay_path}")
            print(f"Sourced {len(self.input_stream)} input events for replay")

    def run(self):
        prompt = CommandPrompt(self)
        print(">>> ", end='', flush=True)

        event = sdl2.SDL_Event()
        toc = 0
        tic_toc_freq = sdl2.SDL_GetPerformanceFrequency()
        period = 1
        excess = 0
        remaining_cycles = 0
        input_line = []

        if self.has_sdl_audio:
            sdl2.SDL_PauseAudioDevice(self.audio_dev, 0)

        cpu_clock_rate = self.atari.color_clock_rate / 3
        reasons = [Atari2600.StoppingReason.FRAME_DONE]

        current_input = self.input_stream[self.atari.frame_number]
        new_input = copy.deepcopy(current_input)

        while not self.done:
            # Timing.
            tic = toc
            toc = sdl2.timer.SDL_GetPerformanceCounter()
            elapsed = (toc - tic) / tic_toc_freq
            if elapsed > 1:
                # Too slow: reset counters. This may be caused by the program being
                # resumed or initialized.
                tic = toc
                elapsed = 0
            elif elapsed < 1/self.max_fps:
                # Too fast: throttle the simulation. This may be caused by
                # VSYNC is not working, for instance because the game window is hidden.
                sdl2.SDL_Delay(int(1000 * (1/self.max_fps - elapsed)))
                toc = sdl2.timer.SDL_GetPerformanceCounter()
                elapsed = (toc - tic) / tic_toc_freq
            period = 0.95 * period + 0.05 * elapsed

            # Parse the SDL events.
            while sdl2.SDL_PollEvent(ctypes.byref(event)) != 0:
                if event.type == sdl2.SDL_QUIT:
                    self.done = True
                elif (event.type == sdl2.SDL_KEYUP or
                      event.type == sdl2.SDL_KEYDOWN) and event.key.repeat == 0:
                    if event.type == sdl2.SDL_KEYDOWN:
                        if (event.key.keysym.sym == sdl2.SDLK_EQUALS or
                            event.key.keysym.sym == sdl2.SDLK_KP_PLUS):
                            self.speed = min(5., self.speed + 0.25)
                            print(f"Speed increased to {self.speed}")
                            continue
                        elif event.key.keysym.sym == sdl2.SDLK_0:
                            self.speed = 1.
                            print(f"Speed reset to {self.speed}")
                            continue
                        elif event.key.keysym.sym == sdl2.SDLK_MINUS:
                            self.speed = max(0., self.speed - 0.25)
                            print(f"Speed decreased to {self.speed}")
                            continue
                    if self.switches_interface.update(event.key, new_input):
                        continue
                    if new_input.peripheral_type == Input.Type.JOYSTICK:
                        if self.joysticks_interface.update(event.key, new_input):
                            continue
                    elif new_input.peripheral_type == Input.Type.PADDLE:
                        if self.paddles_interface.update(event.key, new_input):
                            continue
                elif event.type == sdl2.SDL_WINDOWEVENT:
                    if event.window.event == sdl2.SDL_WINDOWEVENT_RESIZED:
                        pass
                elif (event.type == sdl2.SDL_JOYDEVICEADDED or
                      event.type == sdl2.SDL_CONTROLLERDEVICEADDED):
                    self.joysticks_interface.associate_sdl_joystick(
                        event.cdevice.which, verbose=self.verbosity > 0)
                elif (event.type == sdl2.SDL_JOYDEVICEREMOVED or
                      event.type == sdl2.SDL_CONTROLLERDEVICEREMOVED):
                    self.joysticks_interface.disassociate_sdl_device(
                        event.cdevice.which)
                elif (
                        event.type == sdl2.SDL_JOYAXISMOTION or
                        event.type == sdl2.SDL_JOYHATMOTION or
                        event.type == sdl2.SDL_JOYBUTTONUP or
                        event.type == sdl2.SDL_JOYBUTTONDOWN
                ):
                    if new_input.peripheral_type == Input.Type.JOYSTICK:
                        if self.joysticks_interface.update_sdl_joystick(event, new_input):
                            continue
                    elif new_input.peripheral_type == Input.Type.PADDLE:
                        if self.paddles_interface.update_sdl_joystick(event, new_input):
                            continue

            # Parse the console events.
            if os.name == 'nt':
                pass
            else:
                dr, _, _ = select.select([sys.stdin], [], [], 0.0)
                if sys.stdin in dr:
                    input_line = sys.stdin.read(1024)
                    prompt.onecmd(input_line)
                    print(">>> ", end='', flush=True)

            # Simulate an amount of CPU cycles equivalent to the real time elapsed.
            # Limit this to roughly two video frames.
            if not self.paused:
                remaining_cycles += cpu_clock_rate * elapsed * self.speed
                remaining_cycles = int(
                    min(remaining_cycles, cpu_clock_rate * 2/60))
                while remaining_cycles > 0:
                    # At the beginning of each new frame, adjust the input stream.
                    if Atari2600.StoppingReason.FRAME_DONE in reasons:
                        if new_input.peripheral_type == Input.Type.PADDLE:
                            self.paddles_interface.integrate(new_input)

                    # Feed the *current* input to the console.
                    if new_input != current_input:
                        self.input_stream[self.atari.frame_number] = new_input
                    current_input = self.input_stream[self.atari.frame_number]
                    self.atari.set_panel(current_input.panel)
                    if current_input.peripheral_type == Input.Type.JOYSTICK:
                        for k in range(2):
                            self.atari.set_joystick(k, current_input.peripheral[k])
                    elif current_input.peripheral_type == Input.Type.PADDLE:
                        for k in range(4):
                            self.atari.set_paddle(k, current_input.peripheral[k])
                    new_input = copy.deepcopy(current_input)

                    # Simulate.
                    reasons, remaining_cycles = self.atari.cycle(remaining_cycles)

            # Copy the screen content to the video texture.
            frame = self.atari.get_last_frame()
            pixels = ctypes.c_void_p()
            pitch = ctypes.c_int()
            sdl2.SDL_LockTexture(self.texture, None, ctypes.byref(
                pixels), ctypes.byref(pitch))
            ctypes.memmove(pixels, bytes(frame), frame.width*frame.height*4)
            sdl2.SDL_UnlockTexture(self.texture)
            sdl2.SDL_SetRenderTarget(self.renderer, self.texture)

            # Render the video texture.
            bounds = [0, frame.height - 1]
            sdl2.SDL_SetRenderDrawColor(self.renderer, 0, 0, 0, 0xff)
            sdl2.SDL_RenderClear(self.renderer)
            w = ctypes.c_int()
            h = ctypes.c_int()
            sdl2.SDL_GetWindowSize(self.screen, w, h)
            scale = min(w.value / (frame.width * self.aspect),
                        h.value / (bounds[1] - bounds[0] + 1))
            w_ = int(frame.width * self.aspect * scale)
            h_ = int((bounds[1] - bounds[0] + 1) * scale)
            dh_ = int(bounds[0] * scale)
            r = sdl2.SDL_Rect((int(w.value) - w_)//2,
                              (int(h.value) - h_)//2 - dh_, w_, h_)
            sdl2.SDL_RenderCopy(self.renderer, self.texture, None, r)
            sdl2.SDL_RenderPresent(self.renderer)

        if self.has_sdl_audio:
            sdl2.SDL_PauseAudioDevice(self.audio_dev, 1)


# -------------------------------------------------------------------
# Driver
# -------------------------------------------------------------------

if __name__ == "__main__":
    def make_enum(enum_type):
        class Action(argparse.Action):
            def __call__(self, parser, args, values, option_string=None):
                values = enum_type.__members__[values]
                setattr(args, self.dest, values)
        return Action

    parser = argparse.ArgumentParser()
    parser.add_argument("CART", help="cartridge binary file")
    parser.add_argument("-v", "--verbosity", default=0,
                        action="count", help="increase verbosity")
    parser.add_argument("--silent", default=False,
                        action='store_true', help="disable audio")
    parser.add_argument("-s", "--video-standard",
                        type=str,
                        choices=list(TIA.VideoStandard.__members__),
                        default=None,
                        action=make_enum(TIA.VideoStandard),
                        help="set the Atari2600 video standard")
    parser.add_argument("-t", "--cart-type",
                        type=str,
                        choices=list(jigo2600.Cartridge.Type.__members__),
                        default=Cartridge.Type.UNKNOWN,
                        action=make_enum(Cartridge.Type),
                        help="set the cartidge type (set to UNKNOWN to auto-detect)")
    parser.add_argument("--replay",
                        type=str,
                        default=None,
                        help="choose a recording to replay")
    parser.add_argument("-p", "--peripheral",
                        type=str,
                        choices=list(Input.Type.__members__),
                        default=None,
                        action=make_enum(Input.Type),
                        help="set the attached peripheral type")
    args = parser.parse_args()

    # Get the mixer.
    if not args.silent:
        import sdl2.sdlmixer

    # Create the simulator.
    simulator = Emulator(verbosity=args.verbosity, silent=args.silent)
    simulator.load_cart(
        args.CART, video_standard=args.video_standard, cart_type=args.cart_type, peripheral_type=args.peripheral)

    # Load any replay data.
    if args.replay is not None:
        simulator.replay(args.replay)

    # Run the simulator.
    simulator.run()

#  jigo2600emu.py
#  Jigo2600 emulator Python 3 driver.

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
import re
import select
import sys

import sdl2
import jigo2600
from jigo2600 import Atari2600, Cartridge

# -------------------------------------------------------------------
# Peripherals
# -------------------------------------------------------------------


class Panel(Atari2600.Panel):
    def __init__(self):
        Atari2600.Panel.__init__(self)

    def __eq__(self, other):
        return type(other) is Panel and self.get_value() == other.get_value()

    def __deepcopy__(self, memo):
        p = Panel()
        p.set_value(self.get_value())
        return p

    def __getitem__(self, sw):
        return self.get_switch(sw)

    def __setitem__(self, sw, x):
        return self.set_switch(sw, x)


class Joystick(Atari2600.Joystick):
    def __init__(self):
        Atari2600.Joystick.__init__(self)

    def __eq__(self, other):
        return type(other) is Joystick and self.get_value() == other.get_value()

    def __deepcopy__(self, memo):
        j = Joystick()
        j.set_value(self.get_value())
        return j

    def __getitem__(self, sw):
        return self.get_switch(sw)

    def __setitem__(self, sw, x):
        return self.set_switch(sw, x)


class Input():
    class Type(enum.Enum):
        NONE = 0
        JOYSTICK = 1
        KEYBOARD = 2
        PADDLE = 3

    def __init__(self, peripheral_type=Type.NONE):
        self.frame_number = 0
        self.panel = Panel()
        self.set_peripheral_type(peripheral_type)

    def __eq__(self, other):
        return (
            self.panel == other.panel and
            self.peripheral_type == other.peripheral_type and
            self.peripheral == other.peripheral
        )

    def set_peripheral_type(self, peripheral_type):
        self.peripheral_type = peripheral_type
        if peripheral_type == Input.Type.NONE:
            self.peripheral = None
        elif peripheral_type == Input.Type.JOYSTICK:
            self.peripheral = [Joystick(), Joystick()]


class InputEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, Panel):
            return o.get_value()
        elif isinstance(o, Joystick):
            return o.get_value()
        elif isinstance(o, Input.Type):
            return o.name
        elif isinstance(o, Input):
            return {
                'frame_number': o.frame_number,
                'panel': o.panel,
                'peripheral_type': o.peripheral_type,
                'peripherals': o.peripheral,
            }
        return super().default(o)

# -------------------------------------------------------------------
# Peripheral interfaces
# -------------------------------------------------------------------


class SwitchesInterface():
    def __init__(self):
        self.keys = {
            'color_mode': sdl2.SDLK_1,
            'difficulty_left': sdl2.SDLK_2,
            'difficulty_right': sdl2.SDLK_3,
            'select': sdl2.SDLK_4,
            'reset': sdl2.SDLK_BACKSPACE,
        }

    def update(self, key, input):
        pressed = key.state is sdl2.SDL_PRESSED
        if key.keysym.sym == self.keys['color_mode']:
            input.panel[Panel.Switch.COLOR_MODE] ^= pressed
            if pressed:
                print(f"TV type set to {'black-and-white' if input.panel[Panel.Switch.COLOR_MODE] else 'color'}")
        elif key.keysym.sym == self.keys['difficulty_left']:
            input.panel[Panel.Switch.DIFFICULTY_LEFT] ^= pressed
            if pressed:
                print(f"Difficulty left set to {'B' if input.panel[Panel.Switch.DIFFICULTY_LEFT] else 'A'}")
        elif key.keysym.sym == self.keys['difficulty_right']:
            input.panel[Panel.Switch.DIFFICULTY_RIGHT] ^= pressed
            if pressed:
                print(f"Difficulty right {'B' if input.panel[Panel.Switch.DIFFICULTY_RIGHT] else 'A'}")
        elif key.keysym.sym == self.keys['select']:
            input.panel[Panel.Switch.SELECT] = pressed
            print(f"Game select switch {'pressed' if input.panel[Panel.Switch.SELECT] else 'released'}")
        elif key.keysym.sym == self.keys['reset']:
            input.panel[Panel.Switch.RESET] = pressed
            print(f"Game reset switch {'pressed' if input.panel[Panel.Switch.RESET] else 'released'}")
        else:
            return False
        return True


class JoystickInterface():
    class SDLDevice():
        def __init__(self, joystick_index):
            self.game_controller = None
            self.joystick = None
            self.joystick_id = None
            if sdl2.SDL_IsGameController(joystick_index):
                self.game_controller = sdl2.SDL_GameControllerOpen(
                    joystick_index)
                self.joystick = sdl2.SDL_GameControllerGetJoystick(
                    self.game_controller)
                self.joystick_id = sdl2.SDL_JoystickInstanceID(self.joystick)
                self.name = sdl2.SDL_GameControllerName(self.game_controller)
            else:
                self.joystick = sdl2.SDL_JoystickOpen(joystick_index)
                self.joystick_id = sdl2.SDL_JoystickInstanceID(self.joystick)
                self.name = sdl2.SDL_JoystickName(self.joystick)

        def __del__(self):
            if self.game_controller is not None:
                if sdl2.SDL_GameControllerGetAttached(self.game_controller):
                    sdl2.SDL_GameControllerClose(self.game_controller)
            else:
                if sdl2.SDL_JoystickGetAttached(self.joystick):
                    sdl2.SDL_JoystickClose(self.joystick)

        def __eq__(self, other):
            if type(other) is not JoystickInterface.SDLDevice:
                return False
            if self.game_controller is None and other.game_controller is None:
                return self.game_controller == other.game_controller
            return self.joystick == other.joystick

        def __str__(self):
            return str(self.name)

    def __init__(self):
        self.controllers = [None]*2
        self.keys_state = [None]*2
        for k in range(2):
            self.keys_state[k] = {
                'fire': False,
                'up': False,
                'down': False,
                'left': False,
                'right': False,
            }
        self.keys = [
            {
                'up': sdl2.SDLK_w,
                'down': sdl2.SDLK_s,
                'left': sdl2.SDLK_a,
                'right': sdl2.SDLK_d,
                'fire': sdl2.SDLK_SPACE,
            },
            {
                'up': None,
                'down': None,
                'left': None,
                'right': None,
                'fire': None,
            }
        ]
        self.AXIS_DEAD_ZONE = 8000

    def associate_sdl_joystick(self, joystick_index, verbose=False):
        device = JoystickInterface.SDLDevice(joystick_index)
        for k in range(2):
            if self.controllers[k] != device:
                self.controllers[k] = device
                if verbose:
                    print(f"Associated device {device} to joystick {k}")
                return True
        return False

    def disassociate_sdl_device(self, device_id):
        for k in range(2):
            if self.controllers[k].joystick == device_id and self.controllers[k].game_controller == device_id:
                self.controllers[k] = None

    def reset(self):
        self.__init__()

    def __del__(self):
        self.reset()

    def update(self, key, input):
        for k in range(2):
            joystick = input.peripheral[k]
            if key.keysym.sym == self.keys[k]['fire']:
                self.fire_key = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.FIRE] = self.fire_key
                return True
            elif key.keysym.sym == self.keys[k]['up']:
                self.keys_state[k]['up'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.UP] = self.keys_state[k]['up']
                joystick[Joystick.Switch.DOWN] = self.keys_state[k]['down'] and not self.keys_state[k]['up']
                return True
            elif key.keysym.sym == self.keys[k]['down']:
                self.keys_state[k]['down'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.DOWN] = self.keys_state[k]['down']
                joystick[Joystick.Switch.UP] = self.keys_state[k]['up'] and not self.keys_state[k]['down']
                return True
            elif key.keysym.sym == self.keys[k]['left']:
                self.keys_state[k]['left'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.LEFT] = self.keys_state[k]['left']
                joystick[Joystick.Switch.RIGHT] = self.keys_state[k]['right'] and not self.keys_state[k]['left']
                return True
            elif key.keysym.sym == self.keys[k]['right']:
                self.keys_state[k]['right'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.RIGHT] = self.keys_state[k]['right']
                joystick[Joystick.Switch.LEFT] = self.keys_state[k]['left'] and not self.keys_state[k]['right']
                return True
        return False

    def update_sdl_joystick(self, event, input):
        for k in range(2):
            if not isinstance(self.controllers[k], JoystickInterface.SDLDevice):
                continue
            joystick = input.peripheral[k]
            if event.type == sdl2.SDL_JOYAXISMOTION:
                if event.jaxis.which == self.controllers[k].joystick_id:
                    if event.jaxis.axis == 0:
                        joystick[Joystick.Switch.LEFT] = event.jaxis.value < -self.AXIS_DEAD_ZONE
                        joystick[Joystick.Switch.RIGHT] = event.jaxis.value > self.AXIS_DEAD_ZONE
                        return True
                    elif event.jaxis.axis == 1:
                        joystick[Joystick.Switch.DOWN] = event.jaxis.value < -self.AXIS_DEAD_ZONE
                        joystick[Joystick.Switch.UP] = event.jaxis.value > self.AXIS_DEAD_ZONE
                        return True
            elif event.type == sdl2.SDL_JOYBUTTONUP or event.type == sdl2.SDL_JOYBUTTONDOWN:
                if event.jbutton.which == self.controllers[k].joystick_id:
                    joystick[Joystick.Switch.FIRE] = (
                        event.jbutton.state == sdl2.SDL_PRESSED)
                    return True
            elif event.type == sdl2.SDL_JOYHATMOTION:
                if event.jhat.which == self.controllers[k].joystick_id:
                    joystick.reset_directions()
                    if event.jhat.value == sdl2.SDL_HAT_UP:
                        joystick[Joystick.Switch.UP] = True
                    elif event.jhat.value == sdl2.SDL_HAT_RIGHTUP:
                        joystick[Joystick.Switch.UP] = True
                        joystick[Joystick.Switch.RIGHT] = True
                    elif event.jhat.value == sdl2.SDL_HAT_RIGHT:
                        joystick[Joystick.Switch.RIGHT] = True
                    elif event.jhat.value == sdl2.SDL_HAT_RIGHTDOWN:
                        joystick[Joystick.Switch.RIGHT] = True
                        joystick[Joystick.Switch.DOWN] = True
                    elif event.jhat.value == sdl2.SDL_HAT_DOWN:
                        joystick[Joystick.Switch.DOWN] = True
                    elif event.jhat.value == sdl2.SDL_HAT_LEFTDOWN:
                        joystick[Joystick.Switch.DOWN] = True
                        joystick[Joystick.Switch.LEFT] = True
                    elif event.jhat.value == sdl2.SDL_HAT_LEFT:
                        joystick[Joystick.Switch.LEFT] = True
                    elif event.jhat.value == sdl2.SDL_HAT_LEFTUP:
                        joystick[Joystick.Switch.LEFT] = True
                        joystick[Joystick.Switch.UP] = True
                    return True
        return False

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
        data = self.game.atari.save_state().to_json()
        with open(os.path.expanduser('~/Desktop/state.json'), 'w') as f:
            f.write(data)
        with open(os.path.expanduser('~/Desktop/inputs.json'), 'w') as f:
            f.write(json.dumps(self.game.inputs, indent=4, cls=InputEncoder))

    def do_load(self, line):
        with open(os.path.expanduser('~/Desktop/state.json'), 'r') as f:
            data = f.read()
        state = self.game.atari.make_state()
        state.from_json(data)
        self.game.atari.load_state(state)

    def do_info(self, line):
        print(f"Frame number: {self.game.atari.frame_number}")
        print(f"Color cycle number: {self.game.atari.color_cycle_number}")
        print(f"Video standard: {self.game.atari.video_standard}")
        print(f"Cartridge type: {self.game.atari.cartridge.type}" +
              f" ({self.game.atari.cartridge.size} B)")

    def do_replay(self, reply_path):
        pass


def print_cart_meta(cart_meta):
    print(f'Cartridge metadata')
    print(f'* Name:           {cart_meta["name"]}')
    print(f'* Release date:   {cart_meta["releaseDate"]}')
    print(f'* Video standard: {cart_meta["standard"]}')
    print(f'* Type:           {cart_meta["cartridgeType"]}')
    print(f'* Size:           {cart_meta["size"]} B')
    print(f'* SHA1:           {cart_meta["sha1"]}')

# -------------------------------------------------------------------
# Simulator
# -------------------------------------------------------------------


class Simulator():
    def __init__(self, verbosity=0, silent=False):
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
        self.switches_interface = SwitchesInterface()
        self.joysticks_interface = JoystickInterface()
        self.inputs = []
        self.has_sdl_audio = False
        self.has_sdl_gamecontroller = False
        self.has_sdl_joystick = False

        # Load the cartridge database.
        try:
            with open('cartridges.json') as f:
                self.cart_db = json.load(f)
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
            n = sdl2.SDL_GameControllerAddMappingsFromFile(
                b"gamecontrollerdb.txt")
            if self.verbosity > 0:
                if n == -1:
                    print(f"Warning: could not load the controller database file")
                else:
                    print(f"Loaded {n} game controller mappings")

        # Set the initial content of the input log.
        input = Input()
        input.set_peripheral_type(Input.Type.JOYSTICK)
        self.inputs.append(input)

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

    def play_audio(self, unused, buffer, buffer_size):
        at = ctypes.c_uint8 * buffer_size
        array = ctypes.cast(buffer, ctypes.POINTER(at))
        rate = self.atari.color_clock_rate / self.audio_buffer_rate * self.speed
        self.atari.get_audio_samples(memoryview(array.contents), rate)

    def load_cart(self, cart_path, cart_type=Cartridge.Type.UNKNOWN, video_standard=None):
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
        if len(match) > 0:
            self.cart_meta = match[0]
            if video_standard is None:
                video_standard = Atari2600.VideoStandard.__members__[
                    self.cart_meta['standard']]
            if self.cart_type is Cartridge.Type.UNKNOWN:
                self.cart_type = Cartridge.Type.__members__[
                    self.cart_meta['cartridgeType'].upper()]
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

    def find_input(self, frame_number):
        def xsearch(i, j):
            if self.inputs[j].frame_number <= frame_number:
                return j
            k = (i + j) // 2
            if self.inputs[k].frame_number <= frame_number:
                return xsearch(k, j-1)
            else:
                return xsearch(i, k-1)
        return xsearch(0, len(self.inputs) - 1)

    def retrieve_input(self, frame_number):
        return self.inputs[self.find_input(frame_number)]

    def set_input(self, frame_number, input):
        "Write an input entry to the log."
        "The actual entry rather than a copy of it is entered in the log."
        "Entries are elided if they are redundant."
        input.frame_number = frame_number
        i = self.find_input(frame_number)
        if self.inputs[i].frame_number < frame_number:
            if self.inputs[i] != input:
                self.inputs.insert(i + 1, input)
        else:
            self.inputs[i] = input

    def reset(self):
        self.atari.reset()

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
        self.inputs.clear()
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
            if len(self.inputs) > 0 and self.inputs[-1] == input:
                continue
            self.inputs.append(input)
        if self.verbosity > 0:
            print(f"Loaded replay data {replay_path}")
            print(f"Sourced {len(self.inputs)} input events for replay")

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
            input_detected = False
            new_input = copy.deepcopy(self.retrieve_input(self.atari.frame_number))

            while sdl2.SDL_PollEvent(ctypes.byref(event)) != 0:
                if event.type == sdl2.SDL_QUIT:
                    self.done = True
                elif (event.type == sdl2.SDL_KEYUP or event.type == sdl2.SDL_KEYDOWN) and event.key.repeat == 0:
                    if event.type == sdl2.SDL_KEYDOWN:
                        if event.key.keysym.sym == sdl2.SDLK_EQUALS or event.key.keysym.sym == sdl2.SDLK_KP_PLUS:
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
                    input_detected |= (
                        self.switches_interface.update(event.key, new_input) or
                        self.joysticks_interface.update(event.key, new_input)
                    )
                elif event.type == sdl2.SDL_WINDOWEVENT:
                    if event.window.event == sdl2.SDL_WINDOWEVENT_RESIZED:
                        pass
                elif event.type == sdl2.SDL_JOYDEVICEADDED or event.type == sdl2.SDL_CONTROLLERDEVICEADDED:
                    self.joysticks_interface.associate_sdl_joystick(
                        event.cdevice.which, verbose=self.verbosity > 0)
                elif event.type == sdl2.SDL_JOYDEVICEREMOVED or event.type == sdl2.SDL_CONTROLLERDEVICEREMOVED:
                    self.joysticks_interface.disassociate_sdl_device(
                        event.cdevice.which)
                elif (
                    event.type == sdl2.SDL_JOYAXISMOTION or
                    event.type == sdl2.SDL_JOYHATMOTION or
                    event.type == sdl2.SDL_JOYBUTTONUP or
                    event.type == sdl2.SDL_JOYBUTTONDOWN
                ):
                    input_detected |= (
                        self.joysticks_interface.update_sdl_joystick(event, new_input)
                    )

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
            # Limit this to roughly two video frames frames.
            if not self.paused:
                remaining_cycles += cpu_clock_rate * elapsed * self.speed
                remaining_cycles = int(
                    min(remaining_cycles, cpu_clock_rate * 2/60))
                while remaining_cycles > 0:
                    reasons, remaining_cycles = self.atari.cycle(
                        remaining_cycles)
                    if Atari2600.StoppingReason.FRAME_DONE in reasons:
                        if input_detected:
                            self.set_input(self.atari.frame_number, new_input)
                        input = self.retrieve_input(self.atari.frame_number)
                        self.atari.set_panel(input.panel)
                        if input.peripheral_type == Input.Type.JOYSTICK:
                            for k in range(2):
                                self.atari.set_joystick(k, input.peripheral[k])

            # Move screen to texture
            frame = self.atari.get_last_frame()
            pixels = ctypes.c_void_p()
            pitch = ctypes.c_int()
            sdl2.SDL_LockTexture(self.texture, None, ctypes.byref(
                pixels), ctypes.byref(pitch))
            ctypes.memmove(pixels, bytes(frame), frame.width*frame.height*4)
            sdl2.SDL_UnlockTexture(self.texture)
            sdl2.SDL_SetRenderTarget(self.renderer, self.texture)

            # Render texture
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
                        choices=list(Atari2600.VideoStandard.__members__),
                        default=None,
                        action=make_enum(Atari2600.VideoStandard),
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
    args = parser.parse_args()

    # Get the mixer.
    if not args.silent:
        import sdl2.sdlmixer

    # Create the simulator.
    simulator = Simulator(verbosity=args.verbosity, silent=args.silent)
    simulator.load_cart(
        args.CART, video_standard=args.video_standard, cart_type=args.cart_type)

    # Load any replay data.
    if args.replay is not None:
        simulator.replay(args.replay)

    # Run the simulator.
    simulator.run()

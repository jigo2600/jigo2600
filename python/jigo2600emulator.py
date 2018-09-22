#  jigo2600emu.py
#  Jigo2600 emulator Python 3 driver.

# Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
# This file is part of Jigo2600 and is made available under
# the terms of the BSD license (see the COPYING file).

import argparse
import cmd
import ctypes
import hashlib
import json
import os
import re
import sdl2
import sdl2.sdlmixer
import select
import sys

import jigo2600
from jigo2600 import Atari2600, Cartridge

# -------------------------------------------------------------------
# Helpers
# -------------------------------------------------------------------

class Switches():
    def __init__(self):
        self.reset()

    def reset(self):
        self.select = False
        self.reset = False
        self.color_mode = True
        self.difficulty_left = False
        self.difficulty_right = False

    def update(self, k):
        pressed = k.state is sdl2.SDL_PRESSED
        if (k.keysym.sym == sdl2.SDLK_1):
            self.select = pressed
            print(f"Select switch {'pressed' if self.select else 'released'}")
        elif (k.keysym.sym == sdl2.SDLK_2):
            self.difficulty_left ^= pressed
            if pressed:
                print(f"Difficulty left switch {'on' if self.difficulty_left else 'off'}")
        elif (k.keysym.sym == sdl2.SDLK_3):
            self.difficulty_right ^= pressed
            if pressed:
                print(f"Difficulty right siwtch {'on' if self.difficulty_right else 'off'}")
        elif (k.keysym.sym == sdl2.SDLK_4):
            if pressed:
                self.color_mode ^= pressed
                print(f"Color mode {'on' if self.color_mode else 'off'}")
        elif (k.keysym.sym == sdl2.SDLK_BACKSPACE):
            self.reset = pressed
            print(f"Reset switch {'pressed' if self.reset else 'released'}")
        else:
            return False
        return True

class Joystick():
    def __init__(self, num):    
        self.num = num
        self.reset()
    
    def reset(self):
        self.fire = False
        self.up = False
        self.down = False
        self.left = False
        self.right = False

        self.fire_key = False
        self.up_key = False
        self.down_key = False
        self.left_key = False
        self.right_key = False
    
    def update(self, k):
        if (k.keysym.sym == sdl2.SDLK_SPACE):
            self.fire_key = k.state is sdl2.SDL_PRESSED
            self.fire = self.fire_key
        elif (k.keysym.sym == sdl2.SDLK_w):
            self.up_key = k.state is sdl2.SDL_PRESSED
            self.up = self.up_key
            self.down = self.down_key and not self.up
        elif (k.keysym.sym == sdl2.SDLK_s):
            self.down_key = k.state is sdl2.SDL_PRESSED
            self.down = self.down_key
            self.up = self.up_key and not self.down
        elif (k.keysym.sym == sdl2.SDLK_a):
            self.left_key = k.state is sdl2.SDL_PRESSED
            self.left = self.left_key 
            self.right = self.right_key and not self.left
        elif (k.keysym.sym == sdl2.SDLK_d):
            self.right_key = k.state is sdl2.SDL_PRESSED
            self.right = self.right_key
            self.left = self.left_key and not self.right

    def convert(self):
        j = []
        if self.fire:
            j.append(jigo2600.Atari2600.Joystick.FIRE)
        if self.up:
            j.append(jigo2600.Atari2600.Joystick.UP)
        if self.down:
            j.append(jigo2600.Atari2600.Joystick.DOWN)
        if self.left:
            j.append(jigo2600.Atari2600.Joystick.LEFT)
        if self.right:
            j.append(jigo2600.Atari2600.Joystick.RIGHT)
        return j

class CommandPrompt(cmd.Cmd):
    def __init__(self, game):
        super(CommandPrompt, self).__init__()
        self.game = game

    def do_meta(self, line):
        print_cart_meta(cart_meta)

    def do_pause(self, line):
        self.game.paused = True

    def do_resume(self, line):
        self.game.paused = False
    
    def do_quit(self, line):
        self.game.done = True

    def do_save(self, line):
        data = self.game.atari.save_state().to_json()
        with open(os.path.expanduser('~/Desktop/save.json'), 'w') as f:
            f.write(data)

    def do_load(self, line):
        with open(os.path.expanduser('~/Desktop/save.json'), 'r') as f:
            data = f.read()
        state = self.game.atari.make_state()
        state.from_json(data)
        self.game.atari.load_state(state)

    def do_info(self, line):
        print(f"Frame number: {self.game.atari.frame_number}")
        print(f"Color cycle number: {self.game.atari.color_cycle_number}")
        print(f"Video standard: {self.game.atari.video_standard}")
        print(f"Cartridge type: {self.game.atari.cartridge.type} ({self.game.atari.cartridge.size} B)")

    def do_replay(self, reply_path):
        #replay ../Lib/TAS-Recordings/storming
        try:
            with open(os.path.join(reply_path, 'Header.txt'), 'r') as f:
                header = f.read()
            with open(os.path.join(reply_path, 'Input Log.txt'), 'r') as f:
                inputs = f.read()
        except FileNotFoundError as e:
            print(f"Error: could load reply data: {e}")
            return
        sha1 = re.search('SHA1 ([0-9A-F]+)', header, re.IGNORECASE)
        sha1 = sha1.group(1).upper()

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
    def __init__(self, atari, silent=False):
        self.atari = atari
        self.speed = 1
        self.max_fps = 60.
        self.aspect = 1.8
        self.paused = False
        self.done = False
        self.switches = Switches()
        self.joysticks = [Joystick(0)]

        frame = atari.get_current_frame()
        if sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO) != 0:
            print(sdl2.SDL_GetError())
            sys.exit(-1)
        self.screen = sdl2.SDL_CreateWindow(b"Atari2600",
            sdl2.SDL_WINDOWPOS_UNDEFINED,
            sdl2.SDL_WINDOWPOS_UNDEFINED,
            int(frame.width * self.aspect * 2), int(frame.height * 2),
            sdl2.SDL_WINDOW_SHOWN | sdl2.SDL_WINDOW_RESIZABLE)
        if not self.screen:
            print(sdl2.SDL_GetError())
            sys.exit(-1)
        self.renderer = sdl2.SDL_CreateRenderer(self.screen, -1, sdl2.SDL_RENDERER_PRESENTVSYNC)
        self.texture = sdl2.SDL_CreateTexture(self.renderer,
            sdl2.SDL_PIXELFORMAT_BGRA32,
            sdl2.SDL_TEXTUREACCESS_STREAMING,
            frame.width, frame.height)
        
        self.has_audio = not silent and (sdl2.SDL_Init(sdl2.SDL_INIT_AUDIO) == 0)
        if self.has_audio:
          
            spec = sdl2.SDL_AudioSpec(
                44100,
                sdl2.AUDIO_U8,
                1,
                1024,
                sdl2.SDL_AudioCallback(self.play_audio)) 
            self.audio_spec = spec
            self.audio_dev = sdl2.SDL_OpenAudioDevice(None, 0, spec, self.audio_spec, 0)
            self.audio_buffer_rate = 44100 / 1024
        
    def __del__(self):
        if self.has_audio:
            sdl2.SDL_CloseAudioDevice(self.audio_dev)
        sdl2.SDL_DestroyTexture(self.texture)
        sdl2.SDL_DestroyRenderer(self.renderer)
        sdl2.SDL_DestroyWindow(self.screen)

    def play_audio(self, unused, buffer, bufferSize):
        at = ctypes.c_uint8 * bufferSize
        array = ctypes.cast(buffer, ctypes.POINTER(at))
        rate = self.atari.color_clock_rate / self.audio_buffer_rate * self.speed
        self.atari.get_audio_samples(memoryview(array.contents), rate)

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

        if self.has_audio:
            sdl2.SDL_PauseAudioDevice(self.audio_dev, 0)

        cpu_clock_rate = atari.color_clock_rate / 3 

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
                elif event.type == sdl2.SDL_KEYDOWN and event.key.repeat == 0:
                    if event.key.keysym.sym == sdl2.SDLK_EQUALS or event.key.keysym.sym  == sdl2.SDLK_KP_PLUS:
                        self.speed = min(5., self.speed + 0.25)
                        print(f"Speed increased to {self.speed}")
                    elif event.key.keysym.sym == sdl2.SDLK_0:
                        self.speed = 1.
                        print(f"Speed reset to {self.speed}")
                    elif event.key.keysym.sym == sdl2.SDLK_MINUS:
                        self.speed = max(0., self.speed - 0.25)
                        print(f"Speed decreased to {self.speed}")
                    elif self.joysticks[0].update(event.key):
                        pass
                    elif self.switches.update(event.key):
                        pass
                    else:
                        pass
                elif event.type == sdl2.SDL_KEYUP and event.key.repeat == 0:
                    if self.joysticks[0].update(event.key):
                        pass
                    elif self.switches.update(event.key):
                        pass
                elif event.type == sdl2.SDL_WINDOWEVENT:
                    if event.window.event == sdl2.SDL_WINDOWEVENT_RESIZED:
                        pass

            # Parse the console events.
            dr, _, _ = select.select([sys.stdin],[],[],0.0)
            if sys.stdin in dr:
                input_line = sys.stdin.read(1024)
                prompt.onecmd(input_line)
                print(">>> ", end='', flush=True)

            # Simulate an amount of CPU cycles equivalent to the real time elapsed.
            # Limit this to roughly two video frames frames.
            if not self.paused:
                remaining_cycles += cpu_clock_rate * elapsed * self.speed
                remaining_cycles = int(min(remaining_cycles, cpu_clock_rate * 2/60))
                while remaining_cycles > 0:
                    reasons, remaining_cycles = self.atari.cycle(remaining_cycles)
                    if Atari2600.StoppingReason.FRAME_DONE in reasons:
                        self.atari.set_joystick(0, self.joysticks[0].convert())
                        self.atari.set_switch(Atari2600.Switch.SELECT, self.switches.select)
                        self.atari.set_switch(Atari2600.Switch.COLOR_MODE, self.switches.color_mode)
                        self.atari.set_switch(Atari2600.Switch.RESET, self.switches.reset)
                        self.atari.set_switch(Atari2600.Switch.DIFFICULTY_LEFT, self.switches.difficulty_left)
                        self.atari.set_switch(Atari2600.Switch.DIFFICULTY_RIGHT, self.switches.difficulty_right)

            # Render game
            frame = self.atari.get_last_frame()
            sdl2.SDL_SetRenderDrawColor(self.renderer, 0, 0, 0, 0xff)
            sdl2.SDL_RenderClear(self.renderer)
            w = ctypes.c_int()
            h = ctypes.c_int()
            sdl2.SDL_GetWindowSize(self.screen, w, h)
            scale = min(w.value / (frame.width * self.aspect), h.value / frame.height)
            w_ = int(frame.width * self.aspect * scale)
            h_ = int(frame.height * scale)
            r = sdl2.SDL_Rect((int(w.value) - w_)//2, (int(h.value) - h_)//2, w_, h_)
            pixels = ctypes.c_void_p()
            pitch = ctypes.c_int()
            sdl2.SDL_LockTexture(self.texture, None, ctypes.byref(pixels), ctypes.byref(pitch))
            ctypes.memmove(pixels, bytes(frame), frame.width*frame.height*4)
            sdl2.SDL_UnlockTexture(self.texture)
            sdl2.SDL_SetRenderTarget(self.renderer, self.texture)
            sdl2.SDL_RenderCopy(self.renderer, self.texture, None, r)
            sdl2.SDL_RenderPresent(self.renderer)

        if self.has_audio:
            sdl2.SDL_PauseAudioDevice(self.audio_dev, 1)

# -------------------------------------------------------------------
# Driver
# -------------------------------------------------------------------

if __name__ == "__main__":
    def make_enum(enum_type):
        class Action(argparse.Action):
            def __call__(self, parser, args, values, option_string=None):
                values=enum_type.__members__[values]
                setattr(args, self.dest, values)
        return Action

    parser = argparse.ArgumentParser()
    parser.add_argument("CART", help="cartridge binary file")
    parser.add_argument("-v", "--verbosity", default=0, action="count", help="increase verbosity")
    parser.add_argument("--silent", default=False, action='store_true', help="disable audio")
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
    args = parser.parse_args()

    # Get the cartridge database.
    cart_db = []
    try:
        with open('cartridges.json') as f:
            cart_db = json.load(f)
    except FileNotFoundError:
        if args.verbosity > 0:
            print(f'Warning: could not find the cartridge metadata file')

    # Load the cartridge data.
    cart_path = args.CART
    if args.verbosity > 0:
        print(f'Cartridge file path: {cart_path}')
    with open(cart_path, "rb") as f:
        cart_bytes = f.read()

    # Identify the cartridge in the database.
    sha1 = hashlib.sha1()
    sha1.update(cart_bytes)
    sha1 = sha1.hexdigest()
    match = [x for x in cart_db if x['sha1'] == sha1]
    if len(match) > 0:
        cart_meta = match[0]
        if args.video_standard is None:
            args.video_standard = Atari2600.VideoStandard.__members__[cart_meta['standard']]
        if args.cart_type is Cartridge.Type.UNKNOWN:
            args.cart_type = Cartridge.Type.__members__[cart_meta['cartridgeType'].upper()]
    else:
        cart_meta = None

    # Build the cartridge.
    cart = jigo2600.make_cartridge_from_bytes(cart_bytes, args.cart_type)

    # Install the cartridge in the simulator.
    atari = jigo2600.Atari2600()
    if args.video_standard is not None:
        atari.video_standard = args.video_standard
    atari.cartridge = cart

    if args.verbosity > 0:
        print(f'Video standard: {atari.video_standard}')
        print(f'Color clock rate: {atari.color_clock_rate} Hz')
        print(f'Cartirdge type: {cart.type}')
        if args.silent:
            print('Audio disabled')
        if cart_meta is not None:
            print_cart_meta(cart_meta)

    # Run the simulator.
    simulator = Simulator(atari, silent=args.silent)
    simulator.run()






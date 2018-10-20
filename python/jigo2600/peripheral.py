#  peripheral.py
#  Peripherals controller

# Copyright (c) 2018 The Jigo2600 Team. All rights reserved.
# This file is part of Jigo2600 and is made available under
# the terms of the BSD license (see the COPYING file).

import copy
import ctypes
import enum
import json
import sys

import sdl2
import jigo2600
from jigo2600 import Atari2600


class Panel(Atari2600.Panel):
    def __init__(self):
        Atari2600.Panel.__init__(self)

    def __eq__(self, other):
        return isinstance(other, Panel) and self.get_value() == other.get_value()

    def __deepcopy__(self, memo):
        p = Panel()
        p.set_value(self.get_value())
        return p

    def __getitem__(self, sw):
        return self.get_switch(sw)

    def __setitem__(self, sw, x):
        return self.set_switch(sw, x)

    @classmethod
    def from_json(cls, d):
        x = cls()
        x.set_value(d)
        return x

    def to_json(self):
        return self.get_value()


class Joystick(Atari2600.Joystick):
    def __init__(self):
        Atari2600.Joystick.__init__(self)

    def __eq__(self, other):
        return isinstance(other, Joystick) and self.get_value() == other.get_value()

    def __deepcopy__(self, memo):
        j = Joystick()
        j.set_value(self.get_value())
        return j

    def __getitem__(self, sw):
        return self.get_switch(sw)

    def __setitem__(self, sw, x):
        return self.set_switch(sw, x)

    @classmethod
    def from_json(cls, d):
        x = cls()
        x.set_value(d)
        return x

    def to_json(self):
        return self.get_value()

class Paddle(Atari2600.Paddle):
    def __init__(self):
        Atari2600.Paddle.__init__(self)

    def __eq__(self, other):
        return (
            isinstance(other, Paddle) and
            self.fire == other.fire and
            self.angle == other.angle
        )

    def __deepcopy__(self, memo):
        p = Paddle()
        p.angle = self.angle
        p.fire = self.fire
        return p

    @classmethod
    def from_json(cls, d):
        x = Paddle()
        x.angle = d['angle']
        x.fire = d['fire']
        return x

    def to_json(self):
        return {'angle':self.angle, 'fire':self.fire}

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
        elif peripheral_type == Input.Type.PADDLE:
            self.peripheral = [Paddle() for k in range(4)]

    @classmethod
    def from_json(self, d):
        x = Input()
        x.frame_number = d['frame_number']
        x.panel = Panel.from_json(d['panel'])
        x.peripheral_type = Input.Type[d['peripheral_type']]
        if x.peripheral_type == Input.Type.NONE:
            x.peripheral = []
        elif x.peripheral_type == Input.Type.JOYSTICK:
            x.peripheral = [
                Joystick.from_json(d['peripheral'][i]) for i in range(2)
            ]
        elif x.peripheral_type == Input.Type.PADDLE:
            x.peripheral = [
                Paddle.from_json(d['peripheral'][i]) for i in range(4)
            ]
        elif x.peripheral == Input.Type.KEYBOARD:
            pass
        return x

    def to_json(self):
        return {
            'frame_number': self.frame_number,
            'panel': self.panel.to_json(),
            'peripheral_type': self.peripheral_type.name,
            'peripheral': [x.to_json() for x in self.peripheral],
        }

# -------------------------------------------------------------------
# InputStream
# -------------------------------------------------------------------


class InputStream():
    def __init__(self):
        self.stream = []
        self.reset(Input.Type.NONE)

    def __len__(self):
        return len(self.stream)

    def __getitem__(self, frame_number):
        return self.stream[self.search(frame_number)]

    def __setitem__(self, frame_number, value):
        value.frame_number = frame_number
        i = self.search(frame_number)
        if self.stream[i].frame_number < frame_number:
            if self.stream[i] != value:
                self.stream.insert(i + 1, value)
        else:
            self.stream[i] = value

    def search(self, frame_number):
        def xsearch(i, j):
            if self.stream[j].frame_number <= frame_number:
                return j
            k = (i + j) // 2
            if self.stream[k].frame_number <= frame_number:
                return xsearch(k, j-1)
            else:
                return xsearch(i, k-1)
        return xsearch(0, len(self.stream) - 1)

    def reset(self, peripheral_type):
        self.stream = []
        value = Input()
        value.set_peripheral_type(peripheral_type)
        self.stream.clear()
        self.stream.append(value)

    def load_json(self, d):
        self.stream = []
        for x in d:
            self.stream.append(Input.from_json(x))

    def to_json(self):
        return [x.to_json() for x in self.stream]


# -------------------------------------------------------------------
# Peripheral interfaces
# -------------------------------------------------------------------


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
        if not isinstance(other, SDLDevice):
            return False
        if self.game_controller is None and other.game_controller is None:
            return self.game_controller == other.game_controller
        return self.joystick == other.joystick

    def __str__(self):
        return str(self.name)


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
                value = ('black-and-white' if input.panel[Panel.Switch.COLOR_MODE]
                         else 'color')
                print(f"TV type set to {value}")
        elif key.keysym.sym == self.keys['difficulty_left']:
            input.panel[Panel.Switch.DIFFICULTY_LEFT] ^= pressed
            if pressed:
                value = 'B' if input.panel[Panel.Switch.DIFFICULTY_LEFT] else 'A'
                print(f"Difficulty left set to {value}")
        elif key.keysym.sym == self.keys['difficulty_right']:
            input.panel[Panel.Switch.DIFFICULTY_RIGHT] ^= pressed
            if pressed:
                value = 'B' if input.panel[Panel.Switch.DIFFICULTY_RIGHT] else 'A'
                print(f"Difficulty right {value}")
        elif key.keysym.sym == self.keys['select']:
            input.panel[Panel.Switch.SELECT] = pressed
            value = 'pressed' if input.panel[Panel.Switch.SELECT] else 'released'
            print(f"Game select switch {value}")
        elif key.keysym.sym == self.keys['reset']:
            input.panel[Panel.Switch.RESET] = pressed
            value = 'pressed' if input.panel[Panel.Switch.RESET] else 'released'
            print(f"Game reset switch {value}")
        else:
            return False
        return True


class JoystickInterface():
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
        device = SDLDevice(joystick_index)
        for k in range(2):
            if self.controllers[k] != device:
                self.controllers[k] = device
                if verbose:
                    print(f"Associated device {device} to joystick {k}")
                return True
        return False

    def disassociate_sdl_device(self, device_id):
        for k in range(2):
            if (self.controllers[k].joystick == device_id or
                    self.controllers[k].game_controller == device_id):
                self.controllers[k] = None

    def reset(self):
        self.__init__()

    def __del__(self):
        self.reset()

    def update(self, key, input):
        for k in range(2):
            joystick = input.peripheral[k]
            if key.keysym.sym == self.keys[k]['fire']:
                self.keys_state[k]['fire'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.FIRE] = self.keys_state[k]['fire']
                return True
            elif key.keysym.sym == self.keys[k]['up']:
                self.keys_state[k]['up'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.UP] = self.keys_state[k]['up']
                joystick[Joystick.Switch.DOWN] = (self.keys_state[k]['down'] and
                                                  not self.keys_state[k]['up'])
                return True
            elif key.keysym.sym == self.keys[k]['down']:
                self.keys_state[k]['down'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.DOWN] = self.keys_state[k]['down']
                joystick[Joystick.Switch.UP] = (self.keys_state[k]['up']
                                                and not self.keys_state[k]['down'])
                return True
            elif key.keysym.sym == self.keys[k]['left']:
                self.keys_state[k]['left'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.LEFT] = self.keys_state[k]['left']
                joystick[Joystick.Switch.RIGHT] = (self.keys_state[k]['right']
                                                   and not self.keys_state[k]['left'])
                return True
            elif key.keysym.sym == self.keys[k]['right']:
                self.keys_state[k]['right'] = key.state is sdl2.SDL_PRESSED
                joystick[Joystick.Switch.RIGHT] = self.keys_state[k]['right']
                joystick[Joystick.Switch.LEFT] = (self.keys_state[k]['left']
                                                  and not self.keys_state[k]['right'])
                return True
        return False

    def update_sdl_joystick(self, event, input):
        for k in range(2):
            if not isinstance(self.controllers[k], SDLDevice):
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


class PaddleInterface():
    def __init__(self):
        self.controllers = [None]*4
        self.velocities = [0, 0, 0, 0]
        self.keys = [
            {
                'left': sdl2.SDLK_a,
                'right': sdl2.SDLK_d,
                'fire': sdl2.SDLK_SPACE,
            },
            {
                'left': None,
                'right': None,
                'fire': None,
            },
            {
                'left': None,
                'right': None,
                'fire': None,
            },
            {
                'left': None,
                'right': None,
                'fire': None,
            }
        ]
        self.keys_state = [{
            'fire': False,
            'left': False,
            'right': False,
        } for k in range(4)]

    def associate_sdl_joystick(self, joystick_index, verbose=False):
        device = SDLDevice(joystick_index)
        for k in range(4):
            if self.controllers[k] != device:
                self.controllers[k] = device
                if verbose:
                    print(f"Associated device {device} to paddle {k}")
                return True
        return False

    def disassociate_sdl_device(self, device_id):
        for k in range(2):
            if (self.controllers[k].joystick == device_id or
                    self.controllers[k].game_controller == device_id):
                self.controllers[k] = None

    def reset(self):
        self.__init__()

    def __del__(self):
        self.reset()

    def update(self, key, input):
        for k in range(4):
            paddle = input.peripheral[k]
            if key.keysym.sym == self.keys[k]['fire']:
                paddle.fire = sdl2.SDL_PRESSED
                return True
            elif key.keysym.sym == self.keys[k]['left']:
                self.keys_state[k]['left'] = key.state is sdl2.SDL_PRESSED
                self.velocities[k] = -2 if self.keys_state[k]['left'] else + \
                    2 if self.keys_state[k]['right'] else 0
                return True
            elif key.keysym.sym == self.keys[k]['right']:
                self.keys_state[k]['rigth'] = key.state is sdl2.SDL_PRESSED
                self.velocities[k] = +2 if self.keys_state[k]['rigth'] else - \
                    2 if self.keys_state[k]['right'] else 0
                return True
        return False

    def update_sdl_joystick(self, event, input):
        for k in range(4):
            if not isinstance(self.controllers[k], SDLDevice):
                continue
            paddle = input.peripheral[k]
            if event.type == sdl2.SDL_JOYAXISMOTION:
                if event.jaxis.which == self.controllers[k].joystick_id:
                    if event.jaxis.axis == 0:
                        self.velocities[k] = event.jaxis.value / 10
                        return True
            elif event.type == sdl2.SDL_JOYBUTTONUP or event.type == sdl2.SDL_JOYBUTTONDOWN:
                if event.jbutton.which == self.controllers[k].joystick_id:
                    paddle.fire = (
                        event.jbutton.state == sdl2.SDL_PRESSED)
                    return True
            elif event.type == sdl2.SDL_JOYHATMOTION:
                if event.jhat.which == self.controllers[k].joystick_id:
                    if (
                            event.jhat.value == sdl2.SDL_HAT_RIGHTUP or
                            event.jhat.value == sdl2.SDL_HAT_RIGHT or
                            event.jhat.value == sdl2.SDL_HAT_RIGHTDOWN
                    ):
                        self.velocities[k] = 10
                        return True
                    if (
                            event.jhat.value == sdl2.SDL_HAT_LEFTUP or
                            event.jhat.value == sdl2.SDL_HAT_LEFT or
                            event.jhat.value == sdl2.SDL_HAT_LEFTDOWN
                    ):
                        self.velocities[k] = -10
                        return True
        return False

    def integrate(self, input):
        for k in range(4):
            angle = input.peripheral[k].angle + self.velocities[k]
            input.peripheral[k].angle = max(-135, min(135, angle))

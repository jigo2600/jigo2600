#  debug.py
#  Debugger support

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
from jigo2600 import M6502

def cpu_info(cpu):
    return (
        f"M6507: address:{cpu.address_bus:04x} data:{cpu.data_bus:02x} RW:{cpu.RW} irq:{cpu.irq_line} nmi:{cpu.nmi_line} reset:{cpu.reset_line} num_cycles:{cpu.num_cycles}\n" +
        f"M6507: A:{cpu.A:02x} X:{cpu.X:02x} Y:{cpu.Y:02x} S:{cpu.S:02x} IR:{cpu.IR:02x} PC:{cpu.PC:04x}\n" +
        f"M6507: PCP:{cpu.PCP:04x} T:{cpu.T} TP:{cpu.TP} AD:{cpu.AD:02x} ADD:{cpu.ADD:02x}\n" +
        f"M6507: {cpu.PCIR:04x}  {M6502.decode(cpu.IR)}"
    )

def pia_info(pia):
    return (
        f"M6532: ram{pia.ram}\n" +
        f"M6532: port A:{pia.port_A:02x} ORA:{pia.ORA:02x} DDRA:{pia.DDRA:02x}\n" +
        f"M6532: port B:{pia.port_B:02x} ORB:{pia.ORB:02x} DDRA:{pia.DDRB:02x}\n" +
        f"M6532: timer INTIM:{pia.INTIM:02x} interval:{pia.timer_interval} counter:{pia.timer_counter} interrupt:{pia.timer_interrupt} enabled:{pia.timer_interrupt_enabled}\n" +
        f"M6532: pos edge:{pia.positive_edge_detect} pa7 interrupt:{pia.pa7_interrupt} enabled:{pia.pa7_interrupt_enabled}\n"
    )
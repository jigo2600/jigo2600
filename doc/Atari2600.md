# The Atari 2600

[TOC]

## Overview

The main components of the Atari 2600 are: the [M6507 CPU](M6502.md), the [M6532 chip](#M6532), providing 128 bytes of RAM, IO ports, and a timer, and the [Televisor Interface Adapter (TIA) chip](TIA.md), providing graphics and sound. The CPU is also connected to an interchangeable [game cartridge](#cart), usually containing up to 6 KB of read-only memory containing the program to execute.

The M6507 CPU is clocked at 1.19 MHz. However, about 50% of the time the CPU is busy controlling the generation of the composite TV signal via the TIA chip. The TIA chip, which operates at three times the CPU frequency (3.58 MHz), performs most of the work required to generate the TV signal for one scanline; however, the CPU must reprogram the TIA on the fly in order to make any change in the graphic for the next scanline. Furthermroe, the CPU changes the horizontal positioning of graphical objects such as sprites by trigger the generation of the visual object  *when the TV raster beam is at desired horizontal location*. (objects are re-triggered automatically at the same location in each subsequent scanline).

This process, known as *racing the beam*, is perhaps the most unique aspect of programming the Atari 2600. The difficulty is that the CPU program must work in lockstep with the TV raster beam. While the M6507 CPU is simple enough that instruction timing can be inferred reliably via inspection, programming the graphics is still rather difficult.

This page provides some general information about the Atari 2600. Specific topics are addressed in the following pages:

- The [TIA](TIA.md) sound and graphic co-processor
- The [M6532](M6432.md) interface adapter (also called PIA or RIOT)
- The [M6507](M6502.md) CPU.

## Interconnections and address map

The following figures shows how chips are interconnected in the Atari 2600 motherboard:

![Schematic_Atari2600_2000](http://www.atariage.com/2600/archives/schematics/images/Schematic_Atari2600A_2000.png)

The [M6507 CPU](M6502.md)  exposes only 13 of the internal 16 address bits of the M6502 version of the processor, `A[0]` to `A[12]`. The CPU is connected to three chips:

* The [game cartridge](#cart). This device has 12 address bits connected to `A[0:11]`  and is selected (`CS`) by `A[12]=1` . In most cases, it consists of a ROM, but it can also contain additional RAM or even more complex electronics.
* The [TIA](TIA.md). This chip has 6 address bits connected to `A[0:5]` and is selected (`CS`) by `A[7,12]=[0,0]`.  Internally, the `Rw` line is used in combination with the addresses to select different registers.
* The [M6532 chip](M6532.md). This chip has 7 address bits connected to `A[0:6]` and is selected (`CS`) by  `A[7,12]=[1,0]`.  The line `RSnot`, connected to `A[9]`, acts as an eight address bit as it selects either the chip registers or the RAM. Internally,the `Rw` line is used in combination with the addresses to select different registers.

The address space can thus be summarise as follows:

| Device           | **Register**   | **Rw**   | **A[12]** | **A[11:8]** | **A[7:4]** | **A[3:0]** | **Example** |
| ---------------- | -------------- | -------- | -------- | ------- | ------- | -------- | ------------ |
| **Cartridge**    |                | `.`      | `1`      | `rrrr`  | `rrrr`  | `rrrr`   | `0xf000`      |
| **TIA**          |                | `.`      | `0`      | `....`  | `0...`  | `....`   | `0x0000`      |
| **TIA**          | Register Read  | `1`      | `0`      | `....`  | `0.rr`  | `rrrr`   | `0x0000`      |
| **TIA**          | Register Write | `0`      | `0`      | `....`  | `0...`  | `rrrr`   | `0x0000`      |
| **PIA**          |                | `.`      | `0`      | `....`  | `1...`  | `....`   | `0x0080`      |
| **PIA RAM**      |                | `.`      | `0`      | `..0.`  | `1rrr`  | `rrrr`   | `0x0080`      |
| **PIA Register** |                | `.`      | `0`      | `..1.`  | `1...`  | `....`   | `0x0280`      |
| **PIA Register** | IO ports       | `.`      | `0`      | `..1.`  | `1...`  | `.0rr`   | `0x0280`      |
| **PIA Register** | INTIM          | `1`      | `0`      | `..1.`  | `1...`  | `.1.0`   | `0x0284`      |
| **PIA Register** | INSTAT         | `1`      | `0`      | `..1.`  | `1...`  | `.1.1`   | `0x0285`      |
| **PIA Register** | EDGCTR         | `0`      | `0`      | `..1.`  | `1..0`  | `.1ii`   | `0x0284`      |
| **PIA Register** | TxxT           | `0`      | `0`      | `..1.`  | `1..1`  | `i1rr`   | `0x0294`      |

In the table, `.` means that the bit is ignored, `r` that it is used to select a particular register or memory location, and `i` that it is used as additional argument for an operation when that location is strobed. Note that in some cases the fact that the operation is a read or a write should be considered part of the addressing scheme as it changes how the other bits are interpreted. In other words, the `Rw` bit can be though of as an additional address bit.

## Switches and peripherals

The Atari 2600 has six switches: reset, select, difficulty left, difficulty right, and color mode (the first two work as push buttons).

Peripherals such as joysticks are connected through 9-pin ports. There are several peripherals types, summarised in the following schematics:

![Schematic_Atari2600_Accessories_1000](http://www.atariage.com/2600/archives/schematics/images/Schematic_Atari2600_Accessories_1000.png)

The 9 pins in each port are in turn connected to pins of the [M6532](PIA.md) and [TIA](TIA_Ports.md) coprocessors. There are three types of ports which implement different functionalities:

* **M6532 ports.** The M6532 pins  `PA0-7` and `PB0-7`  can be individually configured as input or output.  When a port is in **input** mode, the co-processors "lightly" pulls up the corresponding pin, so the connected peripheral needs to pull it down in order to register a 0 (which is usually obtained by closing a switch to ground). When the port is configured in output mode, the co-processor dirves its voltage "firmly" in order to write data to a peripheral.

  A practical example of this functionality is the keyboard controller, where "writing" to the keyboard allows the machine to read different rows of buttons. To read a key, the M6532 pulls down a keyboard row line. Then, if a switch on that row is closed (i.e. the button pressed), then the corresponding column is pulled down, which is registed in another port configured instead as input.

* **TIA dumped ports.** The TIA pins `I0-3` offer normally high-impedance and can be used to read the level of the connected line, either high or low. However, the TIA can also "dump" the ports, i.e. drive the pins to ground (as if they were outputs). The pins are connected to RC cirucits, so this feature can be used to [read analogue devices](TIA_Ports.md#analogue) such as paddles by measuring the RC time constant.
* **TIA latched ports.** The TIA pins `I4-5` offer high-impedance. Since they are connected to pull-up resitors in the motherboard, a peripheral can register a 0 by closing to ground the corresponding line. 

The port pins, interconnections, and purposes are summarised in the following tables:

| Switch            | Connection | Comment                                                      |
| ----------------- | ---------- | ------------------------------------------------------------ |
| Start/reset       | M6532 PB0  | Reset the game (reset occurs when the switch is closed, so that the port reads 0) |
| Select            | M6532 PB1  | Select the next option (when the switch is closed and the port reads 0) |
| Unused            | M6532 PB2  | Setting this port to an output allows to use it as storage (this is because written data is buffered by the M6532 and  can be read back). |
| B/W vs color mode | M6532 PB3  | Select color mode (when the switch is open and the port reads 1) or black and white mode. The port is hard-wirred to read 0 (black and white) in the SECAM version of Atari 2600. |
| Unused            | M6532 PB4  | See above.                                                   |
| Unused            | M6532 PB5  | See above.                                                   |
| Difficulty left   | M6532 PB6  | Choose between difficulty A (open switch, port reads 1) and B. |
| Difficulty right  | M6532 PB7  | Difficulty A corresponds to an open switch (reads 1).        |

| Right port pin | Connection | Right joystick                                               | Right pair paddles      | Right keyboard                                               |
| -------------- | ---------- | ------------------------------------------------------------ | ----------------------- | ------------------------------------------------------------ |
| 1              | M6532 PA0  | Up. PA0 must be configured to read the line; moving the joystick *up* pulls down PA0. |                         | Select row 0, i.e. the top row. PA0 must be configured to write the line; the row is selected by writing a 0. |
|                | M6532 PA1  | Down (read op)                                               |                         | Select row 1 (write op)                                      |
| 3              | M6532 PA2  | Left (read op)                                               | Paddle 1 fire (read op) | Select row 2 (write op)                                      |
| 4              | M6532 PA3  | Right (read op)                                              | Paddle 2 fire (read op) | Select row 3, i.e. the bottom row (write op)                 |
| 5              | TIA I2     |                                                              | Paddle 1 angle          | Read a button in column 0 (left)                             |
| 6              | TIA I5     | Fire                                                         |                         | Read column 2 (right)                                        |
| 7              | +VCC       |                                                              |                         |                                                              |
| 8              | GND        |                                                              |                         |                                                              |
| 9              | TIA I3     |                                                              | Paddle 2 angle          | Read column 1(middle)                                        |

| Left port pin | Connection | Left joystick | Left pair of paddles | Left keyboard         |
| ------------- | ---------- | ------------- | -------------------- | --------------------- |
| 1             | M6532 PA4  | Up            |                      | Select row 0 (top)    |
| 2             | M6532 PA5  | Down          |                      | Select row 1          |
| 3             | M6532 PA6  | Left          | Paddle 1 fire        | Select row 2          |
| 4             | M6532 PA7  | Right         | Paddle 2 fire        | Select row 3 (bottom) |
| 5             | TIA I0     |               | Paddle 1 angle       | Read column 0 (left)  |
| 6             | TIA I4     | Fire          |                      | Read column 2 (right) |
| 7             | +VCC       |               |                      |                       |
| 8             | GND        |               |                      |                       |
| 9             | TIA I1     |               | Paddle 2 angle       | Read column 1(middle) |

## Cartridges

In their most simple form, a cartridge is a read-only memory bank with 13 address bits and an 8-bit wide words. In a cycle where the CPU issue a read operation setting the address lines `A[0:12]` (chip select is the same as `A[12]`), the ROM responds by placing the corresponding data by the end of that cycle, so the data can be used by the CPU in the cycle immediately following that.

ROMs have 12 usable addresses (as the 13-th bit is used for chip select), so they can index 4096 bytes, or 4KiB. Hence, addresses are of the form `0x1000`, although the most significant nibble can be anything that has the first bit turned on (since the other bits are ignored); thus it is customary to write them as `0xfxxx`.

The M6507 CPU has also two special addresses, which map to the ROM address space:

- The entry point vector (program address) `0xfffc`-`0xfffd`  and
- The  break point vector (breakpoint rountine address)  `0xfffe`-`0xffff`.

The ROM must return approriate values when these locations are read.

The `RW` line is not connected at all to the ROM, so it is ignored. The CPU shoud thus not attempt to write the data bus while addressing the cartridge, as that would short-circuit the ROM and CPU drivers. An exception is for cartridge that do contain a RAM extension, as explained below.

### Banking

In oder to allow for larger cartridges, **banking** is used. Banking amounts to strobing (reading) certain special memory locations in the cartridge address space. This causes an internal state to switch, after which further read operations pull data from a different area (bank) of the ROM. There are different conventions, discussed below.

#### Standard banking

2KiB and 4KiB cartridges require no banking (for 2KB cartridges address bit `A[11]` is ignored). For other sizes (8KiB, 12KiB, 16KiB and 32KiB) bank switching is obtained by strobing (reading from) the following addresses:

| Cart size | Strobe addresses     | Banks  |
| --------- | -------------------- | ------ |
| 2 KiB     | --                   | 0      |
| 4 KiB     | --                   | 0      |
| 8 KiB     | `0xfff8` to `0xfff9` | 0 to 1 |
| 12 KiB    | `0xfff8` to `0xfffa` | 0 to 2 |
| 16 KiB    | `0xfff6` to `0xfff9` | 0 to 3 |
| 32 KiB    | `0xfff4` to `0xfffb` | 0 to 7 |

#### E0 banking for 8KiB cartridges

Such cartridges divide their 4KiB address space into 1KiB blocks. Each block can then be mapped to any of eight banks (except the last block which is always mapped to bank 7).

| Mapped bank address  | Strobe adress        |                    |
| -------------------- | -------------------- | ------------------ |
| `0xf000` to `0xf3ff` | `0xffe0` to `0xffe7` | Select bank 0 to 7 |
| `0xf400` to `0xf7ff` | `0xffe8` to `0xffef` | Select bank 0 to 7 |
| `0xf800` to `0x3bff` | `0xfff0` to `0xfff7` | Select bank 0 to 7 |
| `0xfc00` to `0xffff` | --                   | Always bank 7      |

#### FE banking for 8KiB cartridges

This is a rare banking mechanism (used by only by Robot Tank and Decathlon) described in [patent EP0116455A2](https://patentimages.storage.googleapis.com/e0/cc/4c/84313d3e9d027c/EP0116455A2.pdf). The cartridge looks for the value `0x1fe` on the address bus (looking at chip select and the  lower 12 address bits), and at the *next cycle* sniffs the value on the data bus, which, for JSR and RTS instructions, contains the highest byte of the 16-bit jump address. This address is then used to select a bank by looking at bit 14 (or bit `0x20` in the highest byte): if the bit is 1 bank 0 is selected and if the bit is 0 bank 1 is selected.

#### F0 banking for 16KiB cartridges

This scheme is only used by Mega Boy. The cart is similar to a standard 16KiB cart, but there is a single strobe address, `0xfff0`, accessing which causes the active bank to increase by one (and eventually wrap back to 0).

#### Other banking methods

There are several other rare banking methods.

### RAM

Some cartridges contain a RAM expansion. Since the cartridge is not connected to the `RW` line from the CPU, writes and reads must use different addresses.

| Cart type | RAM size | Write addresses      | Read addresses       |
| --------- | -------- | -------------------- | -------------------- |
| Standard  | 128 B    | `0xf000` to `0xf07f` | `0xf080` to `0xf0ff` |
| Standard  | 256 B    | `0xf000` to `0xf0ff` | `0xf100` to `0xf1ff` |
| ?         | 256 B    | `0xf800` to `0xf8ff` | `0xf900` to `0xf9ff` |

# References

* http://blog.kevtris.org/blogfiles/Atari%202600%20Mappers.txt
* https://problemkaputt.de/2k6specs.htm
* http://nesdev.com//6502_cpu.txt


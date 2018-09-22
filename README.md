Jigo2600
========

Jigo2600 is an open-source Atari 2600 emulator. The emulator is cycle-perfect. While only the core system and most common cartridges and peripherals are emulated, the source code is very compact as it consists only of a handful of C++ files. The documentation includes an extensive [analysis](doc/Atari2600.md) of the console and of the [TIA](doc/TIA.md) chip in particular. A basic driver in Python is provided to test the emulator.

The code is released under the simplified BSD license (see the COPYING file).

Quick start
-----------

The Python driver is the easiest way to test the emulator.

1. Download a game ROM. There are several free roms at [Atari Age](http://www.atariage.com/software_list.html?SystemID=2600&searchRarity=11). For example:

        curl -L http://atariage.com/2600/roms/GoFish_SP.zip  | bsdtar -xf-

2. Install Python3, SDL2, and pybind11. The easiest way is to install [Anaconda](https://www.anaconda.com/download/) or [Miniconda](https://conda.io/miniconda.html) and then issuing the command:

        conda env create -f python/env-conda.yaml
        conda activate jigo2600

3. Compile the emulator:

        make -C python

4. Run the emulator:

        python3 python/jigo2600emu.py GoFish_NTSC.bin

Versions
--------

* 1.0-beta1 (October 2018) - First release.


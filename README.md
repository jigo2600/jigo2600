Jigo2600
========

Jigo2600 is an open-source Atari 2600 emulator. The emulator is cycle-perfect. While only the core system and most common cartridges and peripherals are emulated, the source code is very compact as it consists only of a handful of C++ files. The documentation includes an extensive [analysis](doc/Atari2600.md) of the console and of the [TIA](doc/TIA.md) chip in particular. A basic driver in Python is provided to test the emulator.

The code is released under the simplified BSD license (see the COPYING file).

Quick start
-----------

The Python driver is the easiest way to test the emulator.

1. Install Python3, SDL2, and pybind11. The easiest way is to install [Anaconda](https://www.anaconda.com/download/) or [Miniconda3](https://conda.io/miniconda.html) and then issue the command:

        conda env create -f python/conda.yaml
        conda activate jigo2600

2. Compile and install the package in the environment just created:

        pip install .

3. Download a game ROM. There are several free roms at [Atari Age](http://www.atariage.com/software_list.html?SystemID=2600&searchRarity=11). For example:

        curl -L http://atariage.com/2600/roms/GoFish_SP.zip -o tmp.zip && unzip -o tmp.zip && rm tmp.zip

4. Run the emulator:

        python -m jigo2600.emulator GoFish_NTSC.bin

Versions
--------

* 1.0-beta2 (October 2018) - Better packaging and peripherals support.
* 1.0-beta1 (October 2018) - First release.

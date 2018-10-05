call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
set src= ^
../src/Atari2600.cpp ^
../src/Atari2600Cartridge.cpp ^
../src/M6502.cpp ../src/M6502Disassembler.cpp ^
../src/M6532.cpp ^
../src/TIA.cpp ^
../src/TIASound.cpp

for /F "tokens=* USEBACKQ" %%F in (`python -m pybind11 --includes`) do (
set pybind11_includes=%%F
)

cl /EHsc /Ox /GL /I..\src\ %pybind11_includes% /LD jigo2600.cpp %src% ^
/link  /LTCG /LIBPATH:"C:\"%HOMEPATH%"\Miniconda3\pkgs\python-3.7.0-hc182675_1004\libs" /OUT:jigo2600.pyd

del jigo2600.lib
del jigo2600.exp
del Atari2600.obj
del Atari2600Cartridge.obj
del M6502.obj
del M6502Disassembler.obj
del M6532.obj
del TIA.obj
del TIASound.obj

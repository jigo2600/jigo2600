rem if "%ARCH%" == "32" (set PLATFORM=x86) else (set PLATFORM=x64)
rem call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" %PLATFORM%
rem set DISTUTILS_USE_SDK=1
rem set MSSdk=1
"%PYTHON%" setup.py install
if errorlevel 1 exit 1
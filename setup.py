from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import sys
import setuptools
import distutils

__version__ = '1.0.0'


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        return pybind11.get_include(self.user)

 
class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    def build_extensions(self):
        # Get the default compiler options for extensions.
        c_opts = []

        # Platform-dependent options.
        pl = sys.platform
        if pl == 'darwin':
            pass
        elif pl == 'nt':
            pass

        # Compiler-dependent options.
        ct = self.compiler.compiler_type
        if ct == 'unix':
            c_opts += [f'-DVERSION_INFO="{self.distribution.get_version()}"']
            c_opts += ['-fvisibility=hidden']
            c_opts += ['-std=c++14']
        elif ct == 'msvc':
            c_opts += [f'/DVERSION_INFO="{self.distribution.get_version()}"']
            c_opts += ['/EHsc']


        # Set the options for each target.
        for ext in self.extensions:
            ext.extra_compile_args = c_opts

        # Build the extension.
        build_ext.build_extensions(self)


setup(
    name='jigo2600',
    version=__version__,
    author='Jigo2600 Team',
    author_email='jigo2600@gmail.com',
    url='https://github.com/jigo2600/jigo2600',
    description='An Atari 2600 emulator',
    long_description=open('README.md').read(),
    packages=['jigo2600'],
    package_dir={'': 'python'},
    package_data={'jigo2600': ['gamecontrollerdb.txt', 'cartridges.json']},
    ext_modules=[
        Extension(
            'jigo2600.core',
            [
                'python/jigo2600/core.cpp',
                'src/Atari2600.cpp',
                'src/Atari2600Cartridge.cpp',
                'src/M6502.cpp',
                'src/M6502Disassembler.cpp',
                'src/M6532.cpp',
                'src/TIA.cpp',
                'src/TIASound.cpp',
            ],
            include_dirs=[
                'src/',
                get_pybind_include(),
                get_pybind_include(user=True)
            ],            
            language='c++'
        ),
    ],
    install_requires=['pybind11>=2.2'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)

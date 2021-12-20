# DECX

This is the git version of the http://code.google.com/p/lagrange c++ version

# Usage

To be written...

## Build instructions

DECX requires that the following dependencies be installed on your system:
- [GFortran]
- [GSL]
- [Boost]
- [LAPACK]
- [CMake]

[gfortran]: https://gcc.gnu.org/wiki/GFortran
[gsl]: https://www.gnu.org/software/gsl/
[boost]: https://www.boost.org/
[LAPACK]: http://www.netlib.org/lapack/
[CMake]: https://cmake.org/

Then, build DECX with:

```bash
# Get source code from public MBB repo.
git clone https://gitlab.mbb.univ-montp2.fr/ibonnici/decx.git

# Choose a build directory.
cd decx/
mkdir build
cd build

# Configure compilation with CMake.
# This step is supposed to fail if a dependency cannot be found on your system.
cmake ../src

# Compile.
make -j 8 # Or any number of workers you can handle.

# Run with desired config file.
./decx my_config_file.txt
```

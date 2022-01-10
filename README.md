# DECX

This is the git version of the http://code.google.com/p/lagrange c++ version

## Usage

To be written...

## Build instructions

### With Singularity

DECX can be run from a singularity container,
provided you have [Singularity] installed.

First, build the container image as root:
```
# Get definition file.
curl https://raw.githubusercontent.com/champost/DECX/main/singularity.def \
     > decx_singularity.def

# Build container.
sudo singularity build decx.sif decx_singularity.def
```

Then, run the container as regular user:
```
./decx.sif my_config_file.txt
```


[Singularity]: https://sylabs.io/

### From source

Install the following dependencies on your system:
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
git clone https://github.com/champost/DECX

# Choose a build directory.
cd decx/
mkdir build
cd build

# Configure compilation with CMake.
# This step is supposed to fail if a dependency cannot be found on your system.
cmake ../src

# Compile.
make -j $(nproc)

# Run with desired config file.
./decx my_config_file.txt
```

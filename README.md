# DECX

This is the git version of the http://code.google.com/p/lagrange c++ version

#### 🚧 🚧 🚧 Project status (2022-04-23) 🚧 🚧 🚧

In October 2021, DECX was running fine,
but it was difficult to install and to configure.
I am currently in the process
of easing installation and configuration of the software,
adding tests, documentation, and easing output retrieval.
Should you have any trouble using it,
or should you like to contribute,
please don't hesitate to file issue/PRs on this repo
and to ping me @iago-lito :)

## Usage

To be written...

## Build instructions

### With Singularity

DECX can be run from a singularity container,
provided you have [Singularity] installed.

First, build the container image as root:
```
# Get definition file.
curl -L \
  https://github.com/champost/DECX/releases/latest/download/singularity.def \
  > decx_singularity.def # (or download by hand)

# Build image.
sudo singularity build decx.sif decx_singularity.def
```

Then run the container as regular user:
```
./decx.sif my_config_file.toml
```

[Singularity]: https://sylabs.io/

### With Docker

DECX can be run from a docker container,
provided you have [Docker] installed.

First, build the container image:
```
# Build image from distant file.
docker buildx build           \
  --build-arg GID=$(id -g)    \
  --build-arg UID=$(id -u)    \
  --build-arg GNAME=$(id -gn) \
  --build-arg UNAME=$(id -un) \
  -t decx                     \
  https://github.com/champost/DECX/releases/latest/download/Dockerfile
```

Then run the container:
```
docker run --rm -it -v ${PWD}:/home/decx decx my_config.file.toml
```

[Docker]: https://www.docker.com/


### From source

Install the following dependencies on your system:
- [GFortran]
- [Boost]
- [CMake]
- [OpenBlas]
- [GSL]
- [LAPACK]

[gfortran]: https://gcc.gnu.org/wiki/GFortran
[boost]: https://www.boost.org/
[CMake]: https://cmake.org/
[OpenBlas]: https://www.openblas.net/
[gsl]: https://www.gnu.org/software/gsl/
[LAPACK]: http://www.netlib.org/lapack/

Then, build DECX with:

```bash
# Get source code from public MBB repo.
git clone --recursive https://github.com/champost/DECX

# Choose a build directory.
cd DECX/
mkdir build
cd build

# Configure compilation with CMake.
# This step is supposed to fail if a dependency cannot be found on your system.
cmake -DCMAKE_BUILD_TYPE=Release ..

# Compile.
make -j $(nproc)

# Run with desired config file.
./decx my_config_file.txt
```

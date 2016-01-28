# DECX

This is the git version of the http://code.google.com/p/lagrange c++ version

# Usage

To be written...

## Build instructions

### Linux

To build lagrange\_cpp binary you need to install :

* lib armadillo dev
* gfortran
* lib nlopt dev
* lib gcc dev
* lib gsl dev
* lib boost dev

Under Debian-like systems :

```
sudo apt-get install gfortran libnlopt-dev libarmadillo-dev gcc g++ libgsl0-dev libboost-dev
```

Go to src/ directory and type

```
make -f makefile.champak clean
make -f makefile.champak
```

It should produce ```lagrange_cpp``` dynamic binary.

You may also want a static binary :

```
make -f makefile.champak clean
make -f makefile.champak static
```

### Windows

What i did to be able to compile on windows :

* Install git with git-bash from https://git-scm.com/
* Install minGW32 from http://www.mingw.org/download/installer
* Install all dev packages inside minGW
* Edit ~/.bashrc in git-bash to change PATH : ```export PATH=$PATH:/c/MinGW/bin/```
* Download lib GSL sources from ftp://ftp.gnu.org/gnu/gsl/
* Compile them
* Download lib boost from http://sourceforge.net/projects/boost/files/boost-binaries/1.60.0/
* Install it in C:\local\boost_1_60_0
* Adjust the src/makefile.win to fit with my files paths
* Run ```make -f makefile.win static``` to get a static bin
* Be satisfied

### MacOS

To get things right under MacOS, you'll need _fink_ or another pseudo-package
manager. The goal is to install correct gcc/g++ and the needed libs :

```
git clone https://github.com/fink/fink.git
cd fink
# the next command is quite long
sudo ./bootstrap
echo ". /sw/bin/init.sh" > ~/.bashr_profile
. ~/.bashrc_profile
# this might also be very long
sudo fink selfupdate-cvs
# and the longest is...this one
sudo fink install gcc49 boost1.55-python27 gsl

cd
cd DECX/src
make -f makefile.mac clean
make -f makefile.mac
```

I'm working on a way to produce a quasi-static binary for MacOSX.
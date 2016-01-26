# DECX

This is the git version of the http://code.google.com/p/lagrange c++ version

## Build instructions

To build lagrange\_cpp binary you need to install :

* lib armadillo dev
* gfortran
* lib nlopt dev
* lib gcc dev
* lib gsl dev

Under Debian-like systems :

```
sudo apt-get install gfortran libnlopt-dev libarmadillo-dev gcc g++ libgsl0-dev
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

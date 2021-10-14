cmake_minimum_required(VERSION 3.9.0)
set(CMAKE_CXX_STANDARD 17)

project(decx VERSION 0.21.0 LANGUAGES C CXX Fortran)

# Check C++/Fortran compatibility according to:
# https://enccs.github.io/cmake-workshop/cxx-fortran/
include(FortranCInterface)
FortranCInterface_VERIFY(CXX)
# Some warning have turned into a hard error since gfortran > 10.
# Ignore and compile anyway (https://forum.abinit.org/viewtopic.php?f=2&t=5205).
set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")

# Dependencies.
find_package(GSL REQUIRED)
find_package(Boost 1.59.0 REQUIRED)

# Release mode.
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(
  decx
  main.cpp
  # Fortran sources
  blas.f
  lapack.f
  my_expokit.f
  my_matexp.f
  # C++ sources
  AncSplit.cpp
  BioGeoTree.cpp
  BioGeoTreeTools.cpp
  BranchSegment.cpp
  InputReader.cpp
  OptimizeBioGeo.cpp
  RateMatrixUtils.cpp
  RateModel.cpp
  Utils.cpp
  node.cpp
  superdouble.cpp
  tree.cpp
  tree_reader.cpp
)

target_link_libraries(decx ${GSL_LIBRARIES})

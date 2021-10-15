cmake_minimum_required(VERSION 3.9.0)
set(CMAKE_CXX_STANDARD 17)

project(decx VERSION 0.21.0 LANGUAGES C CXX Fortran)

# Check C++/Fortran compatibility according to:
# https://enccs.github.io/cmake-workshop/cxx-fortran/
include(FortranCInterface)
fortrancinterface_verify(CXX)
# Some warning have turned into a hard error since gfortran > 10.
# Ignore and compile anyway (https://forum.abinit.org/viewtopic.php?f=2&t=5205).
set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")

# Dependencies.
find_package(GSL REQUIRED)
find_package(Boost 1.59.0 REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

# Release mode.
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_Fortran_FLAGS_RELEASE "-O3")

add_executable(
  decx main.cpp

  # For some reason, these fortran dependencies have been edited
  # so their sources need to stick around as long as no one is competent
  # to update this edition to "expokit" and "matexp".
  my_expokit.f
  my_matexp.f

  # C++ sources
  AncSplit.cpp
  BioGeoTree.cpp
  BioGeoTreeTools.cpp
  BranchSegment.cpp
  InputReader.cpp
  node.cpp
  OptimizeBioGeo.cpp
  RateMatrixUtils.cpp
  RateModel.cpp
  superdouble.cpp
  tree.cpp
  tree_reader.cpp
  Utils.cpp
)

target_link_libraries(
  decx
  ${GSL_LIBRARIES}
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES}
)

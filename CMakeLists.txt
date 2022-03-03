cmake_minimum_required(VERSION 3.9..3.22)

project(decx VERSION 0.21.0 LANGUAGES C CXX Fortran)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Check C++/Fortran compatibility according to:
# https://enccs.github.io/cmake-workshop/cxx-fortran/
include(FortranCInterface)
fortrancinterface_verify(CXX)
# Some warnings have turned into a hard error since gfortran > 10.
# Ignore and compile anyway (https://forum.abinit.org/viewtopic.php?f=2&t=5205).
set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -fallow-argument-mismatch")

# Dependencies.
find_package(GSL REQUIRED)
find_package(Boost 1.59.0 REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)

# Optimize release build.
if(CMAKE_BUILD_TYPE STREQUAL "Release")

  # Inter-procedural optimization.
  include(CheckIPOSupported)
  check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
  if(IPO_SUPPORTED)
    message(STATUS "IPO / LTO supported..")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "IPO / LTO enabled for all targets.")
  else()
    message(STATUS "IPO / LTO not supported: <${IPO_ERROR}>")
  endif()

  # Optimize for local architecture.
  set(flag "-march=native")
  foreach(LG IN ITEMS C CXX Fortran)
    string(TOLOWER ${LG} lg)
    include(Check${LG}CompilerFlag)
    cmake_language(
      CALL check_${lg}_compiler_flag
      ${flag} ${LG}_MARCH_NATIVE_SUPPORTED
    )
    if(${LG}_MARCH_NATIVE_SUPPORTED)
      string(APPEND CMAKE_${LG}_FLAGS_RELEASE " ${flag}")
      message(STATUS "${LG}: ${flag} enabled for all targets.")
    endif()
  endforeach()

  # -O3 and -DNDebug are automatically passed in release mode.
endif()

# External source code dependencies.
add_subdirectory(extern)

# Main application.
add_subdirectory(src)

#!/usr/bin/bash -e

# Compile the project then forward binary to the tests sets.

# Navigate to repo root.
root_folder=$(git rev-parse --show-toplevel)
cd $root_folder

# Compile project.
build_folder="${root_folder}/build"
mkdir -p ${build_folder}
cd ${build_folder}
cmake ..
make -j $(nproc)

# Find clibate.
clibate_exec="${root_folder}/extern/clibate/main.py"

# Come back here for testing.
cd "${root_folder}/tests/input"

${clibate_exec} specs/main.clib -i input

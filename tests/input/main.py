#!/usr/bin/python
"""Call DECX with various inputs
and check that its behaviour on stdout and stderr is consistent with expectations.
Assume it's run from the repo.
"""
from edit import edit
from popen import popen
from folders import test_folder, dummy_input_files_folder, temp_folder, build_folder
from manager import Manager

from multiprocessing import cpu_count
from pathlib import Path
import shutil as shu
import os

# Navigate to repo root.
p = popen("git rev-parse --show-toplevel")
if p.wait():
    raise Exception("Cannot find repo root folder.")
root_folder = Path(p.stdout.read().decode().strip())
os.chdir(root_folder)

# Compile project.
build_folder = Path(root_folder, build_folder)
build_folder.mkdir(exist_ok=True)
os.chdir(build_folder)
if os.system("cmake .."):
    raise Exception("Cannot configure with Cmake.")
if os.system(f"make -j {cpu_count()}"):
    raise Exception("Could not compile project.")
binary = Path(build_folder, "decx")

# Check that base test input files exist.
test_folder = Path(root_folder, *test_folder)
if not test_folder.exists():
    raise Exception(f"Could not find input tests folder {test_folder}.")
os.chdir(test_folder)

# Construct temporary test environment.
temp_folder = Path(test_folder, temp_folder)
if temp_folder.exists():
    raise Exception(f"Temporary folder {temp_folder} already exists: won't replace.")
temp_folder.mkdir()
os.chdir(temp_folder)

# Construct copies to be modified during tests.
input_files = ["config.toml", "newick.tre", "distrib_legacy.txt", "adjacency.txt"]
for file in input_files:
    path = Path(test_folder, dummy_input_files_folder, file)
    if not os.path.isfile(path):
        raise Exception(f"Could not find test input file {path}.")
    shu.copy(path, temp_folder)

# Run the program with config files.
config = input_files[0]
flag = "--check-distribution-file-parsing"
cmd = f"{binary} {config} {flag}"

# Read and execute tests from a dedicated test file.
tests_file = Path(test_folder, "tests")
m = Manager(cmd, tests_file)
while m.step():
    pass

m.summary()

# Remove temporary test environment.
os.chdir(root_folder)
shu.rmtree(temp_folder)

#!/usr/bin/python
"""Call DECX with various inputs
and check that its behaviour on stdout and stderr is consistent with expectations.
Assume it's run from the repo.
"""
from subprocess import Popen, PIPE, STDOUT
from multiprocessing import cpu_count
from pathlib import Path
import shutil as shu
import os


def popen(cmd):
    """Capture shell command output."""
    return Popen(
        cmd,
        shell=True,
        stdin=PIPE,
        stdout=PIPE,
        stderr=PIPE,
        close_fds=True,
    )


# Navigate to repo root.
p = popen("git rev-parse --show-toplevel")
if p.returncode:
    raise Exception("Cannot find repo root folder.")
root_folder = Path(p.stdout.read().decode().strip())
os.chdir(root_folder)

# Compile project.
build_folder = "build"
build_folder = Path(root_folder, build_folder)
build_folder.mkdir(exist_ok=True)
os.chdir(build_folder)
if os.system("cmake .."):
    raise Exception("Cannot configure with Cmake.")
if os.system(f"make -j {cpu_count()}"):
    raise Exception("Could not compile project.")
binary = Path(build_folder, "decx")

# Check that base test input files exist.
test_folder = ["tests", "input"]
test_folder = Path(root_folder, *test_folder)
if not test_folder.exists():
    raise Exception(f"Could not find input tests folder {test_folder}.")
os.chdir(test_folder)

# Construct temporary test environment.
temp_folder = "temp"
temp_folder = Path(test_folder, temp_folder)
if temp_folder.exists():
    raise Exception(f"Temporary folder {temp_folder} already exists: won't replace.")
temp_folder.mkdir()
os.chdir(temp_folder)

# Construct copies to be modified during tests.
input_files = ["config.toml", "newick.tre", "distrib_legacy.txt", "adjacency.txt"]
for file in input_files:
    path = Path(test_folder, "dummy_files", file)
    if not os.path.isfile(path):
        raise Exception(f"Could not find test input file {path}.")
    shu.copy(path, temp_folder)

# Run the program with config files.
config = input_files[0]
flag = "--check-distribution-file-parsing"
cmd = f"{binary} {config} {flag}"
os.system(cmd)

# Remove temporary test environment.
os.chdir(root_folder)
shu.rmtree(temp_folder)

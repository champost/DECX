#!/usr/bin/python
"""Call DECX with various inputs
and check that its behaviour on stdout and stderr is consistent with expectations.
Assume it's run from the repo.
"""
from edit import edit
from popen import popen
from folders import build_folder
from manager import Manager

from multiprocessing import cpu_count
from pathlib import Path
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

# Prepare shell command.
flag = "--check-distribution-file-parsing"
cmd = f"{binary} config.toml {flag}"

# Setup temporary test folder and read tests from the tests file.
m = Manager(cmd, root_folder)
while m.step():
    pass

m.summary()

m.clean()

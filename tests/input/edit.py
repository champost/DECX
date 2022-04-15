"""Introduce modifications to input files.
"""
from popen import popen
from folders import dummy_input_files_folder

from pathlib import Path
import shutil as shu


def edit(file, diff):

    # First, get a fresh copy from upstairs.
    source = Path(Path.cwd().parent, dummy_input_files_folder, file)
    target = Path(Path.cwd(), file)
    shu.copyfile(source, target)

    # Now edit the file on a line-by-line basis. First match.
    with open(file, "r") as handle:
        content = handle.readlines()

    before, after = diff
    for i, line in enumerate(content):
        if line.strip().startswith(before):
            break
    content[i] = after + '\n'

    with open(file, "w") as handle:
        handle.writelines(content)

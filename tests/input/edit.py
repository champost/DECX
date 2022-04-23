"""Introduce modifications to input files.
Modifications are encoded into "diff lines".
Either the line starst with a keyword like COMMENT or UNCOMMENT,
or it's a sequence of two lines with the line to be matched and its replacement.
"""
from popen import popen
from folders import dummy_input_files_folder

from pathlib import Path
import shutil as shu
import re

indent = re.compile(r"\s*")


def edit(file, difflines):

    # First, get a fresh copy from upstairs.
    source = Path(Path.cwd().parent, dummy_input_files_folder, file)
    target = Path(Path.cwd(), file)
    shu.copyfile(source, target)

    # Now edit the file on a line-by-line basis.
    with open(file, "r") as handle:
        content = handle.readlines()

    # Process diff lines one by one, and perform one edit on every pair collected.
    pair = []
    itd = iter(difflines)
    insert = False
    while True:
        try:
            d = next(itd)
        except StopIteration:
            break
        # Spot and process keywords first.
        kw = "COMMENT"
        if d.startswith(kw + " "):
            before = d.replace(kw, "", 1).strip()
            after = "# " + before
            pair = [before, after]

        kw = "UNCOMMENT"
        if d.startswith(kw + " "):
            after = d.replace(kw, "", 1).strip()
            before = "# " + after
            pair = [before, after]

        kw = "INDENT"  # (+ 4 leading spaces)
        if d.startswith(kw + " "):
            before = d.replace(kw, "", 1).strip()
            after = 4 * " " + before
            pair = [before, after]

        kw = "INSERT ABOVE"
        if d.startswith(kw + " "):
            after = d.replace(kw, "", 1).strip()
            try:
                before = next(itd)
            except StopIteration:
                raise Exception(f"No line provided to insert above '{after}'.")
            pair = [before, after]
            insert = True

        if len(pair) < 2:
            pair.append(d)

        if len(pair) == 2:
            # Resolve the pair.
            found = False
            before, after = pair
            sought = after if insert else before
            for i, line in enumerate(content):
                if line.strip().startswith(sought):
                    found = True
                    # Collect original leading whitespace
                    # to reset it after transformation.
                    blank = indent.match(line).group()
                    break
            if not found:
                raise Exception(
                    "Error in test file: "
                    f"could not find the following line in {file}:\n{sought}"
                )
            if insert:
                content.insert(i, before)
                insert = False
            else:
                content[i] = blank + after + "\n"
            pair = []

    with open(file, "w") as handle:
        handle.writelines(content)

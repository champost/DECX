"""Wrap testing primitives into a central object to read/construct/run the tests
and report errors in a contextualized way from a simple textual tests file.
"""
from edit import edit
from expect import expect_success, expect_error, TestFailure
from folders import (
    dummy_input_files_folder,
    test_folder as test_folder_path,
    temp_folder as temp_folder_path,
    specs_folder,
)

from pathlib import Path
import os
import re
import shutil as shu

# Small patterns to interpret key lines in tests file.
comments = re.compile(r"#.*")
chunk_separator = re.compile(r"\n\n+")
success_line = re.compile(r"Success: ?(.*)")
failure_line = re.compile(r"Failure: ?(.*)")
diff_line = re.compile(r"diff \((.*?)\):\s*(PERSIST)?")
error_line = re.compile(r"error \((.*?)\):")


class Manager(object):
    """Read and run test files in the specs folder.
    Happens within a dedicated temporary folder.
    """

    # Escape codes for coloring output.
    red = "\x1b[31m"
    blue = "\x1b[34m"
    green = "\x1b[32m"
    reset = "\x1b[0m"

    def __init__(self, cmd, root_folder):
        """Initialize the test manager and parse test file.
        cmd: The shell call whose output to collect.
        """
        self.root_folder = root_folder

        # Check that base test input files exist.
        self.test_folder = Path(root_folder, *test_folder_path)
        if not self.test_folder.exists():
            raise Exception(f"Could not find input tests folder {self.test_folder}.")
        os.chdir(self.test_folder)

        # Construct temporary test environment.
        self.temp_folder = Path(self.test_folder, temp_folder_path)
        if self.temp_folder.exists():
            raise Exception(
                f"Temporary folder {self.temp_folder} already exists: won't replace."
            )
        self.temp_folder.mkdir()
        os.chdir(self.temp_folder)

        self.reset_config_files()

        self.command = cmd

        self.files = os.listdir(Path(self.test_folder, specs_folder))

        self.failures = []  # Collect [(test file, test name, error message)]

        self.next_file()

    def next_file(self):
        """Go read next test files.
        Return false if none left.
        """
        try:
            self.test_file = self.files.pop(0)
        except IndexError:
            return False

        print(f"\nRead tests from {self.blue}{self.test_file}{self.reset}:")

        self.chunks = self.file_to_chunks(
            Path(self.test_folder, specs_folder, self.test_file)
        )

        # Filled along during chunks reading.
        self.test_name = None  # (the current test name)
        self.expect_success = None  # (boolean)
        self.expected_header = None
        self.expected_matrix = None
        self.expected_error_code = None
        self.expected_error_message = None
        # The scope of some diffs extend beyond the chunk
        # and until the end of the file.
        self.persistent_diffs = [] # [(file, difflines)]

        return True

    def file_to_chunks(self, path):
        with open(path, "r") as file:
            file = file.read()

        # Strip comments.
        file = comments.sub("", file).strip()

        # Separate into testing units.
        return chunk_separator.split(file)

    def reset_config_files(self):
        """Fill up temp folder (again) with dummy configuration files."""
        # Construct copies to be modified during tests.
        prefix = Path("..", dummy_input_files_folder)
        for file in os.listdir(prefix):
            path = Path(prefix, file)
            if not os.path.isfile(path):
                raise Exception(f"Could not find test input file {path}.")
            shu.copy(path, self.temp_folder)

    def step(self) -> bool:
        """Read and interpret next test chunk.
        Throw when a test is failing.
        Return False when exhausted.
        """
        try:
            chunk = self.chunks.pop(0).strip()
        except IndexError:
            if self.next_file():
                return self.step()
            return False

        # Clean slate.
        self.reset_config_files()

        # Record the data we expect to read on stdout.
        if chunk.startswith("Matrix:"):
            _, header, matrix = chunk.split("\n", 2)
            self.expected_header = header
            self.expected_matrix = matrix
            return True

        # Or perform the test.
        if m := success_line.match(chunk):
            self.expect_success = True
        elif m := failure_line.match(chunk):
            self.expect_success = False
        else:
            raise Exception(f"Error in test file: unrecognized chunk type:\n{chunk}")
        self.test_name = m.group(1)

        # Interpret lines in chunk.
        # ASSUMPTION: the tests chunks are correctly written.
        # No(t many) guards in the following parsing algorithm.
        dry_success = False
        try:
            _, chunk = chunk.split("\n", 1)
        except ValueError:
            if self.expect_success:
                dry_success = True
            else:
                raise Exception(
                    "Error in test file: "
                    "not enough information provided in failure test chunk."
                )
        if dry_success:
            self.run_test()
            return True

        diffs = [*self.persistent_diffs] # [(file, difflines)]
        difflines = []
        persist = False
        message = []  # stored by lines
        state = None
        chunk = chunk.split('\n')
        for i, line in enumerate(chunk + ["ENDOFCHUNK"]): # Loop once after termination.
            line = line.strip()
            # Are we entering a new state, and which one?
            change = True
            if m := diff_line.match(line):
                # Entering diff state.
                file = m.group(1)
                persist = bool(m.group(2))
                difflines = []
                state = "diff"
            elif m:= error_line.match(line):
                # Entering message state.
                self.expected_error_code = int(m.group(1))
                state = "message"
            else:
                change = False

            if change or i == len(chunk):
                # Out from previous "diff" state (maybe into a different "diff" state)
                if difflines:
                    diffs.append((file, difflines))
                    if persist:
                        self.persistent_diffs.append((file, difflines))
                difflines = []
                # The current line has already been processed.
                continue

            # Collect expected error.
            if state == "message":
                message.append(line)
            elif state == "diff":
                difflines.append(line)

        # Gather into one message.
        self.expected_error_message = "\n".join(m.strip() for m in message)

        # Perform required diffs.
        for file, difflines in diffs:
            edit(file, difflines)

        # Ready to run the test now.
        self.run_test()

        return True

    def run_test(self):
        print(f"Test: {self.test_name}..", end="")
        try:
            if self.expect_success:
                expect_success(self.command, self.expected_header, self.expected_matrix)
            else:
                expect_error(
                    self.command, self.expected_error_code, self.expected_error_message
                )
        except TestFailure as e:
            print(f" {self.red}FAIL{self.reset}")
            self.failures.append((self.test_file, self.test_name, e.message))
            return
        print(f" {self.green}PASS{self.reset}")

    def summary(self):
        if self.failures:
            print(f"\n{self.red}🗙{self.reset} The following tests failed:")
            for test_file, name, message in self.failures:
                print(
                    f"{self.blue}{name}{self.reset}"
                    f" ('{test_file}' test file)\n{message}"
                )
            return
        print(f"\n{self.green}✔{self.reset} Success.")

    def clean(self):
        "Remove temporary test environment."
        os.chdir(self.root_folder)
        shu.rmtree(self.temp_folder)

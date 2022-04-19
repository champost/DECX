"""Wrap testing primitives into a central object to read/construct/run the tests
and report errors in a contextualized way from a simple textual tests file.
"""
from edit import edit
from expect import expect_success, expect_error, TestFailure
from folders import (
    dummy_input_files_folder,
    test_folder as test_folder_path,
    temp_folder as temp_folder_path,
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
diff_line = re.compile(r"diff \((.*?)\):")
error_line = re.compile(r"error \((.*?)\):")


class Manager(object):
    """Read and run test file.
    Happens within a dedicated temporary folder.
    """

    # Escape codes for coloring output.
    red = "\x1b[31m"
    blue = "\x1b[34m"
    green = "\x1b[32m"
    reset = "\x1b[0m"

    def __init__(self, cmd, filename, root_folder):
        """Initialize the test manager and parse test file.
        cmd: The shell call whose output to collect.
        filename: Path to the test file.
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
        with open(Path(self.test_folder, filename), "r") as file:
            file = file.read()

        # Strip comments.
        file = comments.sub("", file).strip()

        # Separate into testing units.
        self.chunks = chunk_separator.split(file)

        # Filled along during chunks reading.
        self.test_name = None  # (the current test name)
        self.expect_success = None  # (boolean)
        self.expected_header = None
        self.expected_matrix = None
        self.expected_error_code = None
        self.expected_error_message = None
        self.failures = []  # Collect [(test name, error message)]

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

        diffs = []  # [(file, before, after)]
        message = ""
        state = None
        for line in chunk.split("\n"):
            # Collect required diffs.
            if m := diff_line.match(line):
                file = m.group(1)
                state = "before"
            elif state == "before":
                before = line
                state = "after"
            elif state == "after":
                after = line
                diffs.append((file, before, after))
                state = None
            # Collect expected error.
            elif m := error_line.match(line):
                self.expected_error_code = int(m.group(1))
                state = "message"
            elif state == "message":
                message += line.strip() + "\n"
        self.expected_error_message = message.strip()

        # Perform required diffs.
        for file, before, after in diffs:
            edit(file, (before.strip(), after.strip()))

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
            self.failures.append((self.test_name, e.message))
            return
        print(f" {self.green}PASS{self.reset}")

    def summary(self):
        if self.failures:
            print(f"{self.red}🗙{self.reset} The following tests failed:")
            for name, message in self.failures:
                print(f"{self.blue}{name}{self.reset}\n{message}")
            return
        print(f"{self.green}✔{self.reset} Success.")

    def clean(self):
        "Remove temporary test environment."
        os.chdir(self.root_folder)
        shu.rmtree(self.temp_folder)

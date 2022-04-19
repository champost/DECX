"""Wrap testing primitives into a central object to read/construct/run the tests
and report errors in a contextualized way from a simple textual tests file.
"""
from edit import edit
from expect import expect_success, expect_error

import re

# Small patterns to interpret key lines in tests file.
comments = re.compile(r"#.*")
chunk_separator = re.compile(r"\n\n+")
success_line = re.compile(r"Success: ?(.*)")
failure_line = re.compile(r"Failure: ?(.*)")
diff_line = re.compile(r"diff \((.*?)\):")
error_line = re.compile(r"error \((.*?)\):")


class Manager(object):
    """Read and run test file."""

    # Escape codes for coloring output.
    red = "\x1b[31m"
    green = "\x1b[32m"
    reset = "\x1b[0m"

    def __init__(self, cmd, filename):
        """Initialize the test manager and parse test file.
        cmd: The shell call whose output to collect.
        filename: Path to the test file.
        """
        self.command = cmd
        with open(filename, "r") as file:
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

    def step(self) -> bool:
        """Read and interpret next test chunk.
        Throw when a test is failing.
        Return False when exhausted.
        """
        try:
            chunk = self.chunks.pop(0).strip()
        except IndexError:
            return False

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
        except Exception as e:
            print(f" {self.red}FAIL{self.reset}")
            raise e
        print(f" {self.green}PASS{self.reset}")

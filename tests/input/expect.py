"""Interpret program output and compare to expectations.

When expecting success:
extract the matrix displayed during dry-run to compare to hardcoded one.

When expecting failure:
extract error code and stderr to compare to expected error message.
"""
from popen import popen


class TestFailure(Exception):
    def __init__(self, message):
        super().__init__(message)
        self.message = message


def expect_success(cmd: str, expected_matrix_lines: [str]):

    p = popen(cmd)

    if code := p.wait():
        message = p.stderr.read().decode()
        raise TestFailure(
            f"DECX failed while expecting a success (error code {code}):\n{message}"
        )

    # Extract matrix from output.
    output = p.stdout.read().decode()
    _, matrix = output.rsplit(".txt: ", 1)
    actual_lines = matrix.splitlines()

    # Compare matrices line by line without whitespace.
    for actual, expected in zip(actual_lines, expected_matrix_lines):
        actual = actual.strip().split()
        expected = expected.strip().split()
        if actual != expected:
            raise TestFailure(
                "Unexpected distribution line.\nExpected: {}\n  Actual: {}\n".format(
                    " ".join(expected), " ".join(actual)
                )
            )
    a = len(actual_lines)
    e = len(expected_matrix_lines)
    if a > e:
        raise TestFailure(
            "Unexpected distribution trailing lines:\n{}".format(
                "\n".join(actual_lines[e:])
            )
        )
    if e > a:
        raise TestFailure(
            "Missing distribution lines:\n{}".format(
                "\n".join(expected_matrix_lines[a:])
            )
        )


def expect_error(cmd, code, message):

    p = popen(cmd)

    if p.wait() == 0:
        raise TestFailure("DECX Succeeded while expecting a failure.")
    if p.returncode != code:
        raise TestFailure(
            "DECX errored out with the wrong code: "
            f"expected {code}, got {p.returncode}."
        )

    err = p.stderr.read().decode()
    if err.strip().split() != message.strip().split():
        raise TestFailure(
            "  DECX errored with the wrong message, "
            f"expected:\n{message}\n  got:\n{err}\n"
        )

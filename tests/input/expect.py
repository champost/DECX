"""Interpret program output and compare to expectations.

When expecting success:
extract the matrix displayed during dry-run to compare to hardcoded one.

When expecting failure:
extract error code and stderr to compare to expected error message.
"""
from popen import popen


def expect_success(cmd: str, expected_header: str, expected_body: str):

    p = popen(cmd)

    if code := p.wait():
        message = p.stderr.read().decode()
        raise Exception(
            f"DECX failed while expecting a success (error code {code}):\n{message}",
        )

    # Extract matrix from output.
    output = p.stdout.read().decode()
    _, matrix = output.rsplit(".txt: ", 1)
    header, body = matrix.split("\n", 1)

    # Compare headers without whitespaces.
    expected = header.strip().split()
    actual = expected_header.strip().split()
    if expected != actual:
        raise Exception(
            "Unexpected distribution header.\nExpected: {}\nActual: {}\n",
            " ".join(expected),
            " ".join(actual),
        )

    # Compare bodies without whitespaces.
    body = body.strip().splitlines()
    ebody = expected_body.strip().splitlines()
    for actual, expected in zip(body, ebody):
        actual = actual.strip().split()
        expected = expected.strip().split()
        if actual != expected:
            raise Exception(
                "Unexpected distribution line.\nExpected: {}\nActual: {}\n",
                " ".join(expected),
                " ".join(actual),
            )


def expect_error(cmd, code, message):

    p = popen(cmd)

    if p.wait() == 0:
        raise Exception("DECX Succeeded while expecting a failure.")
    if p.returncode != code:
        raise Exception(
            "DECX errored out with the wrong code: "
            f"expected {code}, got {p.returncode}."
        )

    err = p.stderr.read().decode()
    if err.strip().split() != message.strip().split():
        raise Exception(
            "DECX errored with the wrong message, "
            f"expected:\n{message}\ngot:\n{err}\n"
        )

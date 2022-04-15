"""Interpret program output and compare to expectations.

When expecting success:
extract the matrix displayed during dry-run to compare to hardcoded one.

When expecting failure:
extract error code and stderr to compare to expected error message.
"""
from subprocess import Popen

expected_header = "WP EP WN EN CA SA AF MD IN WA AU"  # Areas in config file.
expected_body = """
 Amis: 1 1 0 1 0 1 1 0 1 0 0
 Asin: 0 1 0 1 0 0 1 0 1 0 1
Ccroc: 1 1 0 1 0 1 0 0 1 0 1
 Clat: 1 1 0 0 0 0 0 0 1 0 0
 Cyac: 0 1 1 0 0 0 0 0 1 0 1
  Gav: 0 1 1 1 0 1 1 0 1 0 0
  Tom: 1 1 1 1 0 0 1 0 1 0 1
"""


def expect_success(p: Popen):

    if code := p.returncode:
        print(p.stderr.read().decode())
        raise Exception(f"DECX failed while expecting a success (error code {code}).")

    # Extract matrix from output.
    output = p.stdout.read().decode()
    _, matrix = output.rsplit(".txt: ", 1)
    header, body = matrix.split("\n", 1)
    if header != expected_header:
        raise Exception(
            "Unexpected distribution header.\n"
            f"Expected: {expected_header}\nActual: {header}\n"
        )

    # Compare bodies without whitespaces.
    body = body.strip().splitlines()
    ebody = expected_body.strip().splitlines()
    for actual, expected in zip(body, ebody):
        actual = actual.strip().split()
        expected = expected.strip().split()
        if actual != expected:
            raise Exception(
                "Unexpected distribution line.\n"
                f"Expected: {' '.join(expected)}\nActual: {' '.join(actual)}\n"
            )

"""Interface with external shell command to retrieve output.
"""
from subprocess import Popen, PIPE, STDOUT
import sys


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

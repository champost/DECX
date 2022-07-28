"""This reader checks for existence and conformity
of the distribution matrix within stdout, regardless of whitespace.

    distribution: # Soft match, normalized whitespace.

              area1 area2
        Sp1   0     1
        Sp2   1     1

"""

from checker import Checker
from exceptions import ParseError
from reader import Reader, LinesAutomaton

import re


class DistributionChecker(Checker):

    expecting_stdout = True

    def __init__(self, context):
        self.areas = []
        self.species = []
        self.data = []
        self.context = context

    def check(self, _rn, code, stdout, stderr):
        output = stdout.decode("utf-8")

        # Quick parse matrix within decx special testing output.
        try:
            _, _, matrix = re.split(r"\nDistribution (.*?):", output, 1)
        except Exception as e:
            return (
                f"Could not find legacy and full display in output:\n{output}\n"
                + repr(e)
                + (f"\nstderr ({code}):\n{stderr.decode('utf-8')}" if stderr else "")
            )
        lines = matrix.strip().split("\n")
        areas = lines.pop(0).split()
        species = []
        data = []
        for line in lines:
            sp, d = line.split(":")
            species.append(sp.strip())
            data.append([int(i) for i in d.split()])

        # Compare to expectations.
        if areas != self.areas:
            mess = "Expected to found distribution areas:\n"
            mess += " ".join(self.areas)
            mess += "\nfound instead:\n"
            mess += " ".join(areas)
            return mess

        if species != self.species:
            mess = "Expected to found distribution species:\n"
            mess += " ".join(self.species)
            mess += "\nfound instead:\n"
            mess += " ".join(species)
            return mess

        if data != self.data:
            mess = "Expected to found distribution data:\n  "
            mess += "\n  ".join(" ".join(str(i) for i in d) for d in self.data)
            mess += "\nfound instead:\n  "
            mess += "\n  ".join(" ".join(str(i) for i in d) for d in data)
            return mess

        return None


class DistributionReader(Reader):

    keyword = "distribution"

    def section_match(self, lex):
        self.introduce(lex)
        self.check_colon()
        self.check_empty_line()
        return DistributionAutomaton(self.keyword_context)


class DistributionAutomaton(LinesAutomaton):
    def __init__(self, *args, **kwargs):
        self.checker = DistributionChecker(*args, **kwargs)

    def feed(self, lex):
        if lex.find_empty_line():
            return
        c = self.checker

        cx = lex.lstrip().context
        line = lex.read_line().split()

        # The first line lists areas.
        if not c.areas:
            c.areas += line
            return

        # Subsequent lines are species + data.
        sp = line.pop(0)
        if not sp.endswith(":"):
            raise ParseError(f"Please end species name with colon ':'.", cx)
        c.species.append(sp[:-1])
        for i in line:
            if i not in ("1", "0"):
                raise ParseError(
                    f"{repr(i)} is not valid matrix data. Use '0' or '1'.",
                    cx,
                )
        if (a := len(line)) != (e := len(c.areas)):
            raise ParseError(f"Expected {e} data in line, found {a}.", cx)

        c.data.append([int(i) for i in line])

    def terminate(self):
        return self.checker

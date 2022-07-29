"""This reader checks for existence and conformity
of the adjacency matrix within stdout, regardless of whitespace.
Both the full data and the legacy display are verified.

    adjacency: # Soft match, normalized whitespace, here for two periods.

              area1 area2
        area1   0
        area2   1     1

              area1 area2
        area1   0
        area2   0     1

"""

from checker import Checker
from exceptions import ParseError
from reader import Reader, LinesAutomaton

import re


class AdjacencyChecker(Checker):

    expecting_stdout = True

    def __init__(self, context):
        self.areas = None
        self.data = []  # [periods:[line:[value]]]
        self.context = context

    def check(self, _rn, code, stdout, stderr):
        output = stdout.decode("utf-8")

        # Extract legacy display and full display.
        try:
            _, output = output.split("Reading adjacency matrix file...\n")
            legacy, _, full = re.split(r"\nAdjacency (.*?):\n", output, 1)
        except Exception as e:
            return (
                f"Could not find legacy and full display in output:\n{output}\n"
                + repr(e)
                + (f"\nstderr ({code}):\n{stderr.decode('utf-8')}" if stderr else "")
            )

        # Check legacy display.
        leg = legacy  # to be consumed.
        for p, period in enumerate(self.data):
            # Valid header.
            header, leg = leg.split("\n\n", 1)
            expected = "".join("\t" + a for a in self.areas)
            if header != expected:
                mess = f"Invalid header in adjacency legacy display, period {p}. "
                mess += f"Expected:\n{expected}\n"
                mess += f"Got:\n{header}\n"
                return mess + f"\nLegacy display:\n{legacy}"
            # Valid matrix with row namer.
            for i, (a, row) in enumerate(zip(self.areas, period)):
                name, leg = leg.split("\t", 1)
                if name != a:
                    mess = f"Invalid area name in adjacency legacy display, "
                    mess += f"period {p}, row {i}. "
                    mess += f"Expected: {repr(a)}. Got {repr(name)}."
                    return mess + f"\nLegacy display:\n{legacy}"
                for j in range(i + 1):
                    e = row[j]
                    a, leg = leg.split("\t", 1)
                    if e != a:
                        mess = f"Invalid adjacency value in legacy display, "
                        mess += f"period {p}, row {i}, column {j}. "
                        mess += f"Expected: {repr(e)}. Got {repr(a)}."
                        return mess + f"\nLegacy display:\n{legacy}"
                rest, leg = leg.split("\n", 1)
                if rest:
                    mess = "Unexpected data after end of adjacency legacy display line,"
                    mess += f" period {p}, row {i}: {repr(rest)}"
                    return mess + f"\nLegacy display:\n{legacy}"
            if leg:
                rest, leg = leg.split("\n", 1)
                if rest:
                    mess = "Unexpected data "
                    mess += "after end of adjacency legacy display matrix,"
                    mess += f"period {p}: {repr(rest)}"
                    return mess
        if leg:
            return (
                f"Unexpected data after end of adjacency legacy display: {repr(leg)}."
            )

        # Check full display.
        matrices = [
            [[v for v in row.split()] for row in p.strip().split("\n")]
            for p in full.strip().split("---") if p
        ]
        if matrices != self.data:
            mess = "Adjacency full data mismatch. Expected:\n"
            psep = "\n---\n"
            mess += psep.join(
                "\n".join(" ".join(row) for row in period) for period in self.data
            ) + psep
            mess += f"\nActual:\n{full}"
            return mess

        return None


class AdjacencyReader(Reader):

    keyword = "adjacency"

    def section_match(self, lex):
        self.introduce(lex)
        self.check_colon()
        self.check_empty_line()
        return AdjacencyAutomaton(self.keyword_context)


class AdjacencyAutomaton(LinesAutomaton):
    def __init__(self, *args, **kwargs):
        self.checker = AdjacencyChecker(*args, **kwargs)
        self.period = None  # The matrix currently being parsed.
        self.i = 0  # Current row number being expected.

    def new_period(self):
        c = self.checker
        n = len(c.areas)
        self.period = [n * [None] for _ in range(n)]
        c.data.append(self.period)
        self.i = 0

    def feed(self, lex):
        if lex.find_empty_line():
            # Start expecting a new period.
            self.period = None
            return
        c = self.checker
        cx = lex.lstrip().context
        line = lex.read_line().split()

        if not c.areas:
            # First time we see these areas.
            c.areas = line
            self.new_period()
            return

        if self.period is None:
            # Start a new period, with an expected header.
            if line != c.areas:
                raise ParseError(
                    f"Invalid areas header in period {len(c.data)}:\n"
                    f"{line}\nExpected it to be like the first one:\n{c.areas}",
                    cx,
                )
            self.new_period()
            return

        # Expect a named triangular line.
        p = len(c.data) - 1
        i = self.i
        name = line.pop(0)
        exp = c.areas[i]
        if name != exp:
            raise ParseError(
                f"Invalid area name in period {p}: {repr(name)} "
                f"expected same order as before: {repr(exp)}.",
                cx,
            )
        if len(line) != i + 1:
            raise ParseError(
                f"Expected {i+1} values on row {i}, period {p}, "
                f"found {len(line)} instead.",
                cx,
            )
        # Fill data row.
        period = c.data[p]
        for j, value in enumerate(line):
            period[i][j] = period[j][i] = value

        self.i += 1

    def terminate(self):
        return self.checker

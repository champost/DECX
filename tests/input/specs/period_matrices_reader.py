"""This reader checks for existence and conformity
of the adjacency/rates matrix within stdout, regardless of whitespace.
Both the full data and the legacy display are verified.

    <adjacency|rates>: # Soft match, normalized whitespace, here for two periods.

              area1 area2
        area1   0           ← triangular/binary for adjacency
        area2   1     1

              area1 area2
        area1   0     0.8   ← full/floating point for rates
        area2   0.1     1

"""

from checker import Checker
from exceptions import ParseError
from reader import Reader, LinesAutomaton

import re


class MatricesChecker(Checker):

    expecting_stdout = True

    def __init__(self, type: "adjacency" or "rates", context):
        self.type = type
        self.areas = None
        self.data = []  # [periods:[line:[value]]]
        self.context = context

    def check(self, _rn, code, stdout, stderr):
        output = stdout.decode("utf-8")

        # Extract legacy display and full display.
        try:
            if self.type == "adjacency":
                splitter = "Reading adjacency matrix file...\n"
            else:
                splitter = "Reading rate matrix file\n"
            _, output = output.split(splitter)
            legacy, _, full = re.split(
                r"\n{} (.*?):\n".format(self.type.title()), output, 1
            )
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
            if self.type == "adjacency":
                header, leg = leg.split("\n\n", 1)
                expected = "".join("\t" + a for a in self.areas)
                if header != expected:
                    mess = f"Invalid header in {self.type} legacy display, period {p}. "
                    mess += f"Expected:\n{expected}\n"
                    mess += f"Got:\n{header}\n"
                    return mess + f"\nLegacy display:\n{legacy}"
            # Valid matrix with row namer.
            for i, (a, row) in enumerate(zip(self.areas, period)):
                if self.type == "adjacency":
                    name, leg = leg.split("\t", 1)
                    if name != a:
                        mess = f"Invalid area name in {self.type} legacy display, "
                        mess += f"period {p}, row {i}. "
                        mess += f"Expected: {repr(a)}. Got {repr(name)}."
                        return mess + f"\nLegacy display:\n{legacy}"
                for j in range(i + 1 if self.type == "adjacency" else len(self.areas)):
                    e = row[j]
                    a, leg = leg.split("\t", 1)
                    if self.type == "rates":
                        a = 1.0 if a == " . " else float(a)
                    if e != a:
                        mess = f"Invalid {self.type} value in legacy display, "
                        mess += f"period {p}, row {i}, column {j}. "
                        mess += f"Expected: {repr(e)}. Got {repr(a)}."
                        return mess + f"\nLegacy display:\n{legacy}"
                rest, leg = leg.split("\n", 1)
                if rest:
                    mess = (
                        f"Unexpected data after end of {self.type} legacy display line,"
                    )
                    mess += f" period {p}, row {i}: {repr(rest)}"
                    return mess + f"\nLegacy display:\n{legacy}"
            if leg:
                rest, leg = leg.split("\n", 1)
                if rest:
                    mess = "Unexpected data "
                    mess += "after end of {self.type} legacy display matrix,"
                    mess += f"period {p}: {repr(rest)}"
                    return mess
        if leg:
            return (
                f"Unexpected data after end of {self.type} legacy display: {repr(leg)}."
            )

        # Check full display.
        convert = str if self.type == "adjacency" else float
        matrices = [
            [[convert(v) for v in row.split()] for row in p.strip().split("\n")]
            for p in full.strip().split("---")
            if p
        ]
        if matrices != self.data:
            mess = f"{self.type.title()} matrices full data mismatch. Expected:\n"
            psep = "\n---\n"
            mess += (
                psep.join(
                    "\n".join(" ".join(str(v) for v in row) for row in period)
                    for period in self.data
                )
                + psep
            )
            mess += f"\nActual:\n{full}"
            return mess

        return None


class MatricesReader(Reader):
    def __init__(self, keyword):
        self.keyword = keyword

    def section_match(self, lex):
        self.introduce(lex)
        self.check_colon()
        self.check_empty_line()
        return MatricesAutomaton(self.keyword, self.keyword_context)


class MatricesAutomaton(LinesAutomaton):
    def __init__(self, *args, **kwargs):
        self.checker = MatricesChecker(*args, **kwargs)
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
        exp = i + 1 if c.type == "adjacency" else len(c.areas)
        if len(line) != exp:
            raise ParseError(
                f"Expected {exp} values on row {i}, period {p}, "
                f"found {len(line)} instead.",
                cx,
            )
        # Fill data row.
        period = c.data[p]
        for j, value in enumerate(line):
            if c.type == "adjacency":
                period[i][j] = period[j][i] = value
            else:
                try:
                    period[i][j] = float(value)
                except ValueError:
                    raise ParseError(
                        f"Expected floating number, found {repr(value)} instead",
                        cx,
                    )
        self.i += 1

    def terminate(self):
        return self.checker


# Create both needed readers.
AdjacencyReader = lambda: MatricesReader("adjacency")
RatesReader = lambda: MatricesReader("rates")

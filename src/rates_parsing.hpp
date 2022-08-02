#pragma once

// Parse rates matrices files,
// resulting in the `RatesMap` structure
// dumped into the RateModel.set_Dmask_cell API before resuming program flow.
// The following (legacy) form still needs to be supported:
//
//    1 0 1 0 ← one line + column per area: 1 to indicate connection.
//    0 1 0 1    the matrix is not necessarily symmetric
//    1 0 1 1
//    0 1 1 1
//                           ← skip to separate another period
//    1       0 1    0.00001
//    0.00001 1 0.43 1
//    1       0 0.7  1       ← Explicit float values are input into the matrix.
//    0.43    0 1    0.7
//
// Now allow the following more flexible/readable forms:
//
//    optional row naming, or "namer" matching the header *and its order*.
//    ↓  a  b  c  d  ← optional header reordering area names
//    a  1  0  1  0
//    b  0  1  0  1
//    c  1  0  1  1
//    d  0  1  1  1
//
//    U = 0.43      ← optional variable definitions.
//    V = 0.7
//    e = 0.00001
//
//    1 0 1 e  ← Use them to define next period.
//    e 1 U 1
//    1 0 V 1
//    U 0 1 V
//
//    ~ a b U  ← relative specifications
//    ~ c d e
//    ~ c b 0

#include "lexer.hpp"

#include <string>
#include <vector>

namespace rates {

using RatesMap = std::vector<std::vector<std::vector<double>>>;
using Areas = std::vector<std::string>;

// Dedicate this code to errors with distribution files.
constexpr int RATES_ERROR{5};

RatesMap
parse_file(const File& file, const Areas& areas, const size_t n_periods);

} // namespace rates

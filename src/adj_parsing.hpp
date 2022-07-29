#pragma once

// Parse adjacency matrices files,
// resulting in the `AdjMap` structure
// apparently needed by the rest of the program.
// The following (legacy) form still needs to be supported:
//
//    1 0 1 0 ← one line + column per area: 1 to indicate connection.
//    0 1 0 1    the matrix must be symmetric,
//    1 0 1 1    but the upper triangle is optional (and ignored)
//    0 1 1 1    (we'll stop ignoring and check it as it causes confusion)
//            ← skip to separate another period
//    1 0 1 0
//    0 1 0 0 ← same matrix but modified according to the period.
//    1 0 1 1
//    0 0 1 1
//
// Now allow the following more flexible/readable forms:
//
//    optional row naming, or "namer" matching the header *and its order*.
//    ↓  a  b  c  d  ← optional header reordering area names
//    a  1  0  1  0
//    b  0  1  0  1
//    c  1  0  1  10 ← this value becomes 0 in the second period: compact form.
//    d  0  10 1  1
//
//  Allow upper triangle elision:
//
//       a  b  c  d
//    a  1
//    b  0  1
//    c  1  0  1
//    d  0  10 1  1
//
//  And diagonal elision.
//
//       a  b  c ← watch the uneven header/namer then.
//    b  0
//    c  1  0
//    d  0  10 1
//
//  With no namer, diagonalize the header for better readability
//
//    a
//    0 b
//    1 0 c
//    0 1 1 d
//
//    a       ← opt-out of the compat form and duplicate the matrix for changes.
//    0 b
//    1 0 c
//    0 0 1 d
//
//  Instead of repeating the whole matrix,
//  Just add or remove connections with relative + or - instructions.
//
//    a
//    0 b
//    1 0 c
//    0 1 1 d
//
//    - b d
//    + d a
//         ← separate periods with empty lines.
//    + a b
//    - b d

#include "lexer.hpp"

#include <string>
#include <vector>

namespace adjacency {

using AdjMap = std::vector<std::vector<std::vector<bool>>>;
using Areas = std::vector<std::string>;

// Dedicate this code to errors with distribution files.
constexpr int ADJ_ERROR{4};

AdjMap parse_file(const std::string_view filename,
                  const Areas& areas,
                  const size_t n_periods);

} // namespace adjacency

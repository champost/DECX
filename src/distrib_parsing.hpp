#pragma once

// Parse species distribution file,
// resulting in the `Map` structure
// apparently needed by the rest of the program.
// The following (legacy) form still needs to be supported:
//
//    4 10            <- number of species / number of areas
//    spA 0110101101  <- species name / distribution
//    spB 1010110101
//    spC 0111011100
//    spD 1101011010
//
// Now use the following more flexible/readable form,
// with a mix of various different types of species lines.
//
//    s\a   a b c d e f g h i  <- "species\areas" then optional areanames header
//    spA   1 0 1 1 0 1 0 1 1  <- indifferent whitespace to ease alignment
//    spB 1010110101           <- legacy style
//    spC + a f i              <- name areas in the distributions
//    spD - c e                <- define by *exclusion* of areas
//    spE + *                  <- include all areas with a glob pattern '*'
//
// Note that the list of area names is known prior to parsing,
// because it is specified in the general configuration file,
// considered source of truth in this regard.
// The header may then be omitted, or *complete* but locally reordered.
//
// Another option, if more comfortable, is to transpose the input:
//
//    a\s    spA spB spC spD  <- "area\species" to request transposition.
//    area_1 1   0   0   1
//    area_2 1101
//    area_3 + spC spD
//    area_4 - spB
//
// In this situation, the header becomes mandatory.

#include "lexer.hpp"

#include <map>
#include <string>
#include <vector>

namespace distribution {

using Map = std::map<std::string, std::vector<int>>;
using Areas = std::vector<std::string>;

// Dedicate this code to errors with distribution files.
constexpr int DISTRIB_ERROR{3};

Map parse_file(const File& file, const Areas& areas);

// The various types of distribution files
// are parsed differently.
// Separate the associated parsers into dedicated files.
Map legacy_parse(Lexer& lexer, const size_t n_species, const Areas& areas);
Map species_parse(Lexer& lexer, const Areas& config_areas); // s\a
Map areas_parse(Lexer& lexer, const Areas& config_areas);   // a\s

// Useful print options to debug.
std::ostream& operator<<(std::ostream& out, const Lexer::StepType& t);
std::ostream& operator<<(std::ostream& out, const Lexer::Step& s);

} // namespace distribution

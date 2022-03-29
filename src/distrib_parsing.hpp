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
//    spD - c e                <- define by *exclusion* of areas.
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

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace distribution {

using Map = std::map<std::string, std::vector<int>>;
using Areas = std::vector<std::string>;

// Dedicate this code to errors with distribution file.
constexpr int EXIT_ERROR{2};

Map parse_file(const std::string_view filename, const Areas& areas);

auto read_file(std::string_view path) -> std::string;

// Use this automaton to split input into lines and tokens.
// The lexer owns the input, and returns views to it.
class Lexer {
  const std::string input;
  // The focus always stands once after the last interesting part.
  // (which may be once after the end of input).
  // On every step, we analyze what's under focus,
  // then move it right after whatever we've found.
  size_t focus;

  // Keep track of linecol location within file.
  const std::string_view filename;
  size_t line;
  size_t llf; // Focus value at the end of Last Line.

public:
  Lexer(const std::string_view filename) :
      input(read_file(filename)),
      filename(filename),
      focus(0),
      line(1),
      llf(0){};

  enum StepType {
    Token,
    EndOfLine,
    EndOfFile,
  };

  struct Step {
    StepType type;
    std::optional<std::string_view> token;

    // Source information.
    std::string_view filename{};
    size_t line{0};
    size_t column{0};

    bool has_value() const { return type == StepType::Token; };
    bool is_eol() const { return type == StepType::EndOfLine; };
    bool is_eof() const { return type == StepType::EndOfFile; };
  };

  // Query for next token.
  Step step();

private:
  // Basic queries about current situation.
  bool no_input_left() const;
  bool newline() const;
};

// The various types of distribution files
// are parsed differently.
// Separate the associated parsers into dedicated files.
Map legacy_parse(Lexer& lexer, const Areas& areas);

// Useful print options to debug.
std::ostream& operator<<(std::ostream& out, const Lexer::StepType& t);
std::ostream& operator<<(std::ostream& out, const Lexer::Step& s);

} // namespace distribution

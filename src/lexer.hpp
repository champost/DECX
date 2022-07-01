#pragma once

// A simple lexer, useful to parse the various configuration files.

#include <string>
#include <optional>

auto read_file(std::string_view path) -> std::string;

// Dedicate this code to errors with the lexer.
constexpr int LEXING_ERROR{2};

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

    // Exit with error message pointing to current focus
    // + possible shift within current token.
    [[noreturn]] void source_and_exit(const size_t shift = 0) const;
  };

  // Query for next token.
  Step step();

private:
  // Basic queries about current situation.
  bool no_input_left() const;
  bool newline() const;
};


#include "distrib_parsing.hpp"

#include <fstream>
#include <iostream>

namespace distribution {

// https://stackoverflow.com/a/116220/3719101
auto read_file(std::string_view path) -> std::string {
  constexpr auto read_size = std::size_t(4096);
  auto stream = std::ifstream(path.data());
  stream.exceptions(std::ios_base::badbit);
  auto out = std::string();
  auto buf = std::string(read_size, '\0');
  while (stream.read(&buf[0], read_size)) {
    out.append(buf, 0, stream.gcount());
  }
  out.append(buf, 0, stream.gcount());
  return out;
}

Map parse_file(const std::string_view filename, const Areas& areas) {
  const auto file{read_file(filename)};
  Lexer lexer{filename};

  // Lex to first token.
  auto first{lexer.step()};
  while (first.is_eol()) { // Ignore leading blank/comment lines.
    first = lexer.step();
  }
  if (first.is_eof()) {
    std::cerr << "Error: distribution file '" << filename
              << "' is blank (or it contains only comments)." << std::endl;
    exit(EXIT_ERROR);
  }

  // Use first token to decide which parser to use.
  if (first.token == "s\\a") {
    std::cerr << "s\\a unimplemented." << std::endl;
    exit(-1);
  } else if (first.token == "a\\s") {
    std::cerr << "a\\s unimplemented." << std::endl;
    exit(-1);
  }
  return legacy_parse(lexer, areas);
};

Lexer::Step Lexer::step() {

#define CHECK_EOF()                                                            \
  if (no_input_left()) {                                                       \
    return {StepType::EndOfFile, {}, filename, line, focus - llf};             \
  }
#define CHECK_EOL()                                                            \
  if (newline()) {                                                             \
    ++focus; /* Consume it. */                                                 \
    const auto col{focus - llf};                                               \
    llf = focus;                                                               \
    return {StepType::EndOfLine, {}, filename, line++, col};                   \
  }

  // What's under focus?
  CHECK_EOF()
  CHECK_EOL()
  // Nothing interesting? Skip whitespace.
  while (std::isspace(input[focus])) {
    ++focus;
    CHECK_EOF();
    CHECK_EOL();
  }

  // Skip comments.
  if (input[focus] == '#') {
    while (true) {
      ++focus;
      CHECK_EOF();
      CHECK_EOL();
    }
  }

  // We've come accross a token: collect.
  const auto start{focus};
  while (!isspace(input[focus])) {
    ++focus;
    if (no_input_left()) {
      break;
    }
  }
  return {StepType::Token,
          std::string_view(input).substr(start, focus - start),
          filename,
          line,
          start - llf + 1};
};
bool Lexer::no_input_left() const { return focus == input.size(); };
bool Lexer::newline() const { return input[focus] == '\n'; };

std::ostream& operator<<(std::ostream& out, const Lexer::StepType& t) {
  switch (t) {
  case Lexer::StepType::Token: {
    out << "Token";
    break;
  }
  case Lexer::StepType::EndOfLine: {
    out << "EOL";
    break;
  }
  case Lexer::StepType::EndOfFile: {
    out << "EOF";
    break;
  }
  }
  return out;
};

std::ostream& operator<<(std::ostream& out, const Lexer::Step& s) {
  out << s.type << '(';
  if (s.has_value()) {
    out << *s.token << ", ";
  }
  out << s.filename << ':' << s.line << ':' << s.column << ')';
  return out;
};

} // namespace distribution

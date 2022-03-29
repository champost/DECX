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

  auto step{lexer.step()};
  if (step.is_eol()) {
    std::cout << "EOL" << std::endl;
  } else if (step.token.has_value()) {
    std::cout << "Token: '" << step.token.value() << "'" << std::endl;
  }
  while (!step.is_eof()) {
    step = lexer.step();
    if (step.is_eol()) {
      std::cout << "EOL" << std::endl;
    } else if (step.token.has_value()) {
      std::cout << "Token: '" << step.token.value() << "'" << std::endl;
    }
  }
  std::cout << "EOF" << std::endl;

  return {};
};

Lexer::Step Lexer::step() {

#define CHECK_EOF()                                                            \
  if (no_input_left()) {                                                       \
    return {StepType::EndOfFile, {}};                                          \
  }
#define CHECK_EOL()                                                            \
  if (newline()) {                                                             \
    ++focus; /* Consume it. */                                                 \
    return {StepType::EndOfLine, {}};                                          \
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

  // We've come accross a token: collect.
  const auto start{focus};
  while (!isspace(input[focus])) {
    ++focus;
    if (no_input_left()) {
      break;
    }
  }
  return {StepType::Token,
          std::string_view(input).substr(start, focus - start)};
};
bool Lexer::no_input_left() const { return focus == input.size(); };
bool Lexer::newline() const { return input[focus] == '\n'; };

} // namespace distribution

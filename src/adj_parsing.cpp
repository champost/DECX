#include "adj_parsing.hpp"

#include <iostream>

namespace adjacency {

AdjMap parse_file(const std::string_view filename,
                  const Areas& areas,
                  const size_t n_periods) {
  const auto file{read_file(filename)};
  Lexer lexer{filename};

  // Lex to first token.
  auto first{lexer.step()};
  while (first.is_eol()) { // Ignore leading blank/comment lines.
    first = lexer.step();
  }
  if (first.is_eof()) {
    std::cerr << "Error: adjacency file '" << filename
              << "' is blank (or it contains only comments)." << std::endl;
    exit(ADJ_ERROR);
  }

  // Initialize map to zero.
  const size_t n_areas{areas.size()};
  AdjMap map{std::vector(n_periods,
                         std::vector(n_areas, std::vector(n_areas, false)))};

  return map;
};

} // namespace adjacency

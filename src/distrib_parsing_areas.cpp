#include "distrib_parsing.hpp"

#include <iostream>

namespace distribution {

Map areas_parse(Lexer& lexer, const Areas& config_areas) {
  // Construct the transposed version {area: '10101'},
  // then de-transpose at the end.
  Map tmap{};

  // Mandatory header: the total list of species about to be processed.
  auto next{lexer.step()};
  std::vector<std::string> species{};
  while (next.has_value()) {
    species.emplace_back(*next.token);
    next = lexer.step();
  }
  if (species.empty()) {
    std::cerr << "Error: no species lines provided in distribution file header "
              << "(sought because of 'a\\s' specification)." << std::endl;
    next.source_and_exit();
  }

  return tmap;
}

} // namespace distribution

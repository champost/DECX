#include "distrib_parsing.hpp"

#include <iostream>

namespace distribution {

Map areas_parse(Lexer& lexer, const Areas& config_areas) {
  // Construct the transposed version {area: '10101'},
  // then de-transpose at the end.
  Map tmap{};

  // The following is then heavily inspired or DUPLICATED from species_parse,
  // with adjusted identifiers and error handling.

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

  // Now start parsing areas lines.
  while (!next.has_value()) {
    next = lexer.step();
    if (next.is_eof()) {
      std::cerr << "Error: no areas lines provided in distribution file."
                << std::endl;
      next.source_and_exit();
    }
  }

  // Loop on areas lines,
  // assuming that every iteration starts on a valid first token in line.
  while (!next.is_eof()) {

    const std::string area{*next.token};
    // The area must exist in the config file.
    bool found{false};
    for (const auto a : config_areas) {
      if (area == a) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "Error: area '" << area
                << "' not found in configuration file." << std::endl;
      next.source_and_exit();
    }

    // "Community" is the equivalent to "distribution" in species_parse.
    std::vector<int> community{};
    community.reserve(species.size());

    std::cout << "area: " << area << std::endl;
    break;
  }

  return tmap;
}

} // namespace distribution

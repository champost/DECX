#include "distrib_parsing.hpp"

#include <iostream>

namespace distribution {

Map species_parse(Lexer& lexer, const Areas& config_areas) {
  Map map{};

  // Check whether a header is given with a reordering of areas.
  auto next{lexer.step()};
  std::vector<std::string> areas{};
  areas.reserve(config_areas.size());
  while (next.has_value()) {
    const std::string_view area(*next.token);
    bool found{false};
    for (const auto a : config_areas) {
      if (area == a) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::cerr << "Error: area name '" << area
                << "' found in distribution file does not match any "
                   "area name given in configuration file."
                << std::endl;
      next.source_and_exit();
    }
    areas.emplace_back(area);
    next = lexer.step();
  }
  if (areas.size()) {
    // There is a reordering: check consistency.
    if (areas.size() != config_areas.size()) {
      std::cerr
          << "Error: the number of areas in the distribution file header ("
          << areas.size() << ") "
          << "does not match the number of areas given in the configuration "
             "file ("
          << config_areas.size() << ")." << std::endl;
      next.source_and_exit();
    }
  } else {
    // No reordering: consider that the original order is relevant then.
    std::cout << "No header provided in distribution file: consider areas "
                 "order from config file."
              << std::endl;
    areas = config_areas;
  }

  std::cout << "s\\a parsing only implemented up to this point." << std::endl;
  exit(-1);
  return map;
}

} // namespace distribution

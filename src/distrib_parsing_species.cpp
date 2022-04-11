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

  // Now start parsing species lines.
  while (!next.has_value()) {
    next = lexer.step();
    if (next.is_eof()) {
      std::cerr << "Error: no species lines provided in distribution file."
                << std::endl;
      next.source_and_exit();
    }
  }

  // Loop on species lines,
  // assuming that every iteration starts on a valid first token in line.
  while (!next.is_eof()) {

    const std::string species{*next.token};
    if (map.count(species)) {
      std::cerr << "Error: species '" << species << "' is specified twice."
                << std::endl;
      next.source_and_exit();
    }

    // The rest of the line specifies the distribution of this species.
    std::vector<int> distribution{};
    distribution.reserve(areas.size());

    // There are several types of specification lines.
    // Infer the correct type based on first next token.
    next = lexer.step();
    if (!next.has_value()) {
      std::cerr << "Error: no distribution information provided for species "
                << species << "." << std::endl;
      next.source_and_exit();
    }

    // When given + or -, the next tokens are area names counted within or
    // without the distribution from a uniform 0 or 1 base. Factorize procedure.
    auto plus_minus =
        [&areas, &distribution, &next, &lexer, &config_areas](int base) {
          // (treat brutally: fill with zeroes then linear-search every
          // remaining token on the line to flip corresponding element in
          // distribution to 1).
          for (size_t i{0}; i < areas.size(); ++i) {
            distribution.emplace_back(base);
          }
          next = lexer.step();
          while (next.has_value()) {
            const auto& area{*next.token};
            size_t i_area{0};
            bool found{false};
            for (const auto& a : config_areas) {
              if (a == area) {
                found = true;
                break;
              }
              ++i_area;
            }
            if (!found) {
              std::cerr << "Error: unrecognized area name: '" << area << "'"
                        << std::endl;
              next.source_and_exit();
            }
            distribution[i_area] = 1 - base;
            next = lexer.step();
          }
        };

    if (*next.token == "+") {
      plus_minus(0);
    } else if (*next.token == "-") {
      plus_minus(1);
    } else {
      std::cerr << "Next token " << *next.token << " not implemented yet."
                << std::endl;
      next.source_and_exit();
    }

    map.insert({species, distribution});

    // Go to next species line.
    while (next.is_eol()) {
      next = lexer.step();
    }
  }

  return map;
}

} // namespace distribution

#include "distrib_parsing.hpp"

#include <functional>
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
    std::cerr << "Error: no species lines provided ";
    std::cerr << "in distribution file header ";
    std::cerr << "(expected because of 'a\\s' specification)." << std::endl;
    next.source_and_exit();
  }

  // Now start parsing areas lines.
  while (!next.has_value()) {
    next = lexer.step();
    if (next.is_eof()) {
      std::cerr << "Error: no areas lines provided ";
      std::cerr << "in distribution file." << std::endl;
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
      std::cerr << "Error: area '" << area << "' ";
      std::cerr << "not found in configuration file." << std::endl;
      next.source_and_exit();
    }
    // Also, it must not already be in the map.
    if (tmap.count(area)) {
      std::cerr << "Error: area '" << area << "' ";
      std::cerr << "is specified twice." << std::endl;
      next.source_and_exit();
    }

    // "Community" is the equivalent to "distribution" in species_parse.
    std::vector<int> community{};
    community.reserve(species.size());

    // There are several types of specification lines.
    // Infer the correct type based on first next token.
    next = lexer.step();
    if (!next.has_value()) {
      std::cerr << "Error: no community information provided ";
      std::cerr << "for area " << area << "." << std::endl;
      next.source_and_exit();
    }

    // When given + or -, the next tokens are area names counted within or
    // without the distribution from a uniform 0 or 1 base. Factorize procedure.
    auto plus_minus = [&species, &community, &next, &lexer](int base) {
      // (treat brutally: fill with zeroes then linear-search every
      // remaining token on the line to flip corresponding element in
      // distribution to 1).
      for (size_t i{0}; i < species.size(); ++i) {
        community.emplace_back(base);
      }
      next = lexer.step();
      while (next.has_value()) {
        const auto& single_species{*next.token};
        size_t i_species{0};
        bool found{false};
        for (const auto& s : species) {
          if (s == single_species) {
            found = true;
            break;
          }
          ++i_species;
        }
        if (!found) {
          std::cerr << "Error: unrecognized species name: ";
          std::cerr << "'" << single_species << "' ";
          std::cerr << "(not found in file header)" << std::endl;
          next.source_and_exit();
        }
        community[i_species] = 1 - base;
        next = lexer.step();
      }
    };

    // Use incoming token to determine the line style.
    const auto determinant{*next.token};
    if (determinant == "+") {
      plus_minus(0);
    } else if (determinant == "-") {
      plus_minus(1);
    } else {
      // In this case, expect only ones and zeroes.
      for (const auto c : determinant) {
        if (c != '0' && c != '1') {
          std::cerr << "Error: could not interpret token ";
          std::cerr << "'" << determinant << "' ";
          std::cerr << "as an area community specification." << std::endl;
          std::cerr << "Consider using ones and zeroes, ";
          std::cerr << "or '+' or '-' to explicitly name species." << std::endl;
          next.source_and_exit();
        }
      }

      // Fill with zeroes then flip only elements at the correct indexes.
      for (size_t i{0}; i < species.size(); ++i) {
        community.emplace_back(0);
      }

      // Either we are reading one token like 0010101,
      // or it's split into 0 0 1 0 1 0 1.
      // In any case, the "reading" procedure is the same.
      auto read = [&community, &next, &species](std::function<char()> step,
                                                const bool split) {
        size_t i_digit{0};
        while (true) {
          const auto digit{step()}; // `step` also checks digits correctness.
          if (digit == '\0') {
            // No more digits to read.
            break;
          }
          if (i_digit == species.size()) {
            std::cerr << "Error: too many digits in community specification: ";
            std::cerr << "expected " << species.size() << ", ";
            std::cerr << "got at least " << i_digit + 1 << "." << std::endl;
            next.source_and_exit((split) ? 0 : i_digit);
          }
          // Only flip relevant digits in the right places.
          if (digit == '1') {
            community[i_digit] = 1;
          }
          ++i_digit;
        }
        if (i_digit != species.size()) {
          std::cerr << "Error: not enough digits in community specification: ";
          std::cerr << "expected " << species.size() << ", ";
          std::cerr << "got only " << i_digit << "." << std::endl;
          next.source_and_exit((split) ? 0 : i_digit);
        }
      };

      // Then, depending on the situation,
      // use the right way to "step" from one digit to the next.
      if (determinant.size() > 1) {
        // The distribution is one single token with all digits inside.
        size_t i{0};
        auto step = [&next, &determinant, &i]() -> char {
          if (i < determinant.size()) {
            const auto& c{determinant[i]};
            if (c != '1' && c != '0') {
              std::cerr << "Error: expected 1 or 0, ";
              std::cerr << "got '" << c << "'." << std::endl;
              next.source_and_exit(i - 1);
            }
            ++i;
            return c;
          }
          return '\0';
        };
        read(step, false);
        // There should be nothing left on this line.
        next = lexer.step();
        if (next.has_value()) {
          std::cerr << "Error: unexpected token '";
          std::cerr << *next.token << "'." << std::endl;
          next.source_and_exit();
        }
      } else {
        // The distribution is split into several tokens.
        bool first{true}; // Remain on the token read, so don't step at first.
        auto step = [&next, &lexer, &first]() -> char {
          if (!first) {
            next = lexer.step();
          }
          first = false;
          if (next.has_value()) {
            char res;
            if (*next.token == "1") {
              res = '1';
            } else if (*next.token == "0") {
              res = '0';
            } else {
              std::cerr << "Error: expected 1 or 0, ";
              std::cerr << "got '" << *next.token << "'." << std::endl;
              next.source_and_exit();
            }
            return res;
          }
          return '\0';
        };
        read(step, true);
        next = lexer.step(); // Step off the last token read.
      }
    }

    // Record.
    tmap.insert({area, community});

    // Go to next species line.
    while (next.is_eol()) {
      next = lexer.step();
    }
  }

  // Check for missing areas.
  if (tmap.size() < config_areas.size()) {
    std::vector<std::string_view> missing{};
    for (const auto& a : config_areas) {
      if (!tmap.count(a)) {
        missing.push_back(a);
      }
    }
    std::cerr << "Error: not all areas defined in configuration file ";
    std::cerr << "have been specified." << std::endl;
    std::cerr << "Missing:";
    for (const auto& a : missing) {
      std::cerr << " " << a;
    }
    std::cerr << std::endl;
    next.source_and_exit();
  }

  // Transpose the map from {area: 10101} entries to {species: 1010}.
  // Brutal: fill an empty map first,
  // then linear-search the areas list to get the correct index every time.
  Map map;
  for (const auto& s : species) {
    std::vector<int> zeroes(config_areas.size(), 0);
    map.insert({s, zeroes});
  }
  for (const auto& tentry : tmap) {
    const auto& [area, community] = tentry;
    size_t i_area{0};
    for (const auto& a : config_areas) {
      if (a == area) {
        break;
      }
      ++i_area;
    }
    size_t i_species{0};
    for (const auto& s : species) {
      map[s][i_area] = community[i_species];
      ++i_species;
    }
  }

  return map;
}

} // namespace distribution

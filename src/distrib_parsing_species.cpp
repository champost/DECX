#include "distrib_parsing.hpp"

#include <functional>
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
      std::cerr << "Error: area name '" << area << "' ";
      std::cerr << "found in distribution file ";
      std::cerr << "does not match any area name ";
      std::cerr << "given in configuration file." << std::endl;
      next.source_and_exit();
    }
    areas.emplace_back(area);
    next = lexer.step();
  }
  if (areas.size()) {
    // There is a reordering: check consistency.
    if (areas.size() != config_areas.size()) {
      std::cerr << "Error: the number of areas ";
      std::cerr << "in the distribution file header ";
      std::cerr << "(" << areas.size() << ") ";
      std::cerr << "does not match the number of areas given ";
      std::cerr << "in the configuration file ";
      std::cerr << "(" << config_areas.size() << ")." << std::endl;
      next.source_and_exit();
    }
  } else {
    // No reordering: consider that the original order is relevant then.
    std::cout << "No header provided ";
    std::cout << "in distribution file: ";
    std::cout << "consider areas order from config file." << std::endl;
    areas = config_areas;
  }

  // Only order determines the areas in binary lines,
  // so take care of possible areas reordering.
  // The order given in the file corresponds to the header order,
  // but the order to be recorded in the map must match config order.
  std::map<size_t, size_t> reorder{};
  size_t i_header{0};
  for (const auto& header_area : areas) {
    size_t i_config{0};
    for (const auto& config_area : config_areas) {
      if (config_area == header_area) {
        reorder[i_header] = i_config;
      }
      ++i_config;
    }
    ++i_header;
  }

  // Now start parsing species lines.
  while (!next.has_value()) {
    next = lexer.step();
    if (next.is_eof()) {
      std::cerr << "Error: no species lines provided ";
      std::cerr << "in distribution file." << std::endl;
      next.source_and_exit();
    }
  }

  // Loop on species lines,
  // assuming that every iteration starts on a valid first token in line.
  while (!next.is_eof()) {

    const std::string species{*next.token};
    if (map.count(species)) {
      std::cerr << "Error: species '" << species << "' ";
      std::cerr << "is specified twice." << std::endl;
      next.source_and_exit();
    }

    // The rest of the line specifies the distribution of this species.
    std::vector<int> distribution{};
    distribution.reserve(areas.size());

    // There are several types of specification lines.
    // Infer the correct type based on first next token.
    next = lexer.step();
    if (!next.has_value()) {
      std::cerr << "Error: no distribution information provided ";
      std::cerr << "for species " << species << "." << std::endl;
      next.source_and_exit();
    }

    // When given + or -, the next tokens are area names counted within or
    // without the distribution from a uniform 0 or 1 base. Factorize procedure.
    const auto& plus_minus =
        [&areas, &distribution, &next, &lexer, &config_areas](int base) {
          // (treat brutally: fill with zeroes then linear-search every
          // remaining token on the line to flip corresponding element in
          // distribution to 1) (or reverse depending on base)
          // Special case glob token '*', which means flipping *all* values.
          for (size_t i{0}; i < areas.size(); ++i) {
            distribution.emplace_back(base);
          }
          next = lexer.step();
          if (!next.has_value()) {
            std::cerr << "Error: no areas provided after '";
            std::cerr << ((base) ? '-' : '+') << "' symbol." << std::endl;
            next.source_and_exit();
          }
          bool first{true};
          while (next.has_value()) {
            const auto& area{*next.token};
            if (first && area == "*") {
              for (size_t i{0}; i < areas.size(); ++i) {
                distribution[i] = 1 - base;
              }
              // No further token on the line is expected after a glob.
              next = lexer.step();
              return;
            }
            first = false;
            // Otherwise, look for the areas one by one.
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
              std::cerr << "Error: unrecognized area name: ";
              std::cerr << "'" << area << "'." << std::endl;
              next.source_and_exit();
            }
            distribution[i_area] = 1 - base;
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
          std::cerr << "'" << determinant << "' as ";
          std::cerr << "a species distribution specification." << std::endl;
          std::cerr << "Consider using (only) ones and zeroes, ";
          std::cerr << "or '+' or '-' to explicitly name areas." << std::endl;
          next.source_and_exit();
        }
      }

      // Fill with zeroes then flip only elements at the correct indexes.
      for (size_t i{0}; i < areas.size(); ++i) {
        distribution.emplace_back(0);
      }

      // Either we are reading one token like 0010101,
      // or it's split into 0 0 1 0 1 0 1.
      // In any case, the "reading" procedure is the same.
      auto read = [&distribution, &reorder, &next](std::function<char()> step,
                                                   const bool split) {
        size_t i_digit{0};
        while (true) {
          const auto digit{step()}; // `step` also checks digits correctness.
          if (digit == '\0') {
            // No more digits to read.
            break;
          }
          if (i_digit == reorder.size()) {
            std::cerr << "Error: too many digits ";
            std::cerr << "in distribution specification: ";
            std::cerr << "expected " << reorder.size() << ", ";
            std::cerr << "got at least " << i_digit + 1 << "." << std::endl;
            next.source_and_exit((split) ? 0 : i_digit);
          }
          // Only flip relevant digits in the right places.
          if (digit == '1') {
            distribution[reorder[i_digit]] = 1;
          }
          ++i_digit;
        }
        if (i_digit != reorder.size()) {
          std::cerr << "Error: not enough digits ";
          std::cerr << "in distribution specification: ";
          std::cerr << "expected " << reorder.size() << ", ";
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
          std::cerr << "Error: unexpected token ";
          std::cerr << "'" << *next.token << "'." << std::endl;
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
    map.insert({species, distribution});

    // Go to next species line.
    while (next.is_eol()) {
      next = lexer.step();
    }
  }

  return map;
}

} // namespace distribution

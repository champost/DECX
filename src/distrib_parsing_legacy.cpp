#include "distrib_parsing.hpp"

#include <boost/lexical_cast.hpp>
#include <iostream>

namespace distribution {

// Parse legacy format.
// Only the order of areas matter (the one given in the config file).
Map legacy_parse(Lexer& lexer, const size_t n_species, const Areas& areas) {

  Map map{};

  // Interpret first next token (same line) as a number of areas.
  auto next{lexer.step()};
  if (!next.has_value()) {
    std::cerr << "Error: number of areas not provided "
                 "in legacy-style distributions."
              << std::endl;
    next.source_and_exit();
  }
  size_t n_areas;
  try {
    n_areas = boost::lexical_cast<size_t>(*next.token);
  } catch (boost::bad_lexical_cast) {
    std::cerr << "Error: could not interpret '" << *next.token
              << "' as a number of areas." << std::endl;
    next.source_and_exit();
  }
  if (n_areas != areas.size()) {
    std::cerr << "Error: inconsistent number of areas: " << n_areas
              << " in distributions file but " << areas.size()
              << " in configuration file." << std::endl;
    next.source_and_exit();
  }

  // This should be everything on this line (comments aside).
  next = lexer.step();
  if (next.is_eof()) {
    goto nospecies;
  }
  if (next.has_value()) {
    std::cerr << "Error: unexpected value found in header line: " << *next.token
              << "." << std::endl;
    next.source_and_exit();
  }

  // Now start parsing species lines.
  while (!next.has_value()) {
    next = lexer.step();
    if (next.is_eof()) {
      goto nospecies;
    }
  }

  // Loop on species lines,
  // assuming that every iteration starts on a valid first token in line.
  while (!next.is_eof()) {

    if (map.size() == n_species) {
      std::cerr << "Error: more species found than specified at the beginning "
                   "of the file: expected "
                << n_species << ", got at least " << n_species + 1 << "."
                << std::endl;
      next.source_and_exit();
    }

    const std::string species{*next.token};
    const auto distribution{lexer.step()};

    if (!distribution.has_value()) {
      std::cerr << "Error: no distribution specified for species " << species
                << "." << std::endl;
      distribution.source_and_exit();
    }

    if (map.count(species)) {
      std::cerr << "Error: distribution for species " << species
                << " specified more than once." << std::endl;
      next.source_and_exit();
    }

    // Parse distribution.
    size_t i{0};
    std::vector<int> d{};
    d.reserve(areas.size());
    for (const char c : *distribution.token) {
      switch (c) {
      case '0':
        d.emplace_back(0);
        break;
      case '1':
        d.emplace_back(1);
        break;
      default:
        std::cerr
            << "Error: unexpected char in legacy species distribution format: "
            << c << std::endl;
        distribution.source_and_exit(i);
      }
      if (i >= areas.size()) {
        std::cerr
            << "Error: too many digits in legacy species distribution format: "
            << "expected " << areas.size() << ", got "
            << distribution.token->size() << "." << std::endl;
        distribution.source_and_exit(i);
      }
      ++i;
    }
    if (i != areas.size()) {
      std::cerr
          << "Error: not enough digits in legacy species distribution format: "
          << "expected " << areas.size() << ", got " << i << "." << std::endl;
      distribution.source_and_exit(i - 1);
    }

    // Record.
    map.insert({species, d});

    // Go check next species line.
    next = lexer.step();
    if (next.has_value()) {
      std::cerr << "Error: unexpected token on species line : '" << *next.token
                << "'" << std::endl;
      next.source_and_exit();
    }
    while (next.is_eol()) {
      next = lexer.step();
    }
  }

  if (map.size() < n_species) {
    std::cerr << "Error: not as many species lines than specified at the "
                 "beginning of the file: expected "
              << n_species << ", got only " << map.size() << "." << std::endl;
    next.source_and_exit();
  }

  return map;

nospecies:
  std::cerr << "Error: no species lines provided in distribution file."
            << std::endl;
  next.source_and_exit();
}

} // namespace distribution

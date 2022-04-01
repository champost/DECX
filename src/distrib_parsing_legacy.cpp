#include "distrib_parsing.hpp"

#include <boost/lexical_cast.hpp>
#include <iostream>

namespace distribution {

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
    const auto& species{*next.token};
    const auto distribution{lexer.step()};
    if (!distribution.has_value()) {
      std::cerr << "Error: no distribution specified for species " << species
                << "." << std::endl;
      distribution.source_and_exit();
    }
    std::cout << "species: " << species << '(' << *distribution.token << ')'
              << std::endl;

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

  std::cerr << "legacy style only implemented up to this point." << std::endl;
  exit(-1);

nospecies:
  std::cerr << "Error: no species lines provided in distribution file."
            << std::endl;
  next.source_and_exit();
}
} // namespace distribution

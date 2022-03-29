#include "distrib_parsing.hpp"

#include <boost/lexical_cast.hpp>
#include <iostream>

namespace distribution {

Map legacy_parse(Lexer& lexer, const size_t n_species, const Areas& areas) {

  // Interpret first next token (same line) as a number of areas.
  const auto next{lexer.step()};
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

  std::cerr << "legacy style only implemented up to this point." << std::endl;
  exit(-1);
}

} // namespace distribution

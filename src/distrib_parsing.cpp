#include "distrib_parsing.hpp"

#include <boost/lexical_cast.hpp>
#include <iostream>

namespace distribution {

Map parse_file(const File& file, const Areas& areas) {
  const auto content{read_file(file)};
  Lexer lexer{file};

  // Lex to first token.
  auto first{lexer.step()};
  while (first.is_eol()) { // Ignore leading blank/comment lines.
    first = lexer.step();
  }
  if (first.is_eof()) {
    std::cerr << "Error: distribution file '" << file.name << "' ";
    std::cerr << "is blank (or it contains only comments)." << std::endl;
    exit(DISTRIB_ERROR);
  }

  // Use first token to decide which parser to use.
  if (first.token == "s\\a") {
    return species_parse(lexer, areas);
  } else if (first.token == "a\\s") {
    return areas_parse(lexer, areas);
  }
  // Unrecognized token is interpreted as a number of species.
  size_t n_species;
  try {
    n_species = boost::lexical_cast<size_t>(*first.token);
  } catch (boost::bad_lexical_cast) {
    std::cerr << "Error: could not interpret '" << *first.token << "' ";
    std::cerr << "as a number of species or ";
    std::cerr << "a transposition specification (s\\a or a\\s)." << std::endl;
    first.source_and_exit();
  }
  // In this case, defer to legacy parsing.
  return legacy_parse(lexer, n_species, areas);
};

} // namespace distribution

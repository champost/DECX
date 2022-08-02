#include "rates_parsing.hpp"

#include <iostream>
#include <string>
#include <unordered_map>

namespace rates {

RatesMap
parse_file(const File& file, const Areas& areas, const size_t n_periods) {
  const auto content{read_file(file)};
  Lexer lexer{file};

  // Initialize map to zero.
  const size_t n_areas{areas.size()};
  if (n_areas == 0) {
    std::cerr << "Error: no rates matrices can be defined ";
    std::cerr << "with no areas." << std::endl;
    std::exit(RATES_ERROR);
  }
  if (n_periods == 0) {
    std::cerr << "Error: no rates matrices can be defined ";
    std::cerr << "with no periods." << std::endl;
    std::exit(RATES_ERROR);
  }
  RatesMap map{
      std::vector(n_periods, std::vector(n_areas, std::vector(n_areas, 0.0)))};

  // Indices in the map correspond to areas indices,
  // *except* if they are reordered in the first namer or header.
  // In this situation, all subsequent namers and headers must match reordering,
  // and the map will be reordered accordingly just before returning.
  Areas reorder; // Empty if no reorder.

  // User-defined variables are stored/erased here.
  std::unordered_map<std::string_view, double> variables;

  // Factorize procedures querying token under focus,
  // assuming it's not EOL/EOF.
  const auto to_double = [&](std::string_view token) -> std::optional<double> {
    try {
      return {std::stod(std::string(token))};
    } catch (std::invalid_argument) {
      if (variables.count(token)) {
        return {variables[token]};
      }
      return {};
    }
  };
  const auto is_in = [](std::string_view token, const Areas& list) {
    for (const auto& a : list) {
      if (token == a) {
        return true;
      }
    }
    return false;
  };

  // Major focus for the algorithm.
  auto step{lexer.step()};

  // Exit with correct error code.
  const auto source_and_exit = [&](const size_t shift = 0) {
    step.source_and_exit(shift, RATES_ERROR);
  };

  // Basic token checking, with divergence points.
  const auto not_eof = [&](auto& step) {
    if (step.is_eof()) {
      std::cerr << "Error: unexpected end of file." << std::endl;
      source_and_exit();
    }
  };
  const auto not_eol = [&](auto& step) {
    if (step.is_eol()) {
      std::cerr << "Error: unexpected end of line." << std::endl;
      source_and_exit();
    }
  };
  const auto is_eol = [&](auto& step) {
    not_eof(step);
    if (step.has_value()) {
      std::cerr << "Error: unexpected token after end of line: ";
      std::cerr << "'" << *step.token << "'." << std::endl;
      source_and_exit();
    }
  };
  const auto is_eof = [&](auto& step) {
    while (!step.is_eof()) {
      if (step.has_value()) {
        std::cerr << "Error: unexpected token after end of file: ";
        std::cerr << "'" << *step.token << "'." << std::endl;
        source_and_exit();
      }
      step = lexer.step();
    }
  };
  const auto is_token = [&](auto& step) {
    not_eol(step);
    not_eof(step);
  };
  const auto is_data = [&](auto& step) -> double {
    // Check either for literal double or existing variable.
    is_token(step);
    const auto& d{to_double(*step.token)};
    if (d.has_value()) {
      return *d;
    }
    if (variables.count(*step.token)) {
      return variables[*step.token];
    }
    std::cerr << "Error: token '" << *step.token << "' ";
    std::cerr << "is neither a valid number or a variable name." << std::endl;
    source_and_exit();
    return 0;
  };
  const auto is_area = [&](auto& step) {
    is_token(step);
    if (!is_in(*step.token, areas)) {
      std::cerr << "Error: '" << *step.token << "' is not recognized ";
      std::cerr << "as an area name." << std::endl;
      source_and_exit();
    }
    return *step.token;
  };
  const auto not_duplicate = [&](auto& step, auto& list) {
    // Assuming is_area.
    if (is_in(*step.token, list)) {
      std::cerr << "Error: area name '" << *step.token << "' ";
      std::cerr << "given twice." << std::endl;
      source_and_exit();
    }
  };

  // The main loop iterates over matrices in the file one for every period.
  size_t p{0};

  // We came accross a section with a = in second position.
  // parse it all as a variable definition section
  // before returning to usual flow.
  const auto& parse_variables = [&](auto& last_token) {
    // Assumption: we're standing on the first valid = symbol in the section.
    while (true) {

      const auto& varname = *last_token.token;
      if (is_in(varname, areas)) {
        std::cerr << "Error: cannot use area name ";
        std::cerr << "'" << varname << "' as a variable name." << std::endl;
        source_and_exit();
      }

      // We're expecting a value then.
      step = lexer.step();
      const auto value{is_data(step)};

      // Found: record to the variables list.
      variables[varname] = value;

      // Seek next line or end the section.
      step = lexer.step();
      if (step.has_value()) {
        std::cerr << "Error: unexpected token after variable definition: ";
        std::cerr << "'" << *step.token << "'." << std::endl;
        source_and_exit();
      }
      if (step.is_eof()) {
        return;
      }
      // Next line found.
      step = lexer.step();
      if (!step.has_value()) {
        // End of the variables section.
        return;
      }
      last_token = step;
      step = lexer.step();
      if (!step.has_value() || *step.token != "=") {
        std::cerr << "Error: unexpected token below variables definitions: ";
        std::cerr << "'" << *last_token.token << "'." << std::endl;
        std::cerr << "Consider leaving a blank line ";
        std::cerr << "if a new matrix needs to be specified." << std::endl;
        source_and_exit();
      }
      // There is another variable defined. Start over to parse it.
    }
  };

  // We came accross a period section starting with ~:
  // parse it all as a relative section
  // before returning to usual flow.
  const auto& parse_relative = [&]() {
    // Assumption: we're standing on the first valid ~ symbol in the section.
    while (true) {

      // We're expecting two areas and a value then.
      step = lexer.step();
      is_area(step);
      const auto first{*step.token};
      step = lexer.step();
      is_area(step);
      const auto second{*step.token};
      step = lexer.step();
      const auto value{is_data(step)};

      // Found: edit the map in-place to apply the change.
      size_t i{0};
      size_t j{0};
      for (size_t a{0}; a < areas.size(); ++a) {
        if (areas[a] == first) {
          i = a;
        }
        if (areas[a] == second) {
          j = a;
        }
      }
      for (size_t k{p}; k < n_periods; ++k) {
        map[k][i][j] = value;
      }

      // Seek next line or end the section.
      step = lexer.step();
      if (step.has_value()) {
        std::cerr << "Error: unexpected token after relative instruction: ";
        std::cerr << "'" << *step.token << "'." << std::endl;
        source_and_exit();
      }
      if (step.is_eof()) {
        return;
      }
      // Next line found.
      step = lexer.step();
      if (step.has_value()) {
        if (*step.token == "~") {
          continue;
        } else {
          std::cerr << "Error: unexpected token below relative instruction: ";
          std::cerr << "'" << *step.token << "'." << std::endl;
          std::cerr << "Consider leaving a blank line ";
          std::cerr << "if a new matrix needs to be specified." << std::endl;
          source_and_exit();
        }
      }
      return;
    }
  };

  // One matrix is parsed on every iteration,
  // with possibly a different form every time.
  while (p != n_periods) {

    // Keep track of the matrix structure as we progressively understand it.
    Areas header{}; // Remain empty if none given.
    Areas namer{};  // Remain empty if none given.

    // Lex to first token.
    auto first{step};
    while (first.is_eol()) { // Ignore leading blank/comment lines.
      first = lexer.step();
    }
    not_eof(first);

    // Process possible header and decide matrix organization.
    // After this chunk, `step` must point to the first actual data value.
    step = first;
    if (variables.count(*first.token)) {
      // Check for a variable definitions section
      // (starting with a *re*definition).
      step = lexer.step();
      if (step.has_value() && *step.token == "=") {
        parse_variables(first);
        continue;
      }
    }
    if (!to_double(*first.token).has_value()) {

      // Check for relative section.
      if (p > 0 && *first.token == "~") {
        parse_relative();
        ++p;
        continue;
      }

      // Check for a variable definitions section
      // (starting with unknown variable name).
      step = lexer.step();
      if (step.has_value() && *step.token == "=") {
        parse_variables(first);
        continue;
      }

      // Then it must be an area.
      is_area(first);
      not_eof(step);
      if (!(step.has_value() && to_double(*step.token).has_value())) {
        // This looks like a header. Consume it.
        header.emplace_back(*first.token);
        while (step.has_value()) {
          is_area(step);
          not_duplicate(step, header);
          header.emplace_back(*step.token);
          step = lexer.step();
        }
        // Check that the header is complete.
        if (header.size() < areas.size()) {
          for (const auto& missing : areas) {
            if (!is_in(missing, header)) {
              std::cerr << "Error: area '" << missing << "' ";
              std::cerr << "is missing from header." << std::endl;
              source_and_exit();
            }
          }
        }
        // Move to the next line.
        is_eol(step);
        step = lexer.step();
        if (!to_double(*step.token).has_value()) {
          // There is a namer: start recording it.
          is_area(step);
          // Check consistency with header.
          if (*step.token != header[0]) {
            std::cerr << "Error: row names order ";
            std::cerr << "and column names order do not match: ";
            std::cerr << "expected '" << header[0] << "' ";
            std::cerr << "found '" << *step.token << "' ";
            std::cerr << "instead." << std::endl;
          }
          namer.emplace_back(*step.token);
          // Now focus on the actual data.
          step = lexer.step();
        }
      } else {
        // There is no header but a namer, record the first entry.
        namer.emplace_back(*first.token);
      }
    }
    // At this point, `step` is on the first floating point data in the file,
    // and we know whether there is a namer and/or a header.

    // Read the first data into the matrix.

    size_t i{0};

    // Invoke when targetting a new piece of data (to be input into the matrix).
    const auto fill_data = [&](const size_t j) {
      const double& d{is_data(step)};
      // Set the value for this period and all successive ones.
      for (size_t k{p}; k < n_periods; ++k) {
        map[k][i][j] = d;
      }
    };

    // Invoke when ready to start a new line (assuming we're standing on EOL).
    const auto next_line = [&]() {
      step = lexer.step();
      ++i;
      if (namer.empty()) {
        return;
      }
      is_area(step);
      not_duplicate(step, namer);
      if (!header.empty() && *step.token != header[i]) {
        std::cerr << "Error: row names inconsistent ";
        std::cerr << "with column names: expected ";
        std::cerr << "'" << header[i] << "', got ";
        std::cerr << "'" << *step.token << "'." << std::endl;
        source_and_exit();
      }
      namer.emplace_back(*step.token);
      step = lexer.step();
    };

    // Check that no orders differ within the file.
    const auto& same_order = [&](const Areas& names, const bool namer) {
      // Check against file order if no reordering has been decided.
      const Areas& against{(reorder.empty()) ? areas : reorder};
      const std::string_view origin{(reorder.empty()) ? " (from config file)"
                                                      : ""};
      const std::string_view mess{(namer) ? " (in row names)"
                                          : " (in column names)"};
      // Careful with single-name dropping in case of diagonal elision.
      for (size_t i{0}; i < areas.size(); ++i) {
        if (names[i] != against[i]) {
          std::cerr << "Error: areas cannot be reordered ";
          std::cerr << "within the same rates matrices file." << std::endl;
          std::cerr << "First order found" << origin << ":" << std::endl;
          for (const auto& n : against) {
            std::cerr << " " << n;
          }
          std::cerr << std::endl << "Found now" << mess << ":" << std::endl;
          for (const auto& n : names) {
            std::cerr << " " << n;
          }
          std::cerr << std::endl;
          if (reorder.empty()) {
            std::cerr << "Reordering areas within this file is possible ";
            std::cerr << "provided it's made explicit ";
            std::cerr << "in the first specified matrix." << std::endl;
          }
          source_and_exit();
        }
      }
    };

    // One iteration per line of the matrix (i).
    while (true) {
      // Read whole line at once.
      for (size_t j{0}; j < n_areas; ++j) {
        fill_data(j);
        step = lexer.step();
      }
      is_eol(step);
      // Go check next line if there are some left.
      if (i == n_areas - 1) {
        if (p == n_periods - 1) {
          is_eof(step);
          break;
        }
        is_eol(step);
        break;
      }
      next_line();
    }

    // At this point, we know whether a first header and/or namer
    // has been given, and whether this requires a later reordering of the map.
    if (p == 0) {
      if (!header.empty()) {
        reorder = header; // Copy.
      } else if (!namer.empty()) {
        reorder = namer; // Copy.
      }
    } else {
      // Later, check that orders are always consistent.
      if (!header.empty()) {
        same_order(header, false);
      } else if (!namer.empty()) {
        same_order(namer, true);
      }
    }

    // Go check next section.
    ++p;
  }
  is_eof(step);

  if (reorder.empty()) {
    return map;
  }

  // Shuffle result into a correctly reordered copy.
  RatesMap copy{map};
  std::unordered_map<size_t, size_t> shuffle;
  for (size_t i_old{0}; i_old < areas.size(); ++i_old) {
    for (size_t i_new{0}; i_new < areas.size(); ++i_new) {
      if (areas[i_old] == reorder[i_new]) {
        shuffle[i_new] = i_old;
        break;
      }
    }
  }
  for (size_t p{0}; p < n_periods; ++p) {
    for (size_t i{0}; i < areas.size(); ++i) {
      for (size_t j{0}; j < areas.size(); ++j) {
        copy[p][i][j] = map[p][shuffle[i]][shuffle[j]];
      }
    }
  }

  return copy;
};

} // namespace rates

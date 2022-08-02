#include "adj_parsing.hpp"

#include <iostream>
#include <unordered_map>

namespace adjacency {

AdjMap
parse_file(const File& file, const Areas& areas, const size_t n_periods) {
  const auto content{read_file(file)};
  Lexer lexer{file};

  // Initialize map to identity.
  const size_t n_areas{areas.size()};
  if (n_areas == 0) {
    std::cerr << "Error: no adjacency can be defined ";
    std::cerr << "with no areas." << std::endl;
    std::exit(ADJ_ERROR);
  }
  if (n_periods == 0) {
    std::cerr << "Error: no adjacency can be defined ";
    std::cerr << "with no periods." << std::endl;
    std::exit(ADJ_ERROR);
  }
  AdjMap map{std::vector(n_periods,
                         std::vector(n_areas, std::vector(n_areas, false)))};
  for (size_t k{0}; k < n_periods; ++k) {
    for (size_t i{0}; i < n_areas; ++i) {
      map[k][i][i] = true;
    }
  }
  // Indices in the map correspond to areas indices,
  // *except* if they are reordered in the first namer or header.
  // In this situation, all subsequent namers and headers must match reordering,
  // and the map will be reordered accordingly just before returning.
  Areas reorder; // Empty if no reorder.

  // In case exactly 1 name is missing with diagonal elision.
  const auto& complete_reorder = [&](const Areas& names) {
    for (const auto& a : areas) {
      bool found{false};
      for (const auto& n : names) {
        if (n == a) {
          found = true;
          break;
        }
      }
      if (!found) {
        reorder.emplace_back(a);
        break;
      }
    }
  };

  // Factorize procedures querying token under focus, assuming it's not
  // EOL/EOF.
  const auto is_binary = [](std::string_view token) {
    for (const auto c : token) {
      if (c != '0' && c != '1') {
        return false;
      }
    }
    return true;
  };
  const auto is_relative = [](std::string_view token) {
    return token == "+" || token == "-";
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
    step.source_and_exit(shift, ADJ_ERROR);
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
  const auto is_data = [&](auto& step) {
    is_token(step);
    if (!is_binary(*step.token)) {
      std::cerr << "Error: expected binary data (0 or 1), ";
      std::cerr << "found '" << *step.token << "' instead." << std::endl;
      source_and_exit();
    }
  };
  const auto is_area = [&](auto& step) {
    is_token(step);
    if (!is_in(*step.token, areas)) {
      std::cerr << "Error: '" << *step.token << "' is not recognized ";
      std::cerr << "as an area name." << std::endl;
      source_and_exit();
    }
  };
  const auto not_duplicate = [&](auto& step, auto& list) {
    // Assuming is_area.
    if (is_in(*step.token, list)) {
      std::cerr << "Error: area name '" << *step.token << "' ";
      std::cerr << "given twice." << std::endl;
      source_and_exit();
    }
  };
  const auto check_history_size = [&](auto& step) {
    // Assuming it's binary data.
    std::string mismatch{};
    if (step.token->size() < n_periods) {
      mismatch = "not enough";
    }
    if (step.token->size() > n_periods) {
      mismatch = "too many";
    }
    if (!mismatch.empty()) {
      std::cerr << "Error: " << mismatch << " digits ";
      std::cerr << "in '" << *step.token << "' ";
      std::cerr << "to cover " << n_periods << " periods." << std::endl;
      source_and_exit();
    }
  };

  // The main loop iterates over matrices in the file one for every period.
  size_t p{0};
  bool compact{false}; // Raise if one compact form is found.

  // We came accross a period section starting with + or -:
  // parse it all as a relative section
  // before returning to usual flow.
  const auto& parse_relative = [&]() {
    // Assumption: we're standing on the first valid
    // + or - sign in the section.
    while (true) {
      const bool add{*step.token == "+"};

      // We're expecting two areas then.
      step = lexer.step();
      is_area(step);
      const auto first{*step.token};
      step = lexer.step();
      is_area(step);
      const auto second{*step.token};

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
        map[k][i][j] = map[k][j][i] = add;
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
        if (is_relative(*step.token)) {
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
  while (!compact && p != n_periods) {

    // Keep track of the matrix structure as we progressively understand it.
    Areas header{};          // Remain empty if none given.
    Areas namer{};           // Remain empty if none given.
    bool diag_header{false}; // Raise if header is not on all on the top line.
    bool triangular{false};  // Raise if only the lower half is given.
    bool diag_elided{false}; // Raise if the trivial 1's do not appear.

    // Lex to first token.
    auto first{step};
    while (first.is_eol()) { // Ignore leading blank/comment lines.
      first = lexer.step();
    }
    not_eof(first);

    // Prepare helpful error message
    // in case only one name is missing in header,
    // leading to wrong interpretation as triangular + diag_elided.
    std::optional<std::string_view> missing_in_header;

    // Process header and decide matrix organization.
    // After this chunk, `step` must point to the first actual data value.
    step = first;
    if (!is_binary(*first.token)) {
      if (p > 0 && is_relative(*first.token)) {
        // Coming accross a relative section.
        parse_relative();
        ++p;
        continue;
      }
      // This could be the first token in the header or the namer,
      // depending on the information found next.
      is_area(first);
      step = lexer.step();
      not_eof(step);
      if (step.is_eol()) {
        // A diagonal header is starting, and we'll find the rest of it later.
        diag_header = true;
        diag_elided = true;
        triangular = true;
        header.emplace_back(*first.token);
        step = lexer.step();
        is_data(step);
      } else if (!is_binary(*step.token)) {
        // This is a straight header: consume it.
        header.emplace_back(*first.token);
        while (step.has_value()) {
          is_area(step);
          not_duplicate(step, header);
          header.emplace_back(*step.token);
          step = lexer.step();
        }
        // Check that the header is complete.
        // The last one *may* be missing if diagonal is elided.
        for (const auto& missing : areas) {
          if (!is_in(missing, header)) {
            if (header.size() < areas.size() - 1) {
              std::cerr << "Error: area '" << missing << "' ";
              std::cerr << "is missing from header." << std::endl;
              source_and_exit();
            } else if (header.size() == areas.size() - 1) {
              diag_elided = true;
              triangular = true;
              missing_in_header = {missing};
              break;
            }
          }
        }
        // Then move to the next line.
        step = lexer.step();
        if (!is_binary(*step.token)) {
          // There is a namer: start recording it.
          is_area(step);
          // Check consistency with header.
          if (*step.token != header[0]) {
            // This is not an error yet, but there must be elision.
            if (*step.token != header[1]) {
              std::cerr << "Error: row names order ";
              std::cerr << "and column names order do not match: ";
              std::cerr << "expected '" << header[0] << "' ";
              std::cerr << "(with explicit diagonal) ";
              std::cerr << "or '" << header[1] << "' ";
              std::cerr << "(with implicity diagonal), ";
              std::cerr << "found '" << *step.token << "' ";
              std::cerr << "instead." << std::endl;
              source_and_exit();
            }
            diag_elided = true;
            triangular = true;
          }
          namer.emplace_back(*step.token);
          // Then focus on the actual data.
          step = lexer.step();
          is_data(step);
        }
      } else {
        // There is no header but a namer, record the first entry.
        namer.emplace_back(*first.token);
      }
    }
    // At this point, `step` is on the first binary data in the file,
    // and we know whether there is a namer, a header, whether it's diagonal,
    // and whether the diagonal has been elided.

    // Read the first data into the matrix.

    size_t i{diag_elided};

    // Invoke when targetting a new piece of data (to be input into the matrix).
    const auto fill_data = [&](const size_t j) {
      if (step.token->size() == 1) {
        // Only one digit: this sets the bit for this period
        // and all successive ones.
        for (size_t k{p}; k < n_periods; ++k) {
          map[k][i][j] = *step.token == "1";
          map[k][j][i] = map[k][i][j];
        }
      } else {
        // Several digits: we should read all of them
        // to get this connection's history.
        compact = true;
        check_history_size(step);
        for (size_t k{0}; k < step.token->size(); ++k) {
          map[k][i][j] = (*step.token)[k] == '1';
          map[k][j][i] = map[k][i][j];
        }
      }
    };

    // Invoke when targetting a redundant piece of data (to be verified).
    const auto check_data = [&](const size_t j) {
      bool correct{true};
      if (step.token->size() == 1) {
        for (size_t k{p}; k < n_periods; ++k) {
          if (map[k][i][j] != (*step.token == "1")) {
            correct = false;
            break;
          }
        }
      } else {
        check_history_size(step);
        for (size_t k{0}; k < step.token->size(); ++k) {
          if (map[k][i][j] != ((*step.token)[k] == '1')) {
            correct = false;
            break;
          }
        }
      }
      if (!correct) {
        std::cerr << "Error: found non-symmetric data ";
        std::cerr << "'" << *step.token << "' ";
        std::cerr << "in the symmetric matrix." << std::endl;
        source_and_exit();
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
      const size_t off_start{namer && diag_elided};
      const size_t off_end{!namer && diag_elided};
      for (size_t i{off_start}; i < areas.size() - off_end; ++i) {
        if (names[i] != against[i]) {
          std::cerr << "Error: areas cannot be reordered ";
          std::cerr << "within the same adjacency file." << std::endl;
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

      // Read data before the diagonal (included if any).
      for (size_t j{0}; j <= i - diag_elided; ++j) {
        is_data(step);
        if (triangular || j == i - diag_elided) {
          fill_data(j);
        } else {
          check_data(j);
        }
        step = lexer.step();
      }

      if (diag_header) {
        // Expect the header continued.
        is_area(step);
        not_duplicate(step, header);
        header.emplace_back(*step.token);
        step = lexer.step();
      } else {

        // On the first line, check whether the matrix is full
        // or only lower half.
        if (i == diag_elided) {
          not_eof(step);
          if (step.is_eol()) {
            triangular = true;
            next_line();
            continue;
          }
        }

        // Read data after the diagonal (if any).
        if (!triangular) {
          // Expect fresh data.
          for (size_t j{i + 1}; j < n_areas; ++j) {
            is_data(step);
            fill_data(j);
            step = lexer.step();
          }
        }

        // We should check for end of line,
        // but the error message may be unhelpful in a few corner cases.
        not_eof(step);
        if (step.has_value() && is_binary(*step.token) &&
            missing_in_header.has_value()) {
          std::cerr << "Error: unexpected token after end of line: ";
          std::cerr << "'" << *step.token << "'." << std::endl;
          std::cerr << "Unless area '" << *missing_in_header << "' ";
          std::cerr << "is missing from header?" << std::endl;
          source_and_exit();
        }
        is_eol(step);
      }

      // Go check next line if there are some left.
      if (i == n_areas - 1) {
        if (compact || p == n_periods - 1) {
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
        if (diag_elided) {
          complete_reorder(header);
        }
      } else if (!namer.empty()) {
        reorder = namer; // Copy.
        if (diag_elided) {
          complete_reorder(namer);
        }
      }
    } else {
      // Later, check that orders are always consistent.
      if (!header.empty()) {
        same_order(header, false);
      } else if (!namer.empty()) {
        same_order(namer, true);
      }
    }

    // If the matrix was not compact,
    // go check the next one.
    ++p;
  }
  while (step.is_eol()) {
    step = lexer.step();
  }
  if (!step.is_eof()) {
    std::cerr << "Error: adjacency has been specified ";
    std::cerr << "for the " << n_periods << " periods ";
    std::cerr << "defined in the main configuration file." << std::endl;
    std::cerr << "But there is still information in the file, ";
    std::cerr << "starting with token '" << *step.token << "'." << std::endl;
    source_and_exit();
  }

  if (reorder.empty()) {
    return map;
  }

  // Shuffle result into a correctly reordered copy.
  AdjMap copy{map};
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
      for (size_t j{0}; j <= i; ++j) {
        copy[p][i][j] = copy[p][j][i] = map[p][shuffle[i]][shuffle[j]];
      }
    }
  }

  return copy;
};

} // namespace adjacency

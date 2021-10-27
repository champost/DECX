#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <iostream>
#include <toml++/toml.hpp>

namespace config {

// Context (nested tables sought) useful to provided informative error messages.
struct Context {
  std::vector<std::string> stack;
};
std::ostream& operator<<(std::ostream& out, const Context& c);

// Check that a given node has an expected type.
template <size_t N>
void check_type(const toml::node_view<const toml::node> view,
                const std::string& name,
                const std::array<toml::node_type, N>& expected_types,
                const Context& context) {
  static_assert(N > 0, "You should expect at least 1 type.");
  bool found{false};
  for (auto& t : expected_types) {
    if (view.type() == t) {
      found = true;
      break;
    }
  }
  if (!found) {
    std::cerr << "Configuration error: Identifier '" << name << "' of "
              << context << " should ";
    switch (N) {
    case 1:
      std::cerr << "name a " << expected_types[0];
      break;
    default:
      std::cerr << "either name a " << expected_types[0];
      for (int i{1}; i < expected_types.size(); ++i) {
        std::cerr << " or a " << expected_types[i];
      }
    }
    std::cerr << " (not a " << view.type() << ": " << view.node()->source()
              << ")." << std::endl;
    exit(3);
  }
};

// Look for an optional node, and return monadic type.
template <size_t N>
std::optional<toml::node_view<const toml::node>>
seek_node(const toml::table& table,
          const std::string& name,
          const std::array<toml::node_type, N>& expected_types,
          const Context& context) {

  if (!table.contains(name)) {
    return std::nullopt;
  }

  const auto& view{table[name]};

  check_type(view, name, expected_types, context);

  return {table[name]};
}

// Protect against missing or incorrect nodes.
template <size_t N>
toml::node_view<const toml::node>
require_node(const toml::table& table,
             const std::string& name,
             const std::array<toml::node_type, N>& expected_types,
             const Context& context) {

  const auto& node{seek_node(table, name, expected_types, context)};

  if (!node.has_value()) {
    std::cerr << "Configuration error: parameter '" << name
              << "' is required, but not given in " << context << " ("
              << table.source() << ")." << std::endl;
    exit(3);
  }

  return node.value();
}

// Check that given file can be opened, error and exits otherwise.
// Use the node to locate where in the config file this file was required.
void check_file(const std::string& filename,
                toml::node_view<const toml::node> node);

// Boilerplate type-specific cases.
const toml::table& require_table(const toml::table& table,
                                 const std::string& name,
                                 const Context& context);
const std::string& require_string(const toml::table& table,
                                  const std::string& name,
                                  const Context& context);
bool require_bool(const toml::table& table,
                  const std::string& name,
                  const Context& context);
const std::string& require_file(const toml::table& table,
                                const std::string& name,
                                const Context& context);
// (couldn't manage to return a const ref to string instead)
std::optional<std::string> seek_string(const toml::table& table,
                                       const std::string& name,
                                       const Context& context);
std::optional<std::string> seek_file(const toml::table& table,
                                     const std::string& name,
                                     const Context& context);

// -----------------------------------------------------------------------------
// There also are more sophisticated types of parameters.
// Specify them below.

struct AncestralState {
  // Which ancestral states are required? Either none, or some or all.
  std::vector<std::string> states;
  bool all;
  // (ideal would be a tagged union, but they are a pain to implement in C++)

  // Construct as a particular node requirement.
  AncestralState(const toml::table& table, const Context& context);
  // Erase "some" to make it "all".
  void set_all();
  // Is there any to process?
  bool some();
};

enum class ReportType {
  States,
  Splits,
};

ReportType read_report_type(const toml::table& table, const Context& context);

} // namespace config

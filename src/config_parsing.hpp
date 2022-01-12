#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <iostream>
#include <toml++/toml.hpp>

namespace config {

// Particular, sophisticated parameters.
struct AncestralState {
  // Which ancestral states are required? Either none, or some or all.
  std::vector<std::string> states{};
  bool all{false};
  // (ideal would be a tagged union, but they are a pain to implement in C++)

  // Erase "some" to make it "all".
  void set_all();
  // Is there any to process?
  bool some();
};

enum class ReportType {
  States,
  Splits,
};

// Lighten verbosity of the types used in the files.
using View = toml::node_view<const toml::node>;
using Option = std::optional<View>;
using Type = toml::node_type;
using Table = const toml::table*;
template <std::size_t N> // Useful for a static collection of types.
using Types = const Type (&)[N];
using Name = const std::string&; // Refer to a parameter name.

// Use this object to scroll the TOML parsed config and check:
// - mandatory parameters presence
// - parameters types
// - files existence
// .. but *not* logical interaction between parameters values,
// deferred to later logic.
// Context to keep track of nested visited tables
// and produce more informative error messages.
using Context = std::vector<std::pair<std::string, Table>>;
std::ostream& operator<<(std::ostream& out, const Context& c);

// Match node type against several ones.
template <std::size_t N>
bool is_of_type(View view, Types<N> types) {
  static_assert(N > 0, "You should expect at least 1 type.");
  for (auto& t : types) {
    if (view.type() == t) {
      return true;
    }
  }
  return false;
};

class ConfigChecker {

  Table root;
  // Currently checked node.
  View focal;
  // Currently visited table and its context.
  Table table;
  Context context;

public:
  ConfigChecker(){};
  ConfigChecker(Table r) : root(r), focal((View)r), table(r){};

  // Get focal node sources.
  const toml::source_region& focal_source() const;
  void source_and_exit() const;

  // Check focal node type.
  template <std::size_t N>
  void check_type(Name name, Types<N> expected_types) {
    if (!is_of_type(focal, expected_types)) {
      std::cerr << "Configuration error: Identifier '" << name << "' of "
                << context << " should ";
      switch (N) {
      case 1:
        std::cerr << "name a " << expected_types[0];
        break;
      default:
        std::cerr << "either name a " << expected_types[0];
        for (int i{1}; i < N; ++i) {
          std::cerr << " or a " << expected_types[i];
        }
      }
      std::cerr << " (not a " << focal.type() << ": " << focal_source() << ")."
                << std::endl;
      exit(1);
    }
  };

  // Same for uniform array of that type.
  template <size_t N>
  void check_uniform_array(Name name, Types<N> expected_types) {
    check_type(name, {toml::node_type::array});
    for (auto&& element : *focal.as_array()) {
      if (!is_of_type((View)element, expected_types)) {
        std::cerr << "Configuration error: elements of array '" << name
                  << "' in " << context << " should ";
        switch (N) {
        case 1:
          std::cerr << "be of type " << expected_types[0];
          break;
        default:
          std::cerr << "either be of type " << expected_types[0];
          for (int i{1}; i < N; ++i) {
            std::cerr << " or " << expected_types[i];
          }
        }
        std::cerr << " (not " << element.type() << ": " << element.source()
                  << ")." << std::endl;
        exit(1);
      }
    }
  };

  // Look for an optional node by name, and return monadic type.
  // If present, set it as focal.
  template <size_t N>
  Option seek_node(Name name, Types<N> expected_types) {
    if (!table->contains(name)) {
      return std::nullopt;
    }
    focal = (*table)[name];
    check_type(name, expected_types);
    return focal;
  }

  // Protect against missing or incorrect nodes.
  template <size_t N>
  View require_node(Name name, Types<N> expected_types) {
    const auto& option{seek_node(name, expected_types)};
    if (!option.has_value()) {
      std::cerr << "Configuration error: parameter '" << name
                << "' is required, but not given in " << context << " ("
                << table->source() << ")." << std::endl;
      exit(1);
    }
    return focal;
  }

  // Just check for node presence.
  bool has_node(Name name);

  // Raise error with current node info if file does not exist.
  void check_file(Name filename);

  // Get next required table, update context.
  void into_table(Name table_name);
  // Same with non-required table.
  // Returns true if the table is present. Don't forget to step up then.
  bool into_optional_table(Name table_name);

  // Take one step back up the context hierarchy.
  void step_up();

  // Boilerplate type-specific cases.
  Table require_table(Name name);
  std::string require_string(Name name);
  bool require_bool(Name name);
  int require_integer(Name name);
  double require_float(Name name);
  std::string require_file(Name name);
  // (couldn't manage to return a const ref to string instead)
  std::optional<std::string> seek_file(Name name);
  std::optional<std::string> seek_string(Name name);
  std::optional<int> seek_integer(Name name);
  // Seek or pick default name.
  std::string seek_string_or(Name name, std::string def);

  //---------------------------------------------------------------------------
  // More sophisticated parameters.
  AncestralState read_ancestral_state();
  ReportType read_report_type();

  // Read uniform array types.
  std::vector<double> read_periods();
  // + check for unicity.
  // (item_meaning is used to make error message more informative).
  std::vector<std::string> read_unique_strings(Name name,
                                               const std::string& item_meaning);
  // Same principle as above, but with space-separated words,
  // so the user can type "A B C" instead of ["A", "B", "C"].
  // Multiple/prefix/trailing spaces are allowed.
  std::vector<std::string> read_unique_words(Name name,
                                             const std::string& item_meaning);
  // Abstract over the two previous ones when both are supported.
  std::vector<std::string>
  read_unique_identifiers(Name name, const std::string& item_meaning);

  // Parse distributions specifications.
  // Every given string is one distribution, specified either as:
  //  - binary code: "0110" meaning areas at index 1 and 2.
  //  - areas names: "B C" meaning areas B and C.
  std::vector<std::vector<int>> // indexes into area_names.
  read_distributions(Name name, const std::vector<std::string>& area_names);

  // Same with only one area.
  std::string read_area(Name name, const std::vector<std::string>& area_names);

  // Parse MRCA, filling outer parameters.
  void read_mrcas(std::map<std::string, std::vector<std::string>>& mrcas,
                  std::map<std::string, std::vector<int>>& fixnodes,
                  std::vector<std::string>& fossilnames,
                  std::vector<std::string>& fossiltypes,
                  std::vector<std::string>& fossilareas,
                  std::vector<double>& fossilages,
                  const std::vector<std::string>& area_names);

private:
  std::vector<int>
  read_distribution(const toml::node& node,
                    const std::vector<std::string>& area_names);
};

} // namespace config

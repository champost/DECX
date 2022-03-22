#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <iostream>
#include <toml++/toml.h>

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

// Lighten verbosity of the types used in this namespace.
using View = toml::node_view<const toml::node>;
using Type = toml::node_type;
using Table = const toml::table*;
template <std::size_t N> // Useful for a static collection of types.
using Types = const Type (&)[N];
using Name = std::string_view; // Refer to a parameter name.

// Use this object to scroll the TOML parsed config and check:
// - mandatory parameters presence
// - parameters types
// - files existence
// .. but *not* logical interaction between parameters values,
// deferred to later logic.

// TOML++ nodes seem not to know their parent,
// but parents help much in constructing helpful error messages.
// The corresponding tree is constructed and navigated
// during execution of the reader's methods,
// and dropped with the reader.
struct Node {
  View data;
  std::optional<std::string> name;
  std::optional<Node*> parent;

  Node(View d, std::string n, std::optional<Node*> p) :
      data(d), name(n), parent(p) {}

  // Collect all nodes names back to the root
  // (root is last and elided because it is unnamed).
  std::vector<std::string> hierarchy() const;
};

// Stream all named nodes back to the root.
struct ColonHierarchy {
  Node* node;
};
std::ostream& operator<<(std::ostream& out, const ColonHierarchy& d);

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

class Reader {

  // Try something: any method call is responsible
  // for setting this node to the correct location
  // before termination.
  // Any *public* method call is responsible
  // for setting it to a user-facing *table*.
  Node* focal;

  // Set focus one step down into a new subnode.
  void descend(View data, Name name);

public:
  // Restore focus to parent node.
  void step_up();

  Reader(){};
  Reader(Table root);
  ~Reader();

  // Get focal node sources.
  const toml::source_region& focal_source() const;
  [[noreturn]] void source_and_exit() const;

  // Check focal node TOML type.
  // If wrong, errors with its source and name information.
  template <std::size_t N>
  void check_type(Types<N> expected_types) {
    if (!is_of_type(focal->data, expected_types)) {
      std::cerr << "Configuration error: node should ";
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
      std::cerr << ", not " << focal->data.type() << ". ";
      // Additional information in specific cases.
      if (is_of_type(focal->data, {toml::node_type::integer}) && N == 1 &&
          expected_types[0] == toml::node_type::floating_point) {
        const auto& e{*focal->data.as_integer()};
        std::cerr << "Consider writing `" << e << ".0`";
        std::cerr << " instead of `" << e << "`. ";
      }
      source_and_exit();
    }
  };

  // Same for uniform array of that type.
  // During this pass every value is checked in turn
  // with element number as node name.
  // Then the focal is reset.
  template <size_t N>
  void check_uniform_array(Types<N> expected_types) {
    check_type({toml::node_type::array});
    std::size_t i{1};
    for (auto&& element : *focal->data.as_array()) {
      descend((View)element, std::to_string(i));
      check_type(expected_types);
      step_up();
      ++i;
    }
  };

  // Assuming the focal node is a table,
  // look for an optional node by name and return monadic type.
  // If present and `descend` is true, leave it as focal.
  template <size_t N>
  std::optional<View>
  seek_node(Name name, Types<N> expected_types, const bool descend) {
    if (!is_of_type(focal->data, {toml::node_type::table})) {
      std::cerr << "Error in source code: should not seek node by name "
                   "while standing on a non-table node."
                << std::endl;
      exit(-1);
    }
    const auto& table{focal->data.as_table()};
    if (!table->contains(name)) {
      return {};
    }
    const auto& view{(*table)[name]};
    this->descend(view, name);
    check_type(expected_types);
    if (!descend) {
      step_up();
    }
    return {view};
  }

  // Same, but exit if the node cannot be found.
  template <size_t N>
  View require_node(Name name, Types<N> expected_types, const bool descend) {
    const auto& option{seek_node(name, expected_types, descend)};
    if (option.has_value()) {
      return *option;
    }
    std::cerr << "Configuration error: "
              << "'" << name << "' is required, but not given. ";
    source_and_exit();
  }

  // Assuming the focal node is a table,
  // just check for subnode existence.
  bool has_node(Name name);

  // Raise error with current node info if file does not exist.
  void check_file(Name filename);

  // Get next required table, update context.
  void into_table(Name table_name);
  // Same with non-required table.
  // Returns true if the table is present. Don't forget to step up then.
  bool into_optional_table(Name table_name);

  // Boilerplate type-specific cases.
  Table require_table(Name name);
  std::string require_string(Name name);
  bool require_bool(Name name);
  int require_integer(Name name);
  double require_float(Name name);
  std::string require_file(Name name);
  // (couldn't manage to return a const ref to string instead)
  std::optional<bool> seek_boolean(Name name);
  std::optional<int> seek_integer(Name name);
  std::optional<std::string> seek_file(Name name);
  std::optional<std::string> seek_string(Name name);

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

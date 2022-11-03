#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include "lexer.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <toml++/toml.h>
#include <unordered_set>

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

// Dedicate this code to errors with config file.
constexpr int EXIT_ERROR{1};

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

  // Special-case focal *table* nodes:
  // collect contained names/keys here and remove them on every access.
  // Then, in order to disallow unused parameters,
  // error on step-up if there are names left in this collection.
  std::unordered_set<std::string_view> unused_names;

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
// Use empty array as a wildcard to always get true.
template <std::size_t N>
bool is_of_type(View view, Types<N> types);

class Reader {

  // Try something: any method call is responsible
  // for setting this node to the correct location
  // before termination.
  // Any *public* method call is responsible
  // for setting it to a user-facing *table*.
  Node* focal;

  // Paths specified within the config file are understood relatively to it.
  std::filesystem::path folder;

  // Set focus one step down into a new subnode.
  void descend(View data, Name name);

public:
  // Restore focus to parent node.
  void step_up();

  Reader(){};
  Reader(std::string_view filename, Table root);
  ~Reader();

  // Get focal node sources.
  const toml::source_region& focal_source() const;
  [[noreturn]] void source_and_exit() const;

  // Check focal node TOML type.
  // If wrong, errors with its source and name information.
  template <std::size_t N>
  void check_type(Types<N> expected_types);

  // Same for uniform array of that type.
  // During this pass every value is checked in turn
  // with element number as node name.
  // Then the focal is reset.
  template <size_t N>
  void check_uniform_array(Types<N> expected_types);

  // Assuming the focal node is a table,
  // look for an optional node by name and return monadic type.
  // If present and `descend` is true, leave it as focal.
  template <size_t N>
  std::optional<View>
  seek_node(Name name, Types<N> expected_types, const bool descend);
  // Same with no type-checking.
  std::optional<View> seek_node_any(Name name, const bool descend);

  // Same, but exit if the node cannot be found.
  template <size_t N>
  View require_node(Name name, Types<N> expected_types, const bool descend);
  View require_node_any(Name name, const bool descend);

  // Assuming the focal node is a table,
  // just check for subnode existence.
  bool has_node(Name name);

  // Error with focal info if file does not exist.
  void check_file(Name filename);

  // Boilerplate type-specific cases.  = = = = = = = = = = = =
  bool require_bool(Name name, const bool descend);
  File require_file(Name name, const bool descend);
  double require_float(Name name, const bool descend);
  int require_integer(Name name, const bool descend);
  std::string require_string(Name name, const bool descend);
  Table require_table(Name name, const bool descend);
  // = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

  // Same logic with optional nodes. = = = = = = = = = = = = =
  std::optional<bool> seek_bool(Name name, const bool descend);
  std::optional<File> seek_file(Name name, const bool descend);
  std::optional<double> seek_float(Name name, const bool descend);
  std::optional<int> seek_integer(Name name, const bool descend);
  std::optional<std::string> seek_string(Name name, const bool descend);
  std::optional<Table> seek_table(Name name, const bool descend);
  // = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

  //---------------------------------------------------------------------------
  // More sophisticated parameters.
  AncestralState read_ancestral_state();
  ReportType read_report_type();

  // Periods are either specified with the 'periods' key = 'durations' key.
  // In this case, each value in the array
  // represents the duration of the period.
  // Alternately, it may be specified with the 'dates' key instead,
  // and the values represent (ordered) past dates.
  // In any case the (legacy) durations form is returned.
  // An error is raised if more than one of the three forms is given.
  std::vector<double> read_periods();

  // Check that identifier is valid for region or species name:
  // Authorized characters: [a-Z0-9-_.]
  // Cannot start with a digit.
  // Must contain at least one letter a-Z or underscore '_'.
  // Returns non-empty error message in case the chain is invalid id,
  // along with location of faulty character.
  using IdValidation = std::pair<std::string, size_t>;
  static IdValidation variable_like(const std::string_view chain);
  using IdValider = std::function<IdValidation(const std::string_view)>;

  // Read + check for unicity.
  // (item_meaning is used to make error message more informative).
  std::vector<std::string> read_unique_strings(
      Name name, const std::string_view item_meaning, const IdValider valider);
  // Same principle but with space-separated words,
  // so the user can type "A B C" instead of ["A", "B", "C"].
  // Multiple/prefix/trailing spaces are allowed.
  std::vector<std::string> read_unique_words(Name name,
                                             const std::string& item_meaning,
                                             const IdValider valider);
  // Abstract over the two previous ones when both are supported.
  std::vector<std::string>
  read_unique_identifiers(Name name,
                          const std::string& item_meaning,
                          const IdValider = variable_like);

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
  // Interpret focal node as a areas distribution.
  std::vector<int>
  read_distribution(const std::vector<std::string>& area_names);
};

} // namespace config

#include "config_parsing.tpp"

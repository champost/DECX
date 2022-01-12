#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

const toml::source_region& ConfigChecker::focal_source() const {
  return focal.node()->source();
}

void ConfigChecker::source_and_exit() const {
  std::cerr << "(" << focal_source() << ")" << std::endl;
  exit(1);
}

bool ConfigChecker::has_node(Name name) { return table->contains(name); }

void ConfigChecker::check_file(const std::string& filename) {
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Configuration error: Could not find file " << filename
              << std::endl;
    source_and_exit();
  }
};

void ConfigChecker::step_up() {
  if (context.empty()) {
    std::cerr << "Error in configuration parsing logic: "
              << "cannot step up root context.";
    exit(-1);
  }
  context.pop_back();
  if (context.empty()) {
    // We've stepped back to config file root.
    table = root;
  } else {
    table = context.back().second;
  }
};

void ConfigChecker::into_table(Name table_name) {
  table = require_table(table_name);
  context.push_back({table_name, table});
};

bool ConfigChecker::into_optional_table(Name table_name) {
  const auto& table{seek_node(table_name, {toml::node_type::table})};
  if (table.has_value()) {
    into_table(table_name);
    return true;
  }
  return false;
};

#define DEFINE_REQUIRER(fnname, type, ret)                                     \
  ret ConfigChecker::require_##fnname(Name name) {                             \
    return require_node(name, {toml::node_type::type}).as_##type()->get();     \
  };

DEFINE_REQUIRER(string, string, std::string);
DEFINE_REQUIRER(bool, boolean, bool);
DEFINE_REQUIRER(integer, integer, int);
DEFINE_REQUIRER(float, floating_point, double);

// This one's special because there's no ->get();
Table ConfigChecker::require_table(Name name) {
  return require_node(name, {toml::node_type::table}).as_table();
};

// This one's special because there is the file checking.
std::string ConfigChecker::require_file(Name name) {
  const auto& filename{require_string(name)};
  check_file(filename);
  return filename;
};

#define DEFINE_SEEKER(type, ret)                                               \
  std::optional<ret> ConfigChecker::seek_##type(Name name) {                   \
    const auto& node{seek_node(name, {toml::node_type::type})};                \
    if (node.has_value()) {                                                    \
      return {node.value().as_##type()->get()};                                \
    }                                                                          \
    return {};                                                                 \
  };

DEFINE_SEEKER(string, std::string);
DEFINE_SEEKER(integer, int);

std::optional<std::string> ConfigChecker::seek_file(Name name) {
  const auto& filename{seek_string(name)};
  if (filename.has_value()) {
    check_file(filename.value());
    return filename;
  }
  return {};
};

std::string ConfigChecker::seek_string_or(Name name, std::string def) {
  const auto& node{seek_node(name, {toml::node_type::string})};
  if (node.has_value()) {
    return node.value().as_string()->get();
  }
  return def;
};

// Display context.
std::ostream& operator<<(std::ostream& out, const Context& c) {
  switch (c.size()) {
  case 0:
    out << "root table";
    break;
  case 1:
    out << "table '" << c[0].first << '\'';
    break;
  default:
    out << "table '" << c[0].first;
    for (int i{1}; i < c.size(); ++i) {
      out << ':';
      out << c[i].first;
    }
    out << "'";
  }
  return out;
};

AncestralState ConfigChecker::read_ancestral_state() {
  AncestralState a{};
  // Require a node of several possible types..
  require_node("ancestral_states",
               {toml::node_type::array,
                toml::node_type::boolean,
                toml::node_type::string});
  // Construct the value depending on the node type.
  switch (focal.type()) {
  case toml::node_type::array:
    check_uniform_array("ancestral_states", {toml::node_type::string});
    for (auto& element : *focal.as_array()) {
      a.states.emplace_back(*element.as_string());
    }
    break;
  case toml::node_type::boolean:
    a.all = focal.as_boolean()->get();
    break;
  case toml::node_type::string:
    a.states.emplace_back(focal.as_string()->get());
    break;
  default: // Alternate types have already been ruled out.
    break;
  }
  return a;
};

void AncestralState::set_all() {
  states.clear();
  all = true;
};

bool AncestralState::some() { return !(all || states.empty()); };

ReportType ConfigChecker::read_report_type() {
  auto string{require_string("report")};
  if (string == "states") {
    return ReportType::States;
  } else if (string == "splits") {
    return ReportType::Splits;
  } else {
    std::cerr << "Unknown report type: '" << string << "'. "
              << "Supported types are 'states' and 'splits'." << std::endl;
    source_and_exit();
  }
  exit(-1); // unreachable
}

// TODO: iterate to generate the following ones?
std::vector<double> ConfigChecker::read_periods() {
  auto node{require_node("periods", {toml::node_type::array})};
  check_uniform_array("periods", {toml::node_type::floating_point});
  std::vector<double> periods{};
  for (auto& element : *node.as_array()) {
    periods.emplace_back(*element.as_floating_point());
  }
  return periods;
}

std::vector<std::string>
ConfigChecker::read_unique_strings(Name name, const std::string& item_meaning) {
  auto node{require_node(name, {toml::node_type::array})};
  check_uniform_array(name, {toml::node_type::string});
  std::vector<std::string> result{};
  for (const auto& element : *node.as_array()) {
    focal = toml::node_view(element);
    // Bruteforce check that no duplicate has been given.
    const std::string item{*element.as_string()};
    for (const auto& given : result) {
      if (item == given) {
        std::cerr << item_meaning << " name '" << item << "' given twice."
                  << std::endl;
        source_and_exit();
      }
    }
    result.emplace_back(item);
  }
  return result;
}

std::vector<std::string>
ConfigChecker::read_unique_words(Name name, const std::string& item_meaning) {
  auto words{require_string(name)};
  std::vector<std::string> result{};
  std::string current{};
  bool next{true};
  for (auto c : words) {
    if (std::isspace(c)) {
      next = true;
    } else if (next) {
      if (!current.empty()) {
        // We have a new word.
        // Check against others for unicity.
        for (const auto& other : result) {
          if (other == current) {
            std::cerr << item_meaning << " name '" << current
                      << "' given twice." << std::endl;
            source_and_exit();
          }
        }
        result.push_back(current);
        current = std::string{};
      }
      current.push_back(c);
      next = false;
    } else {
      current.push_back(c);
    }
  }
  return result;
};

std::vector<std::string>
ConfigChecker::read_unique_identifiers(Name name,
                                       const std::string& item_meaning) {
  const auto node{
      require_node(name, {toml::node_type::array, toml::node_type::string})};
  if (node.type() == toml::node_type::string) {
    return read_unique_words(name, item_meaning);
  }
  return read_unique_strings(name, item_meaning);
}

std::vector<std::vector<int>>
ConfigChecker::read_distributions(Name name,
                                  const std::vector<std::string>& area_names) {
  auto node{require_node(name, {toml::node_type::array})};
  check_uniform_array(name, {toml::node_type::string});
  std::vector<std::vector<int>> distributions;
  for (auto& element : *node.as_array()) {
    distributions.push_back(read_distribution(element, area_names));
  }
  return distributions;
}

std::vector<int>
ConfigChecker::read_distribution(const toml::node& node,
                                 const std::vector<std::string>& area_names) {
  focal = toml::node_view(node);
  if (node.type() != toml::node_type::string) {
    std::cerr << "Distribution should be specified as a string, not "
              << node.type() << "." << std::endl;
    source_and_exit();
  }

  std::vector<int> result;

  // Find index of area.
  auto indexof = [&](std::string word) -> std::optional<int> {
    for (int i{0}; i < area_names.size(); ++i) {
      if (word == area_names[i]) {
        return {i};
      }
    }
    return {};
  };

  // First split the input into words.
  std::string input{node.as_string()->get()};
  std::vector<std::string> words{};
  std::string current;
  bool next{true};
  for (const char c : input) {
    if (std::isspace(c)) {
      next = true;
    } else if (next) {
      if (!current.empty()) {
        words.push_back(current);
        current = std::string{};
      }
      current.push_back(c);
      next = false;
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    words.push_back(current);
  }
  if (words.empty()) {
    std::cerr << "Distributions cannot be empty." << std::endl;
    source_and_exit();
  }
  // If there is only one word, it could be a binary specification.
  if (words.size() == 1) {
    const std::string& word{words.back()};
    bool is_binary{true};
    // In case it is, collect the indices.
    int i{0};
    for (char c : word) {
      if (c == '1') {
        result.push_back(i);
      } else if (c != '0') {
        is_binary = false;
        break;
      }
      ++i;
    }
    const auto index{indexof(word)};
    // Forbid the ambiguous case.
    if (is_binary && index.has_value() && word.size() == area_names.size() &&
        !(result.size() == 1 && result[0] == index.value())) {
      std::cerr << "Ambiguous distribution specification : '" << word
                << "' could either represent the single area '" << word
                << "' or a binary set of other areas.."
                << " now be honest: you did that on purpose, right?"
                << std::endl;
      source_and_exit();
    }
    if (!is_binary) {
      // Then it's a single area name.
      result.clear();
      if (index.has_value()) {
        result.push_back(index.value());
      } else {
        std::cerr << "Unknown area name in distribution: '" << word << "'"
                  << " (known areas:";
        for (const auto& area : area_names) {
          std::cout << " " << area;
        }
        std::cout << ")." << std::endl;
        source_and_exit();
      }
    } else if (word.size() != area_names.size()) {
      std::cerr << "Invalid binary specification of a distribution: '" << word
                << "' contains " << word.size() << " digits but there are "
                << area_names.size() << " areas." << std::endl;
      source_and_exit();
    }
  } else {
    // Or else the areas are given by name.
    for (const auto& word : words) {
      const auto& index{indexof(word)};
      if (index.has_value()) {
        result.push_back(index.value());
      } else {
        std::cerr << "Unknown area name in distribution: '" << word << "'"
                  << "(known areas:";
        for (const auto& area : area_names) {
          std::cout << " " << area;
        }
        std::cout << ")." << std::endl;
        source_and_exit();
      }
    }
  }
  return result;
}

std::string
ConfigChecker::read_area(Name name,
                         const std::vector<std::string>& area_names) {
  const std::string area{require_string(name)};
  // Check that the area name is known.
  bool found{false};
  for (const auto& known : area_names) {
    if (area == known) {
      found = true;
      break;
    }
  }
  if (!found) {
    std::cerr << "Unknown area '" << area << "' provided.";
    source_and_exit();
  }
  return area;
};

void ConfigChecker::read_mrcas(
    std::map<std::string, std::vector<std::string>>& mrcas,
    std::map<std::string, std::vector<int>>& fixnodes,
    std::vector<std::string>& fossilnames,
    std::vector<std::string>& fossiltypes,
    std::vector<std::string>& fossilareas,
    std::vector<double>& fossilages,
    const std::vector<std::string>& area_names) {

  // It may be that none is given.
  if (!into_optional_table("mrca")) {
    return;
  }

  // Every given MRCA is a sub-table.
  const auto& table{*focal.as_table()};
  for (const auto& mrca : table) {
    const auto& name{mrca.first};
    into_table(name);
    // Got it.

    // All need to have a list of species.
    const std::vector<std::string> species{
        read_unique_identifiers("species", "Species")};
    mrcas.insert({name, species});

    // The rest will depend on their type.
    const std::string type{require_string("type")};

    if (type == "fixed node") {
      // Then a distribution is given.
      const std::vector<int> distribution{read_distribution(
          *require_node("distribution", {toml::node_type::string}).node(),
          area_names)};

      fixnodes.insert({name, distribution});
    } else {
      if (type == "fossil node") {
        fossiltypes.push_back("N");
        fossilages.push_back(0.);
      } else if (type == "fossil branch") {
        fossiltypes.push_back("B");
        fossilages.push_back(require_float("age"));
      } else {
        std::cerr << "Unknown MRCA type: '" << type << "'.";
        std::cerr << " Supported types are ";
        std::cerr << "'fixed node', 'fossil node' and 'fossil branch'";
        std::cerr << "." << std::endl;
        source_and_exit();
      }
      fossilnames.push_back(name);
      // Then only one area is supported.
      const auto& area{read_area("area", area_names)};
      fossilareas.push_back(area);
    }

    step_up();
  }

  // Step out of MRCA table.
  step_up();
};
} // namespace config

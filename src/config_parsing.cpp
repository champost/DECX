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

Table ConfigChecker::require_table(Name name) {
  return require_node(name, {toml::node_type::table}).as_table();
};

std::string ConfigChecker::require_string(Name name) {
  return require_node(name, {toml::node_type::string}).as_string()->get();
};

bool ConfigChecker::require_bool(Name name) {
  return require_node(name, {toml::node_type::boolean}).as_boolean()->get();
};

int ConfigChecker::require_integer(Name name) {
  return require_node(name, {toml::node_type::integer}).as_integer()->get();
};

std::string ConfigChecker::require_file(Name name) {
  const auto& filename{require_string(name)};
  check_file(filename);
  return filename;
};

std::optional<std::string> ConfigChecker::seek_file(Name name) {
  const auto& filename{seek_string(name)};
  if (filename.has_value()) {
    check_file(filename.value());
    return filename;
  }
  return {};
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

std::vector<std::string> ConfigChecker::read_area_names() {
  auto node{require_node("names", {toml::node_type::array})};
  check_uniform_array("names", {toml::node_type::string});
  std::vector<std::string> names{};
  for (const auto& element : *node.as_array()) {
    // Bruteforce check that no duplicate has been given.
    const std::string name{*element.as_string()};
    for (const auto& given : names) {
      if (name == given) {
        std::cerr << "Area name '" << name << "' given twice." << std::endl;
        source_and_exit();
      }
    }
    names.emplace_back(name);
  }
  return names;
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
  for (char c : input) {
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
                << "now be honest: you did that on purpose, right?"
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

} // namespace config

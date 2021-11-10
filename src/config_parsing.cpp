#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

void ConfigChecker::check_file(const std::string& filename) {
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Configuration error: Could not find file " << filename << " ("
              << focal_source() << ")." << std::endl;
    exit(3);
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

std::optional<std::string> ConfigChecker::seek_string(Name name) {
  const auto& node{seek_node(name, {toml::node_type::string})};
  if (node.has_value()) {
    return {node.value().as_string()->get()};
  }
  return {};
};

std::optional<std::string> ConfigChecker::seek_file(Name name) {
  const auto& filename{seek_string(name)};
  if (filename.has_value()) {
    check_file(filename.value());
    return filename;
  }
  return {};
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
              << "Supported types are 'states' and 'splits' (" << focal_source()
              << ").";
    exit(3);
  }
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
  for (auto& element : *node.as_array()) {
    names.emplace_back(*element.as_string());
  }
  return names;
}

} // namespace config

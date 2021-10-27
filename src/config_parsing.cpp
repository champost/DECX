#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

void check_file(const std::string& filename,
                toml::node_view<const toml::node> node) {
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Configuration error: Could not find file " << filename << " ("
              << node.node()->source() << ")." << std::endl;
    exit(3);
  }
};

const toml::table& require_table(const toml::table& table,
                                 const std::string& name,
                                 const Context& context) {
  return *require_node(table, name, std::array{toml::node_type::table}, context)
              .as_table();
};

const std::string& require_string(const toml::table& table,
                                  const std::string& name,
                                  const Context& context) {
  return require_node(table, name, std::array{toml::node_type::string}, context)
      .as_string()
      ->get();
};

bool require_bool(const toml::table& table,
                  const std::string& name,
                  const Context& context) {
  return require_node(
             table, name, std::array{toml::node_type::boolean}, context)
      .as_boolean()
      ->get();
};

const std::string& require_file(const toml::table& table,
                                const std::string& name,
                                const Context& context) {
  const auto& filename{require_string(table, name, context)};
  check_file(filename, table[name]);
  return filename;
};

std::optional<std::string> seek_string(const toml::table& table,
                                       const std::string& name,
                                       const Context& context) {
  const auto& node{
      seek_node(table, name, std::array{toml::node_type::string}, context)};
  if (node.has_value()) {
    return {node.value().as_string()->get()};
  }
  return {};
};

std::optional<std::string> seek_file(const toml::table& table,
                                     const std::string& name,
                                     const Context& context) {
  const auto& filename{seek_string(table, name, context)};
  if (filename.has_value()) {
    check_file(filename.value(), table[name]);
    return filename;
  }
  return {};
};

// Display context.
std::ostream& operator<<(std::ostream& out, const Context& c) {
  switch (c.stack.size()) {
  case 0:
    out << "root table";
    break;
  case 1:
    out << "table '" << c.stack[0] << '\'';
    break;
  default:
    out << "table '" << c.stack[0];
    for (int i{1}; i < c.stack.size(); ++i) {
      out << ':';
      out << c.stack[i];
    }
    out << "'";
  }
  return out;
};

AncestralState::AncestralState(const toml::table& table,
                               const Context& context) :
    states({}), all(false) {
  // Require a node of several possible types..
  const auto& node{require_node(table,
                                "ancestral_states",
                                std::array{toml::node_type::array,
                                           toml::node_type::boolean,
                                           toml::node_type::string},
                                context)};
  // Construct the value depending on the node type.
  switch (node.type()) {
  case toml::node_type::array:
    for (auto& element : *node.as_array()) {
      if (element.is_string()) {
        this->states.emplace_back(*element.as_string());
      } else {
        std::cerr << "Configuration error: ancestral states array "
                  << "should only contain strings, not " << element.type()
                  << " (" << element.source() << ")." << std::endl;
        exit(3);
      }
    }
    break;
  case toml::node_type::boolean:
    all = node.as_boolean()->get();
    break;
  case toml::node_type::string:
    states.emplace_back(node.as_string()->get());
    break;
  default: // Alternate types have already been ruled out.
    break;
  }
};

void AncestralState::set_all() {
  states.clear();
  all = true;
};

bool AncestralState::some() { return !(all || states.empty()); };

ReportType read_report_type(const toml::table& table, const Context& context) {
  auto node{require_node(
      table, "report", std::array{toml::node_type::string}, context)};
  auto& string{node.as_string()->get()};
  if (string == "states") {
    return ReportType::States;
  } else if (string == "splits") {
    return ReportType::Splits;
  } else {
    std::cerr << "Unknown report type: '" << string << "'. "
              << "Supported types are 'states' and 'splits' ("
              << node.node()->source() << ").";
    exit(3);
  }
}
} // namespace config

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

} // namespace config

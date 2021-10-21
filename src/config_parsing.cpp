#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

std::optional<toml::node_view<const toml::node>>
seek_node(const toml::table& table,
          const std::string& name,
          const toml::node_type& type) {

  if (!table.contains(name)) {
    return std::nullopt;
  }

  const auto& view{table[name]};

  check_type(view, name, type);

  return {table[name]};
}

toml::node_view<const toml::node> require_node(const toml::table& table,
                                               const std::string& name,
                                               const toml::node_type& type) {
  const auto& node{seek_node(table, name, type)};

  if (!node.has_value()) {
    // Here: there is context missing.
    std::cerr << "Configuration error: " << type << " '" << name
              << "' is required, but not given in configuration file."
              << std::endl;
    exit(3);
  }

  return node.value();
}

void check_type(const toml::node_view<const toml::node> view,
                const std::string& name,
                const toml::node_type& type) {
  if (view.type() != type) {
    std::cerr << "Configuration error: Identifier '" << name
              << "' should name a " << type << " (" << view.node()->source()
              << ")." << std::endl;
    exit(3);
  }
};

void check_file(const std::string& filename) {
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Configuration error: Could not find file " << filename << "."
              << std::endl;
    exit(3);
  }
};

const toml::table& require_table(const toml::table& table,
                                 const std::string& name) {
  return *require_node(table, name, toml::node_type::table).as_table();
};

const std::string& require_string(const toml::table& table,
                                  const std::string& name) {
  return require_node(table, name, toml::node_type::string).as_string()->get();
};
const std::string& require_file(const toml::table& table,
                                const std::string& name) {
  const auto& filename{require_string(table, name)};
  check_file(filename);
  return filename;
};

std::optional<std::string> seek_string(const toml::table& table,
                                       const std::string& name) {
  const auto& node{seek_node(table, name, toml::node_type::string)};
  if (node.has_value()) {
    return {node.value().as_string()->get()};
  }
  return {};
};

std::optional<std::string> seek_file(const toml::table& table,
                                     const std::string& name) {
  const auto& filename{seek_string(table, name)};
  if (filename.has_value()) {
    check_file(filename.value());
    return filename;
  }
  return {};
};
} // namespace config

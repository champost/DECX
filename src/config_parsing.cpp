#include "config_parsing.hpp"

#include <iostream>

namespace config {

toml::node_view<toml::node> require_node(toml::table& table,
                                         const std::string& name,
                                         const toml::node_type& type) {
  if (!table.contains(name)) {
    std::cerr << "Configuration error: " << type << " '" << name
              << "' is required, but not given in configuration file."
              << std::endl;
    exit(3);
  }

  auto view{table[name]};

  check_type(view, name, type);

  return table[name];
}

void check_type(toml::node_view<toml::node>& view,
                const std::string& name,
                const toml::node_type& type) {
  if (view.type() != type) {
    std::cerr << "Configuration error: Identifier '" << name
              << "' should name a " << type << " (" << view.node()->source()
              << ")." << std::endl;
    exit(3);
  }
};

toml::table& require_table(toml::table& table, const std::string& name) {
  return *require_node(table, name, toml::node_type::table).as_table();
};

std::string& require_string(toml::table& table, const std::string& name) {
  return require_node(table, name, toml::node_type::string).as_string()->get();
};

} // namespace config

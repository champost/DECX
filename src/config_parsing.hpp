#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <toml++/toml.hpp>

namespace config {

// Protect against missing or incorrect nodes.
toml::node_view<toml::node> require_node(toml::table& table,
                                         const std::string& name,
                                         const toml::node_type& type);

// Protect against missing or incorrect nodes.
void check_type(toml::node_view<toml::node>& node,
                const std::string& name,
                const toml::node_type& type);

// Boilerplate type-specific cases.
toml::table& require_table(toml::table& table, const std::string& name);
std::string& require_string(toml::table& table, const std::string& name);

} // namespace config

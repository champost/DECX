#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <toml++/toml.hpp>

namespace config {

// Protect against missing or incorrect nodes.
std::optional<toml::node_view<const toml::node>>
seek_node(const toml::table& table,
          const std::string& name,
          const toml::node_type& type);
toml::node_view<const toml::node> require_node(const toml::table& table,
                                               const std::string& name,
                                               const toml::node_type& type);

// Protect against missing or incorrect nodes.
void check_type(const toml::node_view<const toml::node> node,
                const std::string& name,
                const toml::node_type& type);

// Check that given file can be opened, error and exits otherwise.
void check_file(const std::string& filename);

// Boilerplate type-specific cases.
const toml::table& require_table(const toml::table& table,
                                 const std::string& name);
const std::string& require_string(const toml::table& table,
                                  const std::string& name);
const std::string& require_file(const toml::table& table,
                                const std::string& name);
// (couldn't manage to return a const ref to string instead)
std::optional<std::string> seek_string(const toml::table& table,
                                       const std::string& name);
std::optional<std::string> seek_file(const toml::table& table,
                                     const std::string& name);

} // namespace config

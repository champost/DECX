#pragma once

// Parse config file with TOML++,
// and use these few utilities to check consistency
// and handle errors.

#include <toml++/toml.hpp>

namespace config {

// Context (nested tables sought) useful to provided informative error messages.
using Context = std::vector<std::string>;
std::ostream& operator<<(std::ostream& out, const Context& c);

// Protect against missing or incorrect nodes.
toml::node_view<const toml::node> require_node(const toml::table& table,
                                               const std::string& name,
                                               const toml::node_type& type,
                                               const Context& context);

// Look for an optional node, and return monadic type.
std::optional<toml::node_view<const toml::node>>
seek_node(const toml::table& table,
          const std::string& name,
          const toml::node_type& type,
          const Context& context);

// Protect against missing or incorrect nodes.
void check_type(const toml::node_view<const toml::node> node,
                const std::string& name,
                const toml::node_type& type,
                const Context& context);

// Check that given file can be opened, error and exits otherwise.
// Use the node to locate where in the config file this file was required.
void check_file(const std::string& filename,
                toml::node_view<const toml::node> node);

// Boilerplate type-specific cases.
const toml::table& require_table(const toml::table& table,
                                 const std::string& name,
                                 const Context& context);
const std::string& require_string(const toml::table& table,
                                  const std::string& name,
                                  const Context& context);
const std::string& require_file(const toml::table& table,
                                const std::string& name,
                                const Context& context);
// (couldn't manage to return a const ref to string instead)
std::optional<std::string> seek_string(const toml::table& table,
                                       const std::string& name,
                                       const Context& context);
std::optional<std::string> seek_file(const toml::table& table,
                                     const std::string& name,
                                     const Context& context);

} // namespace config

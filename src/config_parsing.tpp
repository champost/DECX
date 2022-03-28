#pragma once

#include "config_parsing.hpp"

namespace config {

// Match node type against several ones.
template <std::size_t N>
bool is_of_type(View view, Types<N> types) {
  static_assert(N > 0, "You should expect at least 1 type.");
  for (auto& t : types) {
    if (view.type() == t) {
      return true;
    }
  }
  return false;
};

template <std::size_t N>
void Reader::check_type(Types<N> expected_types) {
  if (!is_of_type(focal->data, expected_types)) {
    std::cerr << "Configuration error: node should ";
    switch (N) {
    case 1:
      std::cerr << "be of type " << expected_types[0];
      break;
    default:
      std::cerr << "either be of type " << expected_types[0];
      for (int i{1}; i < N; ++i) {
        std::cerr << " or " << expected_types[i];
      }
    }
    std::cerr << ", not " << focal->data.type() << ". ";
    // Additional information in specific cases.
    if (is_of_type(focal->data, {toml::node_type::integer}) && N == 1 &&
        expected_types[0] == toml::node_type::floating_point) {
      const auto& e{*focal->data.as_integer()};
      std::cerr << "Consider writing `" << e << ".0`";
      std::cerr << " instead of `" << e << "`. ";
    }
    source_and_exit();
  }
};

template <size_t N>
void Reader::check_uniform_array(Types<N> expected_types) {
  check_type({toml::node_type::array});
  std::size_t i{1};
  for (auto&& element : *focal->data.as_array()) {
    descend((View)element, std::to_string(i));
    check_type(expected_types);
    step_up();
    ++i;
  }
};

template <size_t N>
std::optional<View>
Reader::seek_node(Name name, Types<N> expected_types, const bool descend) {
  if (!is_of_type(focal->data, {toml::node_type::table})) {
    std::cerr << "Error in source code: should not seek node by name "
                 "while standing on a non-table node."
              << std::endl;
    exit(-1);
  }
  const auto& table{focal->data.as_table()};
  if (!table->contains(name)) {
    return {};
  }
  const auto& view{(*table)[name]};
  this->descend(view, name);
  check_type(expected_types);
  if (!descend) {
    step_up();
  }
  return {view};
}

// Same, but exit if the node cannot be found.
template <size_t N>
View Reader::require_node(Name name,
                          Types<N> expected_types,
                          const bool descend) {
  const auto& option{seek_node(name, expected_types, descend)};
  if (option.has_value()) {
    return *option;
  }
  std::cerr << "Configuration error: "
            << "'" << name << "' is required, but not given. ";
  source_and_exit();
}

} // namespace config

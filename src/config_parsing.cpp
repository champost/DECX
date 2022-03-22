#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

// Core reader tree-logic and memory handling. = = = = = = = = = = = = = = = = =
Reader::Reader(Table root) : focal(new Node((View)root, {}, {})){};

void Reader::descend(View data, Name name) {
  focal = new Node(data, std::string(name), focal);
};

void Reader::step_up() {
  if (!focal->parent.has_value()) {
    std::cerr << "Error in configuration parsing logic: "
              << "cannot step up root context.";
    exit(-1);
  }
  // Deallocate the node we are leaving.
  const auto left{focal};
  focal = *left->parent;
  delete left;
};

Reader::~Reader() {
  if (focal->parent.has_value()) {
    std::cerr << "Error in configuration parsing logic: "
              << "destroying config reader before it reached back to root."
              << std::endl;
    exit(-1);
  }
  delete focal;
}
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

std::vector<std::string> Node::hierarchy() const {
  std::vector<std::string> result;
  const Node* current{this};
  while (current->parent.has_value()) {
    if (current->name.has_value()) {
      result.emplace_back(*(current->name));
      current = *current->parent;
    }
  }
  return result;
}

std::ostream& operator<<(std::ostream& out, const ColonHierarchy& d) {
  const auto h{d.node->hierarchy()};
  switch (h.size()) {
  case 0:
    out << "root table";
    break;
  case 1:
    out << "'" << h.back() << "'";
    break;
  default:
    out << "'";
    for (size_t i{h.size() - 1}; i > 0; --i) {
      out << h[i];
      out << ':';
    }
    out << h[0];
    out << "'";
  }
  return out;
};

const toml::source_region& Reader::focal_source() const {
  return focal->data.node()->source();
}

void Reader::source_and_exit() const {
  std::cerr << "(" << ColonHierarchy{focal} << " " << focal_source() << ")"
            << std::endl;
  exit(1);
}

bool Reader::has_node(Name name) {
  return focal->data.as_table()->contains(name);
}

void Reader::check_file(Name filename) {
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Configuration error: "
              << "Could not find file '" << filename << "'. " << std::endl;
    source_and_exit();
  }
};

#define DEFINE_REQUIRER(fnname, type, ret)                                     \
  ret Reader::require_##fnname(Name name, const bool descend) {                \
    return require_node(name, {toml::node_type::type}, descend)                \
        .as_##type()                                                           \
        ->get();                                                               \
  };

DEFINE_REQUIRER(bool, boolean, bool);
DEFINE_REQUIRER(float, floating_point, double);
DEFINE_REQUIRER(integer, integer, int);
DEFINE_REQUIRER(string, string, std::string);

// This one's special because there is the file checking.
std::string Reader::require_file(Name name, const bool descend) {
  // Always descend to get correct error messages about file..
  const auto& filename{require_string(name, true)};
  check_file(filename);
  // .. but restore if needed.
  if (!descend) {
    step_up();
  }
  return filename;
};

Table Reader::require_table(Name name, const bool descend) {
  return require_node(name, {toml::node_type::table}, descend).as_table();
};

#define DEFINE_SEEKER(fnname, type, ret)                                       \
  std::optional<ret> Reader::seek_##fnname(Name name, const bool descend) {    \
    const auto& node{seek_node(name, {toml::node_type::type}, descend)};       \
    if (node.has_value()) {                                                    \
      return {node->as_##type()->get()};                                       \
    }                                                                          \
    return {};                                                                 \
  };

DEFINE_SEEKER(bool, boolean, bool);
DEFINE_SEEKER(float, floating_point, double);
DEFINE_SEEKER(integer, integer, int);
DEFINE_SEEKER(string, string, std::string);

std::optional<std::string> Reader::seek_file(Name name, const bool descend) {
  const auto& filename{seek_string(name, true)};
  if (filename.has_value()) {
    check_file(*filename);
    if (!descend) {
      step_up();
    }
    return filename;
  }
  return {};
};

std::optional<Table> Reader::seek_table(Name name, const bool descend) {
  auto node{seek_node(name, {toml::node_type::table}, descend)};
  if (node.has_value()) {
    return {node->as_table()};
  }
  return {};
};

AncestralState Reader::read_ancestral_state() {
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

bool AncestralState::some() { return all || !states.empty(); };

ReportType Reader::read_report_type() {
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
std::vector<double> Reader::read_periods() {
  auto node{require_node("periods", {toml::node_type::array})};
  check_uniform_array(
      "periods", {toml::node_type::floating_point, toml::node_type::integer});
  std::vector<double> periods{};
  for (auto& element : *node.as_array()) {
    if (element.type() == toml::node_type::floating_point) {
      periods.emplace_back(*element.as_floating_point());
    } else if (element.type() == toml::node_type::integer) {
      periods.emplace_back(static_cast<double>(element.as_integer()->get()));
    } else {
      exit(-1); // unreachable.
    }
  }
  return periods;
}

std::vector<std::string>
Reader::read_unique_strings(Name name, const std::string& item_meaning) {
  auto node{require_node(name, {toml::node_type::array})};
  check_uniform_array(name, {toml::node_type::string});
  std::vector<std::string> result{};
  for (const auto& element : *node.as_array()) {
    focal = toml::node_view(element);
    // Bruteforce check that no duplicate has been given.
    const std::string item{*element.as_string()};
    for (const auto& given : result) {
      if (item == given) {
        std::cerr << item_meaning << " name '" << item << "' given twice."
                  << std::endl;
        source_and_exit();
      }
    }
    result.emplace_back(item);
  }
  return result;
}

std::vector<std::string>
Reader::read_unique_words(Name name, const std::string& item_meaning) {
  auto words{require_string(name)};
  std::vector<std::string> result{};
  std::string current{};
  bool next{true};
  for (auto c : words) {
    if (std::isspace(c)) {
      next = true;
    } else if (next) {
      if (!current.empty()) {
        // We have a new word.
        // Check against others for unicity.
        for (const auto& other : result) {
          if (other == current) {
            std::cerr << item_meaning << " name '" << current
                      << "' given twice." << std::endl;
            source_and_exit();
          }
        }
        result.push_back(current);
        current = std::string{};
      }
      current.push_back(c);
      next = false;
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) {
    result.push_back(current);
  }
  if (result.empty()) {
    std::cerr << "No identifiers provided";
    std::cerr << " for " << item_meaning << "." << std::endl;
    source_and_exit();
  }
  return result;
};

std::vector<std::string>
Reader::read_unique_identifiers(Name name, const std::string& item_meaning) {
  const auto node{
      require_node(name, {toml::node_type::array, toml::node_type::string})};
  if (node.type() == toml::node_type::string) {
    return read_unique_words(name, item_meaning);
  }
  return read_unique_strings(name, item_meaning);
}

std::vector<std::vector<int>>
Reader::read_distributions(Name name,
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
Reader::read_distribution(const toml::node& node,
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
  for (const char c : input) {
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

  auto unknown_area_error{[&](const std::string& word) {
    std::cerr << "Unknown area name in distribution: '" << word << "'"
              << " (known areas:";
    for (const auto& area : area_names) {
      std::cout << " '" << area << "'";
    }
    std::cout << ")." << std::endl;
    source_and_exit();
  }};

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
                << " now be honest: you did that on purpose, right?"
                << std::endl;
      source_and_exit();
    }

    if (!is_binary) {
      // Then it's a single area name.
      result.clear();
      if (index.has_value()) {
        result.push_back(index.value());
      } else {
        unknown_area_error(word);
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
        unknown_area_error(word);
      }
    }
  }
  return result;
}

std::string Reader::read_area(Name name,
                              const std::vector<std::string>& area_names) {
  const std::string area{require_string(name)};
  // Check that the area name is known.
  bool found{false};
  for (const auto& known : area_names) {
    if (area == known) {
      found = true;
      break;
    }
  }
  if (!found) {
    std::cerr << "Unknown area '" << area << "' provided.";
    source_and_exit();
  }
  return area;
};

void Reader::read_mrcas(std::map<std::string, std::vector<std::string>>& mrcas,
                        std::map<std::string, std::vector<int>>& fixnodes,
                        std::vector<std::string>& fossilnames,
                        std::vector<std::string>& fossiltypes,
                        std::vector<std::string>& fossilareas,
                        std::vector<double>& fossilages,
                        const std::vector<std::string>& area_names) {

  // It may be that none is given.
  if (!into_optional_table("mrca")) {
    return;
  }

  // Every given MRCA is a sub-table.
  const auto& table{*focal.as_table()};
  for (const auto& mrca : table) {
    const auto& name{mrca.first};
    into_table(name);
    // Got it.

    // All need to have a list of species.
    const std::vector<std::string> species{
        read_unique_identifiers("species", "Species")};
    mrcas.insert({name, species});

    // The rest will depend on their type.
    const std::string type{require_string("type")};

    if (type == "fixed node") {
      // Then a distribution is given.
      const std::vector<int> distribution{read_distribution(
          *require_node("distribution", {toml::node_type::string}).node(),
          area_names)};

      fixnodes.insert({name, distribution});
    } else {
      if (type == "fossil node") {
        fossiltypes.push_back("N");
        fossilages.push_back(0.);
      } else if (type == "fossil branch") {
        fossiltypes.push_back("B");
        fossilages.push_back(require_float("age"));
      } else {
        std::cerr << "Unknown MRCA type: '" << type << "'.";
        std::cerr << " Supported types are ";
        std::cerr << "'fixed node', 'fossil node' and 'fossil branch'";
        std::cerr << "." << std::endl;
        source_and_exit();
      }
      fossilnames.push_back(name);
      // Then only one area is supported.
      const auto& area{read_area("area", area_names)};
      fossilareas.push_back(area);
    }

    step_up();
  }

  // Step out of MRCA table.
  step_up();
};
} // namespace config

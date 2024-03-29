#include "config_parsing.hpp"

#include <filesystem>
#include <iostream>

namespace config {

// Core reader tree-logic and memory handling. = = = = = = = = = = = = = = = = =
Reader::Reader(std::string_view filename, Table root) :
    focal(new Node((View)root, {}, {})),
    folder(std::filesystem::canonical(filename).parent_path()){};

void Reader::descend(View data, Name name) {
  focal = new Node(data, std::string(name), focal);
};

void Reader::step_up() {
  if (!focal->parent.has_value()) {
    std::cerr << "Error in configuration parsing logic (source code): ";
    std::cerr << "cannot step up root context." << std::endl;
    exit(-1);
  }
  // Disallow unused parameters.
  if (!focal->unused_names.empty()) {
    std::cerr << "Configuration error: table contains unexpected parameter";
    auto it{focal->unused_names.begin()};
    size_t i{focal->unused_names.size()};
    switch (i) {
    case 1:
      std::cerr << " '" << *it << "'";
      break;
    default:
      std::cerr << "s '" << *it << "'";
      while (++it != focal->unused_names.end()) {
        --i;
        if (i > 1) {
          std::cerr << ", '";
        } else {
          std::cerr << " and '";
        }
        std::cerr << *it << "'";
      }
    }
    std::cerr << "." << std::endl;
    source_and_exit();
  }
  // Deallocate the node we are leaving.
  const auto left{focal};
  focal = *left->parent;
  delete left;
};

Reader::~Reader() {
  if (focal->parent.has_value()) {
    std::cerr << "Error in configuration parsing logic (source code): ";
    std::cerr << "destroying config reader ";
    std::cerr << "before it reached back to root." << std::endl;
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
  std::cerr << "(" << ColonHierarchy{focal} << " ";
  std::cerr << focal_source() << ")" << std::endl;
  exit(EXIT_ERROR);
}

std::optional<View> Reader::seek_node_any(Name name, const bool descend) {
  // Use the special none type as a wildcard.
  return seek_node(name, {toml::node_type::none}, descend);
}

// Same, but exit if the node cannot be found.
View Reader::require_node_any(Name name, const bool descend) {
  // Use the special none type as a wildcard.
  return require_node(name, {toml::node_type::none}, descend);
}

bool Reader::has_node(Name name) {
  return focal->data.as_table()->contains(name);
}

void Reader::check_file(Name filename) {
  std::filesystem::path path{folder / filename};
  if (!std::filesystem::exists(path)) {
    std::cerr << "Configuration error: ";
    std::cerr << "Could not find file '" << filename << "' ";
    std::cerr << "in " << folder << "." << std::endl;
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
File Reader::require_file(Name name, const bool descend) {
  // Always descend to get correct error messages about file..
  const auto& filename{require_string(name, true)};
  check_file(filename);
  // .. but restore if needed.
  if (!descend) {
    step_up();
  }
  return {filename, folder / filename};
};

Table Reader::require_table(Name name, const bool descend) {
  const auto& table{
      require_node(name, {toml::node_type::table}, descend).as_table()};
  if (descend) {
    // Collect all names to check that no one is present in the file
    // but unused by the program.
    for (const auto& pair : *table) {
      focal->unused_names.insert(pair.first);
    }
  }
  return table;
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

std::optional<File> Reader::seek_file(Name name, const bool descend) {
  const auto& filename{seek_string(name, true)};
  if (filename.has_value()) {
    check_file(*filename);
    if (!descend) {
      step_up();
    }
    return {{*filename, folder / *filename}};
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
  auto node{require_node("ancestral_states",
                         {toml::node_type::array,
                          toml::node_type::boolean,
                          toml::node_type::string},
                         true)};
  // Construct the value depending on the node type.
  switch (node.type()) {
  case toml::node_type::array:
    check_uniform_array({toml::node_type::string});
    for (auto& element : *node.as_array()) {
      a.states.emplace_back(*element.as_string());
    }
    break;
  case toml::node_type::boolean:
    a.all = node.as_boolean()->get();
    break;
  case toml::node_type::string:
    a.states.emplace_back(node.as_string()->get());
    break;
  default: // Alternate types have already been ruled out.
    break;
  }
  step_up();
  return a;
};

void AncestralState::set_all() {
  states.clear();
  all = true;
};

bool AncestralState::some() { return all || !states.empty(); };

ReportType Reader::read_report_type() {
  auto string{require_string("report", true)};
  ReportType result;
  if (string == "states") {
    result = ReportType::States;
  } else if (string == "splits") {
    result = ReportType::Splits;
  } else {
    std::cerr << "Unknown report type: '" << string << "'. ";
    std::cerr << "Supported types are 'states' and 'splits'." << std::endl;
    source_and_exit();
  }
  step_up();
  return result;
}

std::vector<double> Reader::read_periods() {
  bool dates{false};
  auto node{seek_node("periods", {toml::node_type::array}, true)};
  if (!node.has_value()) {
    node = seek_node("durations", {toml::node_type::array}, true);
  }
  if (!node.has_value()) {
    dates = true;
    node = seek_node("dates", {toml::node_type::array}, true);
  }
  if (!node.has_value()) {
    std::cerr << "Configuration error: ";
    std::cerr << "missing periods specifications." << std::endl;
    std::cerr << "Consider providing ";
    std::cerr << "a 'dates' or a 'durations' array." << std::endl;
    source_and_exit();
  }
  std::vector<double> periods{};
  check_uniform_array(
      {toml::node_type::floating_point, toml::node_type::integer});
  double sum{0.};
  size_t i{1};
  for (auto& element : *node->as_array()) {
    descend((View)element, std::to_string(i));
    double value;
    if (element.type() == toml::node_type::floating_point) {
      value = **element.as_floating_point();
    } else if (element.type() == toml::node_type::integer) {
      value = static_cast<double>(element.as_integer()->get());
    } else {
      exit(-1); // unreachable.
    }
    if (dates) {
      if (value < sum) {
        std::cerr << "Configuration error: dates must be specified ";
        std::cerr << "in increasing order." << std::endl;
        source_and_exit();
      }
      const double& duration{value - sum};
      periods.emplace_back(duration);
      sum += duration;
    } else {
      periods.emplace_back(value);
    }
    step_up();
    ++i;
  }
  step_up();
  return periods;
}

Reader::IdValidation Reader::variable_like(const std::string_view word) {
  std::stringstream error;
  size_t loc{0};
#define ERROR_OUT return {error.str(), loc};

  bool first{true};
  bool found_letter{false};
  for (const auto& c : word) {
    if (!isascii(c)) {
      error << "non-ascii characters are disallowed in identifiers: ";
      error << "'" << word << "'";
      ERROR_OUT;
    }
    if (!(isdigit(c) || isalpha(c) || c == '_' || c == '.' || c == '-')) {
      error << "invalid character in identifier: '" << c << "'";
      ERROR_OUT;
    }
    if (first && isdigit(c)) {
      error << "identifier cannot start with a digit: ";
      error << "'" << word << "'";
      ERROR_OUT;
    }
    if (isalpha(c) || c == '_') {
      found_letter = true;
    }
    if (loc == 0) {
      first = false;
    }
    ++loc;
  }
  if (!found_letter) {
    error << "identifier must contain at least one letter or underscore: ";
    error << "'" << word << "'";
    ERROR_OUT;
  }
  return {{}, 0};
};
#undef ERROR_OUT

std::vector<std::string> Reader::read_unique_strings(
    Name name, const std::string_view item_meaning, const IdValider valider) {
  auto node{require_node(name, {toml::node_type::array}, true)};
  check_uniform_array({toml::node_type::string});
  std::vector<std::string> result{};
  size_t i{1};
  for (const auto& element : *node.as_array()) {
    descend((View)element, std::to_string(i));
    const std::string item{*element.as_string()};
    // Check validity.
    const auto& [error, _] = valider(item);
    if (!error.empty()) {
      std::cerr << "Invalid " << item_meaning << " name: ";
      std::cerr << error << "." << std::endl;
      source_and_exit();
    }
    // Bruteforce check that no duplicate has been given.
    for (const auto& given : result) {
      if (item == given) {
        std::cerr << item_meaning << " name '" << item << "' ";
        std::cerr << "given twice." << std::endl;
        source_and_exit();
      }
    }
    result.emplace_back(item);
    step_up();
    ++i;
  }
  step_up();
  return result;
}

std::vector<std::string> Reader::read_unique_words(
    Name name, const std::string& item_meaning, const IdValider valider) {
  auto words{require_string(name, true)};
  std::vector<std::string> result{};
  std::string current{};
  bool next{true};
  for (auto c : words) {
    if (std::isspace(c)) {
      next = true;
    } else if (next) {
      if (!current.empty()) {
        // We have a new word.
        // Check validity.
        const auto& [error, _] = valider(current);
        if (!error.empty()) {
          std::cerr << "Invalid " << item_meaning << " name: ";
          std::cerr << error << "." << std::endl;
          source_and_exit();
        }
        // Check against others for unicity.
        for (const auto& other : result) {
          if (other == current) {
            std::cerr << item_meaning << " name '" << current << "' ";
            std::cerr << "given twice." << std::endl;
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
    std::cerr << "No identifiers provided for ";
    std::cerr << item_meaning << "." << std::endl;
    source_and_exit();
  }
  step_up();
  return result;
};

std::vector<std::string> Reader::read_unique_identifiers(
    Name name, const std::string& item_meaning, const IdValider valider) {
  const auto node{require_node(
      name, {toml::node_type::array, toml::node_type::string}, false)};
  if (node.type() == toml::node_type::string) {
    return read_unique_words(name, item_meaning, valider);
  }
  return read_unique_strings(name, item_meaning, valider);
}

std::vector<std::vector<int>>
Reader::read_distributions(Name name,
                           const std::vector<std::string>& area_names) {
  auto node{require_node(name, {toml::node_type::array}, true)};
  check_uniform_array({toml::node_type::string});
  std::vector<std::vector<int>> distributions;
  size_t i{1};
  for (auto& element : *node.as_array()) {
    descend((View)element, std::to_string(i));
    distributions.push_back(read_distribution(area_names));
    step_up();
    ++i;
  }
  step_up();
  return distributions;
}

std::vector<int>
Reader::read_distribution(const std::vector<std::string>& area_names) {
  const auto& node{focal->data};
  if (node.type() != toml::node_type::string) {
    std::cerr << "Distribution should be specified ";
    std::cerr << "as a string, not " << node.type() << ".";
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
    std::cerr << "Distributions cannot be empty.";
    source_and_exit();
  }

  auto unknown_area_error{[&](const std::string& word) {
    std::cerr << "Unknown area name ";
    std::cerr << "in distribution: '" << word << "'." << std::endl;
    std::cerr << "(known areas:";
    for (const auto& area : area_names) {
      std::cerr << " '" << area << "'";
    }
    std::cerr << ")" << std::endl;
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

    if (!is_binary) {
      // Then it's a single area name.
      result.clear();
      if (index.has_value()) {
        result.push_back(index.value());
      } else {
        unknown_area_error(word);
      }
    } else if (word.size() != area_names.size()) {
      std::cerr << "Invalid binary specification ";
      std::cerr << "of a distribution:" << std::endl;
      std::cerr << "'" << word << "' contains " << word.size() << " digits ";
      std::cerr << "but there are " << area_names.size();
      std::cerr << " areas." << std::endl;
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
  const std::string area{require_string(name, true)};
  // Check that the area name is known.
  bool found{false};
  for (const auto& known : area_names) {
    if (area == known) {
      found = true;
      break;
    }
  }
  if (!found) {
    std::cerr << "Unknown area '" << area << "' provided." << std::endl;
    source_and_exit();
  }
  step_up();
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
  if (!seek_table("mrca", true).has_value()) {
    return;
  }

  // Every given MRCA is a sub-table.
  for (const auto& mrca : *focal->data.as_table()) {
    const auto& mrca_name{mrca.first};
    if (mrca.second.type() != toml::node_type::table) {
      std::cerr << "All values in `mrca` table should be tables themselves ";
      std::cerr << "but the value '" << mrca_name << "' ";
      std::cerr << "is of type '" << mrca.second.type() << "'. ";
      std::cerr << "Is your MRCA correctly specified ";
      std::cerr << "as a [mrca." << mrca_name << "] table?" << std::endl;
      descend((View)mrca.second, mrca_name);
      source_and_exit();
    }
    require_table(mrca_name, true);

    // All need to have a list of species.
    const std::vector<std::string> species{
        read_unique_identifiers("species", "Species")};
    mrcas.insert({mrca_name, species});

    // The rest will depend on their type.
    const std::string type{require_string("type", true)};

    if (type == "fixed node") {
      step_up();
      // Then a distribution is given.
      require_node_any("distribution", true);
      const std::vector<int> distribution{read_distribution(area_names)};
      fixnodes.insert({mrca_name, distribution});
      step_up();
    } else {
      if (type == "fossil node") {
        step_up();
        fossiltypes.push_back("N");
        fossilages.push_back(0.);
      } else if (type == "fossil branch") {
        step_up();
        fossiltypes.push_back("B");
        fossilages.push_back(require_float("age", false));
      } else {
        std::cerr << "Unknown MRCA type: '" << type << "'." << std::endl;
        std::cerr << "Supported types are ";
        std::cerr << "'fixed node', 'fossil node' ";
        std::cerr << "and 'fossil branch'." << std::endl;
        source_and_exit();
      }
      fossilnames.push_back(mrca_name);
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

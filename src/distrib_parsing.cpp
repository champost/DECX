#include "distrib_parsing.hpp"

#include <fstream>
#include <ios>

namespace distribution {

// https://stackoverflow.com/a/116220/3719101
auto read_file(std::string_view path) -> std::string {
  constexpr auto read_size = std::size_t(4096);
  auto stream = std::ifstream(path.data());
  stream.exceptions(std::ios_base::badbit);
  auto out = std::string();
  auto buf = std::string(read_size, '\0');
  while (stream.read(&buf[0], read_size)) {
    out.append(buf, 0, stream.gcount());
  }
  out.append(buf, 0, stream.gcount());
  return out;
}

Map parse_file(const std::string_view filename, const Areas& areas) {
  const auto file{read_file(filename)};
  return {};
};

} // namespace distribution

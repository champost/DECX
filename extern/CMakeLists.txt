# Third-party header library to parse TOML files.
add_library(toml++ INTERFACE)
target_include_directories(
  toml++ INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/toml++/include"
)

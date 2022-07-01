add_executable(
  decx main.cpp

  # For some reason, these fortran dependencies have been edited
  # so their sources need to stick around as long as no one is competent
  # to update this edition to "expokit" and "matexp".
  my_expokit.f
  my_matexp.f

  # C++ sources
  AncSplit.cpp
  BioGeoTree.cpp
  BioGeoTreeTools.cpp
  BranchSegment.cpp
  InputReader.cpp
  OptimizeBioGeo.cpp
  RateMatrixUtils.cpp
  RateModel.cpp
  Utils.cpp
  config_parsing.cpp
  distrib_parsing.cpp
  distrib_parsing_areas.cpp
  distrib_parsing_legacy.cpp
  distrib_parsing_species.cpp
  lexer.cpp
  node.cpp
  superdouble.cpp
  tree.cpp
  tree_reader.cpp
)

target_link_libraries(
  decx PRIVATE
  ${GSL_LIBRARIES}
  ${BLAS_LIBRARIES}
  ${LAPACK_LIBRARIES}
  toml++
)

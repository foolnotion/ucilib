set(ucilib_FOUND YES)

include(CMakeFindDependencyMacro)
find_dependency(fmt)
find_dependency(reproc++)
find_dependency(tl-expected)

if(ucilib_FOUND)
  include("${CMAKE_CURRENT_LIST_DIR}/ucilibTargets.cmake")
endif()

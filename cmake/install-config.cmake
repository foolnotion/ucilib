include(CMakeFindDependencyMacro)

# fmt and reproc++ are PRIVATE link deps; for static archives consumers must
# be able to resolve $<LINK_ONLY:...> entries in INTERFACE_LINK_LIBRARIES.
find_dependency(fmt)
find_dependency(reproc++)
find_dependency(tl-expected)

include("${CMAKE_CURRENT_LIST_DIR}/ucilibTargets.cmake")

if(PROJECT_IS_TOP_LEVEL)
  set(
      CMAKE_INSTALL_INCLUDEDIR "include/ucilib-${PROJECT_VERSION}"
      CACHE STRING ""
  )
  set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
# should match the name of variable set in the install-config.cmake script
set(package ucilib)

install(
    DIRECTORY
    include/
    "${PROJECT_BINARY_DIR}/export/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT ucilib_Development
)

install(
    TARGETS ucilib_ucilib
    EXPORT ucilibTargets
    RUNTIME #
    COMPONENT ucilib_Runtime
    LIBRARY #
    COMPONENT ucilib_Runtime
    NAMELINK_COMPONENT ucilib_Development
    ARCHIVE #
    COMPONENT ucilib_Development
    INCLUDES #
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    ucilib_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${package}"
    CACHE STRING "CMake package config location relative to the install prefix"
)
set_property(CACHE ucilib_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
mark_as_advanced(ucilib_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${ucilib_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT ucilib_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${ucilib_INSTALL_CMAKEDIR}"
    COMPONENT ucilib_Development
)

install(
    EXPORT ucilibTargets
    NAMESPACE ucilib::
    DESTINATION "${ucilib_INSTALL_CMAKEDIR}"
    COMPONENT ucilib_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()

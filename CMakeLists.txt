cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    ucilib
    VERSION 0.1.0
    DESCRIPTION "UCI Chess Engine Interfadce"
    HOMEPAGE_URL "https://github.com/foolnotion/ucilib"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    ucilib_ucilib
    source/engine.cpp
    source/error.cpp
    source/parser.cpp
)
add_library(ucilib::ucilib ALIAS ucilib_ucilib)

include(GenerateExportHeader)
generate_export_header(
    ucilib_ucilib
    BASE_NAME ucilib
    EXPORT_FILE_NAME export/ucilib/ucilib_export.hpp
    CUSTOM_CONTENT_FROM_VARIABLE pragma_suppress_c4251
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(ucilib_ucilib PUBLIC UCILIB_STATIC_DEFINE)
endif()

set_target_properties(
    ucilib_ucilib PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME ucilib
    OUTPUT_NAME ucilib
)

target_include_directories(
    ucilib_ucilib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_include_directories(
    ucilib_ucilib
    PRIVATE
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_include_directories(
    ucilib_ucilib SYSTEM
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>"
)

target_compile_features(ucilib_ucilib PUBLIC cxx_std_20)

find_package(fmt REQUIRED)
find_package(reproc++ REQUIRED)
find_package(tl-expected REQUIRED)

target_link_libraries(ucilib_ucilib
    PRIVATE fmt::fmt reproc++
    PUBLIC tl::expected
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT ucilib_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of ucilib"
  )
endif()

include(cmake/dev-mode.cmake)

cmake_minimum_required(VERSION 3.14)

project(ucilibTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(ucilib REQUIRED)
  enable_testing()
endif()

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(ucilib_parser_test source/parser_test.cpp)
target_link_libraries(
    ucilib_parser_test PRIVATE
    ucilib::ucilib
    Catch2::Catch2WithMain
)
target_include_directories(
    ucilib_parser_test PRIVATE
    "${PROJECT_SOURCE_DIR}/../source"
)
target_compile_features(ucilib_parser_test PRIVATE cxx_std_20)

catch_discover_tests(ucilib_parser_test)

add_executable(ucilib_engine_test source/engine_test.cpp)
target_link_libraries(
    ucilib_engine_test PRIVATE
    ucilib::ucilib
    Catch2::Catch2WithMain
)
target_compile_features(ucilib_engine_test PRIVATE cxx_std_20)

catch_discover_tests(ucilib_engine_test)

# ---- End-of-file commands ----

add_folders(Test)

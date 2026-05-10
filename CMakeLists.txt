cmake_minimum_required(VERSION 3.25)
project(cppstdlib LANGUAGES CXX)

add_library(settings INTERFACE)
target_compile_options(settings INTERFACE -Wall -Wextra -Werror -std=c++23)

add_library(lib)
target_sources(lib PRIVATE lib.cc)
target_link_libraries(lib PUBLIC settings)

add_executable(app main.cc)
target_link_libraries(app PRIVATE lib)

include(FetchContent)

set(APPROVAL_TESTS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(APPROVAL_TESTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ApprovalTests
    GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp
    GIT_TAG v.10.13.0
)

set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest
    GIT_TAG v2.5.2
)

FetchContent_MakeAvailable(ApprovalTests doctest)

enable_testing()

add_executable(tests tests.cc)
target_link_libraries(tests PRIVATE lib ApprovalTests::ApprovalTests doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(tests)

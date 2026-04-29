cmake_minimum_required(VERSION 3.25)
project(cppstdlib LANGUAGES CXX)

add_library(settings INTERFACE)
set_target_properties(settings PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_options(settings INTERFACE -Wall -Wextra -Werror)

add_library(lib)
target_sources(lib PRIVATE lib.cc)
target_link_libraries(lib PUBLIC settings)

add_executable(app main.cc)
target_link_libraries(app PRIVATE lib)

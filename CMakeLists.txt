cmake_minimum_required(VERSION 3.13)

project(shell-starter-cpp)

# Automatically pick up new/removed sources without re-running cmake manually
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
     src/*.cpp src/*.hpp
     src/**/*.cpp src/**/*.hpp)

set(CMAKE_CXX_STANDARD 23) # Enable the C++23 standard

add_executable(shell ${SOURCE_FILES})

target_link_libraries(shell PRIVATE readline)

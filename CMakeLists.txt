cmake_minimum_required(VERSION 3.14)
project(command-desktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ftxui CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(command-desktop ${SOURCES})
target_include_directories(command-desktop PRIVATE src)
target_link_libraries(command-desktop PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

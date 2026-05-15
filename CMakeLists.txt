cmake_minimum_required(VERSION 3.14)
project(cld VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ftxui CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(cld ${SOURCES})
target_include_directories(cld PRIVATE src)
target_link_libraries(cld PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

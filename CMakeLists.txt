cmake_minimum_required(VERSION 3.14)
project(cld VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ftxui CONFIG QUIET)
if(NOT ftxui_FOUND)
  message(STATUS "FTXUI not found system-wide, fetching via FetchContent...")
  include(FetchContent)
  FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v5.0.0
  )
  FetchContent_MakeAvailable(ftxui)
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(cld ${SOURCES})
target_include_directories(cld PRIVATE src)
target_link_libraries(cld PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

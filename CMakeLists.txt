cmake_minimum_required(VERSION 3.14)
project(cld VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_X11_BACKEND "Build with X11/SDL2 window support" OFF)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

find_package(ftxui CONFIG QUIET)
if(NOT ftxui_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v5.0.0
  )
  FetchContent_MakeAvailable(ftxui)
endif()

file(GLOB_RECURSE SOURCES src/*.cpp)

# Exclude X11 renderer unless enabled
if(NOT ENABLE_X11_BACKEND)
  list(REMOVE_ITEM SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/X11Renderer.cpp
  )
endif()

add_executable(cld ${SOURCES})
target_include_directories(cld PRIVATE src)
target_link_libraries(cld PRIVATE
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(ENABLE_X11_BACKEND)
  find_package(SDL2 REQUIRED)
  find_package(SDL2_ttf REQUIRED)
  target_link_libraries(cld PRIVATE
    SDL2::SDL2
    SDL2::SDL2_ttf
  )
  target_compile_definitions(cld PRIVATE ENABLE_X11_BACKEND)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(cld PRIVATE stdc++fs)
endif()

install(TARGETS cld RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

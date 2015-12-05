cmake_minimum_required(VERSION 2.8)

set(TARGET_NAME deepin-wm-switcher)
project(${TARGET_NAME})

# default use dde-daemon
option(USE_BUILTIN_KEYBINDING "use builtin keybinding handling" OFF)
option(USE_CLANG "use clang++ to build" OFF)

if (USE_CLANG)
    set(CMAKE_CXX_COMPILER clang++)
endif()

add_definitions(-DQT_MESSAGELOGCONTEXT)
add_compile_options(-std=c++11)
configure_file(config.h.in config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src)


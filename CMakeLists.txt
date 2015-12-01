cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_COMPILER clang++)
set(TARGET_NAME deepin-wm-switcher)
project(${TARGET_NAME})

add_compile_options(-std=c++11)

add_subdirectory(src)


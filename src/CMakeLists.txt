set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

find_package(PkgConfig)
pkg_check_modules(DEP_LIBS REQUIRED glib-2.0 x11 xcb-keysyms)

find_package(Qt5Gui)
find_package(Qt5DBus)
find_package(Qt5X11Extras)

add_compile_options(${DEP_LIBS_CFLAGS})
include_directories(${DEP_LIBS_INCLUDE_DIRS})

set(SRCS main.cpp)

add_executable(${TARGET_NAME} ${SRCS})
target_link_libraries(${TARGET_NAME} Qt5::Gui Qt5::DBus Qt5::X11Extras
    ${DEP_LIBS_LIBRARIES})

install(TARGETS ${TARGET_NAME} DESTINATION bin)



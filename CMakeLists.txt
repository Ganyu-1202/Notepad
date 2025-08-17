cmake_minimum_required(VERSION 3.10)
project(Notepad C)

set(CMAKE_C_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

add_executable(Notepad
        main.c
        notepad.h
        file_operations.h
        ui.h
        notepad.c
        ui.c
        file_operations.c
        output_exception.h
        output_exception.c)
target_link_libraries(Notepad ${GTK3_LIBRARIES})

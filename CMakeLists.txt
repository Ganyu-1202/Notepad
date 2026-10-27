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
        appearance.c
        editor_decorations.c
        document_encoding.c
        font_dialog.c
        file_operations.c
        output_exception.h
        output_exception.c)
target_link_libraries(Notepad ${GTK3_LIBRARIES})

include(CTest)
if (BUILD_TESTING)
        add_executable(NotepadAppearanceTest
                tests/appearance_test.c
                notepad.c ui.c appearance.c editor_decorations.c document_encoding.c font_dialog.c file_operations.c output_exception.c)
        target_include_directories(NotepadAppearanceTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(NotepadAppearanceTest ${GTK3_LIBRARIES})
        add_test(NAME appearance COMMAND NotepadAppearanceTest)
        set_tests_properties(appearance PROPERTIES
                ENVIRONMENT "G_DEBUG=fatal-warnings"
                TIMEOUT 30)
endif()

# Windows下生成GUI程序不弹出终端
if (WIN32)
        set_target_properties(Notepad PROPERTIES WIN32_EXECUTABLE TRUE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

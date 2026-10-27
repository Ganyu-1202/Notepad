cmake_minimum_required(VERSION 3.10)
project(Notepad C)

set(CMAKE_C_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

# Application lifecycle, file operations and encoding.
set(NOTEPAD_CORE_SOURCES
        src/core/notepad.c
        src/core/notepad.h
        src/core/file_operations.c
        src/core/file_operations.h
        src/core/document_encoding.c)

# GTK widgets, menus and editor interaction.
set(NOTEPAD_UI_SOURCES
        src/ui/ui.c
        src/ui/ui.h
        src/ui/editor_decorations.c
        src/ui/font_dialog.c
        src/ui/font_picker.c
        src/ui/font_picker.h
        src/ui/tab_settings.c
        src/ui/zoom.c
        src/ui/zoom.h)

# Theme/background rendering and shared helpers.
set(NOTEPAD_THEME_SOURCES src/theme/appearance.c)
set(NOTEPAD_COMMON_SOURCES
        src/common/output_exception.c
        src/common/output_exception.h)

set(NOTEPAD_SOURCES
        ${NOTEPAD_CORE_SOURCES}
        ${NOTEPAD_UI_SOURCES}
        ${NOTEPAD_THEME_SOURCES}
        ${NOTEPAD_COMMON_SOURCES})

add_executable(Notepad src/main.c ${NOTEPAD_SOURCES})
target_include_directories(Notepad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Notepad ${GTK3_LIBRARIES})

include(CTest)
if (BUILD_TESTING)
        add_executable(NotepadAppearanceTest tests/appearance_test.c ${NOTEPAD_SOURCES})
        target_include_directories(NotepadAppearanceTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(NotepadAppearanceTest ${GTK3_LIBRARIES})
        add_test(NAME appearance COMMAND NotepadAppearanceTest)
        set_tests_properties(appearance PROPERTIES
                ENVIRONMENT "G_DEBUG=fatal-warnings"
                TIMEOUT 30)
endif()

# Windows下生成GUI程序不弹出终端
if (WIN32)
        enable_language(RC)
        target_sources(Notepad PRIVATE src/resources/notepad.rc)
        target_include_directories(Notepad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/resources)
        set_target_properties(Notepad PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(DllBot
    VERSION 3.0.1
    LANGUAGES CXX
)


file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" MOD_JSON)
string(JSON MOD_ID GET "${MOD_JSON}" id)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/global.cpp
    src/pathfinder.cpp
    src/macro.cpp
    src/keybinds.cpp
    src/utils/utils.cpp
    src/gdr/gdr.cpp
    src/renderer/renderer.cpp
    src/download/github-api.cpp

    src/hacks/other.cpp
    src/hacks/tps_bypass.cpp
    src/hacks/frame_stepper.cpp
    src/hacks/layout_mode.cpp
    src/hacks/show_trajectory.cpp
    src/hacks/coin_finder.cpp
    src/hacks/clickbot.cpp
    src/hacks/autoclicker.cpp

    src/ui/macro_editor.cpp
    src/ui/macro_timeline_layer.cpp
    src/ui/button_edit_layer.cpp
    src/ui/game_ui.cpp
    src/ui/record_layer.cpp
    src/ui/render_settings_layer.cpp
    src/ui/load_macro_layer.cpp
    src/ui/macro_cell.cpp
    src/ui/clickbot_layer.cpp
    src/ui/button_setting.cpp
    src/ui/macro_index_layer.cpp

    src/practice_fixes/input.cpp
    src/practice_fixes/player.cpp
    src/practice_fixes/play_layer.cpp
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})

# SLC include root
# Allows SLC headers to use:
#   #include "util.hpp"
#   #include "formats/v3/metadata.hpp"
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gdr/slc
)

if (WIN32)
    set(DESKTOP_GEODE_MODS_DIR "C:/Users/drean/OneDrive/Desktop/GeometryDash/geode/mods")

    add_custom_target(${PROJECT_NAME}_DEPLOY_DESKTOP ALL
        DEPENDS ${PROJECT_NAME}_PACKAGE

        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${DESKTOP_GEODE_MODS_DIR}"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/${MOD_ID}.geode"
            "${DESKTOP_GEODE_MODS_DIR}/${MOD_ID}.geode"

        VERBATIM
    )
endif()

if (MSVC)
    # Disable Link-Time Code Generation (WholeProgramOptimization)
    # which causes CL.exe crash
    set_property(
        TARGET ${PROJECT_NAME}
        PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE
    )

    set_property(
        TARGET ${PROJECT_NAME}
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
    )

    set_property(
        TARGET ${PROJECT_NAME}
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
    )

    # Add flags to help with massive files
    target_compile_options(${PROJECT_NAME} PRIVATE
        /Zm200
        /bigobj
        /d2ReducedOptimizeHugeFunctions
        /Od
    )
endif()

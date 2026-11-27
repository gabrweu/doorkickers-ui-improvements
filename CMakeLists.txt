cmake_minimum_required(VERSION 3.20)
project(dk2_eras_ui_improvements VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static CRT so the plugin doesn't depend on a VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Door Kickers 2 is 64-bit; configure with an x64 toolchain")
endif()

# A dk2ml (Door Kickers 2 Native Mod Loader) plugin. The loader's public headers are in dk2ml/ (see dk2ml/README.md).
add_library(dk2_eras_ui_improvements SHARED
    src/plugin.cpp
    src/core/game.cpp
    src/core/gui.cpp
    src/core/settings.cpp
    src/campaigns/cards.cpp
    src/equipment/back.cpp
    src/equipment/entries.cpp
    src/equipment/grid.cpp
    src/loadouts/store.cpp
    src/loadouts/history.cpp
    src/loadouts/panel.cpp
    src/slots/concealment.cpp
    src/slots/slots.cpp
    src/slots/ttbg.cpp
    src/slots/ttmerge.cpp
    src/portraits/picker.cpp
    src/portraits/portraits.cpp
    src/hud/cursor.cpp
    src/hud/hudtip.cpp
    src/hud/wheel.cpp)
# local includes are relative to src/ ("core/game.h")
target_include_directories(dk2_eras_ui_improvements PRIVATE src dk2ml)
target_compile_definitions(dk2_eras_ui_improvements PRIVATE UNICODE _UNICODE PLUGIN_VERSION="${PROJECT_VERSION}") # the manifest
target_link_libraries(dk2_eras_ui_improvements PRIVATE user32)

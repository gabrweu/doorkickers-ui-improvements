cmake_minimum_required(VERSION 3.20)
project(dk2_eras_ui_improvements VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static CRT so the plugin doesn't depend on a VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Door Kickers 2 is 64-bit; configure with an x64 toolchain")
endif()

# the plugin API header (dk2ml.h) from the mod loader repo, or an extracted release's sdk\ folder
set(DK2ML_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../doorkickers-modloader/include" CACHE PATH "folder containing dk2ml.h")
if(NOT EXISTS "${DK2ML_SDK_DIR}/dk2ml.h")
    message(FATAL_ERROR "dk2ml.h not found in DK2ML_SDK_DIR (${DK2ML_SDK_DIR})")
endif()

add_library(dk2_eras_ui_improvements SHARED
    src/back.cpp
    src/cursor.cpp
    src/game.cpp
    src/gui.cpp
    src/entries.cpp
    src/grid.cpp
    src/hudtip.cpp
    src/picker.cpp
    src/plugin.cpp
    src/portraits.cpp
    src/settings.cpp
    src/slots.cpp
    src/ttmerge.cpp)
target_include_directories(dk2_eras_ui_improvements PRIVATE "${DK2ML_SDK_DIR}")
target_compile_definitions(dk2_eras_ui_improvements PRIVATE UNICODE _UNICODE)
target_link_libraries(dk2_eras_ui_improvements PRIVATE user32)

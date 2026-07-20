cmake_minimum_required(VERSION 3.28)

project(laiue_radio
    VERSION 1.0.0
    DESCRIPTION "Native video game music player built from laiue components"
    LANGUAGES C
)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "laiue radio requires Windows and MSVC/clang-cl")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(laiue_radio WIN32
    src/main.c
    src/track.c
    src/track.h
    src/player_state.c
    src/player_state.h
    src/source/khinsider.c
    src/source/khinsider.h
    src/source/radio_source.c
    src/source/radio_source.h
    src/engine/api.h
    src/engine/window.c
    src/engine/window.h
    src/engine/input.c
    src/engine/input.h
    src/engine/time.c
    src/engine/time.h
    src/engine/audio.c
    src/engine/audio.h
    src/engine/ui_renderer_d3d12.c
    src/engine/ui_renderer.h
    src/engine/ui.c
    src/engine/ui.h
    src/engine/ui_font.c
    src/engine/ui_font.h
    src/engine/math.c
    src/engine/math.h
    src/engine/image_wic.c
    src/engine/image_wic.h
    src/engine/generated/ui_vs.h
    src/engine/generated/ui_ps.h
    shaders/ui.hlsl
)
set_source_files_properties(shaders/ui.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)

target_include_directories(laiue_radio PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(laiue_radio PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)
target_compile_options(laiue_radio PRIVATE /W4 /utf-8)
target_link_libraries(laiue_radio PRIVATE
    user32
    gdi32
    d3d12
    dxgi
    dxguid
    mf
    mfplat
    mfuuid
    ole32
    oleaut32
    shlwapi
    uuid
    windowscodecs
    winhttp
    comdlg32
    shell32
)

set_target_properties(laiue_radio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(khinsider_tests
        tests/khinsider_tests.c
        src/track.c
        src/track.h
        src/source/khinsider.c
        src/source/khinsider.h
    )
    target_include_directories(khinsider_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(khinsider_tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(khinsider_tests PRIVATE /W4 /utf-8)
    target_link_libraries(khinsider_tests PRIVATE shlwapi winhttp)
    add_test(NAME khinsider_parser COMMAND khinsider_tests)
endif()

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
    src/source/ocremix.c
    src/source/ocremix.h
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
    add_executable(source_tests
        tests/source_tests.c
        src/track.c
        src/track.h
        src/source/ocremix.c
        src/source/ocremix.h
    )
    target_include_directories(source_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(source_tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(source_tests PRIVATE /W4 /utf-8)
    target_link_libraries(source_tests PRIVATE shlwapi winhttp)
    add_test(NAME online_source_parsers COMMAND source_tests)

    add_executable(ui_font_tests
        tests/ui_font_tests.c
        src/engine/ui_font.c
        src/engine/ui_font.h
    )
    target_include_directories(ui_font_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(ui_font_tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(ui_font_tests PRIVATE /W4 /utf-8)
    target_link_libraries(ui_font_tests PRIVATE gdi32 user32)
    add_test(NAME ui_font_glyph_lookup COMMAND ui_font_tests)

    add_executable(player_state_tests
        tests/player_state_tests.c
        src/player_state.c
        src/player_state.h
        src/track.c
        src/track.h
    )
    target_include_directories(player_state_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(player_state_tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(player_state_tests PRIVATE /W4 /utf-8)
    add_test(NAME player_state_history COMMAND player_state_tests)

    add_executable(track_tests
        tests/track_tests.c
        src/track.c
        src/track.h
    )
    target_include_directories(track_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(track_tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(track_tests PRIVATE /W4 /utf-8)
    add_test(NAME track_local_file COMMAND track_tests)
endif()

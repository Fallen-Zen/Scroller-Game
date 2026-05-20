cmake_minimum_required(VERSION 3.20)
project(Scroller VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies via FetchContent (no manual installs needed) ────────────────
include(FetchContent)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.9
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(SDL2_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        release-2.8.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)

# SDL_mixer: only WAV/OGG, skip heavy codecs
set(SDL2MIXER_OPUS     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD      OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK  OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3      OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL2 SDL2_mixer Catch2)

# ── Game executable ──────────────────────────────────────────────────────────
file(GLOB_RECURSE GAME_SOURCES  CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE GAME_HEADERS  CONFIGURE_DEPENDS "src/*.h")

# Headers are listed alongside sources so IDEs (Xcode, VS) show them in the
# project navigator. CMake will not compile them — listing a .h in an
# executable target is purely for IDE visibility.
add_executable(Scroller ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories(Scroller PRIVATE src)

target_link_libraries(Scroller PRIVATE SDL2::SDL2 SDL2_mixer)

if(WIN32)
    # SDL2main provides the WinMain entry point
    target_link_libraries(Scroller PRIVATE SDL2::SDL2main)

    # Copy SDL2 DLLs next to the .exe after build
    add_custom_command(TARGET Scroller POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:Scroller>
    )
endif()

if(APPLE)
    set_target_properties(Scroller PROPERTIES
        MACOSX_BUNDLE             TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Scroller"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.dev.scroller"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

# Treat warnings as errors in CI; relaxed locally
if(MSVC)
    target_compile_options(Scroller PRIVATE /W4)
else()
    target_compile_options(Scroller PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)

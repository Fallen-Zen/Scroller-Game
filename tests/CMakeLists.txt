file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(ScrollerTests ${TEST_SOURCES})

target_include_directories(ScrollerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ScrollerTests PRIVATE
    Catch2::Catch2WithMain
    SDL2::SDL2   # tests can use SDL types without a window
)

include(CTest)
include(Catch)
catch_discover_tests(ScrollerTests)

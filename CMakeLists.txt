cmake_minimum_required(VERSION 3.25)

project(VibeCity
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(PkgConfig)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vibecity_core
    src/core/Building.cpp
    src/core/Inventory.cpp
    src/core/Map.cpp
    src/core/Simulation.cpp
)

target_include_directories(vibecity_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vibecity_core
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_library(vibecity_game
    src/game/GameSession.cpp
    src/game/Objectives.cpp
    src/game/Scenario.cpp
)

target_link_libraries(vibecity_game
    PUBLIC
        vibecity_core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vibecity_game
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_executable(vibecity_headless
    src/cli/main.cpp
)

target_link_libraries(vibecity_headless
    PRIVATE
        vibecity_game
)

enable_testing()

add_executable(vibecity_core_tests
    tests/simulation_tests.cpp
)

target_link_libraries(vibecity_core_tests
    PRIVATE
        vibecity_core
)

add_test(NAME vibecity_core_tests COMMAND vibecity_core_tests)

add_executable(vibecity_game_tests
    tests/game_tests.cpp
)

target_link_libraries(vibecity_game_tests
    PRIVATE
        vibecity_game
)

add_test(NAME vibecity_game_tests COMMAND vibecity_game_tests)

if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
endif()

if(SDL2_FOUND)
    add_executable(vibecity_client
        src/client/Text.cpp
        src/client/main.cpp
    )

    target_link_libraries(vibecity_client
        PRIVATE
            vibecity_game
            PkgConfig::SDL2
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(vibecity_client
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif()

    add_test(NAME vibecity_client_smoke COMMAND vibecity_client --smoke-test)
    set_tests_properties(vibecity_client_smoke
        PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )
else()
    message(STATUS "SDL2 not found; skipping vibecity_client target")
endif()

cmake_minimum_required(VERSION 3.25)

project(VibeCity
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(PkgConfig)

file(GLOB BUILDING_DEFINITION_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/data/buildings/*.vbd"
)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data/buildings")
foreach(BUILDING_DEFINITION_FILE IN LISTS BUILDING_DEFINITION_FILES)
    get_filename_component(BUILDING_DEFINITION_NAME "${BUILDING_DEFINITION_FILE}" NAME)
    configure_file(
        "${BUILDING_DEFINITION_FILE}"
        "${CMAKE_CURRENT_BINARY_DIR}/data/buildings/${BUILDING_DEFINITION_NAME}"
        COPYONLY
    )
endforeach()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vibecity_core
    src/core/Building.cpp
    src/core/BuildingCatalog.cpp
    src/core/Inventory.cpp
    src/core/Map.cpp
    src/core/Simulation.cpp
    src/core/SimulationState.cpp
)

target_compile_definitions(vibecity_core
    PRIVATE
        VIBECITY_BUILDING_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data/buildings"
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
    src/game/SaveGame.cpp
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

add_executable(vibecity_bench
    src/bench/main.cpp
)

target_link_libraries(vibecity_bench
    PRIVATE
        vibecity_game
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vibecity_bench
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

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
        src/client/BuildMenu.cpp
        src/client/ClientMode.cpp
        src/client/Hud.cpp
        src/client/Inspector.cpp
        src/client/InputController.cpp
        src/client/MapView.cpp
        src/client/Palette.cpp
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

    add_executable(vibecity_client_tests
        src/client/BuildMenu.cpp
        src/client/InputController.cpp
        src/client/MapView.cpp
        src/client/Palette.cpp
        src/client/Text.cpp
        tests/client_tests.cpp
    )

    target_link_libraries(vibecity_client_tests
        PRIVATE
            vibecity_game
            PkgConfig::SDL2
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(vibecity_client_tests
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif()

    add_test(NAME vibecity_client_tests COMMAND vibecity_client_tests)
else()
    message(STATUS "SDL2 not found; skipping vibecity_client target")
endif()

cmake_minimum_required(VERSION 3.20)

project(harmonia
    VERSION 0.1.0
    DESCRIPTION "HARMONIA Autonomous Digital Musical Keyboard"
    LANGUAGES CXX
)

option(HARMONIA_BUILD_HOST "Build host C++23 libraries" ON)
option(HARMONIA_BUILD_TESTS "Build unit tests (requires HARMONIA_BUILD_HOST=ON)" ON)
option(HARMONIA_ENABLE_SANITIZERS "Enable ASan and UBSan on host targets" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

if(HARMONIA_BUILD_HOST)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    add_subdirectory(software/core)
    if(UNIX)
        add_subdirectory(software/tools)
    endif()

    if(HARMONIA_BUILD_TESTS)
        enable_testing()
        add_subdirectory(software/tests)
    endif()
endif()

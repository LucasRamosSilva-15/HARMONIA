include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_executable(harmonia_core_tests
    test_midi_event.cpp
    test_key_acquisition.cpp
)

target_link_libraries(harmonia_core_tests
    PRIVATE
        harmonia::core
        Catch2::Catch2WithMain
)

harmonia_set_warnings(harmonia_core_tests)

if(HARMONIA_ENABLE_SANITIZERS)
    harmonia_enable_sanitizers(harmonia_core_tests)
endif()

catch_discover_tests(harmonia_core_tests)

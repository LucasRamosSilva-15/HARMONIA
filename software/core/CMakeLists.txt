add_library(harmonia_core STATIC
    src/midi_event.cpp
    src/analog_control.cpp
)

add_library(harmonia::core ALIAS harmonia_core)

target_include_directories(harmonia_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(harmonia_core PUBLIC cxx_std_17)

harmonia_set_warnings(harmonia_core)

if(HARMONIA_ENABLE_SANITIZERS)
    harmonia_enable_sanitizers(harmonia_core)
endif()

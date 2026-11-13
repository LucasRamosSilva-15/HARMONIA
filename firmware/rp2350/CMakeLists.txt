cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350 CACHE STRING "Platform type")

include(pico_sdk_import.cmake)

project(harmonia_diag C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

# Reutiliza harmonia_core (C++17) diretamente no firmware RP2350
function(harmonia_set_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endfunction()

add_subdirectory(../../software/core harmonia_core_build)

add_executable(harmonia_diag
    src/main.cpp
    src/gpio_matrix_driver.cpp
    src/adc_driver.cpp
)

target_include_directories(harmonia_diag
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(harmonia_diag
    pico_stdlib
    hardware_adc
    harmonia::core
)

# Habilita saída serial via UART0 (GP0/GP1) para o monitor serial do Wokwi
pico_enable_stdio_uart(harmonia_diag 1)
pico_enable_stdio_usb(harmonia_diag 0)

pico_add_extra_outputs(harmonia_diag)

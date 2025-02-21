cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(p C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.2.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.2.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif ()

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif ()

# Create an executable for the app example using the gathered and filtered sources
add_executable(rp2040 ../../nanomodbus.c rtu-client.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Link against the pico-sdk libraries as needed
target_link_libraries(rp2040 pico_stdlib)
# Link hardware libraries. More than one library can be added here separated by spaces.
target_link_libraries(rp2040 hardware_rtc
        hardware_flash
        hardware_sync
        hardware_adc
)

target_include_directories(rp2040 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

pico_enable_stdio_usb(rp2040 1)
pico_enable_stdio_uart(rp2040 0)

pico_add_extra_outputs(rp2040)
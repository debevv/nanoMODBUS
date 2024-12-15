cmake_minimum_required(VERSION 3.16)
set(PROJ_NAME stm32-blackpill)

include(FetchContent)
FetchContent_Declare(stm32_cmake
        GIT_REPOSITORY https://github.com/ObKo/stm32-cmake
        GIT_TAG v2.1.0
        GIT_SHALLOW TRUE
)
FetchContent_Populate(stm32_cmake)
set(CMAKE_TOOLCHAIN_FILE ${stm32_cmake_SOURCE_DIR}/cmake/stm32_gcc.cmake)

project(${PROJ_NAME} CXX C ASM)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

stm32_fetch_cmsis(F4)
stm32_fetch_hal(F4)

find_package(CMSIS COMPONENTS STM32F401CC REQUIRED)
find_package(HAL COMPONENTS STM32F4 REQUIRED)

add_compile_options(
        -mcpu=cortex-m4
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(FREERTOS_HEAP "4")
set(FREERTOS_PORT "GCC_ARM_CM4F")

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
FetchContent_Declare(freertos_kernel
        GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG V11.1.0
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(freertos_kernel)

set(WIZ_CHIP W5500)

FetchContent_Declare(
        wizchip
        GIT_REPOSITORY https://github.com/donghoonpark/wizchip-cmake
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(wizchip)

FetchContent_Declare(
        nanomodbus
        GIT_REPOSITORY https://github.com/debevv/nanoMODBUS
        GIT_TAG master
        GIT_SHALLOW TRUE
)

FetchContent_GetProperties(nanomodbus)
if (NOT nanomodbus_POPULATED)
    FetchContent_Populate(nanomodbus)
endif ()

add_library(nanomodbus ${nanomodbus_SOURCE_DIR}/nanomodbus.c)
target_include_directories(nanomodbus PUBLIC ${nanomodbus_SOURCE_DIR})

# FetchContent_MakeAvailable(nanomodbus)

set(TARGET_NAMES modbus_rtu modbus_tcp)

foreach (TARGET_NAME ${TARGET_NAMES})

    add_executable(${TARGET_NAME}
            ${TARGET_NAME}.c
            bsp/blackpill/blackpill.c
            nmbs/port.c
    )

    target_include_directories(
            ${TARGET_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/bsp
            ${CMAKE_CURRENT_SOURCE_DIR}/nmbs
    )

    if (${TARGET_NAME} STREQUAL "modbus_rtu")
        target_compile_definitions(
                ${TARGET_NAME} PRIVATE
                NMBS_RTU
        )
    elseif (${TARGET_NAME} STREQUAL "modbus_tcp")
        target_compile_definitions(
                ${TARGET_NAME} PRIVATE
                NMBS_TCP
        )
    endif ()

    target_link_libraries(
            ${TARGET_NAME}
            STM32::NoSys
            CMSIS::STM32::F401CC
            HAL::STM32::F4::CORTEX
            HAL::STM32::F4::RCC
            HAL::STM32::F4::PWR
            HAL::STM32::F4::GPIO
            HAL::STM32::F4::TIM
            HAL::STM32::F4::UART
            HAL::STM32::F4::USART
            HAL::STM32::F4::SPI
            HAL::STM32::F4::DMA
            wizchip
            freertos_kernel
            nanomodbus
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.hex
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
    )

endforeach ()
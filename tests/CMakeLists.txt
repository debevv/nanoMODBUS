cmake_minimum_required(VERSION 3.21)
project(modbusino_tests C)

set(CMAKE_C_STANDARD 99)

include_directories(. ..)

add_definitions(-DMODBUSINO_USE_PRINTF)

add_executable(modbusino_tests ../modbusino.c modbusino_tests.c)

target_link_libraries(modbusino_tests pthread)

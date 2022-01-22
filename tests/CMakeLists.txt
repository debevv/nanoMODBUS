cmake_minimum_required(VERSION 3.21)
project(modbusino_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

include_directories(. ..)

#add_definitions(-DMBSN_DEBUG)

add_executable(modbusino_tests ../modbusino.c modbusino_tests.c)

target_link_libraries(modbusino_tests pthread)

cmake_minimum_required(VERSION 3.16)
project(modbusino C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

include_directories(tests examples .)

#add_definitions(-DMBSN_DEBUG)

add_executable(modbusino_tests modbusino.c tests/modbusino_tests.c)
add_executable(client-tcp modbusino.c examples/client-tcp.c)
add_executable(server-tcp modbusino.c examples/server-tcp.c)

target_link_libraries(modbusino_tests pthread)

cmake_minimum_required(VERSION 3.16)
project(nanomodbus C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wswitch-enum -Wcast-qual -Woverflow")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

include_directories(tests examples/linux .)

add_executable(nanomodbus_tests nanomodbus.c tests/nanomodbus_tests.c)
target_link_libraries(nanomodbus_tests pthread)

add_executable(server_disabled nanomodbus.c tests/server_disabled.c)
target_compile_definitions(server_disabled PUBLIC NMBS_SERVER_DISABLED)

add_executable(client_disabled nanomodbus.c tests/client_disabled.c)
target_compile_definitions(client_disabled PUBLIC NMBS_CLIENT_DISABLED)

add_executable(multi_server_rtu nanomodbus.c tests/multi_server_rtu.c)
target_compile_definitions(multi_server_rtu PUBLIC NMBS_DEBUG)

add_custom_target(tests DEPENDS nanomodbus_tests server_disabled client_disabled multi_server_rtu)

add_executable(client-tcp nanomodbus.c examples/linux/client-tcp.c)
add_executable(server-tcp nanomodbus.c examples/linux/server-tcp.c)

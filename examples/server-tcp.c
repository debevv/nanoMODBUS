#include "modbusino.h"
#include "platform.h"
#include "server-handlers.h"
#include <stdio.h>

/*
 * This example application sets up a TCP server at the specified address and port, and polls from modbus requests
 * from more than one modbus client (more specifically from maximum 1024 clients, since it uses select())
 *
 * This example server supports the following function codes:
 * FC 01 (0x01) Read Coils
 * FC 03 (0x03) Read Holding Registers
 * FC 15 (0x0F) Write Multiple Coils
 * FC 16 (0x10) Write Multiple registers
 *
* Since the platform for this example is linux, the platform arg is used to pass (to the linux TCP read/write
* functions) a pointer to the file descriptor of the current read client connection
 *
 * The platform functions are for Linux systems.
 */

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: server-tcp [address] [port]\n");
        return 1;
    }

    // Set up the TCP server
    int ret = create_tcp_server(argv[1], argv[2]);
    if (ret != 0) {
        fprintf(stderr, "Error creating TCP server\n");
        return 1;
    }

    mbsn_platform_conf platform_conf = {0};
    platform_conf.transport = MBSN_TRANSPORT_TCP;
    platform_conf.read_byte = read_byte_fd_linux;
    platform_conf.write_byte = write_byte_fd_linux;
    platform_conf.sleep = sleep_linux;
    platform_conf.arg = NULL;    // We will set the arg (socket fd) later

    // These functions are defined in server.h
    mbsn_callbacks callbacks = {0};
    callbacks.read_coils = handle_read_coils;
    callbacks.write_multiple_coils = handle_write_multiple_coils;
    callbacks.read_holding_registers = handler_read_holding_registers;
    callbacks.write_multiple_registers = handle_write_multiple_registers;

    // Create the modbus server. It's ok to set address_rtu to 0 since we are on TCP
    mbsn_t mbsn;
    mbsn_error err = mbsn_server_create(&mbsn, 0, &platform_conf, &callbacks);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus server\n");
        return 1;
    }

    // Set only the polling timeout. Byte timeout will be handled by the TCP connection
    mbsn_set_read_timeout(&mbsn, 1000);

    printf("Modbus TCP server started\n");

    // Our server supports requests from more than one client
    while (true) {
        // Our server_poll() function will return the next client TCP connection to read from
        void* conn = server_poll();
        if (!conn)
            break;

        // Set the next connection handler used by the read/write platform functions
        mbsn_set_platform_arg(&mbsn, conn);

        err = mbsn_server_poll(&mbsn);
        if (err != MBSN_ERROR_NONE) {
            fprintf(stderr, "Error polling modbus connection - %s\n", mbsn_strerror(err));
            // In a more complete example, we should handle this error by closing the connection from our side
            break;
        }
    }

    // Close the TCP server
    close_server();

    // No need to destroy the mbsn instance, bye bye
    return 0;
}

/*
 * This example application sets up a TCP server at the specified address and port, and polls from modbus requests
 * from more than one modbus client (more specifically from maximum 1024 clients, since it uses select())
 *
 * This server supports the following function codes:
 * FC 01 (0x01) Read Coils
 * FC 03 (0x03) Read Holding Registers
 * FC 15 (0x0F) Write Multiple Coils
 * FC 16 (0x10) Write Multiple registers
 *
 * Since the platform for this example is linux, the platform arg is used to pass (to the linux file descriptor
 * read/write functions) a pointer to the file descriptor of the current read client connection
 *
 */

#include <stdio.h>

#include "nanomodbus.h"
#include "platform.h"


// The data model of this sever will support coils addresses 0 to 100 and registers addresses from 0 to 32
#define COILS_ADDR_MAX 100
#define REGS_ADDR_MAX 32

// A single nmbs_bitfield variable can keep 2000 coils
nmbs_bitfield server_coils = {0};
uint16_t server_registers[REGS_ADDR_MAX] = {0};

nmbs_error handle_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, void *arg) {
    UNUSED_PARAM(arg);

    if (address + quantity > COILS_ADDR_MAX + 1)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Read our coils values into coils_out
    for (int i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(server_coils, address + i);
        nmbs_bitfield_write(coils_out, i, value);
    }

    return NMBS_ERROR_NONE;
}


nmbs_error handle_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, void *arg) {
    UNUSED_PARAM(arg);

    if (address + quantity > COILS_ADDR_MAX + 1)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Write coils values to our server_coils
    for (int i = 0; i < quantity; i++) {
        nmbs_bitfield_write(server_coils, address + i, nmbs_bitfield_read(coils, i));
    }

    return NMBS_ERROR_NONE;
}


nmbs_error handler_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, void *arg) {
    UNUSED_PARAM(arg);

    if (address + quantity > REGS_ADDR_MAX + 1)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Read our registers values into registers_out
    for (int i = 0; i < quantity; i++)
        registers_out[i] = server_registers[address + i];

    return NMBS_ERROR_NONE;
}


nmbs_error handle_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, void *arg) {
    UNUSED_PARAM(arg);

    if (address + quantity > REGS_ADDR_MAX + 1)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Write registers values to our server_registers
    for (int i = 0; i < quantity; i++)
        server_registers[address + i] = registers[i];

    return NMBS_ERROR_NONE;
}


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

    nmbs_platform_conf platform_conf = {0};
    platform_conf.transport = NMBS_TRANSPORT_TCP;
    platform_conf.read = read_fd_linux;
    platform_conf.write = write_fd_linux;
    platform_conf.arg = NULL;    // We will set the arg (socket fd) later

    // These functions are defined in server.h
    nmbs_callbacks callbacks = {0};
    callbacks.read_coils = handle_read_coils;
    callbacks.write_multiple_coils = handle_write_multiple_coils;
    callbacks.read_holding_registers = handler_read_holding_registers;
    callbacks.write_multiple_registers = handle_write_multiple_registers;

    // Create the modbus server. It's ok to set address_rtu to 0 since we are on TCP
    nmbs_t nmbs;
    nmbs_error err = nmbs_server_create(&nmbs, 0, &platform_conf, &callbacks);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus server\n");
        return 1;
    }

    // Set only the polling timeout. Byte timeout will be handled by the TCP connection
    nmbs_set_read_timeout(&nmbs, 1000);

    printf("Modbus TCP server started\n");

    // Our server supports requests from more than one client
    while (true) {
        // Our server_poll() function will return the next client TCP connection to read from
        void* conn = server_poll();
        if (!conn)
            break;

        // Set the next connection handler used by the read/write platform functions
        nmbs_set_platform_arg(&nmbs, conn);

        err = nmbs_server_poll(&nmbs);
        if (err != NMBS_ERROR_NONE) {
            printf("Error on modbus connection - %s\n", nmbs_strerror(err));
            // In a more complete example, we would handle this error by checking its nmbs_error value
        }
    }

    // Close the TCP server
    close_server();

    // No need to destroy the nmbs instance, bye bye
    return 0;
}

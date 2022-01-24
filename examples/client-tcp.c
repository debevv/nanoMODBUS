#include "modbusino.h"
#include "platform.h"
#include <stdio.h>

/*
 * This example application connects via TCP to a modbus server at the specified address and port, and sends some
 * modbus requests to it.
 *
 * Since the platform for this example is linux, the platform arg is used to pass (to the linux TCP read/write
 * functions) a pointer to the file descriptor of our TCP connection
 *
 * The platform functions are for Linux systems.
 */

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: client-tcp [address] [port]\n");
        return 1;
    }

    // Set up the TCP connection
    void* conn = connect_tcp(argv[1], argv[2]);
    if (!conn) {
        fprintf(stderr, "Error connecting to server\n");
        return 1;
    }

    mbsn_platform_conf platform_conf;
    platform_conf.transport = MBSN_TRANSPORT_TCP;
    platform_conf.read_byte = read_byte_fd_linux;
    platform_conf.write_byte = write_byte_fd_linux;
    platform_conf.sleep = sleep_linux;
    platform_conf.arg = conn;    // Passing our TCP connection handle to the read/write functions

    // Create the modbus client
    mbsn_t mbsn;
    mbsn_error err = mbsn_client_create(&mbsn, &platform_conf);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus client\n");
        if (!mbsn_error_is_exception(err))
            return 1;
    }

    // Set only the response timeout. Byte timeout will be handled by the TCP connection
    mbsn_set_read_timeout(&mbsn, 1000);

    // Write 2 coils from address 64
    mbsn_bitfield coils;
    mbsn_bitfield_write(coils, 0, 1);
    mbsn_bitfield_write(coils, 1, 1);
    err = mbsn_write_multiple_coils(&mbsn, 64, 2, coils);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error writing coils at address 64 - %s\n", mbsn_strerror(err));
        if (!mbsn_error_is_exception(err))
            return 1;
    }

    // Read 3 coils from address 64
    mbsn_bitfield_reset(coils);    // Reset whole bitfield to zero
    err = mbsn_read_coils(&mbsn, 64, 3, coils);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error reading coils at address 64 - %s\n", mbsn_strerror(err));
        if (!mbsn_error_is_exception(err))
            return 1;
    }
    else {
        printf("Coil at address 64 value: %d\n", mbsn_bitfield_read(coils, 0));
        printf("Coil at address 65 value: %d\n", mbsn_bitfield_read(coils, 1));
        printf("Coil at address 66 value: %d\n", mbsn_bitfield_read(coils, 2));
    }

    // Write 2 holding registers at address 26
    uint16_t w_regs[2] = {123, 124};
    err = mbsn_write_multiple_registers(&mbsn, 26, 2, w_regs);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error writing register at address 26 - %s", mbsn_strerror(err));
        if (!mbsn_error_is_exception(err))
            return 1;
    }

    // Read 2 holding registers from address 26
    uint16_t r_regs[2];
    err = mbsn_read_holding_registers(&mbsn, 26, 2, r_regs);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error reading 2 holding registers at address 26 - %s\n", mbsn_strerror(err));
        if (!mbsn_error_is_exception(err))
            return 1;
    }
    else {
        printf("Register at address 26: %d\n", r_regs[0]);
        printf("Register at address 27: %d\n", r_regs[1]);
    }

    // Close the TCP connection
    disconnect(conn);

    // No need to destroy the mbsn instance, bye bye
    return 0;
}

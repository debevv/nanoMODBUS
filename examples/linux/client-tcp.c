/*
 * This example application connects via TCP to a modbus server at the specified address and port, and sends some
 * modbus requests to it.
 *
 * Since the platform for this example is linux, the platform arg is used to pass (to the linux file descriptor
 * read/write functions) a pointer to the file descriptor of our TCP connection
 *
 */

#include <stdio.h>

#include "nanomodbus.h"
#include "platform.h"

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

    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_TCP;
    platform_conf.read = read_fd_linux;
    platform_conf.write = write_fd_linux;
    platform_conf.arg = conn;    // Passing our TCP connection handle to the read/write functions

    // Create the modbus client
    nmbs_t nmbs;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus client\n");
        if (!nmbs_error_is_exception(err))
            return 1;
    }

    // Set only the response timeout. Byte timeout will be handled by the TCP connection
    nmbs_set_read_timeout(&nmbs, 1000);

    // Write 2 coils from address 64
    nmbs_bitfield coils = {0};
    nmbs_bitfield_write(coils, 0, 1);
    nmbs_bitfield_write(coils, 1, 1);
    err = nmbs_write_multiple_coils(&nmbs, 64, 2, coils);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing coils at address 64 - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }

    // Read 3 coils from address 64
    nmbs_bitfield_reset(coils);    // Reset whole bitfield to zero
    err = nmbs_read_coils(&nmbs, 64, 3, coils);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading coils at address 64 - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        printf("Coil at address 64 value: %d\n", nmbs_bitfield_read(coils, 0));
        printf("Coil at address 65 value: %d\n", nmbs_bitfield_read(coils, 1));
        printf("Coil at address 66 value: %d\n", nmbs_bitfield_read(coils, 2));
    }

    // Write 2 holding registers at address 26
    uint16_t w_regs[2] = {123, 124};
    err = nmbs_write_multiple_registers(&nmbs, 26, 2, w_regs);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing register at address 26 - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }

    // Read 2 holding registers from address 26
    uint16_t r_regs[2];
    err = nmbs_read_holding_registers(&nmbs, 26, 2, r_regs);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading 2 holding registers at address 26 - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        printf("Register at address 26: %d\n", r_regs[0]);
        printf("Register at address 27: %d\n", r_regs[1]);
    }

    // Write file
    uint16_t file[4] = {0x0000, 0x00AA, 0x5500, 0xFFFF};
    err = nmbs_write_file_record(&nmbs, 1, 0, file, 4);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing file - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        printf("Write file registers: 0x%04X 0x%04X 0x%04X 0x%04X\n", file[0], file[1], file[2], file[3]);
    }

    // Read file
    memset(file, 0, sizeof(file));
    err = nmbs_read_file_record(&nmbs, 1, 0, file, 4);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing file - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        printf("Read file registers: 0x%04X 0x%04X 0x%04X 0x%04X\n", file[0], file[1], file[2], file[3]);
    }

    // Read basic device identification
    char vendor_name[128], product_code[128], major_minor_revision[128];
    err = nmbs_read_device_identification_basic(&nmbs, vendor_name, product_code, major_minor_revision, 128);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading basic device identification - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        printf("Read basic device identification: %s %s %s\n", vendor_name, product_code, major_minor_revision);
    }

    // Read basic extended device identification
    char mem[8 * 128];
    char* buffers[8];
    for (int i = 0; i < 8; i++)
        buffers[i] = &mem[i * 128];
    uint8_t ids[8];
    uint8_t objects_count = 0;
    err = nmbs_read_device_identification_extended(&nmbs, 0x80, ids, buffers, 8, 128, &objects_count);
    if (err != NMBS_ERROR_NONE) {
        // If err == NMBS_INVALID_ARGUMENT, the length of the ids and buffers arrays (8 here)  is not enough for all
        // the read objects from the server
        fprintf(stderr, "Error reading extended device identification - %s\n", nmbs_strerror(err));
        if (!nmbs_error_is_exception(err))
            return 1;
    }
    else {
        for (int i = 0; i < objects_count; i++)
            printf("Read extended device identification: ID 0x%02x value %s\n", ids[i], buffers[i]);
    }

    // Close the TCP connection
    disconnect(conn);

    // No need to destroy the nmbs instance, bye bye
    return 0;
}

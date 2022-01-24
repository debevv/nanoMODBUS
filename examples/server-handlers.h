#include "modbusino.h"

// Server (slave) handler functions

// This server data model will support coils addresses 0 to 100 and registers addresses from 0 to 26
#define COILS_ADDR_MAX 100
#define REGS_ADDR_MAX 32

// A single mbsn_bitfield variable can keep 2000 coils
mbsn_bitfield server_coils = {0};

uint16_t server_registers[REGS_ADDR_MAX] = {0};


mbsn_error handle_read_coils(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out) {
    if (address + quantity > COILS_ADDR_MAX + 1)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Read our coils values into coils_out
    for (int i = 0; i < quantity; i++) {
        bool value = mbsn_bitfield_read(server_coils, address + i);
        mbsn_bitfield_write(coils_out, i, value);
    }

    return MBSN_ERROR_NONE;
}


mbsn_error handle_write_multiple_coils(uint16_t address, uint16_t quantity, const mbsn_bitfield coils) {
    if (address + quantity > COILS_ADDR_MAX + 1)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Write coils values to our server_coils
    for (int i = 0; i < quantity; i++) {
        mbsn_bitfield_write(server_coils, address + i, mbsn_bitfield_read(coils, i));
    }

    return MBSN_ERROR_NONE;
}


mbsn_error handler_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    if (address + quantity > REGS_ADDR_MAX + 1)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Read our registers values into registers_out
    for (int i = 0; i < quantity; i++)
        registers_out[i] = server_registers[address + i];

    return MBSN_ERROR_NONE;
}


mbsn_error handle_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers) {
    if (address + quantity > REGS_ADDR_MAX + 1)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    // Write registers values to our server_registers
    for (int i = 0; i < quantity; i++)
        server_registers[address + i] = registers[i];

    return MBSN_ERROR_NONE;
}

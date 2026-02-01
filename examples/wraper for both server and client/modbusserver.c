/*
 * modbusserver.c
 *
 *  Created on: Nov 27, 2024
 *      Author: Daniel Mårtensson
 */

#include "modbusserver.h"
#include "modbus.h"

#ifdef IS_MODBUS_SERVER

/* Sizes */
#define COILS_ADDR_MAX 1
#define INPUTS_ADDR_MAX 0
#define HOLDING_REGISTERS_ADDR_MAX 48
#define INPUT_REGISTERS_ADDR_MAX 11
#define FILE_SIZE_MAX 0

/* Memories */
static nmbs_bitfield server_coils = {0};
static nmbs_bitfield server_inputs = {0};
static uint16_t server_holding_registers[HOLDING_REGISTERS_ADDR_MAX + 1] = {0};
static uint16_t server_input_registers[INPUT_REGISTERS_ADDR_MAX + 1] = {0};
static uint16_t server_file[FILE_SIZE_MAX];

/* Server handle */
static nmbs_t* nmbs_server = NULL;

/* (0x0E) Read Device Identification */
nmbs_error handle_read_device_identification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]) {
    switch (object_id) {
        case 0x00:
            strcpy(buffer, "VendorName");
            break;
        case 0x01:
            strcpy(buffer, "ProductCode");
            break;
        case 0x02:
            strcpy(buffer, "MajorMinorRevision");
            break;
        case 0x90:
            strcpy(buffer, "Extended 1");
            break;
        case 0xA0:
            strcpy(buffer, "Extended 2");
            break;
        default:
            return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    return NMBS_ERROR_NONE;
}

/* (0x2B) Read device identification map */
nmbs_error handle_read_device_identification_map(nmbs_bitfield_256 map) {
    nmbs_bitfield_set(map, 0x00);
    nmbs_bitfield_set(map, 0x01);
    nmbs_bitfield_set(map, 0x02);
    nmbs_bitfield_set(map, 0x90);
    nmbs_bitfield_set(map, 0xA0);
    return NMBS_ERROR_NONE;
}

/* (0x15) Write File Record */
nmbs_error handle_write_file_record(uint16_t file_number, uint16_t record_number, const uint16_t* registers, uint16_t count, uint8_t unit_id, void* arg) {
    if (file_number != 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    if ((record_number + count) > FILE_SIZE_MAX) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    memcpy(server_file + record_number, registers, count * sizeof(uint16_t));

    return NMBS_ERROR_NONE;
}

/* (0x14) Read File Record */
nmbs_error handle_read_file_record(uint16_t file_number, uint16_t record_number, uint16_t* registers, uint16_t count, uint8_t unit_id, void* arg) {
    if (file_number != 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    if ((record_number + count) > FILE_SIZE_MAX) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    memcpy(registers, server_file + record_number, count * sizeof(uint16_t));

    return NMBS_ERROR_NONE;
}

/* (0x10) Write Multiple registers */
nmbs_error handle_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg) {
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    for (uint16_t i = 0; i < quantity; i++) {
        server_holding_registers[address + i] = registers[i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x0F) Write Multiple Coils */
nmbs_error handle_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg) {
    if (address + quantity > COILS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        nmbs_bitfield_write(server_coils, address + i, nmbs_bitfield_read(coils, i));
    }

    return NMBS_ERROR_NONE;
}

/* (0x06) Write Single Register */
nmbs_error handle_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
	if (address > HOLDING_REGISTERS_ADDR_MAX + 1) {
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
	}

	server_holding_registers[address] = value;

	return NMBS_ERROR_NONE;
}

/* (0x05) Write Single Coil */
nmbs_error handle_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg) {
    if (address > COILS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

	nmbs_bitfield_write(server_coils, address, nmbs_bitfield_read(&value, 0));

	return NMBS_ERROR_NONE;
}

/* (0x04) Read Input Registers */
nmbs_error handle_read_input_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg){
    if (address + quantity > INPUT_REGISTERS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++){
        registers_out[i] = server_input_registers[address + i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x03) Read Holding Registers */
nmbs_error handle_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg) {
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++){
        registers_out[i] = server_holding_registers[address + i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x02) Read Discrete Inputs */
nmbs_error handle_read_discrete_inputs(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id, void* arg){
    if (address + quantity > INPUTS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(server_inputs, address + i);
        nmbs_bitfield_write(inputs_out, i, value);
    }

    return NMBS_ERROR_NONE;
}

/* (0x01) Read Coils */
nmbs_error handle_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg) {
    if (address + quantity > COILS_ADDR_MAX + 1) {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(server_coils, address + i);
        nmbs_bitfield_write(coils_out, i, value);
    }

    return NMBS_ERROR_NONE;
}

bool modbus_polling(){
	if(nmbs_server){
		nmbs_error err = nmbs_server_poll(nmbs_server);
		if (err != NMBS_ERROR_NONE) {
			return false;
		}
		return true;
	}
	return false;
}

void modbus_set_server_handle(nmbs_t* handle){
	nmbs_server = handle;
}

bool modbus_set_digital_outputs_on_server(const uint8_t outputs[], const uint16_t address, const uint16_t quantity){
    if (address + quantity > COILS_ADDR_MAX + 1) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(outputs, i);
        nmbs_bitfield_write(server_coils, address + i, value);
    }

    return true;
}

bool modbus_set_digital_inputs_on_server(const uint8_t inputs[], const uint16_t address, const uint16_t quantity){
    if (address + quantity > INPUTS_ADDR_MAX + 1) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(inputs, i);
        nmbs_bitfield_write(server_inputs, address + i, value);
    }

    return true;
}

bool modbus_set_analog_inputs_on_server(const uint16_t inputs[], const uint16_t address, const uint16_t quantity){
    if (address + quantity > INPUT_REGISTERS_ADDR_MAX + 1) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++){
    	server_input_registers[address + i] = inputs[i];
    }

    return true;
}

uint16_t* modbus_get_analog_inputs_on_server(){
	return server_input_registers;
}

bool modbus_get_parameters_at_server(uint16_t parameters[], const uint16_t address, const uint16_t quantity){
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++){
    	parameters[i] = server_holding_registers[address + i];
    }

    return true;
}

bool modbus_set_parameters_on_server(const uint16_t parameters[], const uint16_t address, const uint16_t quantity){
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++){
    	server_holding_registers[address + i] = parameters[i];
    }

    return true;
}

uint16_t* modbus_get_parameters_on_server(){
	return server_holding_registers;
}

#endif

/*
 * modbusclient.c
 *
 *  Created on: Nov 27, 2024
 *      Author: Daniel Mårtensson
 */

#include "modbusclient.h"
#include "modbus.h"

#ifdef IS_MODBUS_CLIENT
/* Client handle */
static nmbs_t* nmbs_client;

void modbus_set_client_handle(nmbs_t* handle){
	nmbs_client = handle;
}

bool modbus_get_digital_outputs_from_server(uint8_t outputs[], const uint16_t address, const uint16_t quantity) {
    nmbs_bitfield coils_out = { 0 };
    nmbs_error err = nmbs_read_coils(nmbs_client, address, quantity, coils_out);
    if (err != NMBS_ERROR_NONE) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(coils_out, address + i);
        nmbs_bitfield_write(outputs, i, value);
    }

    return true;
}

bool modbus_get_digital_inputs_from_server(uint8_t inputs[], const uint16_t address, const uint16_t quantity) {
    nmbs_bitfield inputs_out = { 0 };
    nmbs_error err = nmbs_read_discrete_inputs(nmbs_client, address, quantity, inputs_out);
    if (err != NMBS_ERROR_NONE) {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++) {
        bool value = nmbs_bitfield_read(inputs_out, address + i);
        nmbs_bitfield_write(inputs, i, value);
    }

    return true;
}

bool modbus_get_analog_inputs_from_server(uint16_t inputs[], const uint16_t address, const uint16_t quantity) {
    return nmbs_read_input_registers(nmbs_client, address, quantity, inputs) == NMBS_ERROR_NONE ? true : false;
}

bool modbus_set_parameters_to_server(const uint16_t parameters[], const uint16_t address, const uint16_t quantity) {
    return nmbs_write_multiple_registers(nmbs_client, address, quantity, parameters) == NMBS_ERROR_NONE ? true : false;
}

bool modbus_get_parameters_from_server(uint16_t parameters[], const uint16_t address, const uint16_t quantity) {
    return nmbs_read_holding_registers(nmbs_client, address, quantity, parameters) == NMBS_ERROR_NONE ? true : false;
}

#endif

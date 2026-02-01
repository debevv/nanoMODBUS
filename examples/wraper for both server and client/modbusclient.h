/*
 * modbusclient.h
 *
 *  Created on: Nov 27, 2024
 *      Author: Daniel Mårtensson
 */

#ifndef SRC_TOOLS_MODBUS_MODBUSCLIENT_H_
#define SRC_TOOLS_MODBUS_MODBUSCLIENT_H_

#include <stdint.h>
#include <stdbool.h>
#include "modbus.h"
#include "nanomodbus/nanomodbus.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IS_MODBUS_CLIENT
void modbus_set_client_handle(nmbs_t* handle);
bool modbus_get_digital_outputs_from_server(uint8_t outputs[], const uint16_t address, const uint16_t quantity);
bool modbus_get_digital_inputs_from_server(uint8_t inputs[], const uint16_t address, const uint16_t quantity);
bool modbus_get_analog_inputs_from_server(uint16_t inputs[], const uint16_t address, const uint16_t quantity);
bool modbus_set_parameters_to_server(const uint16_t parameters[], const uint16_t address, const uint16_t quantity);
bool modbus_get_parameters_from_server(uint16_t parameters[], const uint16_t address, const uint16_t quantity);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SRC_TOOLS_MODBUS_MODBUSCLIENT_H_ */

/*
 * modbus.c
 *
 *  Created on: Dec 3, 2024
 *      Author: Daniel Mårtensson
 */

#include "modbus.h"
#include "modbusclient.h"
#include "modbusserver.h"

/* Handlers */
#ifdef IS_MODBUS_SERVER
static nmbs_t nmbs_server = {0};
#endif

#ifdef IS_MODBUS_CLIENT
static nmbs_t nmbs_client = {0};
#endif

/* Function pointers */
int32_t (*serial_read_function)(const char port[], uint8_t*, const uint16_t, const int32_t, const bool, const bool) = NULL;
int32_t (*serial_write_function)(const char port[], const uint8_t*, const uint16_t, const int32_t) = NULL;
char port_[20];

/* Read via serial */
int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	if (serial_read_function) {
		/* User defined arguments for reading with CSerialPort over USB */
		const bool eraseDataAfterRead = true;
		const bool readDataLatestAvailable = false;
		return serial_read_function(port_, buf, count, byte_timeout_ms, eraseDataAfterRead, readDataLatestAvailable);
	}
    return 0;
}

/* Write via serial */
int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
	if (serial_write_function) {
		return serial_write_function(port_, buf, count, byte_timeout_ms);
	}
    return 0;
}

void modbus_set_serial_write(int32_t (*serial_write)(const char port[], const uint8_t*, const uint16_t, const int32_t)){
	serial_write_function = serial_write;
}

void modbus_set_serial_read(int32_t (*serial_read)(const char port[], uint8_t*, const uint16_t, const int32_t, const bool, const bool)){
	serial_read_function = serial_read;
}

void modbus_set_serial_port(const char port[]) {
	strcpy(port_, port);
}

#ifdef IS_MODBUS_SERVER

/* Server functions */
bool modbus_server_create_RTU(const uint8_t address) {

	/* Configuration */
	nmbs_platform_conf platform_conf;
	nmbs_platform_conf_create(&platform_conf);
	platform_conf.transport = NMBS_TRANSPORT_RTU;
	platform_conf.read = read_serial;
	platform_conf.write = write_serial;
	platform_conf.arg = NULL;

	/* Callbacks */
	nmbs_callbacks callbacks;
	nmbs_callbacks_create(&callbacks);
	callbacks.read_coils = handle_read_coils;
	callbacks.read_discrete_inputs = handle_read_discrete_inputs;
	callbacks.read_holding_registers = handle_read_holding_registers;
	callbacks.read_input_registers = handle_read_input_registers;
	callbacks.write_single_coil = handle_write_single_coil;
	callbacks.write_single_register = handle_write_single_register;
	callbacks.write_multiple_coils = handle_write_multiple_coils;
	callbacks.write_multiple_registers = handle_write_multiple_registers;
	callbacks.read_file_record = handle_read_file_record;
	callbacks.write_file_record = handle_write_file_record;
	callbacks.read_device_identification_map = handle_read_device_identification_map;
	callbacks.read_device_identification = handle_read_device_identification;

	/* Create the modbus server */
	nmbs_error err = nmbs_server_create(&nmbs_server, address, &platform_conf, &callbacks);
	if (err != NMBS_ERROR_NONE) {
		return false;
	}

	/* Timeouts */
	nmbs_set_read_timeout(&nmbs_server, 1000);
	nmbs_set_byte_timeout(&nmbs_server, 1000);

	/* Set handle */
    modbus_set_server_handle(&nmbs_server);

	/* All went OK */
	return true;
}

bool modbus_server_polling(){
	return modbus_polling();
}

bool modbus_server_set_digital_outputs(const uint8_t outputs[], const uint16_t address, const uint16_t quantity){
	return modbus_set_digital_outputs_on_server(outputs, address, quantity);
}

bool modbus_server_set_digital_inputs(const uint8_t inputs[], const uint16_t address, const uint16_t quantity){
	return modbus_set_digital_inputs_on_server(inputs, address, quantity);
}

bool modbus_server_set_analog_inputs(const uint16_t inputs[], const uint16_t address, const uint16_t quantity){
	return modbus_set_analog_inputs_on_server(inputs, address, quantity);
}

uint16_t* modbus_server_get_analog_inputs(){
	return modbus_get_analog_inputs_on_server();
}

bool modbus_server_get_parameters(uint16_t parameters[], const uint16_t address, const uint16_t quantity){
	return modbus_get_parameters_at_server(parameters, address, quantity);
}

bool modbus_server_set_parameters(const uint16_t parameters[], const uint16_t address, const uint16_t quantity){
	return modbus_set_parameters_on_server(parameters, address, quantity);
}

uint16_t* modbus_server_get_parameters_array(){
	return modbus_get_parameters_on_server();
}

#endif

#ifdef IS_MODBUS_CLIENT

/* Client functions */
bool modbus_client_create_RTU(const uint8_t address) {

    /* Create platform configuration */
    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;
    platform_conf.arg = NULL;

    /* Create client */
    nmbs_error err = nmbs_client_create(&nmbs_client, &platform_conf);
    if (err != NMBS_ERROR_NONE) {
        return false;
    }

    /* Set time out */
    nmbs_set_read_timeout(&nmbs_client, 1000);
    nmbs_set_byte_timeout(&nmbs_client, 1000);

    /* Set address */
    nmbs_set_destination_rtu_address(&nmbs_client, address);

    /* Set handle */
    modbus_set_client_handle(&nmbs_client);

    /* OK */
    return true;
}

void modbus_client_set_RTU_address(const uint8_t address) {
	nmbs_set_destination_rtu_address(&nmbs_client, address);
}

bool modbus_client_get_digital_outputs(uint8_t outputs[], const uint16_t address, const uint16_t quantity){
	return modbus_get_digital_outputs_from_server(outputs, address, quantity);
}

bool modbus_client_get_digital_inputs(uint8_t inputs[], const uint16_t address, const uint16_t quantity){
	return modbus_get_digital_inputs_from_server(inputs, address, quantity);
}

bool modbus_client_get_analog_inputs(uint16_t inputs[], const uint16_t address, const uint16_t quantity){
	return modbus_get_analog_inputs_from_server(inputs, address, quantity);
}

bool modbus_client_set_parameters(const uint16_t parameters[], const uint16_t address, const uint16_t quantity){
	return modbus_set_parameters_to_server(parameters, address, quantity);
}

bool modbus_client_get_parameters(uint16_t parameters[], const uint16_t address, const uint16_t quantity){
	return modbus_get_parameters_from_server(parameters, address, quantity);
}

#endif

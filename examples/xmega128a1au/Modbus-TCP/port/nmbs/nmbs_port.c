/*
 * nmbs_port.c
 *
 * Created: 25.06.2025 14:58:02
 *  Author: Daniel Kampert
 */ 

#include "nmbs_port.h"

#include <string.h>

static int32_t read_socket(uint8_t* buf, uint16_t count, int32_t byte_timeout, void* arg);
static int32_t write_socket(const uint8_t* buf, uint16_t count, int32_t byte_timeout, void* arg);

static nmbs_server_t* server;

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg);
static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg);
static nmbs_error server_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg);
static nmbs_error server_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg);

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* _server) {
	nmbs_platform_conf conf;
	nmbs_callbacks cb;

	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_TCP;
	conf.read = read_socket;
	conf.write = write_socket;

	server = _server;

	nmbs_callbacks_create(&cb);
	cb.read_coils = server_read_coils;
	cb.read_holding_registers = server_read_holding_registers;
	cb.write_single_coil = server_write_single_coil;
	cb.write_multiple_coils = server_write_multiple_coils;
	cb.write_single_register = server_write_single_register;
	cb.write_multiple_registers = server_write_multiple_registers;

	nmbs_error status = nmbs_server_create(nmbs, server->id, &conf, &cb);
	if (status != NMBS_ERROR_NONE) {
		return status;
	}

	nmbs_set_byte_timeout(nmbs, 100);
	nmbs_set_read_timeout(nmbs, 1000);

	return NMBS_ERROR_NONE;
}

nmbs_error nmbs_client_init(nmbs_t* nmbs) {
	nmbs_platform_conf conf;

	nmbs_platform_conf_create(&conf);
	conf.transport = NMBS_TRANSPORT_TCP;
	conf.read = read_socket;
	conf.write = write_socket;

	nmbs_error status = nmbs_client_create(nmbs, &conf);
	if (status != NMBS_ERROR_NONE) {
		return status;
	}

	nmbs_set_byte_timeout(nmbs, 100);
	nmbs_set_read_timeout(nmbs, 1000);

	return NMBS_ERROR_NONE;
}

static nmbs_server_t* get_server(uint8_t id) {
	if (id == server->id) {
		return server;
	}
	else {
		return NULL;
	}
}

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg) {
	nmbs_server_t* server = get_server(unit_id);

	for (size_t i = 0; i < quantity; i++) {
		if ((address >> 3) > COIL_BUF_SIZE) {
			return NMBS_ERROR_INVALID_REQUEST;
		}
		nmbs_bitfield_write(coils_out, address, nmbs_bitfield_read(server->coils, address));
		address++;
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg) {
	nmbs_server_t* server = get_server(unit_id);

	for (size_t i = 0; i < quantity; i++) {
		if (address > REG_BUF_SIZE) {
			return NMBS_ERROR_INVALID_REQUEST;
		}
		registers_out[i] = server->regs[address++];
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error server_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg) {
	uint8_t coil = 0;
	if (value) {
		coil |= 0x01;
	}
	return server_write_multiple_coils(address, 1, &coil, unit_id, arg);
}

static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg) {
	nmbs_server_t* server = get_server(unit_id);

	for (size_t i = 0; i < quantity; i++) {
		if ((address >> 3) > COIL_BUF_SIZE) {
			return NMBS_ERROR_INVALID_REQUEST;
		}
		nmbs_bitfield_write(server->coils, address, nmbs_bitfield_read(coils, i));
		address++;
	}
	return NMBS_ERROR_NONE;
}

static nmbs_error server_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
	uint16_t reg = value;
	return server_write_multiple_registers(address, 1, &reg, unit_id, arg);
}

static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg) {
	nmbs_server_t* server = get_server(unit_id);

	for (size_t i = 0; i < quantity; i++) {
		if (address > REG_BUF_SIZE) {
			return NMBS_ERROR_INVALID_REQUEST;
		}
		server->regs[address++] = registers[i];
	}
	return NMBS_ERROR_NONE;
}

int32_t read_socket(uint8_t* buf, uint16_t count, int32_t byte_timeout, void* arg) {
	uint32_t tick_start = 0;
	while (recv(MB_SOCKET, buf, count) != count) {
		if ((tick_start++) >= (uint32_t) byte_timeout) {
			return 0;
		}
	}
	return count;
}

int32_t write_socket(const uint8_t* buf, uint16_t count, int32_t byte_timeout, void* arg) {
	return send(MB_SOCKET, buf, count);
}

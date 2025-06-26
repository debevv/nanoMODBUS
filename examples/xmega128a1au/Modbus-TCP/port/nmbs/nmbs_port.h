/*
 * nmbs_port.h
 *
 * Created: 25.06.2025 14:58:17
 *  Author: Daniel Kampert
 */ 

#ifndef NMBS_PORT_H_
#define NMBS_PORT_H_

#include "wizchip.h"
#include "nanomodbus.h"

#define MB_SOCKET			1

// Max size of coil and register area
#define COIL_BUF_SIZE		1024
#define REG_BUF_SIZE		2048

typedef struct tNmbsServer {
	uint8_t id;
	uint8_t coils[COIL_BUF_SIZE];
	uint16_t regs[REG_BUF_SIZE];
} nmbs_server_t;

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* server);
nmbs_error nmbs_client_init(nmbs_t* nmbs);

#endif /* NMBS_PORT_H_ */
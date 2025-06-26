/*
 * wizchip_port.h
 *	XMega port for the Wiznet library
 *
 * Created: 20.06.2025 11:37:26
 *  Author: Daniel Kampert
 */

#ifndef WIZCHIP_PORT_H_
#define WIZCHIP_PORT_H_

typedef enum {
	__OK       = 0x00U,
	__ERROR    = 0x01U,
	__BUSY     = 0x02U,
	__TIMEOUT  = 0x03U
} Wiznet_Error_t;

typedef struct 
{
	void* SPI_Instance;
	void* SPI_Port_Instance;
	void* CS_Port_Instance;
	uint8_t CS_Pin_Number;
	void* Reset_Port_Instance;
	uint8_t Reset_Pin_Number;
} Wiznet_Config_t;

Wiznet_Error_t wizchip_register_hal(Wiznet_Config_t* Wiznet_SPI_Config);

#endif /* WIZCHIP_PORT_H_ */
#ifndef NANOMODBUS_CONFIG_H
#define NANOMODBUS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// Max size of coil and register area
#define COIL_BUF_SIZE 1024
#define REG_BUF_SIZE 2048

// NanoModbus include
#include "nanomodbus.h"
#include "stm32f4xx_hal.h"

#ifdef NMBS_TCP
// modbus tcp
#define MB_SOCKET 1
#include <socket.h>
#endif

#ifdef NMBS_RTU
// modbus rtu
#define MB_UART huart1
#define MB_UART_DMA 1
#define MB_RX_BUF_SIZE 256
extern UART_HandleTypeDef MB_UART;
#endif

typedef struct tNmbsServer {
    uint8_t id;
    uint8_t coils[COIL_BUF_SIZE];
    uint16_t regs[REG_BUF_SIZE];
} nmbs_server_t;

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* server);
nmbs_error nmbs_client_init(nmbs_t* nmbs);

#ifdef __cplusplus
}
#endif

#endif
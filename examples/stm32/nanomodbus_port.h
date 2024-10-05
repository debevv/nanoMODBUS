#ifndef NANOMODBUS_CONFIG_H
#define NANOMODBUS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define NANOMB_UART huart1
#define RX_BUF_SIZE 256

// Max size of coil and register area
#define COIL_BUF_SIZE 1024 
#define REG_BUF_SIZE  2048

// Other hardware configuration have done in *.ioc file

// Core name may vary with your hardware
#include "stm32f4xx_hal.h"
// NanoModbus include
#include "nanomodbus.h"

typedef struct tNmbsServer{
    uint8_t  id;
    uint8_t  coils[COIL_BUF_SIZE];
    uint16_t regs[REG_BUF_SIZE];
}nmbs_server_t;

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* server);

extern UART_HandleTypeDef NANOMB_UART;

#ifdef __cplusplus
}
#endif

#endif

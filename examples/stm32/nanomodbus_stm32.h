#ifndef NANOMODBUS_CONFIG_H
#define NANOMODBUS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define NANOMB_UART huart1
#define RX_BUF_SIZE 256
// Other hardware configuration have done in *.ioc file

// Core name may vary with your hardware
#include "stm32f4xx_hal.h"
// NanoModbus include
#include "nanomodbus.h"

void nanomodbus_rtu_init(nmbs_t* nmbs);

extern UART_HandleTypeDef NANOMB_UART;

#ifdef __cplusplus
}
#endif

#endif

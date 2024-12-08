#include "blackpill/blackpill.h"
#include "wizchip.h"

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "nanomodbus.h"
#include "nmbs/port.h"
#include "task.h"
#include <socket.h>

extern SPI_HandleTypeDef hspi1;

wiz_NetInfo net_info = {
        .mac = {0xEA, 0x11, 0x22, 0x33, 0x44, 0xEA},
        .ip = {192, 168, 137, 100},    // You can find this ip if you set your ethernet port ip as 192.168.137.XXX
        .sn = {255, 255, 255, 0},
        .gw = {192, 168, 137, 1},
        .dns = {0, 0, 0, 0},
};    // Network information.

uint8_t transaction_buf_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};    // All 2kB buffer setting for each sockets

#if USE_HAL_SPI_REGISTER_CALLBACKS == 0

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    wizchip_dma_rx_cplt((void*) hspi);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    wizchip_dma_tx_cplt((void*) hspi);
}

#endif

uint32_t HAL_GetTick(void) {
    return xTaskGetTickCount();
}

static void blink(void* args);
static void modbus(void* args);

int main(void) {
    BSP_Init();
    wizchip_register_hal(&hspi1, (fHalSpiTransaction) HAL_SPI_Receive, (fHalSpiTransaction) HAL_SPI_Transmit,
                         (fHalSpiTransactionDma) HAL_SPI_Receive_DMA, (fHalSpiTransactionDma) HAL_SPI_Transmit_DMA,
                         GPIOA, GPIO_PIN_15, (fHalGpioWritePin) HAL_GPIO_WritePin);

    wizchip_init(transaction_buf_sizes, transaction_buf_sizes);
    setSHAR(net_info.mac);
    setSIPR(net_info.ip);
    setSUBR(net_info.sn);
    setGAR(net_info.gw);

    xTaskCreate(blink, "blink", 128, NULL, 4, NULL);
    xTaskCreate(modbus, "modbus", 128 * 16, NULL, 2, NULL);

    vTaskStartScheduler();

    while (true) {}
}

static void blink(void* args) {
    for (;;) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        vTaskDelay(500);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        vTaskDelay(500);
    }
}

static nmbs_t nmbs;
static nmbs_server_t nmbs_server = {
        .id = 0x01,
        .coils =
                {
                        0,
                },
        .regs =
                {
                        0,
                },
};
static void modbus(void* args) {
    int status;

    nmbs_server_init(&nmbs, &nmbs_server);

    for (;;) {
        switch ((status = getSn_SR(MB_SOCKET))) {
            case SOCK_ESTABLISHED:
                nmbs_server_poll(&nmbs);
                break;

            case SOCK_INIT:
                listen(MB_SOCKET);
                break;

            case SOCK_CLOSED:
                socket(MB_SOCKET, Sn_MR_TCP, 502, 0);
                break;

            case SOCK_CLOSE_WAIT:
                disconnect(MB_SOCKET);
                break;

            default:
                taskYIELD();
                break;
        }
    }
}

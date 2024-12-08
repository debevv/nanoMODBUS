#include "blackpill/blackpill.h"
#include "nanomodbus.h"
#include "nmbs/port.h"

#include "FreeRTOS.h"
#include "task.h"

#define TEST_SERVER 1
#define TEST_CLIENT 0

static void blink(void* args);
static void modbus(void* args);

uint32_t HAL_GetTick(void) {
    return xTaskGetTickCount();
}

int main(void) {
    BSP_Init();

    xTaskCreate(blink, "blink", 128, NULL, 4, NULL);
    xTaskCreate(modbus, "modbus", 128 * 16, NULL, 2, NULL);

    vTaskStartScheduler();

    for (;;) {}
}

static void blink(void* args) {
    for (;;) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        vTaskDelay(500);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        vTaskDelay(500);
    }
}

nmbs_t nmbs;
nmbs_server_t nmbs_server = {
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
#if TEST_SERVER
    nmbs_server_init(&nmbs, &nmbs_server);
#endif

#if TEST_CLIENT
    uint8_t coils_test[32];
    uint16_t regs_test[32];
    nmbs_client_init(&nmbs);
#endif

    for (;;) {
#if TEST_SERVER
        nmbs_server_poll(&nmbs);
        taskYIELD();
#endif
#if TEST_CLIENT
        nmbs_set_destination_rtu_address(&nmbs, 0x01);
        nmbs_error status = nmbs_read_holding_registers(&nmbs, 0, 32, regs_test);
        status = nmbs_write_multiple_registers(&nmbs, 0, 32, regs_test);
        if (status != NMBS_ERROR_NONE) {
            while (true) {}
        }
#endif
    }
}

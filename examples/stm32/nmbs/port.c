#include "nmbs/port.h"
#include "FreeRTOS.h"
#include <string.h>

#ifdef NMBS_TCP
static int32_t read_socket(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
static int32_t write_socket(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
#endif
#ifdef NMBS_RTU
static int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

#if MB_UART_DMA
#include "queue.h"
xQueueHandle rtu_rx_q;
uint8_t rtu_rx_b[MB_RX_BUF_SIZE];
#endif

#endif

static nmbs_server_t* server;

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id,
                                    void* arg);
static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out,
                                                uint8_t unit_id, void* arg);
static nmbs_error server_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils,
                                              uint8_t unit_id, void* arg);
static nmbs_error server_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers,
                                                  uint8_t unit_id, void* arg);

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* _server) {
    nmbs_platform_conf conf;
    nmbs_callbacks cb;

    nmbs_platform_conf_create(&conf);
#ifdef NMBS_TCP
    conf.transport = NMBS_TRANSPORT_TCP;
    conf.read = read_socket;
    conf.write = write_socket;
#endif
#ifdef NMBS_RTU
    conf.transport = NMBS_TRANSPORT_RTU;
    conf.read = read_serial;
    conf.write = write_serial;
#endif

    server = _server;

    nmbs_callbacks_create(&cb);
    cb.read_coils = server_read_coils;
    cb.read_holding_registers = server_read_holding_registers;
    cb.write_single_coil = server_write_single_coil;
    cb.write_multiple_coils = server_write_multiple_coils;
    cb.write_single_register = server_write_single_register;
    cb.write_multiple_registers = server_write_multiple_registers;

#if MB_UART_DMA
    rtu_rx_q = xQueueCreate(MB_RX_BUF_SIZE, sizeof(uint8_t));
    HAL_UARTEx_ReceiveToIdle_DMA(&MB_UART, rtu_rx_b, MB_RX_BUF_SIZE);
#endif

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
#ifdef NMBS_TCP
    conf.transport = NMBS_TRANSPORT_TCP;
    conf.read = read_socket;
    conf.write = write_socket;
#endif
#ifdef NMBS_RTU
    conf.transport = NMBS_TRANSPORT_RTU;
    conf.read = read_serial;
    conf.write = write_serial;
#endif

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

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id,
                                    void* arg) {
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

static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out,
                                                uint8_t unit_id, void* arg) {
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

static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils,
                                              uint8_t unit_id, void* arg) {
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

static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers,
                                                  uint8_t unit_id, void* arg) {
    nmbs_server_t* server = get_server(unit_id);

    for (size_t i = 0; i < quantity; i++) {
        if (address > REG_BUF_SIZE) {
            return NMBS_ERROR_INVALID_REQUEST;
        }
        server->regs[address++] = registers[i];
    }
    return NMBS_ERROR_NONE;
}

#ifdef NMBS_TCP
int32_t read_socket(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    uint32_t tick_start = HAL_GetTick();
    while (recv(MB_SOCKET, buf, count) != count) {
        if (HAL_GetTick() - tick_start >= (uint32_t) byte_timeout_ms) {
            return 0;
        }
    }
    return count;
}

int32_t write_socket(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    return send(MB_SOCKET, buf, count);
}
#endif

#ifdef NMBS_RTU
static int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
#if MB_UART_DMA
    uint32_t tick_start = HAL_GetTick();
    while (uxQueueMessagesWaiting(rtu_rx_q) < count) {
        if (HAL_GetTick() - tick_start >= (uint32_t) byte_timeout_ms) {
            return 0;
        }
    }
    for (int i = 0; i < count; i++) {
        xQueueReceive(rtu_rx_q, buf + i, 1);
    }
    return count;
#else
    HAL_StatusTypeDef status = HAL_UART_Receive(&MB_UART, buf, count, byte_timeout_ms);
    if (status == HAL_OK) {
        return count;
    }
    else {
        return 0;
    }
#endif
}
static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
#if MB_UART_DMA
    HAL_UART_Transmit_DMA(&MB_UART, buf, count);
#else
    HAL_StatusTypeDef status = HAL_UART_Transmit(&MB_UART, buf, count, byte_timeout_ms);
    if (status == HAL_OK) {
        return count;
    }
    else {
        return 0;
    }
#endif
}


#if MB_UART_DMA
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (huart == &MB_UART) {
        for (int i = 0; i < Size; i++) {
            xQueueSendFromISR(rtu_rx_q, rtu_rx_b + i, &xHigherPriorityTaskWoken);
        }
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rtu_rx_b, MB_RX_BUF_SIZE);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    // You may add your additional uart handler below
}
#endif

#endif
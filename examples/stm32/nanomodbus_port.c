#include <string.h>
#include "nanomodbus_port.h"

static int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

// Dual buffer setting to isolate dma implementation and ring buffer. 
// You may integrate this feature with dma counter register to minimize memory footprint
static uint8_t rx_dma_buf[RX_BUF_SIZE];

// Ring buffer structure definition
typedef struct tRingBuf {
    uint8_t   data[RX_BUF_SIZE];
    uint16_t  head;
    uint16_t  tail;
    bool      full;
    void      (*overflow_callback)(struct tRingBuf* rq);
} ringBuf;

static ringBuf rb;
static void ringbuf_init(ringBuf* rb, void (*overflow_callback)(struct tRingBuf* rq));
static void ringbuf_overflow_error(ringBuf* rb);

static nmbs_server_t* server;

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg);
static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg);
static nmbs_error server_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg);
static nmbs_error server_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg);
static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg);

nmbs_error nmbs_server_init(nmbs_t* nmbs, nmbs_server_t* _server)
{
    ringbuf_init(&rb, ringbuf_overflow_error);

    nmbs_platform_conf conf;
    nmbs_callbacks     cb;

    nmbs_platform_conf_create(&conf);
    conf.transport = NMBS_TRANSPORT_RTU; 
    conf.read = read_serial;   
    conf.write = write_serial;

    server = _server;

    nmbs_callbacks_create(&cb);
    cb.read_coils                = server_read_coils;
    cb.read_holding_registers    = server_read_holding_registers;
    cb.write_single_coil         = server_write_single_coil;
    cb.write_multiple_coils      = server_write_multiple_coils;
    cb.write_single_register     = server_write_single_register;
    cb.write_multiple_registers  = server_write_multiple_registers;


    nmbs_error status = nmbs_server_create(nmbs, server->id, &conf, &cb);
    if(status != NMBS_ERROR_NONE)
    {
        return status;
    }
    
    nmbs_set_byte_timeout(nmbs, 100);
    nmbs_set_read_timeout(nmbs, 1000);

    HAL_UARTEx_ReceiveToIdle_DMA(&NANOMB_UART, rx_dma_buf, RX_BUF_SIZE);
    return NMBS_ERROR_NONE;
}

static nmbs_server_t* get_server(uint8_t id)
{
    if(id == server->id)
    {
        return server;
    }
    else
    {
        return NULL;
    }
        
}

static nmbs_error server_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg)
{
    nmbs_server_t* server = get_server(unit_id);
    
    for(size_t i = 0; i < quantity; i++)
    {
        if((address>>3) > COIL_BUF_SIZE)
        {
            return NMBS_ERROR_INVALID_REQUEST;
        }
        nmbs_bitfield_write(coils_out, address, nmbs_bitfield_read(server->coils, address));
        address++;
    }
    return NMBS_ERROR_NONE;
}

static nmbs_error server_read_holding_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg)
{
    nmbs_server_t* server = get_server(unit_id);
    
    for(size_t i = 0; i < quantity; i++)
    {
        if(address > REG_BUF_SIZE)
        {
            return NMBS_ERROR_INVALID_REQUEST;
        }
        registers_out[i] = server->regs[address++];
    }
    return NMBS_ERROR_NONE;
}

static nmbs_error server_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg)
{
    uint8_t coil = 0;
    if(value)
    {
        coil |= 0x01;
    }
    server_write_multiple_coils(address, 1, &coil, unit_id, arg);
}

static nmbs_error server_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg)
{
    nmbs_server_t* server = get_server(unit_id);
    
    for(size_t i = 0; i < quantity; i++)
    {
        if((address>>3) > COIL_BUF_SIZE)
        {
            return NMBS_ERROR_INVALID_REQUEST;
        }
        nmbs_bitfield_write(server->coils, address, nmbs_bitfield_read(coils, i));
        address++;
    }
    return NMBS_ERROR_NONE;
}

static nmbs_error server_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg)
{
    uint16_t reg = value;
    server_write_multiple_registers(address, 1, &reg, unit_id, arg);
}

static nmbs_error server_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg)
{
    nmbs_server_t* server = get_server(unit_id);
    
    for(size_t i = 0; i < quantity; i++)
    {
        if(address > REG_BUF_SIZE)
        {
            return NMBS_ERROR_INVALID_REQUEST;
        }
        server->regs[address++] = registers[i];
    }
    return NMBS_ERROR_NONE;
}

// Function to initialize the ring buffer
static void ringbuf_init(ringBuf* rb, void (*overflow_callback)(struct tRingBuf* rq)) {
    memset(rb->data, 0, sizeof(rb->data));
    rb->head = 0;
    rb->tail = 0;
    rb->full = false;
    rb->overflow_callback = overflow_callback;
}

// Function to check if the ring buffer is empty
static bool ringbuf_is_empty(ringBuf* rb) {
    return (!rb->full && (rb->head == rb->tail));
}

// Function to write multiple bytes to the ring buffer
static void ringbuf_put(ringBuf* rb, const uint8_t* data, uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        rb->data[rb->head] = data[i];

        if (rb->full) { // If the buffer is full
            if (rb->overflow_callback) {
                rb->overflow_callback(rb); // Call the overflow callback
            }
            rb->tail = (rb->tail + 1) % RX_BUF_SIZE; // Move tail to overwrite data
        }

        rb->head = (rb->head + 1) % RX_BUF_SIZE;

        rb->full = (rb->head == rb->tail);
    }
}

// Function to read multiple bytes from the ring buffer
static bool ringbuf_get(ringBuf* rb, uint8_t* data, uint16_t length) {
    if (ringbuf_is_empty(rb)) {
        return false; // Return false if the buffer is empty
    }

    for (uint16_t i = 0; i < length; i++) {
        if (ringbuf_is_empty(rb)) { 
            return false; // If no more data, stop reading
        }

        data[i] = rb->data[rb->tail];
        rb->tail = (rb->tail + 1) % RX_BUF_SIZE;
        rb->full = false; // Buffer is no longer full after reading
    }

    return true;
}

uint16_t ringbuf_size(ringBuf* rb) {
    if (rb->full) {
        return RX_BUF_SIZE;
    }
    
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return RX_BUF_SIZE + rb->head - rb->tail;
    }
}

// Example callback function to handle buffer overflow
static void ringbuf_overflow_error(ringBuf* rb)
{
    //In here we may check the overflow situation
    while(true){}
}

// RX event callback from dma
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart == &NANOMB_UART)
    {
        ringbuf_put(&rb, rx_dma_buf, Size);
        HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_dma_buf, RX_BUF_SIZE);
    }
    // You may add your additional uart handler below
}

static int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg)
{
    uint32_t tick_start = HAL_GetTick();
    while(ringbuf_size(&rb) < count)
    {
        if(HAL_GetTick() - tick_start >= (uint32_t)byte_timeout_ms)
        {
            uint16_t size_to_read = ringbuf_size(&rb);
            ringbuf_get(&rb, buf, size_to_read);
            return size_to_read;
        }
    }
    // Read from ring buffer
    if(ringbuf_get(&rb, buf, count))
    {
        return count;
    }
    else
    {
        return 0;
    }
}

static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg)
{
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&NANOMB_UART, buf, count);
    return count;
}


#include <string.h>
#include "nanomodbus_stm32.h"

static int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

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
static void ring_buf_overflow_error(ringBuf* rb);

void nanomodbus_rtu_init(nmbs_t* nmbs)
{
    ring_buf_init(&rb, ring_buf_overflow_error);

    nmbs_platform_conf pf_conf;

    nmbs_platform_conf_create(&pf_conf);
    pf_conf.transport = NMBS_TRANSPORT_RTU; 
    pf_conf.read = read_serial;   
    pf_conf.write = write_serial;
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

// Function to check if the ring buffer is full
static bool ringbuf_is_full(ringBuf* rb) {
    return rb->full;
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
static void ring_buf_overflow_error(ringBuf* rb)
{
    //In here we may check the overflow situation
    while(true){}
}

// Dual buffer setting to isolate dma implementation and ring buffer. 
// You may integrate this feature with dma counter register to minimize memory footprint
static uint8_t rx_dma_buf[RX_BUF_SIZE];

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
        if(HAL_GetTick() - tick_start >= byte_timeout_ms)
        {
            return NMBS_ERROR_TIMEOUT;
        }
    }
    // Read from ring buffer
    if(ringbuf_get(&rb, buf, count))
    {
        return NMBS_ERROR_NONE;
    }
    else
    {
        return NMBS_ERROR_TRANSPORT;
    }
}


static int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg)
{
    HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(&NANOMB_UART, buf, count);

    switch (status)
    {
    case HAL_OK:
        return NMBS_ERROR_NONE;
        break;
    case HAL_ERROR:
        return NMBS_ERROR_TRANSPORT;
        break;
    case HAL_BUSY:
        return NMBS_ERROR_TRANSPORT;
        break;
    case HAL_TIMEOUT:
        return NMBS_ERROR_TIMEOUT;
        break;
    }
    return NMBS_ERROR_INVALID_ARGUMENT;
}


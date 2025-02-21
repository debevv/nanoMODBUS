#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "nanomodbus.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Define UART pins and settings
#define UART_ID uart0
#define BAUD_RATE 19200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_ODD

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_DE_PIN 2    // Data Enable pin for RS485

#define PICO_LED_PIN 25    // Onboard LED pin for the Pico

// The server address
#define RTU_SERVER_ADDRESS 1

// Function prototypes
void onError();
int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

void onError() {
    // Make the LED blink on error
    const uint LED_PIN = PICO_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(LED_PIN, 0);
        sleep_ms(1000);
    }
}

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    uint64_t start_time = time_us_64();
    int32_t bytes_read = 0;
    uint64_t timeout_us = (uint64_t) byte_timeout_ms * 1000;

    while (time_us_64() - start_time < timeout_us && bytes_read < count) {
        if (uart_is_readable(UART_ID)) {
            buf[bytes_read++] = uart_getc(UART_ID);
            start_time = time_us_64();    // Reset start time after a successful read
        }
    }

    return bytes_read;
}

int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    uart_write_blocking(UART_ID, buf, count);
    return count;
}

void pico_setup() {
    printf("Initializing UART...\n");
    // Initialize the UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Actually, we want a different speed
    // The call will return the actual baud rate selected, which will be as close as
    // possible to that requested
    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Initialize the DE pin as output for RS485
    gpio_init(UART_DE_PIN);
    gpio_set_dir(UART_DE_PIN, GPIO_OUT);
    gpio_put(UART_DE_PIN, 0);    // Set DE low to enable receiving initially

    // Initialize the onboard LED
    const uint LED_PIN = PICO_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
}

int main() {
    stdio_init_all();
    sleep_ms(5000);    // Initial pause to catch prints

    printf("Starting Pico setup...\n");
    pico_setup();

    printf("Setting up Modbus platform configuration...\n");
    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;

    printf("Creating Modbus client...\n");
    nmbs_t nmbs;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE) {
        printf("Error creating Modbus client: %d\n", err);
        onError();
    }

    printf("Setting Modbus timeouts...\n");
    nmbs_set_read_timeout(&nmbs, 1000);
    nmbs_set_byte_timeout(&nmbs, 100);

    printf("Setting Modbus destination RTU address...\n");
    nmbs_set_destination_rtu_address(&nmbs, RTU_SERVER_ADDRESS);

    printf("Writing 2 coils from address 64...\n");
    nmbs_bitfield coils = {0};
    nmbs_bitfield_write(coils, 0, 1);
    nmbs_bitfield_write(coils, 1, 1);
    err = nmbs_write_multiple_coils(&nmbs, 64, 2, coils);
    if (err != NMBS_ERROR_NONE) {
        printf("Error writing multiple coils: %d\n", err);
        onError();
    }

    printf("Reading 3 coils from address 64...\n");
    nmbs_bitfield_reset(coils);    // Reset whole bitfield to zero
    err = nmbs_read_coils(&nmbs, 64, 3, coils);
    if (err != NMBS_ERROR_NONE) {
        printf("Error reading coils: %d\n", err);
        onError();
    }

    printf("Writing 2 holding registers at address 26...\n");
    uint16_t w_regs[2] = {123, 124};
    err = nmbs_write_multiple_registers(&nmbs, 26, 2, w_regs);
    if (err != NMBS_ERROR_NONE) {
        printf("Error writing multiple registers: %d\n", err);
        onError();
    }

    printf("Reading 2 holding registers from address 26...\n");
    uint16_t r_regs[2];
    err = nmbs_read_holding_registers(&nmbs, 26, 2, r_regs);
    if (err != NMBS_ERROR_NONE) {
        printf("Error reading holding registers: %d\n", err);
        onError();
    }

    // On success, keep the LED on
    const uint LED_PIN = PICO_LED_PIN;
    gpio_put(LED_PIN, 1);

    printf("Modbus operations completed successfully.\n");

    return 0;
}

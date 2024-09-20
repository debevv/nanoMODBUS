#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nanomodbus.h"

#define WIRE_SIZE 1024
#define UNUSED_PARAM(x) ((x) = (x))

uint32_t run = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

nmbs_t server1 = {0};
nmbs_t server2 = {0};

uint32_t index_w = 0;
uint32_t indices_r[3] = {0};

uint8_t wire[WIRE_SIZE];


int32_t read_wire(uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);

        uint64_t index = (uint64_t) arg;
        uint32_t read = 0;
        for (int i = 0; i < count; i++) {
            if (indices_r[index] == index_w)
                break;

            indices_r[index] = (indices_r[index] + 1) % WIRE_SIZE;
            buf[i] = wire[indices_r[index]];
            read++;
        }

        pthread_mutex_unlock(&mutex);

        if (read != 0)
            return (int32_t) read;

        if (timeout_ms != 0)
            usleep(timeout_ms * 1000);
        else
            return 0;

        timeout_ms = 0;
    }
}

int32_t write_wire(const uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    UNUSED_PARAM(timeout_ms);
    pthread_mutex_lock(&mutex);

    uint64_t index = (uint64_t) arg;
    uint32_t written = 0;
    for (int i = 0; i < count; i++) {
        index_w = (index_w + 1) % WIRE_SIZE;
        indices_r[index] = index_w;
        wire[index_w] = buf[i];
        written++;
    }

    pthread_mutex_unlock(&mutex);

    return (int32_t) written;
}

nmbs_error read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);
    for (int i = 0; i < quantity; i++)
        nmbs_bitfield_write(coils_out, address + i, 1);

    return NMBS_ERROR_NONE;
}

void* poll_server1(void* arg) {
    UNUSED_PARAM(arg);
    while (run) {
        nmbs_server_poll(&server1);
    }

    return NULL;
}

void* poll_server2(void* arg) {
    UNUSED_PARAM(arg);
    while (run) {
        nmbs_server_poll(&server2);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    nmbs_platform_conf c_conf;
    nmbs_platform_conf_create(&c_conf);
    c_conf.arg = wire;
    c_conf.transport = NMBS_TRANSPORT_RTU;
    c_conf.read = read_wire;
    c_conf.write = write_wire;
    c_conf.arg = 0;

    nmbs_platform_conf s1_conf = c_conf;
    s1_conf.arg = (void*) 1;

    nmbs_platform_conf s2_conf = c_conf;
    s2_conf.arg = (void*) 2;

    nmbs_t client = {0};
    nmbs_error err = nmbs_client_create(&client, &c_conf);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus client\n");
        return 1;
    }

    nmbs_set_read_timeout(&client, 5000);
    nmbs_set_byte_timeout(&client, 100);

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_coils = read_coils;

    err = nmbs_server_create(&server1, 33, &s1_conf, &callbacks);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus server 1\n");
        return 1;
    }

    nmbs_set_read_timeout(&server1, 100);
    nmbs_set_byte_timeout(&server1, 100);

    err = nmbs_server_create(&server2, 99, &s2_conf, &callbacks);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus server 2\n");
        return 1;
    }

    nmbs_set_read_timeout(&server2, 100);
    nmbs_set_byte_timeout(&server2, 100);

    pthread_t thread1, thread2;
    int ret = pthread_create(&thread1, NULL, poll_server1, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating thread 1\n");
        return 1;
    }

    ret = pthread_create(&thread2, NULL, poll_server2, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating thread 2\n");
        return 1;
    }

    sleep(1);

    nmbs_bitfield coils;
    for (uint32_t c = 0; c < 10; c++) {
        nmbs_bitfield_write(coils, c, rand() % 1);
    }

    nmbs_set_destination_rtu_address(&client, 33);

    err = nmbs_write_multiple_coils(&client, 0, 10, coils);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing coils to %d %s\n", 33, nmbs_strerror(err));
    }

    nmbs_bitfield coils_read;
    err = nmbs_read_coils(&client, 0, 10, coils_read);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading coils from %d %s\n", 33, nmbs_strerror(err));
    }

    if (memcmp(coils, coils, sizeof(nmbs_bitfield)) != 0) {
        fprintf(stderr, "Coils mismatch from %d\n", 33);
    }

    for (uint32_t c = 0; c < 10; c++) {
        nmbs_bitfield_write(coils, c, rand() % 1);
    }

    nmbs_set_destination_rtu_address(&client, 99);

    err = nmbs_write_multiple_coils(&client, 0, 10, coils);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing coils to %d %s\n", 99, nmbs_strerror(err));
    }

    err = nmbs_read_coils(&client, 0, 10, coils_read);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading coils from %d %s\n", 99, nmbs_strerror(err));
    }

    if (memcmp(coils, coils, sizeof(nmbs_bitfield)) != 0) {
        fprintf(stderr, "Coils mismatch from %d\n", 99);
    }

    sleep(5);

    run = 0;
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}

#include "modbusino_tests.h"
#include "modbusino.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int read_byte_empty(uint8_t* b, int32_t timeout) {
    return 0;
}


int write_byte_empty(uint8_t b, int32_t timeout) {
    return 0;
}


void test_server_create(mbsn_transport transport) {
    mbsn_t mbsn;
    mbsn_error err;

    mbsn_platform_conf platform_conf_empty = {.transport = transport,
                                              .read_byte = read_byte_empty,
                                              .write_byte = write_byte_empty,
                                              .sleep = platform_sleep};

    should("create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(&mbsn, TEST_SERVER_ADDR, &platform_conf_empty, (mbsn_callbacks){});
    check(err);

    should("check parameters and fail to create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(NULL, TEST_SERVER_ADDR, &platform_conf_empty, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    err = mbsn_server_create(&mbsn, 0, &platform_conf_empty, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    mbsn_platform_conf p = platform_conf_empty;
    p.transport = 3;
    err = mbsn_server_create(&mbsn, 0, &p, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    p = platform_conf_empty;
    p.read_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, &p, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    p = platform_conf_empty;
    p.write_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, &p, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);
}


int read_byte_timeout(uint8_t* b, int32_t timeout) {
    usleep(timeout * 1000);
    return 0;
}


int read_byte_timeout_third(uint8_t* b, int32_t timeout) {
    static int stage = 0;
    switch (stage) {
        case 0:
        case 1:
            *b = 1;
            stage++;
            return 1;
        case 2:
            assert(timeout > 0);
            usleep(timeout * 1000 + 100 * 1000);
            stage = 0;
            return 0;
        default:
            stage = 0;
            return -1;
    }
}


void test_server_receive_base(mbsn_transport transport) {
    mbsn_t server;
    mbsn_error err;
    mbsn_platform_conf platform_conf;


    should("honor read_timeout and return normally");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read_byte = read_byte_timeout;
    platform_conf.write_byte = write_byte_empty;
    platform_conf.sleep = platform_sleep;

    const int32_t read_timeout_ms = 250;

    err = mbsn_server_create(&server, TEST_SERVER_ADDR, &platform_conf, (mbsn_callbacks){});
    check(err);

    mbsn_set_read_timeout(&server, read_timeout_ms);
    mbsn_set_byte_timeout(&server, -1);

    const int polls = 5;
    for (int i = 0; i < polls; i++) {
        uint64_t start = now_ms();
        err = mbsn_server_receive(&server);
        check(err);

        uint64_t diff = now_ms() - start;

        assert(diff >= read_timeout_ms);
    }


    should("honor byte_timeout and return MBSN_ERROR_TIMEOUT");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read_byte = read_byte_timeout_third;
    platform_conf.write_byte = write_byte_empty;

    const int32_t byte_timeout_ms = 250;

    err = mbsn_server_create(&server, TEST_SERVER_ADDR, &platform_conf, (mbsn_callbacks){});
    check(err);

    mbsn_set_read_timeout(&server, 1000);
    mbsn_set_byte_timeout(&server, byte_timeout_ms);

    err = mbsn_server_receive(&server);
    expect(err == MBSN_ERROR_TIMEOUT);
}


mbsn_error read_coils_and_error(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out) {}


mbsn_error read_coils_and_no_error(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out) {}


void test_fc1(mbsn_transport transport) {
    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION on calling a FC with unregistered callback");
    start_client_and_server(transport, (mbsn_callbacks){});
    assert(mbsn_read_coils(&CLIENT, 1, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);
    stop_client_and_server();
}


mbsn_transport transports[2] = {MBSN_TRANSPORT_TCP, MBSN_TRANSPORT_RTU};
const char* transports_str[2] = {"TCP", "RTU"};


void for_transports(void (*test_fn)(mbsn_transport), const char* should_str) {
    for (int t = 0; t < sizeof(transports) / sizeof(mbsn_transport); t++) {
        printf("%s: ", transports_str[t]);
        should(should_str);
        test(test_fn(transports[t]));
    }
}


int main() {

    for_transports(test_server_create, "create a modbus server");

    for_transports(test_server_receive_base, "receive no messages without failing");

    for_transports(test_fc1, "send and receive FC 01 (0x01) Read Coils");

    return 0;
}

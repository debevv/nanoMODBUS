#include "modbusino_tests.h"
#include "modbusino.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


void test_server_create(mbsn_transport transport) {
    mbsn_t mbsn;
    mbsn_error err;

    mbsn_transport_conf transport_conf_empty = {.transport = transport,
                                                .read_byte = read_byte_empty,
                                                .write_byte = write_byte_empty};

    should("create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(&mbsn, TEST_SERVER_ADDR, transport_conf_empty, (mbsn_callbacks){});
    check(err);

    should("check parameters and fail to create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(NULL, TEST_SERVER_ADDR, transport_conf_empty, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    err = mbsn_server_create(&mbsn, 0, transport_conf_empty, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    mbsn_transport_conf t = transport_conf_empty;
    t.transport = 3;
    err = mbsn_server_create(&mbsn, 0, t, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    t = transport_conf_empty;
    t.read_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, t, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    t = transport_conf_empty;
    t.write_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, t, (mbsn_callbacks){});
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);
};


void test_server_receive_base(mbsn_transport transport) {
    mbsn_t mbsn;
    mbsn_error err;
    mbsn_transport_conf transport_conf;

    should("honor read_timeout and return normally");
    reset(mbsn);
    transport_conf.transport = transport;
    transport_conf.read_byte = read_byte_timeout_1s;
    transport_conf.write_byte = write_byte_empty;

    const int32_t read_timeout_ms = 250;

    err = mbsn_server_create(&mbsn, TEST_SERVER_ADDR, transport_conf, (mbsn_callbacks){});
    check(err);

    mbsn_set_read_timeout(&mbsn, read_timeout_ms);
    mbsn_set_byte_timeout(&mbsn, -1);

    const int polls = 5;
    for (int i = 0; i < polls; i++) {
        uint64_t start = now_ms();
        err = mbsn_server_receive(&mbsn);
        check(err);

        uint64_t diff = now_ms() - start;

        if (diff < read_timeout_ms) {
            fail();
        }
    }

    should("honor byte_timeout and return MBSN_ERROR_TIMEOUT");
    reset(mbsn);
    transport_conf.transport = transport;
    transport_conf.read_byte = read_byte_timeout_third;
    transport_conf.write_byte = write_byte_empty;

    const int32_t byte_timeout_ms = 250;

    err = mbsn_server_create(&mbsn, TEST_SERVER_ADDR, transport_conf, (mbsn_callbacks){});
    check(err);

    mbsn_set_read_timeout(&mbsn, -1);
    mbsn_set_byte_timeout(&mbsn, byte_timeout_ms);

    err = mbsn_server_receive(&mbsn);
    expect(err == MBSN_ERROR_TIMEOUT);
}


int main() {
    should("create a RTU modbus server");
    test(test_server_create(MBSN_TRANSPORT_TCP));

    should("create a TCP modbus server");
    test(test_server_create(MBSN_TRANSPORT_TCP));

    should("receive no messages as RTU server without failing");
    test(test_server_receive_base(MBSN_TRANSPORT_RTU));

    should("receive no messages as TCP server without failing");
    test(test_server_receive_base(MBSN_TRANSPORT_TCP));

    return 0;
}

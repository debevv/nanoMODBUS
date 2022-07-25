#include "nanomodbus_tests.h"
#include "nanomodbus.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>


int32_t read_empty(uint8_t* b, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int32_t write_empty(const uint8_t* b, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


void test_server_create(nmbs_transport transport) {
    nmbs_t nmbs;
    nmbs_error err;

    nmbs_platform_conf platform_conf_empty = {
            .transport = transport,
            .read = read_empty,
            .write = write_empty,
    };


    nmbs_callbacks callbacks_empty;

    should("create a modbus server");
    reset(nmbs);
    err = nmbs_server_create(&nmbs, TEST_SERVER_ADDR, &platform_conf_empty, &callbacks_empty);
    check(err);

    should("check parameters and fail to create a modbus server");
    reset(nmbs);
    err = nmbs_server_create(NULL, TEST_SERVER_ADDR, &platform_conf_empty, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);

    reset(nmbs);
    err = nmbs_server_create(&nmbs, 0, &platform_conf_empty, &callbacks_empty);
    if (transport == NMBS_TRANSPORT_RTU)
        expect(err == NMBS_ERROR_INVALID_ARGUMENT);
    else
        expect(err == NMBS_ERROR_NONE);

    reset(nmbs);
    nmbs_platform_conf p = platform_conf_empty;
    p.transport = 3;
    err = nmbs_server_create(&nmbs, 0, &p, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);

    reset(nmbs);
    p = platform_conf_empty;
    p.read = NULL;
    err = nmbs_server_create(&nmbs, 0, &p, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);

    reset(nmbs);
    p = platform_conf_empty;
    p.write = NULL;
    err = nmbs_server_create(&nmbs, 0, &p, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);
}


int32_t read_timeout(uint8_t* buf, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(buf);
    UNUSED_PARAM(count);
    UNUSED_PARAM(arg);
    usleep(timeout * 1000);
    return 0;
}

// Timeouts on the second read
int32_t read_timeout_second(uint8_t* buf, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(count);
    UNUSED_PARAM(arg);

    static int stage = 0;
    switch (stage) {
        case 0:
            *buf = 1;
            stage++;
            return (int) count;
        case 1:
            expect(timeout > 0);
            usleep(timeout * 1000 + 100 * 1000);
            stage = 0;
            return 0;
        default:
            stage = 0;
            return -1;
    }
}


void test_server_receive_base(nmbs_transport transport) {
    nmbs_t server;
    nmbs_error err;
    nmbs_platform_conf platform_conf;
    nmbs_callbacks callbacks_empty;


    should("honor read_timeout and return normally");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read = read_timeout;
    platform_conf.write = write_empty;

    const int32_t read_timeout_ms = 250;

    err = nmbs_server_create(&server, TEST_SERVER_ADDR, &platform_conf, &callbacks_empty);
    check(err);

    nmbs_set_read_timeout(&server, read_timeout_ms);
    nmbs_set_byte_timeout(&server, -1);

    const int polls = 5;
    for (int i = 0; i < polls; i++) {
        uint64_t start = now_ms();
        err = nmbs_server_poll(&server);
        check(err);

        uint64_t diff = now_ms() - start;

        expect(diff >= (uint64_t) read_timeout_ms);
    }


    should("honor byte_timeout and return NMBS_ERROR_TIMEOUT");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read = read_timeout_second;
    platform_conf.write = write_empty;

    const int32_t byte_timeout_ms = 250;

    err = nmbs_server_create(&server, TEST_SERVER_ADDR, &platform_conf, &callbacks_empty);
    check(err);

    nmbs_set_read_timeout(&server, 1000);
    nmbs_set_byte_timeout(&server, byte_timeout_ms);

    err = nmbs_server_poll(&server);
    expect(err == NMBS_ERROR_TIMEOUT);
}


nmbs_error read_discrete(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 10 && quantity == 3) {
        nmbs_bitfield_write(coils_out, 0, 1);
        nmbs_bitfield_write(coils_out, 1, 0);
        nmbs_bitfield_write(coils_out, 2, 1);
    }

    if (address == 65526 && quantity == 10) {
        nmbs_bitfield_write(coils_out, 0, 1);
        nmbs_bitfield_write(coils_out, 1, 0);
        nmbs_bitfield_write(coils_out, 2, 1);
        nmbs_bitfield_write(coils_out, 3, 0);
        nmbs_bitfield_write(coils_out, 4, 1);
        nmbs_bitfield_write(coils_out, 5, 0);
        nmbs_bitfield_write(coils_out, 6, 1);
        nmbs_bitfield_write(coils_out, 7, 0);
        nmbs_bitfield_write(coils_out, 8, 1);
        nmbs_bitfield_write(coils_out, 9, 0);
    }

    return NMBS_ERROR_NONE;
}


void test_fc1(nmbs_transport transport) {
    const uint8_t fc = 1;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_coils(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.read_coils = read_discrete});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_coils(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(nmbs_read_coils(&CLIENT, 1, 2001, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_coils(&CLIENT, 65530, 7, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(65530), htons(7)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_coils(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_coils(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_coils(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    nmbs_bitfield bf;
    check(nmbs_read_coils(&CLIENT, 10, 3, bf));
    expect(nmbs_bitfield_read(bf, 0) == 1);
    expect(nmbs_bitfield_read(bf, 1) == 0);
    expect(nmbs_bitfield_read(bf, 2) == 1);

    check(nmbs_read_coils(&CLIENT, 65526, 10, bf));
    expect(nmbs_bitfield_read(bf, 0) == 1);
    expect(nmbs_bitfield_read(bf, 1) == 0);
    expect(nmbs_bitfield_read(bf, 2) == 1);
    expect(nmbs_bitfield_read(bf, 3) == 0);
    expect(nmbs_bitfield_read(bf, 4) == 1);
    expect(nmbs_bitfield_read(bf, 5) == 0);
    expect(nmbs_bitfield_read(bf, 6) == 1);
    expect(nmbs_bitfield_read(bf, 7) == 0);
    expect(nmbs_bitfield_read(bf, 8) == 1);
    expect(nmbs_bitfield_read(bf, 9) == 0);

    stop_client_and_server();
}


void test_fc2(nmbs_transport transport) {
    const uint8_t fc = 2;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_discrete_inputs(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.read_discrete_inputs = read_discrete});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 2001, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_discrete_inputs(&CLIENT, 65530, 7, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(65530), htons(7)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_discrete_inputs(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_discrete_inputs(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    nmbs_bitfield bf;
    check(nmbs_read_discrete_inputs(&CLIENT, 10, 3, bf));
    expect(nmbs_bitfield_read(bf, 0) == 1);
    expect(nmbs_bitfield_read(bf, 1) == 0);
    expect(nmbs_bitfield_read(bf, 2) == 1);

    check(nmbs_read_discrete_inputs(&CLIENT, 65526, 10, bf));
    expect(nmbs_bitfield_read(bf, 0) == 1);
    expect(nmbs_bitfield_read(bf, 1) == 0);
    expect(nmbs_bitfield_read(bf, 2) == 1);
    expect(nmbs_bitfield_read(bf, 3) == 0);
    expect(nmbs_bitfield_read(bf, 4) == 1);
    expect(nmbs_bitfield_read(bf, 5) == 0);
    expect(nmbs_bitfield_read(bf, 6) == 1);
    expect(nmbs_bitfield_read(bf, 7) == 0);
    expect(nmbs_bitfield_read(bf, 8) == 1);
    expect(nmbs_bitfield_read(bf, 9) == 0);

    stop_client_and_server();
}


nmbs_error read_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 10 && quantity == 3) {
        registers_out[0] = 100;
        registers_out[1] = 0;
        registers_out[2] = 200;
    }

    return NMBS_ERROR_NONE;
}


void test_fc3(nmbs_transport transport) {
    const uint8_t fc = 3;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_holding_registers(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.read_holding_registers = read_registers});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_holding_registers(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(nmbs_read_holding_registers(&CLIENT, 1, 126, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_holding_registers(&CLIENT, 0xFFFF, 2, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_holding_registers(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_holding_registers(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_holding_registers(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    uint16_t regs[3];
    check(nmbs_read_holding_registers(&CLIENT, 10, 3, regs));
    expect(regs[0] == 100);
    expect(regs[1] == 0);
    expect(regs[2] == 200);

    stop_client_and_server();
}


void test_fc4(nmbs_transport transport) {
    const uint8_t fc = 4;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_input_registers(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.read_input_registers = read_registers});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_input_registers(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(nmbs_read_input_registers(&CLIENT, 1, 126, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_input_registers(&CLIENT, 0xFFFF, 2, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_input_registers(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_input_registers(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_input_registers(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    uint16_t regs[3];
    check(nmbs_read_input_registers(&CLIENT, 10, 3, regs));
    expect(regs[0] == 100);
    expect(regs[1] == 0);
    expect(regs[2] == 200);

    stop_client_and_server();
}


nmbs_error write_coil(uint16_t address, bool value, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4 && !value)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (address == 5 && value)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    return NMBS_ERROR_NONE;
}


void test_fc5(nmbs_transport transport) {
    const uint8_t fc = 5;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_single_coil(&CLIENT, 0, true) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.write_single_coil = write_coil});

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE when calling with value !0x0000 or 0xFF000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(6), htons(0x0001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(6), htons(0xFFFF)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_single_coil(&CLIENT, 1, true) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_single_coil(&CLIENT, 2, true) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_single_coil(&CLIENT, 3, true) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    check(nmbs_write_single_coil(&CLIENT, 4, true));
    check(nmbs_write_single_coil(&CLIENT, 5, false));

    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(4), htons(0xFF00)}, 4));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0xFF00));

    stop_client_and_server();
}


nmbs_error write_register(uint16_t address, uint16_t value, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4 && !value)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (address == 5 && value)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    return NMBS_ERROR_NONE;
}


void test_fc6(nmbs_transport transport) {
    const uint8_t fc = 6;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_single_register(&CLIENT, 0, 123) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.write_single_register = write_register});

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_single_register(&CLIENT, 1, 123) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_single_register(&CLIENT, 2, 123) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_single_register(&CLIENT, 3, 123) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    check(nmbs_write_single_register(&CLIENT, 4, true));
    check(nmbs_write_single_register(&CLIENT, 5, false));

    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(4), htons(0x123)}, 4));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0x123));

    stop_client_and_server();
}


nmbs_error write_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4) {
        if (quantity != 4)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(nmbs_bitfield_read(coils, 0) == 1);
        expect(nmbs_bitfield_read(coils, 1) == 0);
        expect(nmbs_bitfield_read(coils, 2) == 1);
        expect(nmbs_bitfield_read(coils, 3) == 0);

        return NMBS_ERROR_NONE;
    }

    if (address == 5) {
        if (quantity != 27)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(nmbs_bitfield_read(coils, 26) == 1);

        return NMBS_ERROR_NONE;
    }

    if (address == 7) {
        if (quantity != 1)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        return NMBS_ERROR_NONE;
    }

    return NMBS_ERROR_NONE;
}


void test_fc15(nmbs_transport transport) {
    const uint8_t fc = 15;
    uint8_t raw_res[260];
    nmbs_bitfield bf = {0};
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_multiple_coils(&CLIENT, 0, 1, bf) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.write_multiple_coils = write_coils});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_write_multiple_coils(&CLIENT, 1, 0, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 0x07B0");
    expect(nmbs_write_multiple_coils(&CLIENT, 1, 0x07B1, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_write_multiple_coils(&CLIENT, 0xFFFF, 2, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0), htons(0x0100)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2000), htons(0x0100)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2), htons(0x0100)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    /*
    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when quantity does not match byte count");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(5), htons(0x0303)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
     */

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_multiple_coils(&CLIENT, 1, 1, bf) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_multiple_coils(&CLIENT, 2, 2, bf) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_multiple_coils(&CLIENT, 3, 3, bf) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    nmbs_bitfield_write(bf, 0, 1);
    nmbs_bitfield_write(bf, 1, 0);
    nmbs_bitfield_write(bf, 2, 1);
    nmbs_bitfield_write(bf, 3, 0);
    check(nmbs_write_multiple_coils(&CLIENT, 4, 4, bf));

    nmbs_bitfield_write(bf, 26, 1);
    check(nmbs_write_multiple_coils(&CLIENT, 5, 27, bf));

    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(7), htons(1), htons(0x0100)}, 6));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    stop_client_and_server();
}


nmbs_error write_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, void *arg) {
    UNUSED_PARAM(arg);

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4) {
        if (quantity != 4)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(registers[0] == 255);
        expect(registers[1] == 1);
        expect(registers[2] == 2);
        expect(registers[3] == 3);

        return NMBS_ERROR_NONE;
    }

    if (address == 5) {
        if (quantity != 27)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(registers[26] == 26);

        return NMBS_ERROR_NONE;
    }

    if (address == 7) {
        if (quantity != 1)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        return NMBS_ERROR_NONE;
    }

    return NMBS_ERROR_NONE;
}


void test_fc16(nmbs_transport transport) {
    const uint8_t fc = 16;
    uint8_t raw_res[260];
    uint16_t registers[125];
    nmbs_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_multiple_registers(&CLIENT, 0, 1, registers) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(nmbs_callbacks){.write_multiple_registers = write_registers});

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_write_multiple_registers(&CLIENT, 1, 0, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 0x007B");
    expect(nmbs_write_multiple_registers(&CLIENT, 1, 0x007C, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_write_multiple_registers(&CLIENT, 0xFFFF, 2, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0), htons(0x0200), htons(0)}, 7));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2000), htons(0x0200), htons(0)}, 7));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2), htons(0x0200), htons(0)}, 7));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    /*
    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when quantity does not match byte count");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(5), htons(0x0303)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
     */

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_multiple_registers(&CLIENT, 1, 1, registers) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_multiple_registers(&CLIENT, 2, 2, registers) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_multiple_registers(&CLIENT, 3, 3, registers) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    registers[0] = 255;
    registers[1] = 1;
    registers[2] = 2;
    registers[3] = 3;
    check(nmbs_write_multiple_registers(&CLIENT, 4, 4, registers));

    registers[26] = 26;
    check(nmbs_write_multiple_registers(&CLIENT, 6, 27, registers));

    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(7), htons(1), htons(0x0200), htons(0)}, 7));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    stop_client_and_server();
}


nmbs_transport transports[2] = {NMBS_TRANSPORT_RTU, NMBS_TRANSPORT_TCP};
const char* transports_str[2] = {"RTU", "TCP"};

void for_transports(void (*test_fn)(nmbs_transport), const char* should_str) {
    for (unsigned long t = 0; t < sizeof(transports) / sizeof(nmbs_transport); t++) {
        printf("Should %s on %s:\n", should_str, transports_str[t]);
        test(test_fn(transports[t]));
    }
}

int main() {
    for_transports(test_server_create, "create a modbus server");

    for_transports(test_server_receive_base, "receive no messages without failing");

    for_transports(test_fc1, "send and receive FC 01 (0x01) Read Coils");

    for_transports(test_fc2, "send and receive FC 02 (0x02) Read Discrete Inputs");

    for_transports(test_fc3, "send and receive FC 03 (0x03) Read Holding Registers");

    for_transports(test_fc4, "send and receive FC 04 (0x04) Read Input Registers");

    for_transports(test_fc5, "send and receive FC 05 (0x05) Write Single Coil");

    for_transports(test_fc6, "send and receive FC 06 (0x06) Write Single Register");

    for_transports(test_fc15, "send and receive FC 15 (0x0F) Write Multiple Coils");

    for_transports(test_fc16, "send and receive FC 16 (0x10) Write Multiple registers");

    return 0;
}

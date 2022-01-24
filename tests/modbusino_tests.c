#include "modbusino_tests.h"
#include "modbusino.h"
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>


int read_byte_empty(uint8_t* b, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int write_byte_empty(uint8_t b, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


void test_server_create(mbsn_transport transport) {
    mbsn_t mbsn;
    mbsn_error err;

    mbsn_platform_conf platform_conf_empty = {.transport = transport,
                                              .read_byte = read_byte_empty,
                                              .write_byte = write_byte_empty,
                                              .sleep = platform_sleep};

    mbsn_callbacks callbacks_empty;

    should("create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(&mbsn, TEST_SERVER_ADDR, &platform_conf_empty, &callbacks_empty);
    check(err);

    should("check parameters and fail to create a modbus server");
    reset(mbsn);
    err = mbsn_server_create(NULL, TEST_SERVER_ADDR, &platform_conf_empty, &callbacks_empty);
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    err = mbsn_server_create(&mbsn, 0, &platform_conf_empty, &callbacks_empty);
    if (transport == MBSN_TRANSPORT_RTU)
        expect(err == MBSN_ERROR_INVALID_ARGUMENT);
    else
        expect(err == MBSN_ERROR_NONE);

    reset(mbsn);
    mbsn_platform_conf p = platform_conf_empty;
    p.transport = 3;
    err = mbsn_server_create(&mbsn, 0, &p, &callbacks_empty);
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    p = platform_conf_empty;
    p.read_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, &p, &callbacks_empty);
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);

    reset(mbsn);
    p = platform_conf_empty;
    p.write_byte = NULL;
    err = mbsn_server_create(&mbsn, 0, &p, &callbacks_empty);
    expect(err == MBSN_ERROR_INVALID_ARGUMENT);
}


int read_byte_timeout(uint8_t* b, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(arg);
    usleep(timeout * 1000);
    return 0;
}


int read_byte_timeout_third(uint8_t* b, int32_t timeout, void* arg) {
    UNUSED_PARAM(arg);

    static int stage = 0;
    switch (stage) {
        case 0:
        case 1:
            *b = 1;
            stage++;
            return 1;
        case 2:
            expect(timeout > 0);
            usleep(timeout * 1000 + 100 * 1000);
            stage = 0;
            return 0;
        default:
            stage = 0;
            return -1;
    }
}


void test_server_receive_base(mbsn_transport transport) {
    mbsn_t server, client;
    mbsn_error err;
    mbsn_platform_conf platform_conf;
    mbsn_callbacks callbacks_empty;


    should("honor read_timeout and return normally");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read_byte = read_byte_timeout;
    platform_conf.write_byte = write_byte_empty;
    platform_conf.sleep = platform_sleep;

    const int32_t read_timeout_ms = 250;

    err = mbsn_server_create(&server, TEST_SERVER_ADDR, &platform_conf, &callbacks_empty);
    check(err);

    mbsn_set_read_timeout(&server, read_timeout_ms);
    mbsn_set_byte_timeout(&server, -1);

    const int polls = 5;
    for (int i = 0; i < polls; i++) {
        uint64_t start = now_ms();
        err = mbsn_server_poll(&server);
        check(err);

        uint64_t diff = now_ms() - start;

        expect(diff >= (uint64_t) read_timeout_ms);
    }


    should("honor byte_timeout and return MBSN_ERROR_TIMEOUT");
    reset(server);
    platform_conf.transport = transport;
    platform_conf.read_byte = read_byte_timeout_third;
    platform_conf.write_byte = write_byte_empty;

    const int32_t byte_timeout_ms = 250;

    err = mbsn_server_create(&server, TEST_SERVER_ADDR, &platform_conf, &callbacks_empty);
    check(err);

    mbsn_set_read_timeout(&server, 1000);
    mbsn_set_byte_timeout(&server, byte_timeout_ms);

    err = mbsn_server_poll(&server);
    expect(err == MBSN_ERROR_TIMEOUT);


    should("honor byte spacing on RTU");
    if (transport == MBSN_TRANSPORT_RTU) {
        reset(client);
        platform_conf.transport = transport;
        platform_conf.read_byte = read_byte_socket_client;
        platform_conf.write_byte = write_byte_socket_client;

        reset_sockets();

        check(mbsn_client_create(&client, &platform_conf));

        mbsn_set_byte_spacing(&client, 200);

        uint64_t start = now_ms();
        check(mbsn_send_raw_pdu(&client, 1, (uint16_t[]){htons(1), htons(1)}, 4));
        uint64_t diff = now_ms() - start;
        expect(diff >= 200 * 8);
    }
}


mbsn_error read_discrete(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 10 && quantity == 3) {
        mbsn_bitfield_write(coils_out, 0, 1);
        mbsn_bitfield_write(coils_out, 1, 0);
        mbsn_bitfield_write(coils_out, 2, 1);
    }

    if (address == 65526 && quantity == 10) {
        mbsn_bitfield_write(coils_out, 0, 1);
        mbsn_bitfield_write(coils_out, 1, 0);
        mbsn_bitfield_write(coils_out, 2, 1);
        mbsn_bitfield_write(coils_out, 3, 0);
        mbsn_bitfield_write(coils_out, 4, 1);
        mbsn_bitfield_write(coils_out, 5, 0);
        mbsn_bitfield_write(coils_out, 6, 1);
        mbsn_bitfield_write(coils_out, 7, 0);
        mbsn_bitfield_write(coils_out, 8, 1);
        mbsn_bitfield_write(coils_out, 9, 0);
    }

    return MBSN_ERROR_NONE;
}


void test_fc1(mbsn_transport transport) {
    const uint8_t fc = 1;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_read_coils(&CLIENT, 0, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.read_coils = read_discrete});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_read_coils(&CLIENT, 1, 0, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(mbsn_read_coils(&CLIENT, 1, 2001, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_read_coils(&CLIENT, 65530, 7, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(65530), htons(7)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_read_coils(&CLIENT, 1, 1, NULL) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_read_coils(&CLIENT, 2, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_read_coils(&CLIENT, 3, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    mbsn_bitfield bf;
    check(mbsn_read_coils(&CLIENT, 10, 3, bf));
    expect(mbsn_bitfield_read(bf, 0) == 1);
    expect(mbsn_bitfield_read(bf, 1) == 0);
    expect(mbsn_bitfield_read(bf, 2) == 1);

    check(mbsn_read_coils(&CLIENT, 65526, 10, bf));
    expect(mbsn_bitfield_read(bf, 0) == 1);
    expect(mbsn_bitfield_read(bf, 1) == 0);
    expect(mbsn_bitfield_read(bf, 2) == 1);
    expect(mbsn_bitfield_read(bf, 3) == 0);
    expect(mbsn_bitfield_read(bf, 4) == 1);
    expect(mbsn_bitfield_read(bf, 5) == 0);
    expect(mbsn_bitfield_read(bf, 6) == 1);
    expect(mbsn_bitfield_read(bf, 7) == 0);
    expect(mbsn_bitfield_read(bf, 8) == 1);
    expect(mbsn_bitfield_read(bf, 9) == 0);

    stop_client_and_server();
}


void test_fc2(mbsn_transport transport) {
    const uint8_t fc = 2;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_read_discrete_inputs(&CLIENT, 0, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.read_discrete_inputs = read_discrete});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_read_discrete_inputs(&CLIENT, 1, 0, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(mbsn_read_discrete_inputs(&CLIENT, 1, 2001, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_read_discrete_inputs(&CLIENT, 65530, 7, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(65530), htons(7)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_read_discrete_inputs(&CLIENT, 1, 1, NULL) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_read_discrete_inputs(&CLIENT, 2, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_read_discrete_inputs(&CLIENT, 3, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    mbsn_bitfield bf;
    check(mbsn_read_discrete_inputs(&CLIENT, 10, 3, bf));
    expect(mbsn_bitfield_read(bf, 0) == 1);
    expect(mbsn_bitfield_read(bf, 1) == 0);
    expect(mbsn_bitfield_read(bf, 2) == 1);

    check(mbsn_read_discrete_inputs(&CLIENT, 65526, 10, bf));
    expect(mbsn_bitfield_read(bf, 0) == 1);
    expect(mbsn_bitfield_read(bf, 1) == 0);
    expect(mbsn_bitfield_read(bf, 2) == 1);
    expect(mbsn_bitfield_read(bf, 3) == 0);
    expect(mbsn_bitfield_read(bf, 4) == 1);
    expect(mbsn_bitfield_read(bf, 5) == 0);
    expect(mbsn_bitfield_read(bf, 6) == 1);
    expect(mbsn_bitfield_read(bf, 7) == 0);
    expect(mbsn_bitfield_read(bf, 8) == 1);
    expect(mbsn_bitfield_read(bf, 9) == 0);

    stop_client_and_server();
}


mbsn_error read_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 10 && quantity == 3) {
        registers_out[0] = 100;
        registers_out[1] = 0;
        registers_out[2] = 200;
    }

    return MBSN_ERROR_NONE;
}


void test_fc3(mbsn_transport transport) {
    const uint8_t fc = 3;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_read_holding_registers(&CLIENT, 0, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.read_holding_registers = read_registers});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_read_holding_registers(&CLIENT, 1, 0, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(mbsn_read_holding_registers(&CLIENT, 1, 126, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_read_holding_registers(&CLIENT, 0xFFFF, 2, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_read_holding_registers(&CLIENT, 1, 1, NULL) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_read_holding_registers(&CLIENT, 2, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_read_holding_registers(&CLIENT, 3, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    uint16_t regs[3];
    check(mbsn_read_holding_registers(&CLIENT, 10, 3, regs));
    expect(regs[0] == 100);
    expect(regs[1] == 0);
    expect(regs[2] == 200);

    stop_client_and_server();
}


void test_fc4(mbsn_transport transport) {
    const uint8_t fc = 4;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_read_input_registers(&CLIENT, 0, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.read_input_registers = read_registers});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_read_input_registers(&CLIENT, 1, 0, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(mbsn_read_input_registers(&CLIENT, 1, 126, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_read_input_registers(&CLIENT, 0xFFFF, 2, NULL) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2001)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_read_input_registers(&CLIENT, 1, 1, NULL) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_read_input_registers(&CLIENT, 2, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_read_input_registers(&CLIENT, 3, 1, NULL) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    uint16_t regs[3];
    check(mbsn_read_input_registers(&CLIENT, 10, 3, regs));
    expect(regs[0] == 100);
    expect(regs[1] == 0);
    expect(regs[2] == 200);

    stop_client_and_server();
}


mbsn_error write_coil(uint16_t address, bool value) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4 && !value)
        return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (address == 5 && value)
        return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

    return MBSN_ERROR_NONE;
}


void test_fc5(mbsn_transport transport) {
    const uint8_t fc = 5;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_write_single_coil(&CLIENT, 0, true) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.write_single_coil = write_coil});

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE when calling with value !0x0000 or 0xFF000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(6), htons(0x0001)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(6), htons(0xFFFF)}, 4));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_write_single_coil(&CLIENT, 1, true) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_write_single_coil(&CLIENT, 2, true) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_write_single_coil(&CLIENT, 3, true) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    check(mbsn_write_single_coil(&CLIENT, 4, true));
    check(mbsn_write_single_coil(&CLIENT, 5, false));

    should("echo request's address and value");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(4), htons(0xFF00)}, 4));
    check(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0xFF00));

    stop_client_and_server();
}


mbsn_error write_register(uint16_t address, uint16_t value) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4 && !value)
        return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (address == 5 && value)
        return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

    return MBSN_ERROR_NONE;
}


void test_fc6(mbsn_transport transport) {
    const uint8_t fc = 6;
    uint8_t raw_res[260];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_write_single_register(&CLIENT, 0, 123) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.write_single_register = write_register});

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_write_single_register(&CLIENT, 1, 123) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_write_single_register(&CLIENT, 2, 123) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_write_single_register(&CLIENT, 3, 123) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    check(mbsn_write_single_register(&CLIENT, 4, true));
    check(mbsn_write_single_register(&CLIENT, 5, false));

    should("echo request's address and value");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(4), htons(0x123)}, 4));
    check(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0x123));

    stop_client_and_server();
}


mbsn_error write_coils(uint16_t address, uint16_t quantity, const mbsn_bitfield coils) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4) {
        if (quantity != 4)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(mbsn_bitfield_read(coils, 0) == 1);
        expect(mbsn_bitfield_read(coils, 1) == 0);
        expect(mbsn_bitfield_read(coils, 2) == 1);
        expect(mbsn_bitfield_read(coils, 3) == 0);

        return MBSN_ERROR_NONE;
    }

    if (address == 5) {
        if (quantity != 27)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(mbsn_bitfield_read(coils, 26) == 1);

        return MBSN_ERROR_NONE;
    }

    if (address == 7) {
        if (quantity != 1)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        return MBSN_ERROR_NONE;
    }

    return MBSN_ERROR_NONE;
}


void test_fc15(mbsn_transport transport) {
    const uint8_t fc = 15;
    uint8_t raw_res[260];
    mbsn_bitfield bf = {0};
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_write_multiple_coils(&CLIENT, 0, 1, bf) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.write_multiple_coils = write_coils});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_write_multiple_coils(&CLIENT, 1, 0, bf) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 0x07B0");
    expect(mbsn_write_multiple_coils(&CLIENT, 1, 0x07B1, bf) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_write_multiple_coils(&CLIENT, 0xFFFF, 2, bf) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0), htons(0x0100)}, 6));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2000), htons(0x0100)}, 6));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2), htons(0x0100)}, 6));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    /*
    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when quantity does not match byte count");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(5), htons(0x0303)}, 6));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);
     */

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_write_multiple_coils(&CLIENT, 1, 1, bf) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_write_multiple_coils(&CLIENT, 2, 2, bf) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_write_multiple_coils(&CLIENT, 3, 3, bf) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    mbsn_bitfield_write(bf, 0, 1);
    mbsn_bitfield_write(bf, 1, 0);
    mbsn_bitfield_write(bf, 2, 1);
    mbsn_bitfield_write(bf, 3, 0);
    check(mbsn_write_multiple_coils(&CLIENT, 4, 4, bf));

    mbsn_bitfield_write(bf, 26, 1);
    check(mbsn_write_multiple_coils(&CLIENT, 5, 27, bf));

    should("echo request's address and value");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(7), htons(1), htons(0x0100)}, 6));
    check(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    stop_client_and_server();
}


mbsn_error write_registers(uint16_t address, uint16_t quantity, const uint16_t* registers) {
    if (address == 1)
        return -1;

    if (address == 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4) {
        if (quantity != 4)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(registers[0] == 255);
        expect(registers[1] == 1);
        expect(registers[2] == 2);
        expect(registers[3] == 3);

        return MBSN_ERROR_NONE;
    }

    if (address == 5) {
        if (quantity != 27)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        expect(registers[26] == 26);

        return MBSN_ERROR_NONE;
    }

    if (address == 7) {
        if (quantity != 1)
            return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;

        return MBSN_ERROR_NONE;
    }

    return MBSN_ERROR_NONE;
}


void test_fc16(mbsn_transport transport) {
    const uint8_t fc = 16;
    uint8_t raw_res[260];
    uint16_t registers[125];
    mbsn_callbacks callbacks_empty = {0};

    start_client_and_server(transport, &callbacks_empty);

    should("return MBSN_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(mbsn_write_multiple_registers(&CLIENT, 0, 1, registers) == MBSN_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    start_client_and_server(transport, &(mbsn_callbacks){.write_multiple_registers = write_registers});

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(mbsn_write_multiple_registers(&CLIENT, 1, 0, registers) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with quantity > 0x007B");
    expect(mbsn_write_multiple_registers(&CLIENT, 1, 0x007C, registers) == MBSN_ERROR_INVALID_ARGUMENT);

    should("immediately return MBSN_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(mbsn_write_multiple_registers(&CLIENT, 0xFFFF, 2, registers) == MBSN_ERROR_INVALID_ARGUMENT);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(0), htons(0x0200), htons(0)}, 7));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(2000), htons(0x0200), htons(0)}, 7));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(0xFFFF), htons(2), htons(0x0200), htons(0)}, 7));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    /*
    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when quantity does not match byte count");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(1), htons(5), htons(0x0303)}, 6));
    expect(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 2) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);
     */

    should("return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(mbsn_write_multiple_registers(&CLIENT, 1, 1, registers) == MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(mbsn_write_multiple_registers(&CLIENT, 2, 2, registers) == MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(mbsn_write_multiple_registers(&CLIENT, 3, 3, registers) == MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    registers[0] = 255;
    registers[1] = 1;
    registers[2] = 2;
    registers[3] = 3;
    check(mbsn_write_multiple_registers(&CLIENT, 4, 4, registers));

    registers[26] = 26;
    check(mbsn_write_multiple_registers(&CLIENT, 6, 27, registers));

    should("echo request's address and value");
    check(mbsn_send_raw_pdu(&CLIENT, fc, (uint16_t[]){htons(7), htons(1), htons(0x0200), htons(0)}, 7));
    check(mbsn_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    stop_client_and_server();
}


mbsn_transport transports[2] = {MBSN_TRANSPORT_RTU, MBSN_TRANSPORT_TCP};
const char* transports_str[2] = {"RTU", "TCP"};

void for_transports(void (*test_fn)(mbsn_transport), const char* should_str) {
    for (unsigned long t = 0; t < sizeof(transports) / sizeof(mbsn_transport); t++) {
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

#include "nanomodbus_tests.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t callbacks_user_data = -64;

uint8_t check_user_data(void* data) {
    return *((int64_t*) data) == callbacks_user_data;
}

int32_t read_empty(uint8_t* data, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(data);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int32_t write_empty(const uint8_t* data, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(data);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


void test_server_create(nmbs_transport transport) {
    nmbs_t nmbs;
    nmbs_error err = NMBS_ERROR_NONE;

    nmbs_platform_conf platform_conf_empty;
    nmbs_platform_conf_create(&platform_conf_empty);
    platform_conf_empty.transport = transport;
    platform_conf_empty.read = read_empty;
    platform_conf_empty.write = write_empty;

    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

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
    nmbs_platform_conf conf = platform_conf_empty;
    conf.transport = 3;
    err = nmbs_server_create(&nmbs, 0, &conf, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);

    reset(nmbs);
    conf = platform_conf_empty;
    conf.read = NULL;
    err = nmbs_server_create(&nmbs, 0, &conf, &callbacks_empty);
    expect(err == NMBS_ERROR_INVALID_ARGUMENT);

    reset(nmbs);
    conf = platform_conf_empty;
    conf.write = NULL;
    err = nmbs_server_create(&nmbs, 0, &conf, &callbacks_empty);
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
    nmbs_error err = NMBS_ERROR_NONE;
    nmbs_platform_conf platform_conf;
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    should("honor read_timeout and return normally");
    reset(server);

    nmbs_platform_conf_create(&platform_conf);
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


nmbs_error read_discrete(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_coils(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_coils = read_discrete;

    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_coils(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(nmbs_read_coils(&CLIENT, 1, 2001, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_coils(&CLIENT, 65530, 7, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(65530), htons(7)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_coils(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_coils(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_coils(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    nmbs_bitfield bf = {0};
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

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_coils(&CLIENT, 2, 1, bf) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(10), htons(3)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


void test_fc2(nmbs_transport transport) {
    const uint8_t fc = 2;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_discrete_inputs(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_discrete_inputs = read_discrete;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 2000");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 2001, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_discrete_inputs(&CLIENT, 65530, 7, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(65530), htons(7)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_discrete_inputs(&CLIENT, 1, 1, NULL) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_discrete_inputs(&CLIENT, 2, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_discrete_inputs(&CLIENT, 3, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    nmbs_bitfield bf = {0};
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

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_discrete_inputs(&CLIENT, 2, 1, bf) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(10), htons(3)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error read_registers(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (address == 1)
        return -1;

    if (address == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (address == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (address == 4) {
        if (quantity != 4)
            return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

        registers_out[0] = 255;
        registers_out[1] = 1;
        registers_out[2] = 2;
        registers_out[3] = 3;

        return NMBS_ERROR_NONE;
    }

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_holding_registers(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_holding_registers = read_registers;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_holding_registers(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(nmbs_read_holding_registers(&CLIENT, 1, 126, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_holding_registers(&CLIENT, 0xFFFF, 2, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(0xFFFF), htons(2)}, 4));
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

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_holding_registers(&CLIENT, 2, 1, regs) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(10), htons(3)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


void test_fc4(nmbs_transport transport) {
    const uint8_t fc = 4;
    uint8_t raw_res[260];
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_input_registers(&CLIENT, 0, 1, NULL) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_input_registers = read_registers;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_input_registers(&CLIENT, 1, 0, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 125");
    expect(nmbs_read_input_registers(&CLIENT, 1, 126, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_input_registers(&CLIENT, 0xFFFF, 2, NULL) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(0xFFFF), htons(2)}, 4));
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

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_input_registers(&CLIENT, 2, 1, regs) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2001)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(10), htons(3)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error write_coil(uint16_t address, bool value, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_single_coil(&CLIENT, 0, true) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.write_single_coil = write_coil;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE when calling with value not 0x0000 or 0xFF000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(6), htons(0x0001)}, 4));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(6), htons(0xFFFF)}, 4));
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
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(4), htons(0xFF00)}, 4));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0xFF00));

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("ignore response when sending to broadcast address");
        expect(nmbs_write_single_coil(&CLIENT, 2, true) == NMBS_ERROR_NONE);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(2), htons(0x00FF)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(4), htons(0x0000)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error write_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_single_register(&CLIENT, 0, 123) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.write_single_register = write_register;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_single_register(&CLIENT, 1, 123) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_single_register(&CLIENT, 2, 123) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_single_register(&CLIENT, 3, 123) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    check(nmbs_write_single_register(&CLIENT, 4, 123));
    check(nmbs_write_single_register(&CLIENT, 5, 0));

    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(4), htons(0x123)}, 4));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(4));
    expect(((uint16_t*) raw_res)[1] == ntohs(0x123));

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("ignore response when sending to broadcast address");
        expect(nmbs_write_single_register(&CLIENT, 2, 123) == NMBS_ERROR_NONE);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(2), htons(0x123)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(4), htons(0x123)}, 4));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error write_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_multiple_coils(&CLIENT, 0, 1, bf) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.write_multiple_coils = write_coils;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_write_multiple_coils(&CLIENT, 1, 0, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 0x07B0");
    expect(nmbs_write_multiple_coils(&CLIENT, 1, 0x07B1, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_write_multiple_coils(&CLIENT, 0xFFFF, 2, bf) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0), htons(0x0100)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2000), htons(0x0100)}, 6));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(0xFFFF), htons(2), htons(0x0100)}, 6));
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
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(7), htons(1), htons(0x0100)}, 6));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("ignore response when sending to broadcast address");
        expect(nmbs_write_multiple_coils(&CLIENT, 2, 2, bf) == NMBS_ERROR_NONE);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(1), htons(0x0100)}, 6));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(4), htons(1), htons(0x0100)}, 6));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error write_registers(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

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
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_multiple_registers(&CLIENT, 0, 1, registers) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.write_multiple_registers = write_registers;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_write_multiple_registers(&CLIENT, 1, 0, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 0x007B");
    expect(nmbs_write_multiple_registers(&CLIENT, 1, 0x007C, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_write_multiple_registers(&CLIENT, 0xFFFF, 2, registers) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity 0");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(0), htons(0x0200), htons(0)}, 7));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE from server when calling with quantity > 2000");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2000), htons(0x0200), htons(0)}, 7));
    expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS from server when calling with address + quantity > 0xFFFF + 1");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(0xFFFF), htons(2), htons(0x0200), htons(0)},
                            7));
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
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(7), htons(1), htons(0x0200), htons(0)}, 7));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("ignore response when sending to broadcast address");
        expect(nmbs_write_multiple_registers(&CLIENT, 2, 2, registers) == NMBS_ERROR_NONE);

        should("receive no response when sending invalid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(1), htons(2000), htons(0x0200), htons(0)},
                                7));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending valid request to broadcast address");
        check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]) {htons(7), htons(1), htons(0x0200), htons(0)}, 7));
        expect(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 2) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error read_file(uint16_t file_number, uint16_t record_number, uint16_t* registers, uint16_t count, uint8_t unit_id,
                     void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (file_number == 1)
        return -1;

    if (file_number == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (file_number == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (file_number == 4 && record_number == 4 && count == 4) {
        registers[0] = 0;
        registers[1] = 0xFF;
        registers[2] = 0xAA55;
        registers[3] = 0xFFFF;
    }

    if (file_number == 255 && record_number == 9999 && count == 124)
        registers[123] = 42;

    return NMBS_ERROR_NONE;
}


void test_fc20(nmbs_transport transport) {
    uint16_t registers[128];
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_file_record(&CLIENT, 1, 0, NULL, 0) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_file_record = read_file;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with file_number 0");
    expect(nmbs_read_file_record(&CLIENT, 0, 0, registers, 1) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with record_number > 9999");
    expect(nmbs_read_file_record(&CLIENT, 1, 10000, registers, 1) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_ERROR_INVALID_ARGUMENT when calling with count > 124");
    expect(nmbs_read_file_record(&CLIENT, 1, 0, registers, 125) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_read_file_record(&CLIENT, 1, 1, registers, 3) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_read_file_record(&CLIENT, 2, 1, registers, 3) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_read_file_record(&CLIENT, 3, 1, registers, 3) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("read with no error");
    check(nmbs_read_file_record(&CLIENT, 4, 4, registers, 4));
    expect(registers[0] == 0);
    expect(registers[1] == 0xFF);
    expect(registers[2] == 0xAA55);
    expect(registers[3] == 0xFFFF);

    check(nmbs_read_file_record(&CLIENT, 255, 9999, registers, 124));
    expect(registers[123] == 42);

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_file_record(&CLIENT, 2, 1, registers, 3) == NMBS_ERROR_TIMEOUT);
    }

    stop_client_and_server();
}


nmbs_error write_file(uint16_t file_number, uint16_t record_number, const uint16_t* registers, uint16_t count,
                      uint8_t unit_id, void* arg) {
    UNUSED_PARAM(arg);
    UNUSED_PARAM(unit_id);

    if (check_user_data(arg) != 1)
        return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE;

    if (file_number == 1)
        return -1;

    if (file_number == 2)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (file_number == 3)
        return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (file_number == 4 && record_number == 4 && count == 4) {
        expect(registers[0] == 0);
        expect(registers[1] == 0xFF);
        expect(registers[2] == 0xAA55);
        expect(registers[3] == 0xFFFF);

        return NMBS_ERROR_NONE;
    }

    if (file_number == 255 && record_number == 9999 && count == 122)
        expect(registers[121] == 42);

    return NMBS_ERROR_NONE;
}


void test_fc21(nmbs_transport transport) {
    uint16_t registers[128];
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_write_file_record(&CLIENT, 1, 0, NULL, 0) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.write_file_record = write_file;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with file_number 0");
    expect(nmbs_write_file_record(&CLIENT, 0, 0, registers, 1) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with record_number > 9999");
    expect(nmbs_write_file_record(&CLIENT, 1, 10000, registers, 1) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with count > 123");
    expect(nmbs_write_file_record(&CLIENT, 1, 0, registers, 123) == NMBS_ERROR_INVALID_ARGUMENT);

    should("return NMBS_EXCEPTION_SERVER_DEVICE_FAILURE when server handler returns any non-exception error");
    expect(nmbs_write_file_record(&CLIENT, 1, 1, registers, 1) == NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS if returned by server handler");
    expect(nmbs_write_file_record(&CLIENT, 2, 2, registers, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE if returned by server handler");
    expect(nmbs_write_file_record(&CLIENT, 3, 3, registers, 3) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("write with no error");
    registers[0] = 0;
    registers[1] = 0xFF;
    registers[2] = 0xAA55;
    registers[3] = 0xFFFF;
    check(nmbs_write_file_record(&CLIENT, 4, 4, registers, 4));

    registers[121] = 42;
    check(nmbs_write_file_record(&CLIENT, 255, 9999, registers, 122));

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("ignore response when sending to broadcast address");
        expect(nmbs_write_file_record(&CLIENT, 2, 2, registers, 2) == NMBS_ERROR_NONE);
    }

    stop_client_and_server();
}

void test_fc23(nmbs_transport transport) {
    uint16_t registers[125];
    uint16_t registers_write[125];
    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);

    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_write_registers(&CLIENT, 0, 1, registers, 0, 1, registers_write) ==
           NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_holding_registers = read_registers;
    callbacks.write_multiple_registers = write_registers;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity 0");
    expect(nmbs_read_write_registers(&CLIENT, 1, 0, registers, 1, 0, registers_write) == NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with quantity > 0x007B");
    expect(nmbs_read_write_registers(&CLIENT, 1, 0x007C, registers, 1, 0x007C, registers_write) ==
           NMBS_ERROR_INVALID_ARGUMENT);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with address + quantity > 0xFFFF + 1");
    expect(nmbs_read_write_registers(&CLIENT, 0xFFFF, 2, registers, 0xFFFF, 2, registers_write) ==
           NMBS_ERROR_INVALID_ARGUMENT);

    should("write with no error");
    registers_write[0] = 255;
    registers_write[1] = 1;
    registers_write[2] = 2;
    registers_write[3] = 3;
    check(nmbs_read_write_registers(&CLIENT, 4, 4, registers, 4, 4, registers_write));
    for (int i = 0; i < 4; i++) {
        expect(registers[i] == registers_write[i]);
    }

    /* TODO:
    should("echo request's address and value");
    check(nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t*) (uint16_t[]){htons(7), htons(1), htons(0x0200), htons(0)}, 7));
    check(nmbs_receive_raw_pdu_response(&CLIENT, raw_res, 4));

    expect(((uint16_t*) raw_res)[0] == ntohs(7));
    expect(((uint16_t*) raw_res)[1] == ntohs(1));
    */

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending to broadcast address");
        expect(nmbs_read_write_registers(&CLIENT, 2, 2, registers, 2, 2, registers_write) == NMBS_ERROR_TIMEOUT);
    }
    stop_client_and_server();
}

nmbs_error read_device_identification_map(nmbs_bitfield_256 map) {
    nmbs_bitfield_set(map, 0x00);
    nmbs_bitfield_set(map, 0x01);
    nmbs_bitfield_set(map, 0x02);
    nmbs_bitfield_set(map, 0x03);
    nmbs_bitfield_set(map, 0x04);
    nmbs_bitfield_set(map, 0x05);
    nmbs_bitfield_set(map, 0x06);
    nmbs_bitfield_set(map, 0x80);
    nmbs_bitfield_set(map, 0x91);
    nmbs_bitfield_set(map, 0xA2);
    nmbs_bitfield_set(map, 0xB3);
    return NMBS_ERROR_NONE;
}

nmbs_error read_device_identification_map_incomplete(nmbs_bitfield_256 map) {
    nmbs_bitfield_set(map, 0x00);
    nmbs_bitfield_set(map, 0x02);
    return NMBS_ERROR_NONE;
}

nmbs_error read_device_identification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]) {
    switch (object_id) {
        case 0x00:
            strcpy(buffer, "VendorName");
            break;
        case 0x01:
            strcpy(buffer, "ProductCode");
            break;
        case 0x02:
            strcpy(buffer, "MajorMinorRevision");
            break;
        case 0x03:
            strncpy(buffer,
                    "VendorUrl90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusiz"
                    "e0123456",
                    NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x04:
            strncpy(buffer,
                    "ProductName90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdus"
                    "ize0123456",
                    NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x05:
            strncpy(buffer,
                    "ModelName90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusiz"
                    "e0123456",
                    NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x06:
            strcpy(buffer, "UserApplicationName");
            break;
        case 0x80:
        case 0x91:
        case 0xA2:
        case 0xB3:
            strncpy(buffer,
                    "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456",
                    NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        default:
            return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    return NMBS_ERROR_NONE;
}

void test_fc43_14(nmbs_transport transport) {
    const uint8_t fc = 43;
    const uint8_t mei = 14;
    const uint8_t buf_size = 128;

    char mem[7 * buf_size];
    char* buffers[7];
    for (int i = 0; i < 7; i++) {
        buffers[i] = &mem[i * buf_size];
    }

    nmbs_callbacks callbacks_empty;
    nmbs_callbacks_create(&callbacks_empty);
    start_client_and_server(transport, &callbacks_empty);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION when callback is not registered server-side");
    expect(nmbs_read_device_identification(&CLIENT, 0x00, buffers[0], buf_size) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    stop_client_and_server();

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    callbacks.read_device_identification = read_device_identification;
    callbacks.read_device_identification_map = read_device_identification_map;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when calling with an invalid Object Id");
    expect(nmbs_read_device_identification(&CLIENT, 0x07, buffers[0], buf_size) == NMBS_ERROR_INVALID_ARGUMENT);

    stop_client_and_server();

    nmbs_callbacks_create(&callbacks);
    callbacks.read_device_identification = read_device_identification;
    callbacks.read_device_identification_map = read_device_identification_map_incomplete;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("return NMBS_ERROR_SERVER_DEVICE_FAILURE when not exposing Basic object IDs");
    expect(nmbs_read_device_identification_basic(&CLIENT, buffers[0], buffers[1], buffers[2], buf_size) ==
           NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);

    stop_client_and_server();

    callbacks.read_device_identification = read_device_identification;
    callbacks.read_device_identification_map = read_device_identification_map;
    start_client_and_server(transport, &callbacks);
    nmbs_set_callbacks_arg(&SERVER, (void*) &callbacks_user_data);

    should("return NMBS_EXCEPTION_ILLEGAL_FUNCTION with wrong MEI type");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {69, 1, 0}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_FUNCTION);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE with wrong Read Device ID code");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 0, 0}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_VALUE with wrong Read Device ID code");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 5, 0}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS with reserved Object ID");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 1, 0x07}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS with reserved Object ID");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 4, 0x07}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS with out of range Object ID");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 1, 0x03}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS with out of range Object ID");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 2, 0x01}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS with out of range Object ID");
    nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 3, 0x02}, 3);
    expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    should("read basic object ids with no error");
    check(nmbs_read_device_identification_basic(&CLIENT, buffers[0], buffers[1], buffers[2], buf_size));
    expect(strcmp(buffers[0], "VendorName") == 0);
    expect(strcmp(buffers[1], "ProductCode") == 0);
    expect(strcmp(buffers[2], "MajorMinorRevision") == 0);

    should("read regular object ids with no error");
    check(nmbs_read_device_identification_regular(&CLIENT, buffers[0], buffers[1], buffers[2], buffers[3], buf_size));
    expect(strcmp(buffers[0], "VendorUrl90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdu"
                              "size0123456") == 0);
    expect(strcmp(buffers[1], "ProductName90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthep"
                              "dusize0123456") == 0);
    expect(strcmp(buffers[2], "ModelName90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdu"
                              "size0123456") == 0);
    expect(strcmp(buffers[3], "UserApplicationName") == 0);

    should("immediately return NMBS_ERROR_INVALID_ARGUMENT when buffers_count is smaller that retrieved object ids");
    uint8_t object_id = 0x80;
    uint8_t objects_count = 0;
    uint8_t ids[7];
    expect(nmbs_read_device_identification_extended(&CLIENT, object_id, ids, buffers, 1, buf_size, &objects_count) ==
           NMBS_ERROR_INVALID_ARGUMENT);

    should("read extended object ids with no error");
    check(nmbs_read_device_identification_extended(&CLIENT, object_id, ids, buffers, 7, buf_size, &objects_count));
    expect(strcmp(buffers[0],
                  "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456") == 0);
    expect(strcmp(buffers[1],
                  "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456") == 0);
    expect(strcmp(buffers[2],
                  "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456") == 0);
    expect(strcmp(buffers[3],
                  "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456") == 0);

    if (transport == NMBS_TRANSPORT_RTU) {
        nmbs_set_destination_rtu_address(&CLIENT, NMBS_BROADCAST_ADDRESS);

        should("receive no response when sending valid request to broadcast address");
        expect(nmbs_read_device_identification_extended(&CLIENT, object_id, ids, buffers, 7, buf_size,
                                                        &objects_count) == NMBS_ERROR_TIMEOUT);

        should("receive no response when sending invalid request to broadcast address");
        nmbs_send_raw_pdu(&CLIENT, fc, (uint8_t[]) {mei, 3, 0x02}, 3);
        expect(nmbs_receive_raw_pdu_response(&CLIENT, NULL, 2) == NMBS_ERROR_TIMEOUT);
    }

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

int main(int argc, char* argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

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

    for_transports(test_fc20, "send and receive FC 20 (0x14) Read File Record");

    for_transports(test_fc21, "send and receive FC 21 (0x15) Write File Record");

    for_transports(test_fc23, "send and receive FC 23 (0x17) Read/Write Multiple Registers");

    for_transports(test_fc43_14, "send and receive FC 43 / 14 (0x2B / 0x0E) Read Device Identification");

    return 0;
}

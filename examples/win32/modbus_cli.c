/*
 * This example application uses the win32 API to read a single modbus
 * register from a server. 
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#include "..\..\nanomodbus.h"
#include "comm.h"

#define NMBS_DEBUG 1
#define RTU_SERVER_ADDRESS 1

HANDLE hComm;
int reg_to_read;
int commPort_In;

void parseCmdLine(int argc, char** argv) {

    if (argc > 1) {
        commPort_In = atoi(argv[1]);
        reg_to_read = atoi(argv[2]);
    }

    if (reg_to_read == 0 || commPort_In == 0) {
        printf("please specify both a comm port and a register to read\n");
        exit(0);
    }
}

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    return ReadCommPort(hComm, buf, count, byte_timeout_ms);
}

int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    return WriteToCommPort(hComm, buf, count);
}

void onError(nmbs_error err) {
    printf("error: %d\n", err);
    exit(0);
}

void ReadRegister(uint16_t reg) {

    nmbs_platform_conf platform_conf;
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;

    nmbs_t nmbs;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE)
        onError(err);

    nmbs_set_read_timeout(&nmbs, 1000);
    nmbs_set_byte_timeout(&nmbs, 100);

    nmbs_set_destination_rtu_address(&nmbs, RTU_SERVER_ADDRESS);

    uint16_t r_regs[2];
    err = nmbs_read_holding_registers(&nmbs, reg, 1, r_regs);
    if (err != NMBS_ERROR_NONE)
        onError(err);

    printf("register %d is set to: %d\n", reg, r_regs[0]);
}

int main(int argc, char** argv) {

    printf("modbus_cli - CLI to read modbus registers\n");
    printf("Usage: modbus_cli comport register\n\n");

    parseCmdLine(argc, argv);

    if (!InitCommPort(&hComm, commPort_In)) {
        printf("error opening output comm port %d\n", commPort_In);
        exit(0);
    }

    ReadRegister(reg_to_read);
}
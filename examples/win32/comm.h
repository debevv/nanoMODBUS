#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

// function prototypes
bool SetLocalBaudRate(HANDLE hComm, DWORD baudrate);
bool InitCommPort(HANDLE* hComm, int PortNumber);
bool CloseCommPort(HANDLE* hComm);
int32_t WriteToCommPort(HANDLE hComm, uint8_t* data_ptr, int data_length);
int32_t ReadCommPort(HANDLE hComm, uint8_t* buf, uint16_t count, int32_t byte_timeout_ms);
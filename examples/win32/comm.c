#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

UINT WriteToCommPort(HANDLE hComm, uint8_t* data_ptr, int data_length) {

    int actual_length;

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    bool Status = WriteFile(hComm,       // Handle to the Serialport
                            data_ptr,    // Data to be written to the port
                            data_length,
                            &actual_length,    // No of bytes written to the port
                            NULL);

    if (!Status)
        printf("\nWriteFile() failed with error %d.\n", GetLastError());

    if (data_length != actual_length)
        printf("\nWriteFile() failed to write all bytes.\n");

    return data_length;
}

bool SetLocalBaudRate(HANDLE hComm, DWORD baudrate) {
    bool Status;
    DCB dcb = {0};

    // Setting the Parameters for the SerialPort
    // but first grab the current settings
    dcb.DCBlength = sizeof(dcb);

    Status = GetCommState(hComm, &dcb);
    if (!Status)
        return false;

    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;             // ByteSize = 8
    dcb.StopBits = ONESTOPBIT;    // StopBits = 1
    dcb.Parity = NOPARITY;        // Parity = None

    Status = SetCommState(hComm, &dcb);

    return Status;
}

bool InitCommPort(HANDLE* hComm, int PortNumber) {
    uint8_t SerialBuffer[2048] = {0};
    COMMTIMEOUTS timeouts = {0};
    bool Status;
    char commName[50] = {0};
    int serialPort = 9;

    // special syntax needed for comm ports > 10, but syntax also works for comm ports < 10
    sprintf_s(commName, sizeof(commName), "\\\\.\\COM%d", PortNumber);

    // Open the serial com port
    *hComm = CreateFileA(commName,
                         GENERIC_READ | GENERIC_WRITE,    // Read/Write Access
                         0,                               // No Sharing, ports cant be shared
                         NULL,                            // No Security
                         OPEN_EXISTING,                   // Open existing port only
                         0,
                         NULL);    // Null for Comm Devices

    if (*hComm == INVALID_HANDLE_VALUE)
        return false;

    Status = SetLocalBaudRate(*hComm, 9600);
    if (!Status)
        return false;

    // setup the timeouts for the SerialPort
    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts

    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;

    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(*hComm, &timeouts))
        return false;

    Status = PurgeComm(*hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);    // clear the buffers before we start
    if (!Status)
        return false;

    return true;
}

bool CloseCommPort(HANDLE* hComm) {
    if (hComm != INVALID_HANDLE_VALUE)
        CloseHandle(hComm);
    else
        return false;

    return true;
}

int32_t ReadCommPort(HANDLE hComm, uint8_t* buf, uint16_t count, int32_t byte_timeout_ms) {
    int TotalBytesRead = 0;
    bool Status = false;
    bool TimedOut = false;
    ULONGLONG StartTime = 0;
    uint8_t b;
    int tmpByteCount;

    StartTime = GetTickCount64();

    do {
        // read one byte
        Status = ReadFile(hComm, &b, 1, &tmpByteCount, NULL);

        // can't read from port at all??
        if (!Status)
            return false;

        // put one byte into our buffer
        if (tmpByteCount == 1) {
            buf[TotalBytesRead++] = b;
        }

        // did we time out yet??
        if (GetTickCount64() - StartTime > byte_timeout_ms) {
            TimedOut = true;
            break;
        }

    } while (TotalBytesRead < count);

    return TotalBytesRead;
}
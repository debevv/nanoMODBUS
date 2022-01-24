# MODBUSino - A compact MODBUS RTU/TCP C library for microcontrollers

MODBUSino is a small C library that implements the Modbus protocol. It is especially useful in resource-constrained
system like microcontrollers.  
Its main features are:

- Compact size: only ~1000 lines of code
- No dynamic memory allocations
- Transports:
    - RTU
    - TCP
- Roles
    - Client
    - Server
- Function codes:
    - 01 (0x01) Read Coils
    - 02 (0x02) Read Discrete Inputs
    - 03 (0x03) Read Holding Registers
    - 04 (0x04) Read Input Registers
    - 05 (0x05) Write Single Coil
    - 06 (0x06) Write Single Register
    - 15 (0x0F) Write Multiple Coils
    - 16 (0x10) Write Multiple registers
- Platform-agnostic
    - Requires only C99 and its standard library
    - Data transport read/write function are implemented by the user
- Broadcast requests and responses

## At a glance

```C
#include "modbusino.h"
#include "my_platform_stuff.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    // Set up the TCP connection
    void* conn = my_connect_tcp(argv[1], argv[2]);
    if (!conn) {
        fprintf(stderr, "Error connecting to server\n");
        return 1;
    }

    // my_transport_read_byte, my_transport_write_byte and my_sleep are implemented by the user 
    mbsn_platform_conf platform_conf;
    platform_conf.transport = MBSN_TRANSPORT_TCP;
    platform_conf.read_byte = my_transport_read_byte;
    platform_conf.write_byte = my_transport_write_byte;
    platform_conf.sleep = my_sleep_linux;
    platform_conf.arg = conn;    // Passing our TCP connection handle to the read/write functions

    // Create the modbus client
    mbsn_t mbsn;
    mbsn_error err = mbsn_client_create(&mbsn, &platform_conf);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus client\n");
        return 1;
    }

    // Set only the response timeout. Byte timeout will be handled by the TCP connection
    mbsn_set_read_timeout(&mbsn, 1000);

    // Write 2 holding registers at address 26
    uint16_t w_regs[2] = {123, 124};
    err = mbsn_write_multiple_registers(&mbsn, 26, 2, w_regs);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error writing register at address 26 - %s", mbsn_strerror(err));
        return 1;
    }

    // Read 2 holding registers from address 26
    uint16_t r_regs[2];
    err = mbsn_read_holding_registers(&mbsn, 26, 2, r_regs);
    if (err != MBSN_ERROR_NONE) {
        fprintf(stderr, "Error reading 2 holding registers at address 26 - %s\n", mbsn_strerror(err));
        return 1;
    }
    
    // Close the TCP connection
    my_disconnect(conn);
    
    return 0;
}
```

## Installation

Just copy `modbusino.c` and `modbusino.h` inside your application codebase.

## Platform functions

MODBUSino requires the implementation of 3 platform functions, which are passed as function pointers when creating a
client/server instance.

### Transport read/write

```C
int read_byte(uint8_t* b, int32_t, void* arg)
int write_byte(uint8_t b, int32_t, void* arg)
```

These are your platform-specific methods that read/write data to/from a serial port or a TCP connection.  
Both methods should block until the requested byte is read/written.  
If your implementation uses a read/write timeout, and the timeout expires, the methods should return 0.  
Their return values should be:

- `1` in case of success
- `0` if no data is available immediately or after an internal timeout expiration
- `-1` in case of error

### Sleep

```C
void sleep(uint32_t milliseconds, void* arg)
```

This function should sleep for the specified amount of milliseconds.

### Platform functions argument

Platform function can access to arbitrary user data through their `void* arg` argument. The argument is useful, for
example, to pass to read/write function the connection they should operate on.  
Its initial value can be set on client/server instance creation and changed any time with `mbsn_set_platform_arg` API
method.

## Platform endianness

MODBUSino will attempt to detect the endianness of the platform at build time. If the automatic detection fails, you can
set manually the endianness of the platform by defining either `MBSN_BIG_ENDIAN` or `MBSN_LITTLE_ENDIAN` in your build
flags.

## Tests and examples

Tests and examples can be built and run on Linux with CMake:

```sh
mkdir build && cd build
cmake ..
make
```

## Misc

- To reduce code size, you can define `MBSN_STRERROR_DISABLED` to disable the code that converts `mbsn_error`s to
  strings
- Debug prints about received and sent messages can be enabled by defining `MBSN_DEBUG`

## API reference

API reference is available in the repository [wiki]()

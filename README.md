# nanoMODBUS - A compact MODBUS RTU/TCP C library for embedded/microcontrollers

**If you found this library useful, buy me a coffee on**
[<img src='https://storage.ko-fi.com/cdn/brandasset/logo_white_stroke.png' width='80'>](https://ko-fi.com/B0B2LK779)

nanoMODBUS is a small C library that implements the Modbus protocol. It is especially useful in embedded and
resource-constrained systems like microcontrollers.  
Its main features are:

- Compact size
    - Only ~2000 lines of code
    - Client and server code can be disabled, if not needed
- No dynamic memory allocations
- Transports:
    - RTU
    - TCP
- Roles:
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
    - 20 (0x14) Read File Record
    - 21 (0x15) Write File Record
    - 23 (0x17) Read/Write Multiple registers
    - 43/14 (0x2B/0x0E) Read Device Identification
- Platform-agnostic
    - Requires only C99 and its standard library
    - Data transport read/write functions are implemented by the user
- User-definable CRC function for better performance
- Broadcast requests and responses

## At a glance

```C
#include <stdio.h>

#include "nanomodbus.h"
#include "my_platform_stuff.h"

int main(int argc, char* argv[]) {
    // Set up the TCP connection
    void* conn = my_connect_tcp(argv[1], argv[2]);
    if (!conn) {
        fprintf(stderr, "Error connecting to server\n");
        return 1;
    }

    // my_transport_read() and my_transport_write() are implemented by the user 
    nmbs_platform_conf platform_conf;
    nmbs_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_TCP;
    platform_conf.read = my_transport_read;
    platform_conf.write = my_transport_write;
    platform_conf.arg = conn;    // Passing our TCP connection handle to the read/write functions

    // Create the modbus client
    nmbs_t nmbs;
    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error creating modbus client\n");
        return 1;
    }

    // Set only the response timeout. Byte timeout will be handled by the TCP connection
    nmbs_set_read_timeout(&nmbs, 1000);

    // Write 2 holding registers at address 26
    uint16_t w_regs[2] = {123, 124};
    err = nmbs_write_multiple_registers(&nmbs, 26, 2, w_regs);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error writing register at address 26 - %s", nmbs_strerror(err));
        return 1;
    }

    // Read 2 holding registers from address 26
    uint16_t r_regs[2];
    err = nmbs_read_holding_registers(&nmbs, 26, 2, r_regs);
    if (err != NMBS_ERROR_NONE) {
        fprintf(stderr, "Error reading 2 holding registers at address 26 - %s\n", nmbs_strerror(err));
        return 1;
    }
    
    // Close the TCP connection
    my_disconnect(conn);
    
    return 0;
}
```

## Installation

### Manual

Just copy `nanomodbus.c` and `nanomodbus.h` inside your application codebase.

### CMake project

nanomodbus supports library linking by using CMake.

```cmake
FetchContent_Declare(
        nanomodbus
        GIT_REPOSITORY https://github.com/debevv/nanoMODBUS
        GIT_TAG master # or the version you want
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(nanomodbus)

#...

add_executable(your_program source_codes)
target_link_libraries(your_program nanomodbus)
```

## API reference

API reference is available in the repository's [GitHub Pages](https://debevv.github.io/nanoMODBUS/nanomodbus_8h.html).

## Platform functions

nanoMODBUS requires the implementation of 2 platform-specific functions, defined as function pointers when creating a
client/server instance.

### Transport read/write

```C
int32_t read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
int32_t write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
```

These are your platform-specific functions that read/write data to/from a serial port or a TCP connection.  
Both methods should block until either:

- `count` bytes of data are read/written
- the byte timeout, with `byte_timeout_ms >= 0`, expires

A value `< 0` for `byte_timeout_ms` means infinite timeout.  
With a value `== 0` for `byte_timeout_ms`, the method should read/write once in a non-blocking fashion and return
immediately.

Their return value should be the number of bytes actually read/written, or `< 0` in case of error.  
A return value between `0` and `count - 1` will be treated as if a timeout occurred on the transport side. All other
values will be treated as transport errors.

### Callbacks and platform functions arguments

Server callbacks and platform functions can access arbitrary user data through their `void* arg` argument. The argument
is useful, for example, to pass the connection a function should operate on.  
Their initial values can be set via the `nmbs_set_callbacks_arg` and `nmbs_set_platform_arg` API methods.

## Tests and examples

Tests and examples can be built and run on Linux with CMake:

```sh
mkdir build && cd build
cmake ..
make
```

Please refer to `examples/arduino/README.md` for more info about building and running Arduino examples.

## Misc

- To reduce code size, you can define the following `#define`s:
    - `NMBS_CLIENT_DISABLED` to disable all client code
    - `NMBS_SERVER_DISABLED` to disable all server code
    - To disable individual server callbacks, define the following:
        - `NMBS_SERVER_READ_COILS_DISABLED`
        - `NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED`
        - `NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED`
        - `NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED`
        - `NMBS_SERVER_WRITE_SINGLE_COIL_DISABLED`
        - `NMBS_SERVER_WRITE_SINGLE_REGISTER_DISABLED`
        - `NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED`
        - `NMBS_SERVER_WRITE_MULTIPLE_REGISTERS_DISABLED`
        - `NMBS_SERVER_READ_FILE_RECORD_DISABLED`
        - `NMBS_SERVER_WRITE_FILE_RECORD_DISABLED`
        - `NMBS_SERVER_READ_WRITE_REGISTERS_DISABLED`
        - `NMBS_SERVER_READ_DEVICE_IDENTIFICATION_DISABLED`
    - `NMBS_STRERROR_DISABLED` to disable the code that converts `nmbs_error`s to strings
    - `NMBS_BITFIELD_MAX` to set the size of the `nmbs_bitfield` type, used to store coil values (default is `2000`)
- Debug prints about received and sent messages can be enabled by defining `NMBS_DEBUG`

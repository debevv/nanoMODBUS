/*
    nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers

    MIT License

    Copyright (c) 2022 Valerio De Benedetto (@debevv)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/


/** @file */

/*! \mainpage nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers
 * nanoMODBUS is a small C library that implements the Modbus protocol. It is especially useful in resource-constrained
 * system like microcontrollers.
 *
 * GtiHub: <a href="https://github.com/debevv/nanoMODBUS">https://github.com/debevv/nanoMODBUS</a>
 *
 * API reference: \link nanomodbus.h \endlink
 *
 */

#ifndef NANOMODBUS_H
#define NANOMODBUS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * nanoMODBUS errors.
 * Values <= 0 are library errors, > 0 are modbus exceptions.
 */
typedef enum nmbs_error {
    // Library errors
    NMBS_ERROR_INVALID_UNIT_ID = -7,  /**< Received invalid unit ID in response from server */
    NMBS_ERROR_INVALID_TCP_MBAP = -6, /**< Received invalid TCP MBAP */
    NMBS_ERROR_CRC = -5,              /**< Received invalid CRC */
    NMBS_ERROR_TRANSPORT = -4,        /**< Transport error */
    NMBS_ERROR_TIMEOUT = -3,          /**< Read/write timeout occurred */
    NMBS_ERROR_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    NMBS_ERROR_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    NMBS_ERROR_NONE = 0,              /**< No error */

    // Modbus exceptions
    NMBS_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
    NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
    NMBS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    NMBS_EXCEPTION_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} nmbs_error;

/**
 * Return whether the nmbs_error is a modbus exception
 * @e nmbs_error to check
 */
#define nmbs_error_is_exception(e) ((e) > 0 && (e) < 5)


/**
 * Bitfield consisting of 2000 coils/discrete inputs
 */
typedef uint8_t nmbs_bitfield[250];

/**
 * Read a bit from the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_read(bf, b) ((bool) ((bf)[(b) / 8] & (0x1 << ((b) % 8))))

/**
 * Write value v to the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_write(bf, b, v)                                                                                  \
    (((bf)[(b) / 8]) = ((v) ? (((bf)[(b) / 8]) | (0x1 << ((b) % 8))) : (((bf)[(b) / 8]) & ~(0x1 << ((b) % 8)))))

/**
 * Reset (zero) the whole bitfield
 */
#define nmbs_bitfield_reset(bf) memset(bf, 0, sizeof(nmbs_bitfield))


/**
 * Modbus transport type.
 */
typedef enum nmbs_transport {
    NMBS_TRANSPORT_RTU = 1,
    NMBS_TRANSPORT_TCP = 2,
} nmbs_transport;


/**
 * nanoMODBUS platform configuration struct.
 * Passed to nmbs_server_create() and nmbs_client_create().
 *
 * read() and write() are the platform-specific methods that read/write data to/from a serial port or a TCP connection.
 *
 * Both methods should block until either:
 * - `count` bytes of data are read/written
 * - the byte timeout, with `byte_timeout_ms >= 0`, expires
 *
 * A value `< 0` for `byte_timeout_ms` means no timeout.
 *
 * Their return value should be the number of bytes actually read/written, or `< 0` in case of error.
 * A return value between `0` and `count - 1` will be treated as if a timeout occurred on the transport side. All other
 * values will be treated as transport errors.
 *
 * These methods accept a pointer to arbitrary user-data, which is the arg member of this struct.
 * After the creation of an instance it can be changed with nmbs_set_platform_arg().
 */
typedef struct nmbs_platform_conf {
    nmbs_transport transport; /*!< Transport type */
    int32_t (*read)(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms,
                    void* arg); /*!< Bytes read transport function pointer */
    int32_t (*write)(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms,
                     void* arg); /*!< Bytes write transport function pointer */
    void* arg;                   /*!< User data, will be passed to functions above */
} nmbs_platform_conf;


/**
 * Modbus server request callbacks. Passed to nmbs_server_create().
 *
 * These methods accept a pointer to arbitrary user data, which is the arg member of the nmbs_platform_conf that was passed
 * to nmbs_server_create together with this struct.
 *
 * `unit_id` is the RTU unit ID of the request sender. It is always 0 on TCP.
 */
typedef struct nmbs_callbacks {
#ifndef NMBS_SERVER_DISABLED
#ifndef NMBS_SERVER_READ_COILS_DISABLED
    nmbs_error (*read_coils)(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg);
#endif

#ifndef NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED
    nmbs_error (*read_discrete_inputs)(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id,
                                       void* arg);
#endif

#ifndef NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED
    nmbs_error (*read_holding_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id,
                                         void* arg);
#endif

#ifndef NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED
    nmbs_error (*read_input_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id,
                                       void* arg);
#endif

#ifndef NMBS_SERVER_WRITE_SINGLE_COIL_DISABLED
    nmbs_error (*write_single_coil)(uint16_t address, bool value, uint8_t unit_id, void* arg);
#endif

#ifndef NMBS_SERVER_WRITE_SINGLE_REGISTER_DISABLED
    nmbs_error (*write_single_register)(uint16_t address, uint16_t value, uint8_t unit_id, void* arg);
#endif

#ifndef NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED
    nmbs_error (*write_multiple_coils)(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id,
                                       void* arg);
#endif

#ifndef NMBS_SERVER_WRITE_MULTIPLE_REGISTERS_DISABLED
    nmbs_error (*write_multiple_registers)(uint16_t address, uint16_t quantity, const uint16_t* registers,
                                           uint8_t unit_id, void* arg);
#endif

#ifndef NMBS_SERVER_READ_FILE_RECORD_DISABLED
    nmbs_error (*read_file_record)(uint16_t file_number, uint16_t record_number, uint16_t* registers, uint16_t count,
                                   uint8_t unit_id, void* arg);
#endif

#ifndef NMBS_SERVER_WRITE_FILE_RECORD_DISABLED
    nmbs_error (*write_file_record)(uint16_t file_number, uint16_t record_number, const uint16_t* registers,
                                    uint16_t count, uint8_t unit_id, void* arg);
#endif
#endif

    void* arg;    // User data, will be passed to functions above
} nmbs_callbacks;


/**
 * nanoMODBUS client/server instance type. All struct members are to be considered private, it is not advisable to read/write them directly.
 */
typedef struct nmbs_t {
    struct {
        uint8_t buf[260];
        uint16_t buf_idx;

        uint8_t unit_id;
        uint8_t fc;
        uint16_t transaction_id;
        bool broadcast;
        bool ignored;
    } msg;

    nmbs_callbacks callbacks;

    int32_t byte_timeout_ms;
    int32_t read_timeout_ms;

    nmbs_platform_conf platform;

    uint8_t address_rtu;
    uint8_t dest_address_rtu;
    uint16_t current_tid;
} nmbs_t;

/**
 * Modbus broadcast address. Can be passed to nmbs_set_destination_rtu_address().
 */
static const uint8_t NMBS_BROADCAST_ADDRESS = 0;

/** Set the request/response timeout.
 * If the target instance is a server, sets the timeout of the nmbs_server_poll() function.
 * If the target instance is a client, sets the response timeout after sending a request. In case of timeout,
 * the called method will return NMBS_ERROR_TIMEOUT.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_read_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Set the timeout between the reception/transmission of two consecutive bytes.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_byte_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Set the pointer to user data argument passed to platform functions.
 * @param nmbs pointer to the nmbs_t instance
 * @param arg user data argument
 */
void nmbs_set_platform_arg(nmbs_t* nmbs, void* arg);

#ifndef NMBS_SERVER_DISABLED
/** Create a new Modbus server.
 * @param nmbs pointer to the nmbs_t instance where the client will be created.
 * @param address_rtu RTU address of this server. Can be 0 if transport is not RTU.
 * @param platform_conf nmbs_platform_conf struct with platform configuration. It may be discarded after calling this method.
 * @param callbacks nmbs_callbacks struct with server request callbacks. It may be discarded after calling this method.
 *
 * @return NMBS_ERROR_NONE if successful, NMBS_ERROR_INVALID_ARGUMENT otherwise.
 */
nmbs_error nmbs_server_create(nmbs_t* nmbs, uint8_t address_rtu, const nmbs_platform_conf* platform_conf,
                              const nmbs_callbacks* callbacks);

/** Handle incoming requests to the server.
 * This function should be called in a loop in order to serve any incoming request. Its maximum duration, in case of no
 * received request, is the value set with nmbs_set_read_timeout() (unless set to < 0).
 * @param nmbs pointer to the nmbs_t instance
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_server_poll(nmbs_t* nmbs);

/** Set the pointer to user data argument passed to server request callbacks.
 * @param nmbs pointer to the nmbs_t instance
 * @param arg user data argument
 */
void nmbs_set_callbacks_arg(nmbs_t* nmbs, void* arg);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Create a new Modbus client.
 * @param nmbs pointer to the nmbs_t instance where the client will be created.
 * @param platform_conf nmbs_platform_conf struct with platform configuration. It may be discarded after calling this method.
 *
* @return NMBS_ERROR_NONE if successful, NMBS_ERROR_INVALID_ARGUMENT otherwise.
 */
nmbs_error nmbs_client_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf);

/** Set the recipient server address of the next request on RTU transport.
 * @param nmbs pointer to the nmbs_t instance
 * @param address server address
 */
void nmbs_set_destination_rtu_address(nmbs_t* nmbs, uint8_t address);

/** Send a FC 01 (0x01) Read Coils request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils_out nmbs_bitfield where the coils will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield coils_out);

/** Send a FC 02 (0x02) Read Discrete Inputs request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of inputs
 * @param inputs_out nmbs_bitfield where the discrete inputs will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_discrete_inputs(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out);

/** Send a FC 03 (0x03) Read Holding Registers request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_holding_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/** Send a FC 04 (0x04) Read Input Registers request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_input_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/** Send a FC 05 (0x05) Write Single Coil request
 * @param nmbs pointer to the nmbs_t instance
 * @param address coil address
 * @param value coil value
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_single_coil(nmbs_t* nmbs, uint16_t address, bool value);

/** Send a FC 06 (0x06) Write Single Register request
 * @param nmbs pointer to the nmbs_t instance
 * @param address register address
 * @param value register value
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_single_register(nmbs_t* nmbs, uint16_t address, uint16_t value);

/** Send a FC 15 (0x0F) Write Multiple Coils
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils bitfield of coils values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_multiple_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const nmbs_bitfield coils);

/** Send a FC 16 (0x10) Write Multiple Registers
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers array of registers values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_multiple_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const uint16_t* registers);

/** Send a FC 20 (0x14) Read File Record
 * @param nmbs pointer to the nmbs_t instance
 * @param file_number file number (1 to 65535)
 * @param record_number record number from file (0000 to 9999)
 * @param registers array of registers to read
 * @param count count of registers
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_file_record(nmbs_t* nmbs, uint16_t file_number, uint16_t record_number, uint16_t* registers,
                                 uint16_t count);

/** Send a FC 21 (0x15) Write File Record
 * @param nmbs pointer to the nmbs_t instance
 * @param file_number file number (1 to 65535)
 * @param record_number record number from file (0000 to 9999)
 * @param registers array of registers to write
 * @param count count of registers
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_file_record(nmbs_t* nmbs, uint16_t file_number, uint16_t record_number, const uint16_t* registers,
                                  uint16_t count);

/** Send a FC 23 (0x17) Read Write Multiple registers
 * @param nmbs pointer to the nmbs_t instance
 * @param read_address starting read address
 * @param read_quantity quantity of registers to read
 * @param registers_out array where the read registers will be stored
 * @param write_address starting write address
 * @param write_quantity quantity of registers to write
 * @param registers array of registers values to write
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_write_registers(nmbs_t* nmbs, uint16_t read_address, uint16_t read_quantity,
                                     uint16_t* registers_out, uint16_t write_address, uint16_t write_quantity,
                                     const uint16_t* registers);

/** Send a raw Modbus PDU.
 * CRC on RTU will be calculated and sent by this function.
 * @param nmbs pointer to the nmbs_t instance
 * @param fc request function code
 * @param data request data. It's up to the caller to convert this data to network byte order
 * @param data_len length of the data parameter
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_send_raw_pdu(nmbs_t* nmbs, uint8_t fc, const uint8_t* data, uint16_t data_len);

/** Receive a raw response Modbus PDU.
 * @param nmbs pointer to the nmbs_t instance
 * @param data_out response data. It's up to the caller to convert this data to host byte order.
 * @param data_out_len length of the data_out parameter
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_receive_raw_pdu_response(nmbs_t* nmbs, uint8_t* data_out, uint16_t data_out_len);
#endif

/** Calculate the Modbus CRC of some data.
 * @param data Data
 * @param length Length of the data
 */
uint16_t nmbs_crc_calc(const uint8_t* data, uint32_t length);

#ifndef NMBS_STRERROR_DISABLED
/** Convert a nmbs_error to string
 * @param error error to be converted
 *
 * @return string representation of the error
 */
const char* nmbs_strerror(nmbs_error error);
#endif

#ifdef __cplusplus
}    // extern "C"
#endif

#endif    //NANOMODBUS_H

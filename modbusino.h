/** @file */

/*! \mainpage MODBUSino - A compact MODBUS RTU/TCP C library for microcontrollers
 * MODBUSino is a small C library that implements the Modbus protocol. It is especially useful in resource-constrained
 * system like microcontrollers.
 *
 * GtiHub: <a href="https://github.com/debevv/MODBUSino">https://github.com/debevv/MODBUSino</a>
 *
 * API reference: \link modbusino.h \endlink
 *
 */

#ifndef MODBUSINO_H
#define MODBUSINO_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * MODBUSino errors.
 * Values <= 0 are library errors, > 0 are modbus exceptions.
 */
typedef enum mbsn_error {
    // Library errors
    MBSN_ERROR_TRANSPORT = -4,        /**< Transport error */
    MBSN_ERROR_TIMEOUT = -3,          /**< Read/write timeout occurred */
    MBSN_ERROR_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    MBSN_ERROR_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    MBSN_ERROR_NONE = 0,              /**< No error */

    // Modbus exceptions
    MBSN_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
    MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
    MBSN_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    MBSN_EXCEPTION_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} mbsn_error;

/**
 * Return whether the mbsn_error is a modbus exception
 * @e mbsn_error to check
 */
#define mbsn_error_is_exception(e) ((e) > 0 && (e) < 5)


/**
 * Bitfield consisting of 2000 coils/discrete inputs
 */
typedef uint8_t mbsn_bitfield[250];

/**
 * Read a bit from the mbsn_bitfield bf at position b
 */
#define mbsn_bitfield_read(bf, b) ((bool) ((bf)[(b) / 8] & (0x1 << ((b) % 8))))

/**
 * Write value v to the mbsn_bitfield bf at position b
 */
#define mbsn_bitfield_write(bf, b, v)                                                                                  \
    (((bf)[(b) / 8]) = ((v) ? (((bf)[(b) / 8]) | (0x1 << ((b) % 8))) : (((bf)[(b) / 8]) & ~(0x1 << ((b) % 8)))))

/**
 * Reset (zero) the whole bitfield
 */
#define mbsn_bitfield_reset(bf) memset(bf, 0, sizeof(mbsn_bitfield))


/**
 * Modbus transport type.
 */
typedef enum mbsn_transport {
    MBSN_TRANSPORT_RTU = 1,
    MBSN_TRANSPORT_TCP = 2,
} mbsn_transport;


/**
 * MODBUSino platform configuration struct.
 * Passed to mbsn_server_create() and mbsn_client_create().
 *
 * read_byte() and write_byte() are the platform-specific methods that read/write data to/from a serial port or a TCP connection.
 * Both methods should block until the requested byte is read/written.
 * If your implementation uses a read/write timeout, and the timeout expires, the methods should return 0.
 * Their return values should be:
 * - `1` in case of success
 * - `0` if no data is available immediately or after an internal timeout expiration
 * - `-1` in case of error
 *
 * sleep() is the platform-specific method to pause for a certain amount of milliseconds.
 *
 * These methods accept a pointer to arbitrary user-data, which is the arg member of this struct.
 * After the creation of an instance it can be changed with mbsn_set_platform_arg().
 */
typedef struct mbsn_platform_conf {
    mbsn_transport transport;                         /*!< Transport type */
    int (*read_byte)(uint8_t* b, int32_t, void* arg); /*!< Byte read transport function pointer */
    int (*write_byte)(uint8_t b, int32_t, void* arg); /*!< Byte write transport function pointer */
    void (*sleep)(uint32_t milliseconds, void* arg);  /*!< Sleep function pointer */
    void* arg;                                        /*!< User data, will be passed to functions above */
} mbsn_platform_conf;


/**
 * Modbus server request callbacks. Passed to mbsn_server_create().
 */
typedef struct mbsn_callbacks {
    mbsn_error (*read_coils)(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out);
    mbsn_error (*read_discrete_inputs)(uint16_t address, uint16_t quantity, mbsn_bitfield inputs_out);
    mbsn_error (*read_holding_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    mbsn_error (*read_input_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    mbsn_error (*write_single_coil)(uint16_t address, bool value);
    mbsn_error (*write_single_register)(uint16_t address, uint16_t value);
    mbsn_error (*write_multiple_coils)(uint16_t address, uint16_t quantity, const mbsn_bitfield coils);
    mbsn_error (*write_multiple_registers)(uint16_t address, uint16_t quantity, const uint16_t* registers);
} mbsn_callbacks;


/**
 * MODBUSino client/server instance type. All struct members are to be considered private, it is not advisable to read/write them directly.
 */
typedef struct mbsn_t {
    struct {
        uint8_t buf[260];
        uint16_t buf_idx;

        uint8_t unit_id;
        uint8_t fc;
        uint16_t transaction_id;
        bool broadcast;
        bool ignored;
    } msg;

    mbsn_callbacks callbacks;

    int32_t byte_timeout_ms;
    int32_t read_timeout_ms;
    uint32_t byte_spacing_ms;

    mbsn_platform_conf platform;

    uint8_t address_rtu;
    uint8_t dest_address_rtu;
    uint16_t current_tid;
} mbsn_t;

/**
 * Modbus broadcast address. Can be passed to mbsn_set_destination_rtu_address().
 */
static const uint8_t MBSN_BROADCAST_ADDRESS = 0;


/** Create a new Modbus client.
 * @param mbsn pointer to the mbsn_t instance where the client will be created.
 * @param platform_conf mbsn_platform_conf struct with platform configuration.
 *
* @return MBSN_ERROR_NONE if successful, MBSN_ERROR_INVALID_ARGUMENT otherwise.
 */
mbsn_error mbsn_client_create(mbsn_t* mbsn, const mbsn_platform_conf* platform_conf);

/** Create a new Modbus server.
 * @param mbsn pointer to the mbsn_t instance where the client will be created.
 * @param address_rtu RTU address of this server. Can be 0 if transport is not RTU.
 * @param platform_conf mbsn_platform_conf struct with platform configuration.
 * @param callbacks mbsn_callbacks struct with server request callbacks.
 *
 * @return MBSN_ERROR_NONE if successful, MBSN_ERROR_INVALID_ARGUMENT otherwise.
 */
mbsn_error mbsn_server_create(mbsn_t* mbsn, uint8_t address_rtu, const mbsn_platform_conf* platform_conf,
                              const mbsn_callbacks* callbacks);

/** Set the request/response timeout.
 * If the target instance is a server, sets the timeout of the mbsn_server_poll() function.
 * If the target instance is a client, sets the response timeout after sending a request. In case of timeout, the called method will return MBSN_ERROR_TIMEOUT.
 * @param mbsn pointer to the mbsn_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void mbsn_set_read_timeout(mbsn_t* mbsn, int32_t timeout_ms);

/** Set the timeout between the reception of two consecutive bytes.
 * @param mbsn pointer to the mbsn_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void mbsn_set_byte_timeout(mbsn_t* mbsn, int32_t timeout_ms);

/** Set the spacing between two sent bytes. This value is ignored when transport is not RTU.
 * @param mbsn pointer to the mbsn_t instance
 * @param timeout_ms time spacing in milliseconds.
 */
void mbsn_set_byte_spacing(mbsn_t* mbsn, uint32_t spacing_ms);

/** Set the pointer to user data argument passed to platform functions.
 * @param mbsn pointer to the mbsn_t instance
 * @param arg user data argument
 */
void mbsn_set_platform_arg(mbsn_t* mbsn, void* arg);

/** Set the recipient server address of the next request on RTU transport.
 * @param mbsn pointer to the mbsn_t instance
 * @param address server address
 */
void mbsn_set_destination_rtu_address(mbsn_t* mbsn, uint8_t address);

/** Handle incoming requests to the server.
 * This function should be called in a loop in order to serve any incoming request. Its maximum duration, in case of no
 * received request, is the value set with mbsn_set_read_timeout() (unless set to < 0).
 * @param mbsn pointer to the mbsn_t instance
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_server_poll(mbsn_t* mbsn);

/** Send a FC 01 (0x01) Read Coils request
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils_out mbsn_bitfield where the coils will be stored
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_read_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield coils_out);

/** Send a FC 02 (0x02) Read Discrete Inputs request
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of inputs
 * @param inputs_out mbsn_bitfield where the discrete inputs will be stored
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_read_discrete_inputs(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield inputs_out);

/** Send a FC 03 (0x03) Read Holding Registers request
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_read_holding_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/** Send a FC 04 (0x04) Read Input Registers request
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_read_input_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out);

/** Send a FC 05 (0x05) Write Single Coil request
 * @param mbsn pointer to the mbsn_t instance
 * @param address coil address
 * @param value coil value
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_write_single_coil(mbsn_t* mbsn, uint16_t address, bool value);

/** Send a FC 06 (0x06) Write Single Register request
 * @param mbsn pointer to the mbsn_t instance
 * @param address register address
 * @param value register value
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_write_single_register(mbsn_t* mbsn, uint16_t address, uint16_t value);

/** Send a FC 15 (0x0F) Write Multiple Coils
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils bitfield of coils values
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_write_multiple_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const mbsn_bitfield coils);

/** Send a FC 16 (0x10) Write Multiple Registers
 * @param mbsn pointer to the mbsn_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers array of registers values
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_write_multiple_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const uint16_t* registers);

/** Send a raw Modbus PDU.
 * CRC on RTU will be calculated and sent by this function.
 * @param mbsn pointer to the mbsn_t instance
 * @param fc request function code
 * @param data request data. It's up to the caller to convert this data to network byte order
 * @param data_len length of the data parameter
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_send_raw_pdu(mbsn_t* mbsn, uint8_t fc, const void* data, uint32_t data_len);

/** Receive a raw response Modbus PDU.
 * @param mbsn pointer to the mbsn_t instance
 * @param data_out response data. It's up to the caller to convert this data to host byte order.
 * @param data_out_len length of the data_out parameter
 *
 * @return MBSN_ERROR_NONE if successful, other errors otherwise.
 */
mbsn_error mbsn_receive_raw_pdu_response(mbsn_t* mbsn, void* data_out, uint32_t data_out_len);

#ifndef MBSN_STRERROR_DISABLED
/** Convert a mbsn_error to string
 * @param error error to be converted
 *
 * @return string representation of the error
 */
const char* mbsn_strerror(mbsn_error error);
#endif


#endif    //MODBUSINO_H

#ifndef MODBUSINO_H
#define MODBUSINO_H

#include <stdbool.h>
#include <stdint.h>


typedef enum mbsn_error {
    // Library errors
    MBSN_ERROR_TRANSPORT = -4,
    MBSN_ERROR_TIMEOUT = -3,
    MBSN_ERROR_INVALID_RESPONSE = -2,
    MBSN_ERROR_INVALID_ARGUMENT = -1,
    MBSN_ERROR_NONE = 0,

    // Modbus exceptions
    MBSN_EXCEPTION_ILLEGAL_FUNCTION = 1,
    MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,
    MBSN_EXCEPTION_ILLEGAL_DATA_VALUE = 3,
    MBSN_EXCEPTION_SERVER_DEVICE_FAILURE = 4,
} mbsn_error;

#define mbsn_error_is_exception(e) ((e) > 0 && (e) < 5)


typedef uint8_t mbsn_bitfield[250];

#define mbsn_bitfield_read(bf, b) ((bool) ((bf)[(b) / 8] & (0x1 << ((b) % 8))))

#define mbsn_bitfield_write(bf, b, v)                                                                                  \
    (((bf)[(b) / 8]) = ((v) ? (((bf)[(b) / 8]) | (0x1 << ((b) % 8))) : (((bf)[(b) / 8]) & ~(0x1 << ((b) % 8)))))


typedef enum mbsn_transport {
    MBSN_TRANSPORT_RTU = 1,
    MBSN_TRANSPORT_TCP = 2,
} mbsn_transport;

typedef struct mbsn_platform_conf {
    mbsn_transport transport;
    int (*read_byte)(uint8_t* b, int32_t);
    int (*write_byte)(uint8_t b, int32_t);
    void (*sleep)(uint32_t milliseconds);
} mbsn_platform_conf;


typedef struct mbsn_callbacks {
    mbsn_error (*read_coils)(uint16_t address, uint16_t quantity, mbsn_bitfield coils_out);
    mbsn_error (*read_discrete_inputs)(uint16_t address, uint16_t quantity, mbsn_bitfield inputs_out);
    mbsn_error (*read_holding_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    mbsn_error (*read_input_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    mbsn_error (*write_single_coil)(uint16_t address, bool value);
    mbsn_error (*write_single_register)(uint16_t address, uint16_t value);
    mbsn_error (*write_multiple_coils)(uint16_t address, uint16_t quantity, mbsn_bitfield coils);
    mbsn_error (*write_multiple_registers)(uint16_t address, uint16_t quantity, uint16_t* registers);
} mbsn_callbacks;


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

    mbsn_platform_conf platform;

    uint8_t address_rtu;
    uint8_t dest_address_rtu;
} mbsn_t;


static const uint8_t MBSN_BROADCAST_ADDRESS = 0;


mbsn_error mbsn_client_create(mbsn_t* mbsn, const mbsn_platform_conf* platform_conf);

mbsn_error mbsn_server_create(mbsn_t* mbsn, uint8_t address, const mbsn_platform_conf* platform_conf,
                              mbsn_callbacks callbacks);

void mbsn_set_read_timeout(mbsn_t* mbsn, int32_t timeout_ms);

void mbsn_set_byte_timeout(mbsn_t* mbsn, int32_t timeout_ms);

void mbsn_set_destination_rtu_address(mbsn_t* mbsn, uint8_t address);

mbsn_error mbsn_server_receive(mbsn_t* mbsn);

mbsn_error mbsn_read_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield coils_out);

mbsn_error mbsn_read_discrete_inputs(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield inputs_out);

mbsn_error mbsn_read_holding_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out);

mbsn_error mbsn_read_input_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out);

mbsn_error mbsn_write_single_coil(mbsn_t* mbsn, uint16_t address, bool value);

mbsn_error mbsn_write_single_register(mbsn_t* mbsn, uint16_t address, uint16_t value);

mbsn_error mbsn_write_multiple_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const mbsn_bitfield coils);

mbsn_error mbsn_write_multiple_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const uint16_t* registers);

mbsn_error mbsn_send_raw_pdu(mbsn_t* mbsn, uint8_t fc, const void* data, uint32_t data_len);

mbsn_error mbsn_receive_raw_pdu_response(mbsn_t* mbsn, void* data_out, uint32_t data_out_len);

const char* mbsn_strerror(int error);


#endif    //MODBUSINO_H

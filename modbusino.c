#include "modbusino.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>


#if !defined(MBSN_BIG_ENDIAN) && !defined(MBSN_LITTLE_ENDIAN)
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || defined(__BIG_ENDIAN__) || defined(__ARMEB__) ||          \
        defined(__THUMBEB__) || defined(__AARCH64EB__) || defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#define MBSN_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) ||  \
        defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
#define MBSN_LITTLE_ENDIAN
#else
#error "Failed to automatically detect platform endianness. Please define either MBSN_BIG_ENDIAN or MBSN_LITTLE_ENDIAN"
#endif
#endif


#define get_1(m)                                                                                                       \
    (m)->msg_buf[(m)->msg_buf_idx];                                                                                    \
    (m)->msg_buf_idx++
#define put_1(m, b)                                                                                                    \
    (m)->msg_buf[(m)->msg_buf_idx] = (b);                                                                              \
    (m)->msg_buf_idx++

#ifdef MBSN_BIG_ENDIAN
#define get_2(m) ((m)->msg_buf[(m)->msg_buf[(m)->msg_buf_idx]);                                                        \
    (m)->msg_buf_idx += 2
#define put_2(m, w) (m)->msg_buf(m)[(m)->msg_buf_idx]] = (w);                                                          \
    (m)->msg_buf_idx += 2
#else
#define get_2(m)                                                                                                       \
    ((m)->msg_buf[(m)->msg_buf_idx] >> 8 | (m)->msg_buf[(m)->msg_buf_idx] << 8);                                       \
    (m)->msg_buf_idx += 2
#define put_2(m, w)                                                                                                    \
    (m)->msg_buf[(m)->msg_buf_idx] = ((w) >> 8 | (w) << 8);                                                            \
    (m)->msg_buf_idx += 2
#endif


typedef struct msg_state {
    uint8_t unit_id;
    uint8_t fc;
    uint16_t transaction_id;
    bool broadcast;
    bool ignored;
} msg_state;


static msg_state msg_state_create() {
    msg_state s;
    memset(&s, 0, sizeof(msg_state));
    return s;
}


static msg_state res_from_req(msg_state* req) {
    msg_state res = *req;
    return res;
}


static msg_state req_create(uint8_t unit_id, uint8_t fc) {
    static uint16_t TID = 0;

    msg_state req = msg_state_create();
    req.unit_id = unit_id;
    req.fc = fc;
    req.transaction_id = TID;
    if (req.unit_id == 0)
        req.broadcast = true;

    TID++;
    return req;
}


int mbsn_create(mbsn_t* mbsn, mbsn_transport_conf transport_conf) {
    if (!mbsn)
        return MBSN_ERROR_INVALID_ARGUMENT;

    memset(mbsn, 0, sizeof(mbsn_t));

    mbsn->byte_timeout_ms = -1;
    mbsn->read_timeout_ms = -1;

    mbsn->transport = transport_conf.transport;
    if (mbsn->transport != MBSN_TRANSPORT_RTU && mbsn->transport != MBSN_TRANSPORT_TCP)
        return MBSN_ERROR_INVALID_ARGUMENT;

    if (!transport_conf.read_byte || !transport_conf.write_byte)
        return MBSN_ERROR_INVALID_ARGUMENT;

    mbsn->transport_read_byte = transport_conf.read_byte;
    mbsn->transport_write_byte = transport_conf.write_byte;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_client_create(mbsn_t* mbsn, mbsn_transport_conf transport_conf) {
    return mbsn_create(mbsn, transport_conf);
}


mbsn_error mbsn_server_create(mbsn_t* mbsn, uint8_t address, mbsn_transport_conf transport_conf,
                              mbsn_callbacks callbacks) {
    if (address == 0)
        return MBSN_ERROR_INVALID_ARGUMENT;

    mbsn_error ret = mbsn_create(mbsn, transport_conf);
    if (ret != MBSN_ERROR_NONE)
        return ret;

    mbsn->address_rtu = address;
    mbsn->callbacks = callbacks;

    return MBSN_ERROR_NONE;
}


void mbsn_set_read_timeout(mbsn_t* mbsn, int32_t timeout_ms) {
    mbsn->read_timeout_ms = timeout_ms;
}


void mbsn_set_byte_timeout(mbsn_t* mbsn, int32_t timeout_ms) {
    mbsn->byte_timeout_ms = timeout_ms;
}


void mbsn_client_set_server_address_rtu(mbsn_t* mbsn, uint8_t address) {
    mbsn->server_dest_address_rtu = address;
}


static uint16_t crc_calc(const uint8_t* data, unsigned int length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= (uint16_t) data[i];
        for (int j = 8; j != 0; j--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }

    return crc;
}


static void reset_msg_buf(mbsn_t* mbsn) {
    mbsn->msg_buf_idx = 0;
}


static mbsn_error recv(mbsn_t* mbsn, uint32_t count) {
    int r = 0;
    while (r != count) {
        int ret = mbsn->transport_read_byte(mbsn->msg_buf + mbsn->msg_buf_idx + r, mbsn->byte_timeout_ms);
        if (ret == 0)
            return MBSN_ERROR_TIMEOUT;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        r++;
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error send(mbsn_t* mbsn) {
    for (int i = 0; i < mbsn->msg_buf_idx; i++) {
        int ret = mbsn->transport_write_byte(mbsn->msg_buf[i], mbsn->read_timeout_ms);
        if (ret == 0)
            return MBSN_ERROR_TIMEOUT;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error recv_msg_header(mbsn_t* mbsn, msg_state* s_out, bool* first_byte_received) {
    // We wait for the read timeout here, just for the first message byte
    int32_t old_byte_timeout = mbsn->byte_timeout_ms;
    mbsn->byte_timeout_ms = mbsn->read_timeout_ms;

    *s_out = msg_state_create();
    reset_msg_buf(mbsn);
    if (first_byte_received)
        *first_byte_received = false;

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        mbsn_error err = recv(mbsn, 1);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != MBSN_ERROR_NONE)
            return err;

        if (first_byte_received)
            *first_byte_received = true;

        uint8_t unit_id = get_1(mbsn);

        // Check if message is for us
        if (unit_id == 0)
            s_out->broadcast = true;
        else if (unit_id != mbsn->address_rtu)
            s_out->ignored = true;
        else
            s_out->ignored = false;

        err = recv(mbsn, 1);
        if (err != MBSN_ERROR_NONE)
            return err;

        s_out->fc = get_1(mbsn);
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn_error err = recv(mbsn, 1);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != MBSN_ERROR_NONE)
            return err;

        if (first_byte_received)
            *first_byte_received = true;

        err = recv(mbsn, 7);
        if (err != MBSN_ERROR_NONE)
            return err;

        reset_msg_buf(mbsn);

        uint16_t protocol_id = get_2(mbsn);
        uint16_t length = get_2(mbsn);    // We should actually check the length of the request against this value
        s_out->unit_id = get_1(mbsn);
        s_out->fc = get_1(mbsn);

        if (protocol_id != 0)
            return MBSN_ERROR_TRANSPORT;

        if (length > 255)
            return MBSN_ERROR_TRANSPORT;
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error recv_msg_footer(mbsn_t* mbsn) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        mbsn_error err = recv(mbsn, 2);
        if (err != MBSN_ERROR_NONE)
            return err;

        uint16_t recv_crc = get_2(mbsn);
        uint16_t crc = crc_calc(mbsn->msg_buf, mbsn->msg_buf_idx);
        if (recv_crc != crc)
            return MBSN_ERROR_TRANSPORT;
    }

    return MBSN_ERROR_NONE;
}


static void send_msg_header(mbsn_t* mbsn, msg_state* s, uint8_t data_length) {
    reset_msg_buf(mbsn);

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        put_1(mbsn, s->unit_id);
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        put_2(mbsn, s->transaction_id);
        put_2(mbsn, 0);
        put_2(mbsn, 1 + 1 + data_length);
        put_2(mbsn, s->unit_id);
    }

    put_1(mbsn, s->fc);
}


static mbsn_error send_msg_footer(mbsn_t* mbsn) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint16_t crc = crc_calc(mbsn->msg_buf, mbsn->msg_buf_idx);
        put_2(mbsn, crc);
    }

    mbsn_error err = send(mbsn);
    return err;
}


static mbsn_error handle_exception(mbsn_t* mbsn, msg_state* req, uint8_t exception) {
    msg_state res = *req;
    req->fc += 0x80;

    send_msg_header(mbsn, &res, 1);
    put_1(mbsn, exception);

    return send_msg_footer(mbsn);
}


static mbsn_error handle_read_discrete(mbsn_t* mbsn, msg_state* req,
                                       mbsn_error (*callback)(uint16_t, uint16_t, mbsn_bitfield)) {
    mbsn_error err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        uint16_t address = get_2(mbsn);
        uint16_t quantity = get_2(mbsn);

        if (quantity < 1 || quantity > 2000)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > 65535)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            mbsn_bitfield bf;
            err = callback(address, quantity, bf);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                uint8_t discrete_bytes = (quantity / 8) + 1;
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, discrete_bytes);

                put_1(mbsn, discrete_bytes);

                for (int i = 0; i < discrete_bytes; i++) {
                    put_1(mbsn, bf[i]);
                }

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_read_registers(mbsn_t* mbsn, msg_state* req,
                                        mbsn_error (*callback)(uint16_t, uint16_t, uint16_t*)) {
    mbsn_error err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        uint16_t address = get_2(mbsn);
        uint16_t quantity = get_2(mbsn);

        if (quantity < 1 || quantity > 125)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > 65535)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            uint16_t regs[125];
            err = callback(address, quantity, regs);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                uint8_t regs_bytes = quantity * 2;
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, regs_bytes);

                put_1(mbsn, regs_bytes);

                for (int i = 0; i < quantity; i++) {
                    put_2(mbsn, regs[i]);
                }

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_read_coils(mbsn_t* mbsn, msg_state* req) {
    return handle_read_discrete(mbsn, req, mbsn->callbacks.read_coils);
}


static mbsn_error handle_read_discrete_inputs(mbsn_t* mbsn, msg_state* req) {
    return handle_read_discrete(mbsn, req, mbsn->callbacks.read_discrete_inputs);
}


static mbsn_error handle_read_holding_registers(mbsn_t* mbsn, msg_state* req) {
    return handle_read_registers(mbsn, req, mbsn->callbacks.read_holding_registers);
}


static mbsn_error handle_read_input_registers(mbsn_t* mbsn, msg_state* req) {
    return handle_read_registers(mbsn, req, mbsn->callbacks.read_input_registers);
}


static mbsn_error handle_write_single_coil(mbsn_t* mbsn, msg_state* req) {
    mbsn_error err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        uint16_t address = get_2(mbsn);
        uint16_t value = get_2(mbsn);

        if (value != 0 && value != 0xFF00)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (mbsn->callbacks.write_single_coil) {
            err = mbsn->callbacks.write_single_coil(address, value == 0 ? false : true);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, 2);

                put_2(mbsn, address);

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_single_register(mbsn_t* mbsn, msg_state* req) {
    mbsn_error err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        uint16_t address = get_2(mbsn);
        uint16_t value = get_2(mbsn);

        if (mbsn->callbacks.write_single_register) {
            err = mbsn->callbacks.write_single_register(address, value);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, 1);

                put_2(mbsn, value);

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_multiple_coils(mbsn_t* mbsn, msg_state* req) {
    mbsn_error err = recv(mbsn, 5);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address = get_2(mbsn);
    uint16_t quantity = get_2(mbsn);
    uint8_t coils_bytes = get_1(mbsn);

    err = recv(mbsn, coils_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    mbsn_bitfield coils;
    for (int i = 0; i < coils_bytes; i++)
        coils[i] = get_1(mbsn);

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (quantity < 1 || quantity > 0x07B0)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (coils_bytes == 0)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((quantity / 8) + 1 != coils_bytes)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (mbsn->callbacks.write_multiple_coils) {
            err = mbsn->callbacks.write_multiple_coils(address, quantity, coils);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, 4);

                put_2(mbsn, address);
                put_2(mbsn, quantity);

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_multiple_registers(mbsn_t* mbsn, msg_state* req) {
    mbsn_error err = recv(mbsn, 5);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address = get_2(mbsn);
    uint16_t quantity = get_2(mbsn);
    uint8_t registers_bytes = get_1(mbsn);

    err = recv(mbsn, registers_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t registers[0x007B];
    for (int i = 0; i < quantity; i++) {
        registers[i] = get_2(mbsn);
    }

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (quantity < 1 || quantity > 0x007B)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (registers_bytes == 0)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (registers_bytes != quantity * 2)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (mbsn->callbacks.write_multiple_registers) {
            err = mbsn->callbacks.write_multiple_registers(address, quantity, registers);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                send_msg_header(mbsn, &res, 4);

                put_2(mbsn, address);
                put_2(mbsn, quantity);

                err = send_msg_footer(mbsn);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_req_fc(mbsn_t* mbsn, msg_state* req) {
    mbsn_error err;
    switch (req->fc) {
        case 1:
            err = handle_read_coils(mbsn, req);
            break;

        case 2:
            err = handle_read_discrete_inputs(mbsn, req);
            break;

        case 3:
            err = handle_read_holding_registers(mbsn, req);
            break;

        case 4:
            err = handle_read_input_registers(mbsn, req);
            break;

        case 5:
            err = handle_write_single_coil(mbsn, req);
            break;

        case 6:
            err = handle_write_single_register(mbsn, req);
            break;

        case 15:
            err = handle_write_multiple_coils(mbsn, req);
            break;

        case 16:
            err = handle_write_multiple_registers(mbsn, req);
            break;

        default:
            err = MBSN_EXCEPTION_ILLEGAL_FUNCTION;
    }

    return err;
}


mbsn_error mbsn_server_receive(mbsn_t* mbsn) {
    msg_state req = msg_state_create();

    bool first_byte_received = false;
    mbsn_error err = recv_msg_header(mbsn, &req, &first_byte_received);
    if (err != MBSN_ERROR_NONE) {
        if (!first_byte_received && err == MBSN_ERROR_TIMEOUT)
            return MBSN_ERROR_NONE;
        else
            return err;
    }

    /*
    // We should wait for the read timeout for the first message byte
    int32_t old_byte_timeout = mbsn->byte_timeout_ms;
    mbsn->byte_timeout_ms = mbsn->read_timeout_ms;

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint8_t id;
        mbsn_error err = recv_1(mbsn, &id, &req.crc);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != 0) {
            if (err == MBSN_ERROR_TIMEOUT)
                return MBSN_ERROR_NONE;
            else
                return err;
        }

        // Check if request is for us
        if (id == 0)
            req.broadcast = true;
        else if (id != mbsn->address_rtu)
            req.ignored = true;
        else
            req.ignored = false;

        err = recv_1(mbsn, &req.fc, &req.crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn_error err = recv_2(mbsn, &req.transaction_id, NULL);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != 0) {
            if (err == MBSN_ERROR_TIMEOUT)
                return MBSN_ERROR_NONE;
            else
                return err;
        }

        uint16_t protocol_id = 0xFFFF;
        err = recv_2(mbsn, &protocol_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        uint16_t length = 0xFFFF;
        err = recv_2(mbsn, &length, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = recv_1(mbsn, &req.unit_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = recv_1(mbsn, &req.fc, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        if (protocol_id != 0)
            return MBSN_ERROR_TRANSPORT;

        // TODO maybe we should actually check the length of the request against this value
        if (length == 0xFFFF)
            return MBSN_ERROR_TRANSPORT;
    }
     */
    return handle_req_fc(mbsn, &req);
}


static mbsn_error recv_res_header(mbsn_t* mbsn, msg_state* req, msg_state* res_out) {
    mbsn_error err = recv_msg_header(mbsn, res_out, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (res_out->transaction_id != req->transaction_id)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (res_out->ignored)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (res_out->fc != req->fc)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


static mbsn_error read_discrete(mbsn_t* mbsn, uint8_t fc, uint16_t address, uint16_t quantity, mbsn_bitfield values) {
    if (quantity < 1 || quantity > 2000)
        return MBSN_ERROR_INVALID_ARGUMENT;

    msg_state req = req_create(mbsn->server_dest_address_rtu, fc);
    send_msg_header(mbsn, &req, 4);

    put_2(mbsn, address);
    put_2(mbsn, quantity);

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 1);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t coils_bytes = get_1(mbsn);

    err = recv(mbsn, coils_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    for (int i = 0; i < coils_bytes; i++) {
        values[i] = get_1(mbsn);
    }

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_read_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield coils_out) {
    return read_discrete(mbsn, 1, address, quantity, coils_out);
}


mbsn_error mbsn_read_discrete_inputs(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield inputs_out) {
    return read_discrete(mbsn, 2, address, quantity, inputs_out);
}


mbsn_error read_registers(mbsn_t* mbsn, uint8_t fc, uint16_t address, uint16_t quantity, uint16_t* registers) {
    if (quantity < 1 || quantity > 125)
        return MBSN_ERROR_INVALID_ARGUMENT;

    msg_state req = req_create(mbsn->server_dest_address_rtu, fc);
    send_msg_header(mbsn, &req, 4);

    put_2(mbsn, address);
    put_2(mbsn, quantity);

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 1);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t registers_bytes = get_1(mbsn);

    err = recv(mbsn, registers_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    for (int i = 0; i < registers_bytes / 2; i++) {
        registers[i] = get_2(mbsn);
    }

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (registers_bytes != quantity * 2)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_read_holding_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    return read_registers(mbsn, 3, address, quantity, registers_out);
}


mbsn_error mbsn_read_input_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    return read_registers(mbsn, 4, address, quantity, registers_out);
}


mbsn_error mbsn_write_single_coil(mbsn_t* mbsn, uint16_t address, bool value) {
    msg_state req = req_create(mbsn->server_dest_address_rtu, 5);
    send_msg_header(mbsn, &req, 4);

    put_2(mbsn, address);
    put_2(mbsn, value ? 0xFF00 : 0);

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res = get_2(mbsn);
    uint16_t value_res = get_2(mbsn);

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (value_res != value)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_write_single_register(mbsn_t* mbsn, uint16_t address, uint16_t value) {
    msg_state req = req_create(mbsn->server_dest_address_rtu, 5);
    send_msg_header(mbsn, &req, 4);

    put_2(mbsn, address);
    put_2(mbsn, value);

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res = get_2(mbsn);
    uint16_t value_res = get_2(mbsn);

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (value_res != value)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_write_multiple_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const mbsn_bitfield coils) {
    if (quantity < 0 || quantity > 0x07B0)
        return MBSN_ERROR_INVALID_ARGUMENT;

    uint8_t coils_bytes = (quantity / 8) + 1;

    msg_state req = req_create(mbsn->server_dest_address_rtu, 15);
    send_msg_header(mbsn, &req, 5 + coils_bytes);

    put_2(mbsn, address);
    put_2(mbsn, quantity);
    put_1(mbsn, coils_bytes);

    for (int i = 0; i < coils_bytes; i++) {
        put_1(mbsn, coils[i]);
    }

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res = get_2(mbsn);
    uint16_t quantity_res = get_2(mbsn);

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (quantity_res != quantity)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_write_multiple_registers(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const uint16_t* registers) {
    if (quantity < 0 || quantity > 0x007B)
        return MBSN_ERROR_INVALID_ARGUMENT;

    uint8_t registers_bytes = quantity * 2;

    msg_state req = req_create(mbsn->server_dest_address_rtu, 16);
    send_msg_header(mbsn, &req, 5 + registers_bytes);

    put_2(mbsn, address);
    put_2(mbsn, quantity);
    put_1(mbsn, registers_bytes);

    for (int i = 0; i < quantity; i++) {
        put_2(mbsn, registers[i]);
    }

    mbsn_error err = send_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv(mbsn, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res = get_2(mbsn);
    uint16_t quantity_res = get_2(mbsn);

    err = recv_msg_footer(mbsn);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (quantity_res != quantity)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}

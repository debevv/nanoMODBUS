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
#error "Failed to automatically detect system endianness. Please define either MBSN_BIG_ENDIAN or MBSN_LITTLE_ENDIAN"
#endif
#endif

#ifdef MBSN_BIG_ENDIAN
#define HTONS(x) (x)
#define NTOHS(x) (x)
#else
#define HTONS(x) (((x) >> 8) | ((x) << 8))
#define NTOHS(x) (((x) >> 8) | ((x) << 8))
#endif


typedef struct msg_state {
    uint8_t unit_id;
    uint8_t fc;
    uint16_t transaction_id;
    bool broadcast;
    bool ignored;
    uint16_t crc;
} msg_state;


static msg_state msg_state_create() {
    msg_state s;
    memset(&s, 0, sizeof(msg_state));
    s.crc = 0xFFFF;
    return s;
}


static msg_state res_from_req(msg_state* req) {
    msg_state res = *req;
    res.crc = 0xFFFF;
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


static uint16_t crc_add_n(uint16_t crc, const uint8_t* data, unsigned int length) {
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


static uint16_t crc_add_1(uint16_t crc, const uint8_t b) {
    return crc_add_n(crc, &b, 1);
}


static uint16_t crc_add_2(uint16_t crc, const uint8_t w) {
    return crc_add_n(crc, &w, 2);
}


static mbsn_error recv_n(mbsn_t* mbsn, uint8_t* buf, uint32_t count, uint16_t* crc) {
    int r = 0;
    while (r != count) {
        int ret = mbsn->transport_read_byte(buf + r, mbsn->byte_timeout_ms);
        if (ret == 0)
            return MBSN_ERROR_TIMEOUT;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        *crc = crc_add_1(*crc, buf[r]);
        r++;
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error recv_1(mbsn_t* mbsn, uint8_t* b, uint16_t* crc) {
    return recv_n(mbsn, b, 1, crc);
}


static mbsn_error recv_2(mbsn_t* mbsn, uint16_t* w, uint16_t* crc) {
    mbsn_error err = recv_n(mbsn, (uint8_t*) w, 2, crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    *w = NTOHS(*w);
    return MBSN_ERROR_NONE;
}


static mbsn_error send_n(mbsn_t* mbsn, uint8_t* buf, uint32_t count, uint16_t* crc) {
    int w = 0;
    while (w != count) {
        int ret = mbsn->transport_write_byte(buf[w], mbsn->read_timeout_ms);
        if (ret == 0)
            return MBSN_ERROR_TIMEOUT;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        w++;
    }

    if (crc)
        *crc = crc_add_n(*crc, buf, count);

    return MBSN_ERROR_NONE;
}


static mbsn_error send_1(mbsn_t* mbsn, uint8_t b, uint16_t* crc) {
    return send_n(mbsn, &b, 1, crc);
}


static mbsn_error send_2(mbsn_t* mbsn, uint16_t w, uint16_t* crc) {
    w = HTONS(w);
    return send_n(mbsn, (uint8_t*) &w, 2, crc);
}


static mbsn_error recv_msg_header(mbsn_t* mbsn, msg_state* s_out) {
    // We wait for the read timeout here, just for the first message byte
    int32_t old_byte_timeout = mbsn->byte_timeout_ms;
    mbsn->byte_timeout_ms = mbsn->read_timeout_ms;

    *s_out = msg_state_create();

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint8_t unit_id;
        mbsn_error err = recv_1(mbsn, &unit_id, &s_out->crc);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != 0) {
            if (err == MBSN_ERROR_TIMEOUT)
                return MBSN_ERROR_NONE;
            else
                return err;
        }

        // Check if message is for us
        if (unit_id == 0)
            s_out->broadcast = true;
        else if (unit_id != mbsn->address_rtu)
            s_out->ignored = true;
        else
            s_out->ignored = false;

        err = recv_1(mbsn, &s_out->fc, &s_out->crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn_error err = recv_2(mbsn, &s_out->transaction_id, NULL);

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

        err = recv_1(mbsn, &s_out->unit_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = recv_1(mbsn, &s_out->fc, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        if (protocol_id != 0)
            return MBSN_ERROR_TRANSPORT;

        // TODO maybe we should actually check the length of the request against this value
        if (length == 0xFFFF)
            return MBSN_ERROR_TRANSPORT;
    }

    return MBSN_ERROR_NONE;
}

static mbsn_error recv_msg_footer(mbsn_t* mbsn, msg_state* s) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint16_t recv_crc;
        mbsn_error err = recv_2(mbsn, &recv_crc, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        if (recv_crc != s->crc)
            return MBSN_ERROR_TRANSPORT;
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error send_msg_header(mbsn_t* mbsn, msg_state* s, uint8_t data_length) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        s->crc = 0xFFFF;
        mbsn_error err = send_1(mbsn, s->unit_id, &s->crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn_error err = send_2(mbsn, s->transaction_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        const uint16_t protocol_id = 0;
        err = send_2(mbsn, protocol_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, 1 + 1 + data_length, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_1(mbsn, s->unit_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    mbsn_error err = send_1(mbsn, s->fc, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    return MBSN_ERROR_NONE;
}


static mbsn_error send_msg_footer(mbsn_t* mbsn, msg_state* s) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU)
        return send_2(mbsn, s->crc, NULL);

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_exception(mbsn_t* mbsn, msg_state* req, uint8_t exception) {
    msg_state res = *req;
    req->fc += 0x80;

    mbsn_error err = send_msg_header(mbsn, &res, 1);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_1(mbsn, exception, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    return send_msg_footer(mbsn, &res);
}


static mbsn_error handle_read_discrete(mbsn_t* mbsn, msg_state* req,
                                       mbsn_error (*callback)(uint16_t, uint16_t, mbsn_bitfield)) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, req);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (quantity < 1 || quantity > 2000)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) addr + (uint32_t) quantity > 65535)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            mbsn_bitfield bf;
            err = callback(addr, quantity, bf);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                uint8_t discrete_bytes = (quantity / 8) + 1;
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, discrete_bytes);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_1(mbsn, discrete_bytes, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_n(mbsn, bf, discrete_bytes, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_msg_footer(mbsn, &res);
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
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, req);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (quantity < 1 || quantity > 125)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) addr + (uint32_t) quantity > 65535)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            uint16_t regs[125];
            err = callback(addr, quantity, regs);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                uint8_t regs_bytes = quantity * 2;
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, regs_bytes);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_1(mbsn, regs_bytes, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                for (int i = 0; i < quantity; i++) {
                    err = send_2(mbsn, regs[i], &res.crc);
                    if (err != MBSN_ERROR_NONE)
                        return err;
                }

                err = send_msg_footer(mbsn, &res);
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
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, req);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (value != 0 && value != 0xFF00)
            return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (mbsn->callbacks.write_single_coil) {
            err = mbsn->callbacks.write_single_coil(addr, value == 0 ? false : true);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, 2);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_msg_footer(mbsn, &res);
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
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, req);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!req->ignored) {
        if (mbsn->callbacks.write_single_register) {
            err = mbsn->callbacks.write_single_register(addr, value);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_ILLEGAL_FUNCTION);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, 1);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, value, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_msg_footer(mbsn, &res);
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
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t coils_bytes;
    err = recv_1(mbsn, &coils_bytes, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    mbsn_bitfield coils;
    err = recv_n(mbsn, coils, coils_bytes, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, req);
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
            err = mbsn->callbacks.write_multiple_coils(addr, quantity, coils);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, 4);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, quantity, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_msg_footer(mbsn, &res);
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
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t registers_bytes;
    err = recv_1(mbsn, &registers_bytes, &req->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t registers[0x007B];
    for (int i = 0; i < quantity; i++) {
        err = recv_2(mbsn, registers + i, &req->crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    err = recv_msg_footer(mbsn, req);
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
            err = mbsn->callbacks.write_multiple_registers(addr, quantity, registers);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return handle_exception(mbsn, req, MBSN_EXCEPTION_SERVER_DEVICE_FAILURE);
                else
                    return err;
            }

            if (!req->broadcast) {
                msg_state res = res_from_req(req);

                err = send_msg_header(mbsn, &res, 4);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, quantity, &res.crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_msg_footer(mbsn, &res);
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

    mbsn_error err = recv_msg_header(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

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
    mbsn_error err = recv_msg_header(mbsn, res_out);
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


static mbsn_error read_discrete(mbsn_t* mbsn, uint8_t fc, uint16_t address, uint16_t quantity, mbsn_bitfield* values) {
    if (quantity < 1 || quantity > 2000)
        return MBSN_ERROR_INVALID_ARGUMENT;

    msg_state req = req_create(mbsn->server_dest_address_rtu, fc);
    mbsn_error err = send_msg_header(mbsn, &req, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, quantity, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t coils_bytes;
    err = recv_1(mbsn, &coils_bytes, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_n(mbsn, (uint8_t*) &values, coils_bytes, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_read_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield* coils_out) {
    return read_discrete(mbsn, 1, address, quantity, coils_out);
}


mbsn_error mbsn_read_discrete_inputs(mbsn_t* mbsn, uint16_t address, uint16_t quantity, mbsn_bitfield* inputs_out) {
    return read_discrete(mbsn, 1, address, quantity, inputs_out);
}


mbsn_error read_registers(mbsn_t* mbsn, uint8_t fc, uint16_t address, uint16_t quantity, uint16_t* registers) {
    if (quantity < 1 || quantity > 125)
        return MBSN_ERROR_INVALID_ARGUMENT;

    msg_state req = req_create(mbsn->server_dest_address_rtu, fc);
    mbsn_error err = send_msg_header(mbsn, &req, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, quantity, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t registers_bytes;
    err = recv_1(mbsn, &registers_bytes, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    for (int i = 0; i < registers_bytes / 2; i++) {
        err = recv_2(mbsn, registers + i, &res.crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    err = recv_msg_footer(mbsn, &res);
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
    mbsn_error err = send_msg_header(mbsn, &req, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, value ? 0xFF00 : 0, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res;
    err = recv_2(mbsn, &address_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value_res;
    err = recv_2(mbsn, &value_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, &res);
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
    mbsn_error err = send_msg_header(mbsn, &req, 4);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, value, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res;
    err = recv_2(mbsn, &address_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value_res;
    err = recv_2(mbsn, &value_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (value_res != value)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_write_multiple_coils(mbsn_t* mbsn, uint16_t address, uint16_t quantity, const mbsn_bitfield* coils) {
    if (quantity < 0 || quantity > 0x07B0)
        return MBSN_ERROR_INVALID_ARGUMENT;

    uint8_t coils_bytes = (quantity / 8) + 1;

    msg_state req = req_create(mbsn->server_dest_address_rtu, 15);
    mbsn_error err = send_msg_header(mbsn, &req, 5 + coils_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, quantity, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_1(mbsn, coils_bytes, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_n(mbsn, (uint8_t*) coils, coils_bytes, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res;
    err = recv_2(mbsn, &address_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity_res;
    err = recv_2(mbsn, &quantity_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, &res);
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
    mbsn_error err = send_msg_header(mbsn, &req, 5 + registers_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, address, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_2(mbsn, quantity, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_1(mbsn, registers_bytes, &req.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    for(int i = 0; i < quantity; i++) {
        err = send_2(mbsn, registers[i], &req.crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    err = send_msg_footer(mbsn, &req);
    if (err != MBSN_ERROR_NONE)
        return err;

    msg_state res;
    err = recv_res_header(mbsn, &req, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t address_res;
    err = recv_2(mbsn, &address_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity_res;
    err = recv_2(mbsn, &quantity_res, &res.crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = recv_msg_footer(mbsn, &res);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (address_res != address)
        return MBSN_ERROR_INVALID_RESPONSE;

    if (quantity_res != quantity)
        return MBSN_ERROR_INVALID_RESPONSE;

    return MBSN_ERROR_NONE;
}

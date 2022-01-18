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
"in modbusino_platform.h"
#endif
#endif

#ifdef MBSN_BIG_ENDIAN
#define HTONS(x) (x)
#define NTOHS(x) (x)
#else
#define HTONS(x) (((x) >> 8) | ((x) << 8))
#define NTOHS(x) (((x) >> 8) | ((x) << 8))
#endif


#define CLOCKS_PER_MS ((uint64_t) (CLOCKS_PER_SEC / 1000));


typedef struct req_state {
    uint8_t client_id;
    uint8_t fc;
    uint16_t transaction_id;
    bool broadcast;
    bool ignored;
    uint16_t crc;
} req_state;


static uint64_t clock_ms() {
    return (uint64_t) clock() / CLOCKS_PER_MS;
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


/*
mbsn_error read(uint8_t* buf, uint64_t len, uint64_t byte_timeout_ms, mbsn_error (*transport_read_byte)(uint8_t*)) {
    uint64_t now = clock_ms();
    int r = 0;
    while (r != len) {
        int ret = transport_read_byte(buf + r);

        if (byte_timeout_ms > 0 && clock_ms() - now >= byte_timeout_ms)
            return MBSN_ERROR_TIMEOUT;

        if (ret == 0)
            continue;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        r++;
    }

    return 0;
}
*/


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
        //start = clock_ms();
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

/*
void put_send_1(mbsn_t* mbsn, uint8_t w) {
    mbsn->send_buf[mbsn->send_idx] = w;
    mbsn->send_idx++;
    assert(mbsn->send_idx <= SEND_BUF_SIZE);
}


void put_send_2(mbsn_t* mbsn, uint16_t w) {
    mbsn->send_buf[mbsn->send_idx] = HTONS(w);
    mbsn->send_idx += 2;
    assert(mbsn->send_idx <= SEND_BUF_SIZE);
}


void put_send_n(mbsn_t* mbsn, uint8_t* buf, uint32_t n) {
    assert(mbsn->send_idx + n <= SEND_BUF_SIZE);
    memcpy(mbsn->send_buf + mbsn->send_idx, buf, n);
    mbsn->send_idx += n;
}


mbsn_error send(mbsn_t* mbsn) {
    const uint32_t target = mbsn->send_idx;
    //uint64_t start = clock_ms();
    while (mbsn->send_idx != 0) {
        mbsn->send_idx--;

        mbsn_error err = mbsn->transport_write_byte(mbsn->send_buf[target - mbsn->send_idx - 1]);

        if (mbsn->byte_timeout_ms > 0 && clock_ms() - start >= mbsn->byte_timeout_ms)
            return MBSN_ERROR_TIMEOUT;

        if (err == 0)
            continue;
        else if (err != 1)
            return MBSN_ERROR_TRANSPORT;

        //start = clock_ms();
    }

    return MBSN_ERROR_NONE;
}
*/


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


static mbsn_error send_mbap(mbsn_t* mbsn, req_state* s, uint16_t data_length) {
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

    err = send_1(mbsn, s->client_id, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    return MBSN_ERROR_NONE;
}


static mbsn_error send_req_header(mbsn_t* mbsn, req_state* s, uint8_t data_length, uint16_t* crc) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        mbsn_error err = send_1(mbsn, s->client_id, crc);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_1(mbsn, s->fc, crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        return send_mbap(mbsn, s, data_length);
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error send_req_footer(mbsn_t* mbsn, uint16_t crc) {
    if (mbsn->transport == MBSN_TRANSPORT_RTU)
        return send_2(mbsn, crc, NULL);

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_exception(mbsn_t* mbsn, req_state* s, uint8_t exception) {
    uint16_t crc = 0xFFFF;
    mbsn_error err = send_req_header(mbsn, s, 1, &crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_1(mbsn, s->fc + 0x80, &crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    err = send_1(mbsn, exception, &crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    return send_req_footer(mbsn, crc);
}


static mbsn_error handle_read_discrete(mbsn_t* mbsn, req_state* s,
                                       mbsn_error (*callback)(uint16_t, uint16_t, mbsn_bitfield)) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (quantity < 1 || quantity > 2000)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((uint32_t) addr + (uint32_t) quantity > 65535)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (!s->ignored) {
        if (callback) {
            mbsn_bitfield bf;
            err = callback(addr, quantity, bf);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            if (!s->broadcast) {
                uint8_t discrete_bytes = (quantity / 8) + 1;
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, discrete_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_1(mbsn, discrete_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_n(mbsn, bf, discrete_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_read_registers(mbsn_t* mbsn, req_state* s,
                                        mbsn_error (*callback)(uint16_t, uint16_t, uint16_t*)) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (quantity < 1 || quantity > 125)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((uint32_t) addr + (uint32_t) quantity > 65535)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (!s->ignored) {
        if (callback) {
            uint16_t regs[125];

            err = callback(addr, quantity, regs);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            for (int i = 0; i < quantity; i++)
                regs[i] = HTONS(regs[i]);

            if (!s->broadcast) {
                uint8_t regs_bytes = quantity * 2;
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, regs_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_1(mbsn, regs_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_n(mbsn, (uint8_t*) regs, regs_bytes, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_read_coils(mbsn_t* mbsn, req_state* s) {
    return handle_read_discrete(mbsn, s, mbsn->callbacks.read_coils);
}


static mbsn_error handle_read_discrete_inputs(mbsn_t* mbsn, req_state* s) {
    return handle_read_discrete(mbsn, s, mbsn->callbacks.read_discrete_inputs);
}


static mbsn_error handle_read_holding_registers(mbsn_t* mbsn, req_state* s) {
    return handle_read_registers(mbsn, s, mbsn->callbacks.read_holding_registers);
}


static mbsn_error handle_read_input_registers(mbsn_t* mbsn, req_state* s) {
    return handle_read_registers(mbsn, s, mbsn->callbacks.read_input_registers);
}


static mbsn_error handle_write_single_coil(mbsn_t* mbsn, req_state* s) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (value != 0 && value != 0xFF00)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (!s->ignored) {
        if (mbsn->callbacks.write_single_coil) {
            err = mbsn->callbacks.write_single_coil(addr, value == 0 ? false : true);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            if (!s->broadcast) {
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, 2, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_single_register(mbsn_t* mbsn, req_state* s) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (!s->ignored) {
        if (mbsn->callbacks.write_single_register) {
            err = mbsn->callbacks.write_single_register(addr, value);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            if (!s->broadcast) {
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, 1, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, value, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_multiple_coils(mbsn_t* mbsn, req_state* s) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t coils_bytes;
    err = recv_1(mbsn, &coils_bytes, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    mbsn_bitfield coils;
    err = recv_n(mbsn, coils, coils_bytes, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (quantity < 1 || quantity > 0x07B0)    // 0x07B0 == 1968
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (coils_bytes == 0)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((quantity / 8) + 1 != coils_bytes)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (!s->ignored) {
        if (mbsn->callbacks.write_multiple_coils) {
            err = mbsn->callbacks.write_multiple_coils(addr, quantity, coils);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            if (!s->broadcast) {
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, 4, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, quantity, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


static mbsn_error handle_write_multiple_registers(mbsn_t* mbsn, req_state* s) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t registers_bytes;
    err = recv_1(mbsn, &registers_bytes, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t registers[0x007B];
    err = recv_n(mbsn, (uint8_t*) registers, registers_bytes, &s->crc);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t recv_crc;
    err = recv_2(mbsn, &recv_crc, NULL);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (recv_crc != s->crc)
        return MBSN_ERROR_TRANSPORT;

    if (quantity < 1 || quantity > 0x007B)    // 0x007B == 123
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (registers_bytes == 0)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (registers_bytes != quantity * 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    for (int i = 0; i < quantity; i++)
        registers[i] = NTOHS(registers[i]);

    if (!s->ignored) {
        if (mbsn->callbacks.write_multiple_registers) {
            err = mbsn->callbacks.write_multiple_registers(addr, quantity, registers);
            if (err != MBSN_ERROR_NONE) {
                if (err < 0)
                    return MBSN_EXCEPTION_SERVER_DEVICE_FAILURE;
                else
                    return err;
            }

            if (!s->broadcast) {
                uint16_t crc = 0xFFFF;

                err = send_req_header(mbsn, s, 4, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, addr, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_2(mbsn, quantity, &crc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_req_footer(mbsn, crc);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    return MBSN_ERROR_NONE;
}


int mbsn_server_receive(mbsn_t* mbsn) {
    req_state s = {0};
    s.crc = 0xFFFF;

    // We should wait for the read timeout for the first message byte
    int32_t old_byte_timeout = mbsn->byte_timeout_ms;
    mbsn->byte_timeout_ms = mbsn->read_timeout_ms;

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint8_t id;
        mbsn_error err = recv_1(mbsn, &id, &s.crc);

        mbsn->byte_timeout_ms = old_byte_timeout;

        if (err != 0) {
            if (err == MBSN_ERROR_TIMEOUT)
                return MBSN_ERROR_NONE;
            else
                return err;
        }

        // Check if request is for us
        if (id == 0)
            s.broadcast = true;
        else if (id != mbsn->address_rtu)
            s.ignored = true;
        else
            s.ignored = false;

        err = recv_1(mbsn, &s.fc, &s.crc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn_error err = recv_2(mbsn, &s.transaction_id, NULL);

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

        err = recv_1(mbsn, &s.client_id, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = recv_1(mbsn, &s.fc, NULL);
        if (err != MBSN_ERROR_NONE)
            return err;

        if (protocol_id != 0)
            return MBSN_ERROR_TRANSPORT;

        // TODO maybe we should actually check the length of the request against this value
        if (length == 0xFFFF)
            return MBSN_ERROR_TRANSPORT;
    }

    mbsn_error err;

    switch (s.fc) {
        case 1:
            err = handle_read_coils(mbsn, &s);
            break;

        case 2:
            err = handle_read_discrete_inputs(mbsn, &s);
            break;

        case 3:
            err = handle_read_holding_registers(mbsn, &s);
            break;

        case 4:
            err = handle_read_input_registers(mbsn, &s);
            break;

        case 5:
            err = handle_write_single_coil(mbsn, &s);
            break;

        case 6:
            err = handle_write_single_register(mbsn, &s);
            break;

        case 15:
            err = handle_write_multiple_coils(mbsn, &s);
            break;

        case 16:
            err = handle_write_multiple_registers(mbsn, &s);
            break;

        default:
            err = MBSN_EXCEPTION_ILLEGAL_FUNCTION;
    }

    if (err != MBSN_ERROR_NONE) {
        if (!s.broadcast && !s.ignored && mbsn_error_is_exception(err)) {
            err = handle_exception(mbsn, &s, err);
            if (err != MBSN_ERROR_NONE)
                return err;
        }
        else
            return err;
    }

    return MBSN_ERROR_NONE;
}

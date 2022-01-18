#include "modbusino.h"
#include "modbusino_platform.h"
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
#define HTONS(x) void(0)
#define NTOHS(x) void(0)
#else
#define HTONS(x) (((x) >> 8) | ((x) << 8));
#define NTOHS(x) (((x) >> 8) | ((x) << 8));
#endif

#define CLOCKS_PER_MS ((uint64_t) (CLOCKS_PER_SEC / 1000));

uint64_t clock_ms() {
    return (uint64_t) clock() / CLOCKS_PER_MS;
}

int mbsn_create(mbsn_t* mbsn, mbsn_transport transport) {
    memset(mbsn, 0, sizeof(mbsn_t));

    mbsn->byte_timeout_ms = -1;
    mbsn->response_timeout_ms = -1;

    mbsn->transport = transport;
    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        mbsn->transport_read_byte = mbsn_rtu_read_byte;
        mbsn->transport_write_byte = mbsn_rtu_write_byte;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        mbsn->transport_read_byte = mbsn_tcp_read_byte;
        mbsn->transport_write_byte = mbsn_tcp_write_byte;
    }
    else {
        return MBSN_ERROR_INVALID_ARGUMENT;
    }

    return MBSN_ERROR_NONE;
}


mbsn_error mbsn_client_create(mbsn_t* mbsn, mbsn_transport transport) {
    return mbsn_create(mbsn, transport);
}


mbsn_error mbsn_server_create(mbsn_t* mbsn, mbsn_transport transport, uint8_t address, mbsn_callbacks callbacks) {
    mbsn_error ret = mbsn_create(mbsn, transport);
    mbsn->address_rtu = address;
    mbsn->callbacks = callbacks;
    return ret;
}


void mbsn_set_byte_timeout(mbsn_t* mbsn, int64_t timeout_ms) {
    mbsn->byte_timeout_ms = timeout_ms;
}


void mbsn_set_response_timeout(mbsn_t* mbsn, int64_t timeout_ms) {
    mbsn->response_timeout_ms = timeout_ms;
}


void mbsn_client_set_server_address_rtu(mbsn_t* mbsn, uint8_t address) {
    mbsn->server_dest_address_rtu = address;
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

mbsn_error recv_n(mbsn_t* mbsn, uint8_t* buf, uint32_t count) {
    //uint64_t start = clock_ms();
    int r = 0;
    while (r != count) {
        int ret = mbsn->transport_read_byte(buf + r);

        /*
        if (mbsn->byte_timeout_ms > 0 && clock_ms() - start >= mbsn->byte_timeout_ms)
            return MBSN_ERROR_TIMEOUT;
        */

        if (ret == 0)
            continue;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        r++;
        //start = clock_ms();
    }

    return MBSN_ERROR_NONE;
}


mbsn_error recv_1(mbsn_t* mbsn, uint8_t* b) {
    return recv_n(mbsn, b, 1);
}


mbsn_error recv_2(mbsn_t* mbsn, uint16_t* w) {
    mbsn_error err = recv_n(mbsn, (uint8_t*) w, 2);
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


mbsn_error send_n(mbsn_t* mbsn, uint8_t* buf, uint32_t count) {
    int w = 0;
    while (w != count) {
        int ret = mbsn->transport_write_byte(buf[w]);
        if (ret == 0)
            continue;
        else if (ret != 1)
            return MBSN_ERROR_TRANSPORT;

        w++;
    }

    return MBSN_ERROR_NONE;
}


mbsn_error send_1(mbsn_t* mbsn, uint8_t b) {
    return send_n(mbsn, &b, 1);
}


mbsn_error send_2(mbsn_t* mbsn, uint16_t w) {
    w = HTONS(w);
    return send_n(mbsn, (uint8_t*) &w, 2);
}


void handle_exception(mbsn_t* mbsn, uint16_t fc, uint8_t exception) {
    send_1(mbsn, fc + 0x80);
    send_1(mbsn, exception);
}


mbsn_error handle_read_discrete(mbsn_t* mbsn, uint16_t fc, bool ignored, bool broadcast,
                                mbsn_error (*callback)(uint16_t, uint16_t, mbsn_bitfield)) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (quantity < 1 || quantity > 2000)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((uint32_t) addr + (uint32_t) quantity > 65535)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (!ignored) {
        if (callback) {
            mbsn_bitfield bf;
            err = callback(addr, quantity, bf);
            if (err != MBSN_ERROR_NONE)
                return err;

            if (!broadcast) {
                err = send_1(mbsn, fc);
                if (err != MBSN_ERROR_NONE)
                    return err;

                uint8_t discrete_bytes = (quantity / 8) + 1;
                err = send_1(mbsn, discrete_bytes);
                if (err != MBSN_ERROR_NONE)
                    return err;

                err = send_n(mbsn, bf, discrete_bytes);
                if (err != MBSN_ERROR_NONE)
                    return err;
            }
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }
}


mbsn_error handle_read_registers(mbsn_t* mbsn, uint16_t fc, bool ignored, bool broadcast,
                                 mbsn_error (*callback)(uint16_t, uint16_t, uint16_t*)) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (quantity < 1 || quantity > 125)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((uint32_t) addr + (uint32_t) quantity > 65535)
        return MBSN_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    if (!ignored) {
        if (callback) {
            uint16_t regs[125];
            err = callback(addr, quantity, regs);
            if (err != MBSN_ERROR_NONE)
                return err;

            for (int i = 0; i < quantity; i++)
                regs[i] = HTONS(regs[i]);

            if (!broadcast) {
                send_1(mbsn, fc);

                uint8_t regs_bytes = quantity * 2;
                send_1(mbsn, regs_bytes);

                err = send_n(mbsn, (uint8_t*) regs, regs_bytes);
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


mbsn_error handle_read_coils(mbsn_t* mbsn, bool ignored, bool broadcast) {
    return handle_read_discrete(mbsn, 1, ignored, broadcast, mbsn->callbacks.read_coils);
}


mbsn_error handle_read_discrete_inputs(mbsn_t* mbsn, bool ignored, bool broadcast) {
    return handle_read_discrete(mbsn, 2, ignored, broadcast, mbsn->callbacks.read_discrete_inputs);
}


mbsn_error handle_read_holding_registers(mbsn_t* mbsn, bool ignored, bool broadcast) {
    return handle_read_registers(mbsn, 3, ignored, broadcast, mbsn->callbacks.read_holding_registers);
}


mbsn_error handle_read_input_registers(mbsn_t* mbsn, bool ignored, bool broadcast) {
    return handle_read_registers(mbsn, 4, ignored, broadcast, mbsn->callbacks.read_input_registers);
}


mbsn_error handle_write_single_coil(mbsn_t* mbsn, bool ignored, bool broadcast) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (value != 0 && value != 0xFF00)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (!ignored) {
        if (mbsn->callbacks.write_single_coil) {
            err = mbsn->callbacks.write_single_coil(addr, value == 0 ? false : true);
            if (err != MBSN_ERROR_NONE)
                return err;
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    if (!broadcast) {
        err = send_1(mbsn, 5);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, addr);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    return MBSN_ERROR_NONE;
}


mbsn_error handle_write_single_register(mbsn_t* mbsn, bool ignored, bool broadcast) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t value;
    err = recv_2(mbsn, &value);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!ignored) {
        if (mbsn->callbacks.write_single_register) {
            err = mbsn->callbacks.write_single_register(addr, value);
            if (err != MBSN_ERROR_NONE)
                return err;
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    if (!broadcast) {
        err = send_1(mbsn, 6);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, addr);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, value);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    return MBSN_ERROR_NONE;
}


mbsn_error handle_write_multiple_coils(mbsn_t* mbsn, bool ignored, bool broadcast) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t coils_bytes;
    err = recv_1(mbsn, &coils_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (quantity < 1 || quantity > 0x07B0)    // 0x07B0 == 1968
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (coils_bytes == 0)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if ((quantity / 8) + 1 != coils_bytes)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    mbsn_bitfield coils;
    err = recv_n(mbsn, coils, coils_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (!ignored) {
        if (mbsn->callbacks.write_multiple_coils) {
            err = mbsn->callbacks.write_multiple_coils(addr, quantity, coils);
            if (err != MBSN_ERROR_NONE)
                return err;
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    if (!broadcast) {
        err = send_1(mbsn, 15);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, addr);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, quantity);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    return MBSN_ERROR_NONE;
}


mbsn_error handle_write_multiple_registers(mbsn_t* mbsn, bool ignored, bool broadcast) {
    uint16_t addr;
    mbsn_error err = recv_2(mbsn, &addr);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint16_t quantity;
    err = recv_2(mbsn, &quantity);
    if (err != MBSN_ERROR_NONE)
        return err;

    uint8_t registers_bytes;
    err = recv_1(mbsn, &registers_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    if (quantity < 1 || quantity > 0x007B)    // 0x007B == 123
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (registers_bytes == 0)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    if (registers_bytes != quantity * 2)
        return MBSN_EXCEPTION_ILLEGAL_DATA_VALUE;

    uint16_t registers[0x007B];
    err = recv_n(mbsn, (uint8_t*) registers, registers_bytes);
    if (err != MBSN_ERROR_NONE)
        return err;

    for (int i = 0; i < quantity; i++)
        registers[i] = NTOHS(registers[i]);

    if (!ignored) {
        if (mbsn->callbacks.write_multiple_registers) {
            err = mbsn->callbacks.write_multiple_registers(addr, quantity, registers);
            if (err != MBSN_ERROR_NONE)
                return err;
        }
        else {
            return MBSN_EXCEPTION_ILLEGAL_FUNCTION;
        }
    }

    if (!broadcast) {
        err = send_1(mbsn, 16);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, addr);
        if (err != MBSN_ERROR_NONE)
            return err;

        err = send_2(mbsn, quantity);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    return MBSN_ERROR_NONE;
}

int mbsn_server_receive(mbsn_t* mbsn) {
    mbsn_error err = MBSN_ERROR_NONE;
    uint8_t fc = 0;
    bool broadcast = false;
    bool ignored = false;

    if (mbsn->transport == MBSN_TRANSPORT_RTU) {
        uint8_t id;
        err = recv_1(mbsn, &id);
        if (err != 0)
            return err;

        // Check if request is for us
        if (id == 0)
            broadcast = true;
        else if (id != mbsn->address_rtu)
            ignored = true;
        else
            ignored = false;

        err = recv_1(mbsn, &fc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }
    else if (mbsn->transport == MBSN_TRANSPORT_TCP) {
        err = recv_1(mbsn, &fc);
        if (err != MBSN_ERROR_NONE)
            return err;
    }

    switch (fc) {
        case 1:
            err = handle_read_coils(mbsn, ignored, broadcast);
            break;

        case 2:
            err = handle_read_discrete_inputs(mbsn, ignored, broadcast);
            break;

        case 3:
            err = handle_read_holding_registers(mbsn, ignored, broadcast);
            break;

        case 4:
            err = handle_read_input_registers(mbsn, ignored, broadcast);
            break;

        case 5:
            err = handle_write_single_coil(mbsn, ignored, broadcast);
            break;

        case 6:
            err = handle_write_single_register(mbsn, ignored, broadcast);
            break;

        case 15:
            err = handle_write_multiple_coils(mbsn, ignored, broadcast);
            break;

        case 16:
            err = handle_write_multiple_registers(mbsn, ignored, broadcast);
            break;

        default:
            err = MBSN_EXCEPTION_ILLEGAL_FUNCTION;
    }

    if (err != MBSN_ERROR_NONE) {
        if (!broadcast && !ignored && mbsn_error_is_exception(err))
            handle_exception(mbsn, fc, err);
        else
            return err;
    }

    return MBSN_ERROR_NONE;
}
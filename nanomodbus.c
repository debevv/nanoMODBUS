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

#include "nanomodbus.h"
#include <stdbool.h>
#include <string.h>


#ifdef NMBS_DEBUG
#include <stdio.h>
#define NMBS_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define NMBS_DEBUG_PRINT(...) (void) (0)
#endif


static uint8_t get_1(nmbs_t* nmbs) {
    uint8_t result = nmbs->msg.buf[nmbs->msg.buf_idx];
    nmbs->msg.buf_idx++;
    return result;
}


static void put_1(nmbs_t* nmbs, uint8_t data) {
    nmbs->msg.buf[nmbs->msg.buf_idx] = data;
    nmbs->msg.buf_idx++;
}


static void discard_1(nmbs_t* nmbs) {
    nmbs->msg.buf_idx++;
}


static uint16_t get_2(nmbs_t* nmbs) {
    uint16_t result =
            ((uint16_t) nmbs->msg.buf[nmbs->msg.buf_idx]) << 8 | (uint16_t) nmbs->msg.buf[nmbs->msg.buf_idx + 1];
    nmbs->msg.buf_idx += 2;
    return result;
}


static void put_2(nmbs_t* nmbs, uint16_t data) {
    nmbs->msg.buf[nmbs->msg.buf_idx] = (uint8_t) ((data >> 8) & 0xFFU);
    nmbs->msg.buf[nmbs->msg.buf_idx + 1] = (uint8_t) data;
    nmbs->msg.buf_idx += 2;
}


static void msg_buf_reset(nmbs_t* nmbs) {
    nmbs->msg.buf_idx = 0;
}


static void msg_state_reset(nmbs_t* nmbs) {
    msg_buf_reset(nmbs);
    nmbs->msg.unit_id = 0;
    nmbs->msg.fc = 0;
    nmbs->msg.transaction_id = 0;
    nmbs->msg.broadcast = false;
    nmbs->msg.ignored = 0;
}


#ifndef NMBS_CLIENT_DISABLED
static void msg_state_req(nmbs_t* nmbs, uint8_t fc) {
    if (nmbs->current_tid == UINT16_MAX)
        nmbs->current_tid = 1;
    else
        nmbs->current_tid++;

    msg_state_reset(nmbs);
    nmbs->msg.unit_id = nmbs->dest_address_rtu;
    nmbs->msg.fc = fc;
    nmbs->msg.transaction_id = nmbs->current_tid;
    if (nmbs->msg.unit_id == 0 && nmbs->platform.transport == NMBS_TRANSPORT_RTU)
        nmbs->msg.broadcast = true;
}
#endif


int nmbs_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf) {
    if (!nmbs)
        return NMBS_ERROR_INVALID_ARGUMENT;

    memset(nmbs, 0, sizeof(nmbs_t));

    nmbs->byte_timeout_ms = -1;
    nmbs->read_timeout_ms = -1;

    if (!platform_conf)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if (platform_conf->transport != NMBS_TRANSPORT_RTU && platform_conf->transport != NMBS_TRANSPORT_TCP)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if (!platform_conf->read || !platform_conf->write)
        return NMBS_ERROR_INVALID_ARGUMENT;

    nmbs->platform = *platform_conf;

    return NMBS_ERROR_NONE;
}


void nmbs_set_read_timeout(nmbs_t* nmbs, int32_t timeout_ms) {
    nmbs->read_timeout_ms = timeout_ms;
}


void nmbs_set_byte_timeout(nmbs_t* nmbs, int32_t timeout_ms) {
    nmbs->byte_timeout_ms = timeout_ms;
}


void nmbs_set_destination_rtu_address(nmbs_t* nmbs, uint8_t address) {
    nmbs->dest_address_rtu = address;
}


void nmbs_set_platform_arg(nmbs_t* nmbs, void* arg) {
    nmbs->platform.arg = arg;
}


uint16_t nmbs_crc_calc(const uint8_t* data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
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

    return (uint16_t) (crc << 8) | (uint16_t) (crc >> 8);
}


static nmbs_error recv(nmbs_t* nmbs, uint16_t count) {
    int32_t ret =
            nmbs->platform.read(nmbs->msg.buf + nmbs->msg.buf_idx, count, nmbs->byte_timeout_ms, nmbs->platform.arg);

    if (ret == count)
        return NMBS_ERROR_NONE;

    if (ret < count) {
        if (ret < 0)
            return NMBS_ERROR_TRANSPORT;

        return NMBS_ERROR_TIMEOUT;
    }

    return NMBS_ERROR_TRANSPORT;
}


static nmbs_error send(nmbs_t* nmbs, uint16_t count) {
    int32_t ret = nmbs->platform.write(nmbs->msg.buf, count, nmbs->byte_timeout_ms, nmbs->platform.arg);

    if (ret == count)
        return NMBS_ERROR_NONE;

    if (ret < count) {
        if (ret < 0)
            return NMBS_ERROR_TRANSPORT;

        return NMBS_ERROR_TIMEOUT;
    }

    return NMBS_ERROR_TRANSPORT;
}


static nmbs_error recv_msg_footer(nmbs_t* nmbs) {
    NMBS_DEBUG_PRINT("\n");

    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        uint16_t crc = nmbs_crc_calc(nmbs->msg.buf, nmbs->msg.buf_idx);

        nmbs_error err = recv(nmbs, 2);
        if (err != NMBS_ERROR_NONE)
            return err;

        uint16_t recv_crc = get_2(nmbs);

        if (recv_crc != crc)
            return NMBS_ERROR_CRC;
    }

    return NMBS_ERROR_NONE;
}


static nmbs_error recv_msg_header(nmbs_t* nmbs, bool* first_byte_received) {
    // We wait for the read timeout here, just for the first message byte
    int32_t old_byte_timeout = nmbs->byte_timeout_ms;
    nmbs->byte_timeout_ms = nmbs->read_timeout_ms;

    msg_state_reset(nmbs);

    *first_byte_received = false;

    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        nmbs_error err = recv(nmbs, 1);

        nmbs->byte_timeout_ms = old_byte_timeout;

        if (err != NMBS_ERROR_NONE)
            return err;

        *first_byte_received = true;

        nmbs->msg.unit_id = get_1(nmbs);

        err = recv(nmbs, 1);
        if (err != NMBS_ERROR_NONE)
            return err;

        nmbs->msg.fc = get_1(nmbs);
    }
    else if (nmbs->platform.transport == NMBS_TRANSPORT_TCP) {
        nmbs_error err = recv(nmbs, 1);

        nmbs->byte_timeout_ms = old_byte_timeout;

        if (err != NMBS_ERROR_NONE)
            return err;

        *first_byte_received = true;

        // Advance buf_idx
        discard_1(nmbs);

        err = recv(nmbs, 7);
        if (err != NMBS_ERROR_NONE)
            return err;

        // Starting over
        msg_buf_reset(nmbs);

        nmbs->msg.transaction_id = get_2(nmbs);
        uint16_t protocol_id = get_2(nmbs);
        uint16_t length = get_2(nmbs);    // We should actually check the length of the request against this value
        nmbs->msg.unit_id = get_1(nmbs);
        nmbs->msg.fc = get_1(nmbs);

        if (protocol_id != 0)
            return NMBS_ERROR_INVALID_TCP_MBAP;

        if (length > 255)
            return NMBS_ERROR_INVALID_TCP_MBAP;
    }

    return NMBS_ERROR_NONE;
}


static void put_msg_header(nmbs_t* nmbs, uint16_t data_length) {
    msg_buf_reset(nmbs);

    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        put_1(nmbs, nmbs->msg.unit_id);
    }
    else if (nmbs->platform.transport == NMBS_TRANSPORT_TCP) {
        put_2(nmbs, nmbs->msg.transaction_id);
        put_2(nmbs, 0);
        put_2(nmbs, (uint16_t) (1 + 1 + data_length));
        put_1(nmbs, nmbs->msg.unit_id);
    }

    put_1(nmbs, nmbs->msg.fc);
}


static nmbs_error send_msg(nmbs_t* nmbs) {
    NMBS_DEBUG_PRINT("\n");

    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        uint16_t crc = nmbs_crc_calc(nmbs->msg.buf, nmbs->msg.buf_idx);
        put_2(nmbs, crc);
    }

    nmbs_error err = send(nmbs, nmbs->msg.buf_idx);

    return err;
}


#ifndef NMBS_SERVER_DISABLED
static nmbs_error recv_req_header(nmbs_t* nmbs, bool* first_byte_received) {
    nmbs_error err = recv_msg_header(nmbs, first_byte_received);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        // Check if request is for us
        if (nmbs->msg.unit_id == NMBS_BROADCAST_ADDRESS)
            nmbs->msg.broadcast = true;
        else if (nmbs->msg.unit_id != nmbs->address_rtu)
            nmbs->msg.ignored = true;
        else
            nmbs->msg.ignored = false;
    }

    return NMBS_ERROR_NONE;
}


static void put_res_header(nmbs_t* nmbs, uint16_t data_length) {
    put_msg_header(nmbs, data_length);
    NMBS_DEBUG_PRINT("NMBS res -> fc %d\t", nmbs->msg.fc);
}


static nmbs_error send_exception_msg(nmbs_t* nmbs, uint8_t exception) {
    nmbs->msg.fc += 0x80;
    put_msg_header(nmbs, 1);
    put_1(nmbs, exception);

    NMBS_DEBUG_PRINT("NMBS res -> exception %d\n", exception);

    return send_msg(nmbs);
}
#endif


#ifndef NMBS_CLIENT_DISABLED
static nmbs_error recv_res_header(nmbs_t* nmbs) {
    uint16_t req_transaction_id = nmbs->msg.transaction_id;
    uint8_t req_unit_id = nmbs->msg.unit_id;
    uint8_t req_fc = nmbs->msg.fc;

    bool first_byte_received;
    nmbs_error err = recv_msg_header(nmbs, &first_byte_received);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (nmbs->platform.transport == NMBS_TRANSPORT_TCP) {
        if (nmbs->msg.transaction_id != req_transaction_id)
            return NMBS_ERROR_INVALID_TCP_MBAP;
    }

    if (nmbs->msg.unit_id != req_unit_id)
        return NMBS_ERROR_INVALID_UNIT_ID;

    if (nmbs->msg.fc != req_fc) {
        if (nmbs->msg.fc - 0x80 == req_fc) {
            err = recv(nmbs, 1);
            if (err != NMBS_ERROR_NONE)
                return err;

            uint8_t exception = get_1(nmbs);
            err = recv_msg_footer(nmbs);
            if (err != NMBS_ERROR_NONE)
                return err;

            if (exception < 1 || exception > 4)
                return NMBS_ERROR_INVALID_RESPONSE;

            NMBS_DEBUG_PRINT("exception %d\n", exception);
            return exception;
        }

        return NMBS_ERROR_INVALID_RESPONSE;
    }

    NMBS_DEBUG_PRINT("NMBS res <- fc %d\t", nmbs->msg.fc);

    return NMBS_ERROR_NONE;
}


static void put_req_header(nmbs_t* nmbs, uint16_t data_length) {
    put_msg_header(nmbs, data_length);
    NMBS_DEBUG_PRINT("NMBS req -> fc %d\t", nmbs->msg.fc);
}
#endif


#ifndef NMBS_SERVER_DISABLED
#if !defined(NMBS_SERVER_READ_COILS_DISABLED) || !defined(NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED)
static nmbs_error handle_read_discrete(nmbs_t* nmbs, nmbs_error (*callback)(uint16_t, uint16_t, nmbs_bitfield, void*)) {
    nmbs_error err = recv(nmbs, 4);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t quantity = get_2(nmbs);

    NMBS_DEBUG_PRINT("a %d\tq %d", address, quantity);

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (quantity < 1 || quantity > 2000)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            nmbs_bitfield bitfield = {0};
            err = callback(address, quantity, bitfield, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                uint8_t discrete_bytes = (quantity / 8) + 1;
                put_res_header(nmbs, 1 + discrete_bytes);

                put_1(nmbs, discrete_bytes);

                NMBS_DEBUG_PRINT("b %d\t", discrete_bytes);

                NMBS_DEBUG_PRINT("coils ");
                for (int i = 0; i < discrete_bytes; i++) {
                    put_1(nmbs, bitfield[i]);
                    NMBS_DEBUG_PRINT("%d", bitfield[i]);
                }

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


#if !defined(NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED) || !defined(NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED)
static nmbs_error handle_read_registers(nmbs_t* nmbs, nmbs_error (*callback)(uint16_t, uint16_t, uint16_t*, void*)) {
    nmbs_error err = recv(nmbs, 4);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t quantity = get_2(nmbs);

    NMBS_DEBUG_PRINT("a %d\tq %d", address, quantity);

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (quantity < 1 || quantity > 125)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (callback) {
            uint16_t regs[125] = {0};
            err = callback(address, quantity, regs, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                uint8_t regs_bytes = quantity * 2;
                put_res_header(nmbs, 1 + regs_bytes);

                put_1(nmbs, regs_bytes);

                NMBS_DEBUG_PRINT("b %d\t", regs_bytes);

                NMBS_DEBUG_PRINT("regs ");
                for (int i = 0; i < quantity; i++) {
                    put_2(nmbs, regs[i]);
                    NMBS_DEBUG_PRINT("%d", regs[i]);
                }

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_SERVER_READ_COILS_DISABLED
static nmbs_error handle_read_coils(nmbs_t* nmbs) {
    return handle_read_discrete(nmbs, nmbs->callbacks.read_coils);
}
#endif


#ifndef NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED
static nmbs_error handle_read_discrete_inputs(nmbs_t* nmbs) {
    return handle_read_discrete(nmbs, nmbs->callbacks.read_discrete_inputs);
}
#endif


#ifndef NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED
static nmbs_error handle_read_holding_registers(nmbs_t* nmbs) {
    return handle_read_registers(nmbs, nmbs->callbacks.read_holding_registers);
}
#endif


#ifndef NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED
static nmbs_error handle_read_input_registers(nmbs_t* nmbs) {
    return handle_read_registers(nmbs, nmbs->callbacks.read_input_registers);
}
#endif


#ifndef NMBS_SERVER_WRITE_SINGLE_COIL_DISABLED
static nmbs_error handle_write_single_coil(nmbs_t* nmbs) {
    nmbs_error err = recv(nmbs, 4);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t value = get_2(nmbs);

    NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value);

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (nmbs->callbacks.write_single_coil) {
            if (value != 0 && value != 0xFF00)
                return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

            err = nmbs->callbacks.write_single_coil(address, value == 0 ? false : true, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                put_res_header(nmbs, 4);

                put_2(nmbs, address);
                put_2(nmbs, value);
                NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value);

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_SERVER_WRITE_SINGLE_REGISTER_DISABLED
static nmbs_error handle_write_single_register(nmbs_t* nmbs) {
    nmbs_error err = recv(nmbs, 4);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t value = get_2(nmbs);

    NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value);

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (nmbs->callbacks.write_single_register) {
            err = nmbs->callbacks.write_single_register(address, value, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                put_res_header(nmbs, 4);

                put_2(nmbs, address);
                put_2(nmbs, value);
                NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value);

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED
static nmbs_error handle_write_multiple_coils(nmbs_t* nmbs) {
    nmbs_error err = recv(nmbs, 5);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t quantity = get_2(nmbs);
    uint8_t coils_bytes = get_1(nmbs);

    NMBS_DEBUG_PRINT("a %d\tq %d\tb %d\tcoils ", address, quantity, coils_bytes);

    err = recv(nmbs, coils_bytes);
    if (err != NMBS_ERROR_NONE)
        return err;

    nmbs_bitfield coils;
    for (int i = 0; i < coils_bytes; i++) {
        coils[i] = get_1(nmbs);
        NMBS_DEBUG_PRINT("%d ", coils[i]);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (quantity < 1 || quantity > 0x07B0)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (coils_bytes == 0)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((quantity / 8) + 1 != coils_bytes)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (nmbs->callbacks.write_multiple_coils) {
            err = nmbs->callbacks.write_multiple_coils(address, quantity, coils, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                put_res_header(nmbs, 4);

                put_2(nmbs, address);
                put_2(nmbs, quantity);
                NMBS_DEBUG_PRINT("a %d\tq %d", address, quantity);

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_SERVER_WRITE_MULTIPLE_REGISTERS_DISABLED
static nmbs_error handle_write_multiple_registers(nmbs_t* nmbs) {
    nmbs_error err = recv(nmbs, 5);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t address = get_2(nmbs);
    uint16_t quantity = get_2(nmbs);
    uint8_t registers_bytes = get_1(nmbs);

    NMBS_DEBUG_PRINT("a %d\tq %d\tb %d\tregs ", address, quantity, registers_bytes);

    err = recv(nmbs, registers_bytes);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint16_t registers[0x007B];
    for (int i = 0; i < registers_bytes / 2; i++) {
        registers[i] = get_2(nmbs);
        NMBS_DEBUG_PRINT("%d ", registers[i]);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.ignored) {
        if (quantity < 1 || quantity > 0x007B)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

        if (registers_bytes == 0)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (registers_bytes != quantity * 2)
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_DATA_VALUE);

        if (nmbs->callbacks.write_multiple_registers) {
            err = nmbs->callbacks.write_multiple_registers(address, quantity, registers, nmbs->platform.arg);
            if (err != NMBS_ERROR_NONE) {
                if (nmbs_error_is_exception(err))
                    return send_exception_msg(nmbs, err);

                return send_exception_msg(nmbs, NMBS_EXCEPTION_SERVER_DEVICE_FAILURE);
            }

            if (!nmbs->msg.broadcast) {
                put_res_header(nmbs, 4);

                put_2(nmbs, address);
                put_2(nmbs, quantity);
                NMBS_DEBUG_PRINT("a %d\tq %d", address, quantity);

                err = send_msg(nmbs);
                if (err != NMBS_ERROR_NONE)
                    return err;
            }
        }
        else {
            return send_exception_msg(nmbs, NMBS_EXCEPTION_ILLEGAL_FUNCTION);
        }
    }

    return NMBS_ERROR_NONE;
}
#endif


static nmbs_error handle_req_fc(nmbs_t* nmbs) {
    NMBS_DEBUG_PRINT("fc %d\t", nmbs->msg.fc);

    nmbs_error err;
    switch (nmbs->msg.fc) {
#ifndef NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED
        case 1:
            err = handle_read_coils(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_READ_DISCRETE_INPUTS_DISABLED
        case 2:
            err = handle_read_discrete_inputs(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_READ_HOLDING_REGISTERS_DISABLED
        case 3:
            err = handle_read_holding_registers(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_READ_INPUT_REGISTERS_DISABLED
        case 4:
            err = handle_read_input_registers(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_WRITE_SINGLE_COIL_DISABLED
        case 5:
            err = handle_write_single_coil(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_WRITE_SINGLE_REGISTER_DISABLED
        case 6:
            err = handle_write_single_register(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_WRITE_MULTIPLE_COILS_DISABLED
        case 15:
            err = handle_write_multiple_coils(nmbs);
            break;
#endif

#ifndef NMBS_SERVER_WRITE_MULTIPLE_REGISTERS_DISABLED
        case 16:
            err = handle_write_multiple_registers(nmbs);
            break;
#endif

        default:
            err = NMBS_EXCEPTION_ILLEGAL_FUNCTION;
    }

    return err;
}


nmbs_error nmbs_server_create(nmbs_t* nmbs, uint8_t address_rtu, const nmbs_platform_conf* platform_conf,
                              const nmbs_callbacks* callbacks) {
    if (platform_conf->transport == NMBS_TRANSPORT_RTU && address_rtu == 0)
        return NMBS_ERROR_INVALID_ARGUMENT;

    nmbs_error ret = nmbs_create(nmbs, platform_conf);
    if (ret != NMBS_ERROR_NONE)
        return ret;

    nmbs->address_rtu = address_rtu;
    nmbs->callbacks = *callbacks;

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_server_poll(nmbs_t* nmbs) {
    msg_state_reset(nmbs);

    bool first_byte_received = false;
    nmbs_error err = recv_req_header(nmbs, &first_byte_received);
    if (err != NMBS_ERROR_NONE) {
        if (!first_byte_received && err == NMBS_ERROR_TIMEOUT)
            return NMBS_ERROR_NONE;

        return err;
    }

#ifdef NMBS_DEBUG
    printf("NMBS req <- ");
    if (nmbs->platform.transport == NMBS_TRANSPORT_RTU) {
        if (nmbs->msg.broadcast)
            printf("broadcast\t");

        printf("client_id %d\t", nmbs->msg.unit_id);
    }
#endif

    err = handle_req_fc(nmbs);
    if (err != NMBS_ERROR_NONE) {
        if (!nmbs_error_is_exception(err))
            return err;
    }

    return err;
}
#endif


#ifndef NMBS_CLIENT_DISABLED
nmbs_error nmbs_client_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf) {
    return nmbs_create(nmbs, platform_conf);
}


static nmbs_error read_discrete(nmbs_t* nmbs, uint8_t fc, uint16_t address, uint16_t quantity, nmbs_bitfield values) {
    if (quantity < 1 || quantity > 2000)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    msg_state_req(nmbs, fc);
    put_req_header(nmbs, 4);

    put_2(nmbs, address);
    put_2(nmbs, quantity);

    NMBS_DEBUG_PRINT("a %d\tq %d", address, quantity);

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, 1);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint8_t coils_bytes = get_1(nmbs);
    NMBS_DEBUG_PRINT("b %d\t", coils_bytes);

    err = recv(nmbs, coils_bytes);
    if (err != NMBS_ERROR_NONE)
        return err;

    NMBS_DEBUG_PRINT("coils ");
    for (int i = 0; i < coils_bytes; i++) {
        values[i] = get_1(nmbs);
        NMBS_DEBUG_PRINT("%d", values[i]);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_read_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield coils_out) {
    return read_discrete(nmbs, 1, address, quantity, coils_out);
}


nmbs_error nmbs_read_discrete_inputs(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out) {
    return read_discrete(nmbs, 2, address, quantity, inputs_out);
}


static nmbs_error read_registers(nmbs_t* nmbs, uint8_t fc, uint16_t address, uint16_t quantity, uint16_t* registers) {
    if (quantity < 1 || quantity > 125)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    msg_state_req(nmbs, fc);
    put_req_header(nmbs, 4);

    put_2(nmbs, address);
    put_2(nmbs, quantity);

    NMBS_DEBUG_PRINT("a %d\tq %d ", address, quantity);

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, 1);
    if (err != NMBS_ERROR_NONE)
        return err;

    uint8_t registers_bytes = get_1(nmbs);
    NMBS_DEBUG_PRINT("b %d\t", registers_bytes);

    err = recv(nmbs, registers_bytes);
    if (err != NMBS_ERROR_NONE)
        return err;

    NMBS_DEBUG_PRINT("regs ");
    for (int i = 0; i < registers_bytes / 2; i++) {
        registers[i] = get_2(nmbs);
        NMBS_DEBUG_PRINT("%d", registers[i]);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (registers_bytes != quantity * 2)
        return NMBS_ERROR_INVALID_RESPONSE;

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_read_holding_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    return read_registers(nmbs, 3, address, quantity, registers_out);
}


nmbs_error nmbs_read_input_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out) {
    return read_registers(nmbs, 4, address, quantity, registers_out);
}


nmbs_error nmbs_write_single_coil(nmbs_t* nmbs, uint16_t address, bool value) {
    msg_state_req(nmbs, 5);
    put_req_header(nmbs, 4);

    uint16_t value_req = value ? 0xFF00 : 0;

    put_2(nmbs, address);
    put_2(nmbs, value_req);

    NMBS_DEBUG_PRINT("a %d\tvalue %d ", address, value_req);

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.broadcast) {
        err = recv_res_header(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        err = recv(nmbs, 4);
        if (err != NMBS_ERROR_NONE)
            return err;

        uint16_t address_res = get_2(nmbs);
        uint16_t value_res = get_2(nmbs);

        NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value_res);

        err = recv_msg_footer(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        if (address_res != address)
            return NMBS_ERROR_INVALID_RESPONSE;

        if (value_res != value_req)
            return NMBS_ERROR_INVALID_RESPONSE;
    }

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_write_single_register(nmbs_t* nmbs, uint16_t address, uint16_t value) {
    msg_state_req(nmbs, 6);
    put_req_header(nmbs, 4);

    put_2(nmbs, address);
    put_2(nmbs, value);

    NMBS_DEBUG_PRINT("a %d\tvalue %d", address, value);

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.broadcast) {
        err = recv_res_header(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        err = recv(nmbs, 4);
        if (err != NMBS_ERROR_NONE)
            return err;

        uint16_t address_res = get_2(nmbs);
        uint16_t value_res = get_2(nmbs);
        NMBS_DEBUG_PRINT("a %d\tvalue %d ", address, value_res);

        err = recv_msg_footer(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        if (address_res != address)
            return NMBS_ERROR_INVALID_RESPONSE;

        if (value_res != value)
            return NMBS_ERROR_INVALID_RESPONSE;
    }

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_write_multiple_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const nmbs_bitfield coils) {
    if (quantity < 1 || quantity > 0x07B0)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    uint8_t coils_bytes = (quantity / 8) + 1;

    msg_state_req(nmbs, 15);
    put_req_header(nmbs, 5 + coils_bytes);

    put_2(nmbs, address);
    put_2(nmbs, quantity);
    put_1(nmbs, coils_bytes);
    NMBS_DEBUG_PRINT("a %d\tq %d\tb %d\t", address, quantity, coils_bytes);

    NMBS_DEBUG_PRINT("coils ");
    for (int i = 0; i < coils_bytes; i++) {
        put_1(nmbs, coils[i]);
        NMBS_DEBUG_PRINT("%d ", coils[i]);
    }

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.broadcast) {
        err = recv_res_header(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        err = recv(nmbs, 4);
        if (err != NMBS_ERROR_NONE)
            return err;

        uint16_t address_res = get_2(nmbs);
        uint16_t quantity_res = get_2(nmbs);
        NMBS_DEBUG_PRINT("a %d\tq %d", address_res, quantity_res);

        err = recv_msg_footer(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        if (address_res != address)
            return NMBS_ERROR_INVALID_RESPONSE;

        if (quantity_res != quantity)
            return NMBS_ERROR_INVALID_RESPONSE;
    }

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_write_multiple_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const uint16_t* registers) {
    if (quantity < 1 || quantity > 0x007B)
        return NMBS_ERROR_INVALID_ARGUMENT;

    if ((uint32_t) address + (uint32_t) quantity > ((uint32_t) 0xFFFF) + 1)
        return NMBS_ERROR_INVALID_ARGUMENT;

    uint8_t registers_bytes = quantity * 2;

    msg_state_req(nmbs, 16);
    put_req_header(nmbs, 5 + registers_bytes);

    put_2(nmbs, address);
    put_2(nmbs, quantity);
    put_1(nmbs, registers_bytes);
    NMBS_DEBUG_PRINT("a %d\tq %d\tb %d\t", address, quantity, registers_bytes);

    NMBS_DEBUG_PRINT("regs ");
    for (int i = 0; i < quantity; i++) {
        put_2(nmbs, registers[i]);
        NMBS_DEBUG_PRINT("%d ", registers[i]);
    }

    nmbs_error err = send_msg(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    if (!nmbs->msg.broadcast) {
        err = recv_res_header(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        err = recv(nmbs, 4);
        if (err != NMBS_ERROR_NONE)
            return err;

        uint16_t address_res = get_2(nmbs);
        uint16_t quantity_res = get_2(nmbs);
        NMBS_DEBUG_PRINT("a %d\tq %d", address_res, quantity_res);

        err = recv_msg_footer(nmbs);
        if (err != NMBS_ERROR_NONE)
            return err;

        if (address_res != address)
            return NMBS_ERROR_INVALID_RESPONSE;

        if (quantity_res != quantity)
            return NMBS_ERROR_INVALID_RESPONSE;
    }

    return NMBS_ERROR_NONE;
}


nmbs_error nmbs_send_raw_pdu(nmbs_t* nmbs, uint8_t fc, const uint8_t* data, uint16_t data_len) {
    msg_state_req(nmbs, fc);
    put_msg_header(nmbs, data_len);

    NMBS_DEBUG_PRINT("raw ");
    for (uint16_t i = 0; i < data_len; i++) {
        put_1(nmbs, data[i]);
        NMBS_DEBUG_PRINT("%d ", data[i]);
    }

    return send_msg(nmbs);
}


nmbs_error nmbs_receive_raw_pdu_response(nmbs_t* nmbs, uint8_t* data_out, uint16_t data_out_len) {
    nmbs_error err = recv_res_header(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    err = recv(nmbs, data_out_len);
    if (err != NMBS_ERROR_NONE)
        return err;

    for (uint16_t i = 0; i < data_out_len; i++) {
        data_out[i] = get_1(nmbs);
    }

    err = recv_msg_footer(nmbs);
    if (err != NMBS_ERROR_NONE)
        return err;

    return NMBS_ERROR_NONE;
}
#endif


#ifndef NMBS_STRERROR_DISABLED
const char* nmbs_strerror(nmbs_error error) {
    switch (error) {
        case NMBS_ERROR_INVALID_UNIT_ID:
            return "invalid unit ID received";

        case NMBS_ERROR_INVALID_TCP_MBAP:
            return "invalid TCP MBAP received";

        case NMBS_ERROR_CRC:
            return "invalid CRC received";

        case NMBS_ERROR_TRANSPORT:
            return "transport error";

        case NMBS_ERROR_TIMEOUT:
            return "timeout";

        case NMBS_ERROR_INVALID_RESPONSE:
            return "invalid response received";

        case NMBS_ERROR_INVALID_ARGUMENT:
            return "invalid argument provided";

        case NMBS_ERROR_NONE:
            return "no error";

        case NMBS_EXCEPTION_ILLEGAL_FUNCTION:
            return "modbus exception 1: illegal function";

        case NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
            return "modbus exception 2: illegal data address";

        case NMBS_EXCEPTION_ILLEGAL_DATA_VALUE:
            return "modbus exception 3: data value";

        case NMBS_EXCEPTION_SERVER_DEVICE_FAILURE:
            return "modbus exception 4: server device failure";

        default:
            return "unknown error";
    }
}
#endif

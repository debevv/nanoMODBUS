#include "modbusino.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

mbsn_error read_byte(uint8_t* b) {}

mbsn_error write_byte(uint8_t b) {}

int main() {
    mbsn_t mbsn = {0};
    mbsn_error err = mbsn_server_create(
            &mbsn,
            (mbsn_transport_conf){.transport = MBSN_TRANSPORT_TCP, .read_byte = read_byte, .write_byte = write_byte});

    assert(err == MBSN_ERROR_NONE);

    mbsn_bitfield bf;
    memset(bf, 0, sizeof(bf));

    mbsn_bitfield_write(bf, 7, true);
    printf("%d\n", mbsn_bitfield_read(bf, 7));

    mbsn_bitfield_write(bf, 22, true);
    printf("%d\n", mbsn_bitfield_read(bf, 22));

    mbsn_bitfield_write(bf, 0, true);
    printf("%d\n", mbsn_bitfield_read(bf, 0));

    mbsn_bitfield_write(bf, 24, true);
    printf("%d\n", mbsn_bitfield_read(bf, 24));

    return 0;
}

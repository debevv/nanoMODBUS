#include "modbusino.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    mbsn_t mbsn = {0};
    mbsn_error err = mbsn_client_create(&mbsn, MBSN_TRANSPORT_TCP);
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

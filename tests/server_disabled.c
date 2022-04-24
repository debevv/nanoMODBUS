#define NMBS_SERVER_DISABLED

#include <unistd.h>

#include "nanomodbus.h"

#define UNUSED_PARAM(x) ((x) = (x))


int read_byte_empty(uint8_t* b, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int write_byte_empty(uint8_t b, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


void platform_sleep(uint32_t milliseconds, void* arg) {
    UNUSED_PARAM(arg);
    usleep(milliseconds * 1000);
}

int main() {
    nmbs_t nmbs;

    nmbs_platform_conf platform_conf_empty = {.transport = NMBS_TRANSPORT_TCP,
                                              .read_byte = read_byte_empty,
                                              .write_byte = write_byte_empty,
                                              .sleep = platform_sleep};

    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf_empty);
    if(err != 0)
        return 1;

    return 0;
}

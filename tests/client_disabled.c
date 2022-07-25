#include "nanomodbus.h"

#define UNUSED_PARAM(x) ((x) = (x))


int32_t read_empty(uint8_t* b, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int32_t write_empty(const uint8_t* b, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(b);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int main() {
    nmbs_t nmbs;

    nmbs_platform_conf platform_conf_empty = {
            .transport = NMBS_TRANSPORT_TCP,
            .read = read_empty,
            .write = write_empty,
    };

    nmbs_callbacks callbacks_empty = {0};

    nmbs_error err = nmbs_server_create(&nmbs, 1, &platform_conf_empty, &callbacks_empty);
    if (err != 0)
        return 1;

    return 0;
}

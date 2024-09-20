#include "nanomodbus.h"

#define UNUSED_PARAM(x) ((x) = (x))


int32_t read_empty(uint8_t* data, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(data);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int32_t write_empty(const uint8_t* data, uint16_t count, int32_t timeout, void* arg) {
    UNUSED_PARAM(data);
    UNUSED_PARAM(count);
    UNUSED_PARAM(timeout);
    UNUSED_PARAM(arg);
    return 0;
}


int main(int argc, char* argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    nmbs_t nmbs;

    nmbs_platform_conf platform_conf_empty;
    nmbs_platform_conf_create(&platform_conf_empty);
    platform_conf_empty.transport = NMBS_TRANSPORT_TCP;
    platform_conf_empty.read = read_empty;
    platform_conf_empty.write = write_empty;

    nmbs_error err = nmbs_client_create(&nmbs, &platform_conf_empty);
    if (err != 0)
        return 1;

    return 0;
}

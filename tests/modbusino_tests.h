#include "modbusino.h"
#include <time.h>
#include <unistd.h>

unsigned int nesting = 0;

#define fail() (assert(false))

#define should(s)                                                                                                      \
    {                                                                                                                  \
        for (int i = 0; i < nesting; i++) {                                                                            \
            printf("\t");                                                                                              \
        }                                                                                                              \
        printf("Should %s\n", (s));                                                                                    \
    }

#define expect(c) assert(c)

#define check(err) (expect((err) == MBSN_ERROR_NONE))

#define reset(mbsn) (memset(&(mbsn), 0, sizeof(mbsn_t)))

#define test(f) (nesting++, (f), nesting--)


const uint8_t TEST_SERVER_ADDR = 1;

static uint64_t now_ms() {
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t) (ts.tv_sec) * 1000 + (uint64_t) (ts.tv_nsec) / 1000000;
}

int read_byte_empty(uint8_t* b, uint32_t timeout) {
    return 0;
}

int write_byte_empty(uint8_t b, uint32_t timeout) {
    return 0;
}

int read_byte_timeout_1s(uint8_t* b, uint32_t timeout) {
    usleep(timeout * 1000);
    return 0;
}

int read_byte_timeout_third(uint8_t* b, uint32_t timeout) {
    static int stage = 0;
    switch (stage) {
        case 0:
        case 1:
            *b = 1;
            stage++;
            return 1;
        case 2:
            usleep(timeout * 1000 + 100);
            stage = 0;
            return 0;
        default:
            stage = 0;
            return -1;
    }
}

#include "modbusino.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define expect(c) (assert(c))

#define check(err) (expect((err) == MBSN_ERROR_NONE))

#define reset(mbsn) (memset(&(mbsn), 0, sizeof(mbsn_t)))

#define test(f) (nesting++, (f), nesting--)

/*
#define htons(w) (((((uint16_t) (w) &0xFF)) << 8) | (((uint16_t) (w) &0xFF00) >> 8))
#define ntohs(w) (((((uint16_t) (w) &0xFF)) << 8) | (((uint16_t) (w) &0xFF00) >> 8))
*/

const uint8_t TEST_SERVER_ADDR = 1;


unsigned int nesting = 0;

int sockets[2] = {-1, -1};

bool server_stopped = true;
pthread_mutex_t server_stopped_m = PTHREAD_MUTEX_INITIALIZER;
pthread_t server_thread;
mbsn_t CLIENT, SERVER;


#define should(s)                                                                                                      \
    for (int i = 0; i < nesting; i++) {                                                                                \
        printf("\t");                                                                                                  \
    }                                                                                                                  \
    printf("Should %s\n", (s))


uint64_t now_ms() {
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t) (ts.tv_sec) * 1000 + (uint64_t) (ts.tv_nsec) / 1000000;
}


void platform_sleep(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}


void reset_sockets() {
    if (sockets[0] != -1)
        close(sockets[0]);

    if (sockets[1] != -1)
        close(sockets[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
}


int read_byte_fd(int fd, uint8_t* b, int32_t timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval* tv_p = NULL;
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv_p = &tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }

    int ret = select(fd + 1, &rfds, NULL, NULL, tv_p);
    if (ret == 0) {
        return 0;
    }
    else if (ret == 1) {
        ssize_t r = read(fd, b, 1);
        if (r != 1)
            return -1;
        else {
            return 1;
        }
    }
    else
        return -1;
}


int write_byte_fd(int fd, uint8_t b, int32_t timeout_ms) {
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    struct timeval* tv_p = NULL;
    struct timeval tv;
    if (timeout_ms >= 0) {
        tv_p = &tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }

    int ret = select(fd + 1, NULL, &wfds, NULL, tv_p);
    if (ret == 0) {
        return 0;
    }
    else if (ret == 1) {
        ssize_t r = write(fd, &b, 1);
        if (r != 1)
            return -1;
        else {
            return 1;
        }
    }
    else
        return -1;
}


int read_byte_socket_server(uint8_t* b, int32_t timeout_ms) {
    return read_byte_fd(sockets[0], b, timeout_ms);
}


int write_byte_socket_server(uint8_t b, int32_t timeout_ms) {
    return write_byte_fd(sockets[0], b, timeout_ms);
}


int read_byte_socket_client(uint8_t* b, int32_t timeout_ms) {
    return read_byte_fd(sockets[1], b, timeout_ms);
}


int write_byte_socket_client(uint8_t b, int32_t timeout_ms) {
    return write_byte_fd(sockets[1], b, timeout_ms);
}


mbsn_platform_conf mbsn_platform_conf_server;
mbsn_platform_conf* platform_conf_socket_server(mbsn_transport transport) {
    mbsn_platform_conf_server.transport = transport;
    mbsn_platform_conf_server.read_byte = read_byte_socket_server;
    mbsn_platform_conf_server.write_byte = write_byte_socket_server;
    mbsn_platform_conf_server.sleep = platform_sleep;
    return &mbsn_platform_conf_server;
}


mbsn_platform_conf mbsn_platform_conf_client;
mbsn_platform_conf* platform_conf_socket_client(mbsn_transport transport) {
    mbsn_platform_conf_client.transport = transport;
    mbsn_platform_conf_client.read_byte = read_byte_socket_client;
    mbsn_platform_conf_client.write_byte = write_byte_socket_client;
    mbsn_platform_conf_client.sleep = platform_sleep;
    return &mbsn_platform_conf_client;
}


bool is_server_listen_thread_stopped() {
    bool stopped = false;
    assert(pthread_mutex_lock(&server_stopped_m) == 0);
    stopped = server_stopped;
    assert(pthread_mutex_unlock(&server_stopped_m) == 0);
    return stopped;
}


void* server_listen_thread() {
    while (true) {
        if (is_server_listen_thread_stopped())
            break;

        check(mbsn_server_receive(&SERVER));
    }

    return NULL;
}


void stop_client_and_server() {
    if (!is_server_listen_thread_stopped()) {
        assert(pthread_mutex_lock(&server_stopped_m) == 0);
        server_stopped = true;
        assert(pthread_mutex_unlock(&server_stopped_m) == 0);
        assert(pthread_join(server_thread, NULL) == 0);
    }
}


void start_client_and_server(mbsn_transport transport, mbsn_callbacks server_callbacks) {
    assert(pthread_mutex_destroy(&server_stopped_m) == 0);
    assert(pthread_mutex_init(&server_stopped_m, NULL) == 0);

    reset_sockets();

    reset(SERVER);
    reset(CLIENT);

    check(mbsn_server_create(&SERVER, TEST_SERVER_ADDR, platform_conf_socket_server(transport), server_callbacks));
    check(mbsn_client_create(&CLIENT, platform_conf_socket_client(transport)));

    mbsn_set_destination_rtu_address(&CLIENT, TEST_SERVER_ADDR);
    mbsn_set_read_timeout(&SERVER, 500);
    mbsn_set_byte_timeout(&SERVER, 100);

    mbsn_set_read_timeout(&CLIENT, 5000);
    mbsn_set_byte_timeout(&CLIENT, 100);

    assert(pthread_mutex_lock(&server_stopped_m) == 0);
    server_stopped = false;
    assert(pthread_mutex_unlock(&server_stopped_m) == 0);
    assert(pthread_create(&server_thread, NULL, server_listen_thread, &SERVER) == 0);
}

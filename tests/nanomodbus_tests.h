
#include "nanomodbus.h"
#undef NDEBUG

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#ifdef _WIN32
#include <windows.h>
typedef SOCKET socket_t;
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
typedef suseconds_t __suseconds_t;
#endif
#include <unistd.h>


#define expect(expr) assert(expr)

#define check(err) (expect((err) == NMBS_ERROR_NONE))

#define reset(nmbs) (memset(&(nmbs), 0, sizeof(nmbs_t)))

#define test(f) (nesting++, (f), nesting--)

#define UNUSED_PARAM(x) ((x) = (x))

const uint8_t TEST_SERVER_ADDR = 1;
unsigned int nesting = 0;

#ifdef _WIN32
WSADATA wsaData;
SOCKET socks[2];
int create_socket_pair(SOCKET socks[2]);
#else
#endif


int sockets[2] = {-1, -1};

bool server_stopped = true;
pthread_mutex_t server_stopped_m = PTHREAD_MUTEX_INITIALIZER;
pthread_t server_thread;
nmbs_t CLIENT, SERVER;


#define should(s)                                                                                                      \
    for (unsigned int i = 0; i < nesting; i++) {                                                                       \
        printf("\t");                                                                                                  \
    }                                                                                                                  \
    printf("Should %s\n", (s))


uint64_t now_ms(void) {
    uint64_t milliseconds;

#if defined(_WIN32)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    milliseconds = (counter.QuadPart * 1000000LL) / frequency.QuadPart;
#else
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    milliseconds = ts.tv_sec * 1000LL + (uint64_t) ts.tv_nsec / 1000000;
#endif
    //return (uint64_t) (ts.tv_sec) * 1000 + (uint64_t) (ts.tv_nsec) / 1000000;
    return milliseconds;
}


void reset_sockets(void) {
    if (sockets[0] != -1)
        close(sockets[0]);

    if (sockets[1] != -1)
        close(sockets[1]);

#ifdef _WIN32
    expect(create_socket_pair(socks) == 0);
    sockets[0] = socks[0];
    sockets[1] = socks[1];
#else
    expect(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
#endif
}


int32_t read_fd(int fd, uint8_t* buf, uint16_t count, int32_t timeout_ms) {
    uint16_t total = 0;
    while (total != count) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval* tv_p = NULL;
        struct timeval tv;
        if (timeout_ms >= 0) {
            tv_p = &tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = ((int32_t) timeout_ms % 1000) * 1000;
        }

        int ret = select(fd + 1, &rfds, NULL, NULL, tv_p);
        if (ret == 0) {
            return total;
        }

        if (ret == 1) {
#ifdef _WIN32
            ssize_t r = recv(fd, (char*) buf + total, 1, 0);
#else
            ssize_t r = read(fd, buf + total, 1);
#endif
            if (r <= 0)
                return -1;

            total += r;
        }
        else
            return -1;
    }

    return total;
}


int32_t write_fd(int fd, const uint8_t* buf, uint16_t count, int32_t timeout_ms) {
    uint16_t total = 0;
    while (total != count) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);

        struct timeval* tv_p = NULL;
        struct timeval tv;
        if (timeout_ms >= 0) {
            tv_p = &tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = ((int32_t) timeout_ms % 1000) * 1000;
        }

        int ret = select(fd + 1, NULL, &wfds, NULL, tv_p);
        if (ret == 0) {
            return 0;
        }

        if (ret == 1) {
#ifdef _WIN32
            ssize_t w = send(fd, (const char*) buf + total, count, 0);
#else
            ssize_t w = write(fd, buf + total, count);
#endif
            if (w <= 0)
                return -1;

            total += w;
        }
        else
            return -1;
    }

    return total;
}


int32_t read_socket_server(uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    UNUSED_PARAM(arg);
    return read_fd(sockets[0], buf, count, timeout_ms);
}


int32_t write_socket_server(const uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    UNUSED_PARAM(arg);
    return write_fd(sockets[0], buf, count, timeout_ms);
}


int32_t read_socket_client(uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    UNUSED_PARAM(arg);
    return read_fd(sockets[1], buf, count, timeout_ms);
}


int32_t write_socket_client(const uint8_t* buf, uint16_t count, int32_t timeout_ms, void* arg) {
    UNUSED_PARAM(arg);
    return write_fd(sockets[1], buf, count, timeout_ms);
}


nmbs_platform_conf nmbs_platform_conf_server;
nmbs_platform_conf* platform_conf_socket_server(nmbs_transport transport) {
    nmbs_platform_conf_create(&nmbs_platform_conf_server);
    nmbs_platform_conf_server.transport = transport;
    nmbs_platform_conf_server.read = read_socket_server;
    nmbs_platform_conf_server.write = write_socket_server;
    return &nmbs_platform_conf_server;
}


nmbs_platform_conf nmbs_platform_conf_client;
nmbs_platform_conf* platform_conf_socket_client(nmbs_transport transport) {
    nmbs_platform_conf_create(&nmbs_platform_conf_client);
    nmbs_platform_conf_client.transport = transport;
    nmbs_platform_conf_client.read = read_socket_client;
    nmbs_platform_conf_client.write = write_socket_client;
    return &nmbs_platform_conf_client;
}


bool is_server_listen_thread_stopped(void) {
    bool stopped = false;
    expect(pthread_mutex_lock(&server_stopped_m) == 0);
    stopped = server_stopped;
    expect(pthread_mutex_unlock(&server_stopped_m) == 0);
    return stopped;
}


void* server_listen_thread(void* arg) {
    UNUSED_PARAM(arg);
    while (true) {
        if (is_server_listen_thread_stopped())
            break;
        check(nmbs_server_poll(&SERVER));
        // nmbs_error err = nmbs_server_poll(&SERVER);
        // asm("nop");
        // printf("server err: %d\n", err);
        // check(err);
    }

    return NULL;
}

void stop_client_and_server(void) {
    if (!is_server_listen_thread_stopped()) {
        expect(pthread_mutex_lock(&server_stopped_m) == 0);
        server_stopped = true;
        expect(pthread_mutex_unlock(&server_stopped_m) == 0);
        expect(pthread_join(server_thread, NULL) == 0);
    }
}


void start_client_and_server(nmbs_transport transport, const nmbs_callbacks* server_callbacks) {
    expect(pthread_mutex_destroy(&server_stopped_m) == 0);
    expect(pthread_mutex_init(&server_stopped_m, NULL) == 0);

    reset_sockets();

    reset(SERVER);
    reset(CLIENT);

    check(nmbs_server_create(&SERVER, TEST_SERVER_ADDR, platform_conf_socket_server(transport), server_callbacks));
    check(nmbs_client_create(&CLIENT, platform_conf_socket_client(transport)));

    nmbs_set_destination_rtu_address(&CLIENT, TEST_SERVER_ADDR);
    nmbs_set_read_timeout(&SERVER, 500);
    nmbs_set_byte_timeout(&SERVER, 100);

    nmbs_set_read_timeout(&CLIENT, 1000);
    nmbs_set_byte_timeout(&CLIENT, 100);

    expect(pthread_mutex_lock(&server_stopped_m) == 0);
    server_stopped = false;
    expect(pthread_mutex_unlock(&server_stopped_m) == 0);
    expect(pthread_create(&server_thread, NULL, server_listen_thread, &SERVER) == 0);
}


int create_socket_pair(SOCKET socks[2]) {

    WSACleanup();
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return -1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET)
        return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    bind(listener, (struct sockaddr*) &addr, sizeof(addr));
    listen(listener, 1);

    int addrlen = sizeof(addr);
    getsockname(listener, (struct sockaddr*) &addr, &addrlen);

    socks[0] = socket(AF_INET, SOCK_STREAM, 0);
    connect(socks[0], (struct sockaddr*) &addr, sizeof(addr));

    socks[1] = accept(listener, NULL, NULL);
    closesocket(listener);
    return 0;
}
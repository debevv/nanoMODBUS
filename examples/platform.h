#include "nanomodbus.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>


// Read/write/sleep platform functions

int read_byte_fd_linux(uint8_t* b, int32_t timeout_ms, void* arg) {
    int fd = *(int*) arg;

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


int write_byte_fd_linux(uint8_t b, int32_t timeout_ms, void* arg) {
    int fd = *(int*) arg;

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


void sleep_linux(uint32_t milliseconds, void* arg) {
    usleep(milliseconds * 1000);
}


// Connection management

int client_connection = -1;

void* connect_tcp(const char* address, const char* port) {
    struct addrinfo ainfo = {0};
    struct addrinfo *results, *rp;
    int fd;

    ainfo.ai_family = AF_INET;
    ainfo.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo(address, port, &ainfo, &results);
    if (ret != 0)
        return NULL;

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        ret = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0)
            break;
        else
            close(fd);
    }

    freeaddrinfo(results);

    if (rp == NULL)
        return NULL;

    client_connection = fd;
    return &client_connection;
}


int server_fd = -1;
int client_read_fd = -1;
fd_set client_connections;

void close_server_on_exit(int sig) {
    if (server_fd != -1)
        close(server_fd);
}


int create_tcp_server(const char* address, const char* port) {
    struct addrinfo ainfo = {0};
    struct addrinfo *results, *rp;
    int fd = -1;

    ainfo.ai_family = AF_INET;
    ainfo.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo(address, port, &ainfo, &results);
    if (ret != 0)
        return -1;

    for (rp = results; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
        if (ret != 0)
            return -1;

        ret = bind(fd, rp->ai_addr, rp->ai_addrlen);
        if (ret != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        ret = listen(fd, 1);
        if (ret != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(results);

    signal(SIGINT, close_server_on_exit);
    signal(SIGTERM, close_server_on_exit);
    signal(SIGQUIT, close_server_on_exit);
    signal(SIGSTOP, close_server_on_exit);
    signal(SIGHUP, close_server_on_exit);

    server_fd = fd;
    FD_ZERO(&client_connections);

    return 0;
}


void* server_poll() {
    fd_set read_fd_set;
    FD_ZERO(&read_fd_set);

    while (true) {
        read_fd_set = client_connections;
        FD_SET(server_fd, &read_fd_set);

        int ret = select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL);
        if (ret < 0)
            return NULL;

        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (FD_ISSET(i, &read_fd_set)) {
                if (i == server_fd) {
                    struct sockaddr_in client_addr;
                    socklen_t client_addr_size = sizeof(client_addr);

                    int client = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_size);
                    if (client < 0) {
                        fprintf(stderr, "Error accepting client connection from %s - %s\n",
                                inet_ntoa(client_addr.sin_addr), strerror(errno));
                        continue;
                    }

                    FD_SET(client, &client_connections);
                    printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
                }
                else {
                    client_read_fd = i;
                    return &client_read_fd;
                }
            }
        }
    }
}


void disconnect(void* conn) {
    int fd = *(int*) conn;
    close(fd);
}


void close_server() {
    close(server_fd);
}

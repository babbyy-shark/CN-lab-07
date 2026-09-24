#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include "protocol.h"

int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len > 0) {
        ssize_t sent = send(fd, p, len, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += sent;
        len -= sent;
    }
    return 0;
}

int recv_all(int fd, void *buf, size_t len) {
    char *p = (char *)buf;
    while (len > 0) {
        ssize_t r = recv(fd, p, len, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return -1;
        p += r;
        len -= r;
    }
    return 0;
}
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "protocol.h"

/* Loops until all bytes are sent over the socket */
int send_all(int fd, const void *buf, size_t len) {
    size_t total_sent = 0;
    const char *ptr = buf;

    while (total_sent < len) {
        ssize_t n = send(fd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by a signal, retry
            }
            return -1; // Actual send error
        }
        total_sent += n;
    }
    return 0;
}

/* Loops until all expected bytes are received from the socket */
int recv_all(int fd, void *buf, size_t len) {
    size_t total_received = 0;
    char *ptr = buf;

    while (total_received < len) {
        ssize_t n = recv(fd, ptr + total_received, len - total_received, 0);
        if (n == 0) {
            return -1; // Peer closed the connection prematurely
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by a signal, retry
            }
            return -1; // Actual receive error
        }
        total_received += n;
    }
    return 0;
}
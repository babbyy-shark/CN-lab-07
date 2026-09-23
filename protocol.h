#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

#define REQ_ITEM  0
#define REQ_CLOSE 1

#define RESP_OK   0
#define RESP_ERR  1

#define MSG_LEN   128

typedef struct {
    int request_type; /* 0 = item, 1 = close */
    int upc;          /* 3-digit code (ignored for close) */
    int number;       /* quantity (ignored for close) */
} Request;

typedef struct {
    int response_type;      /* 0 = ok, 1 = error */
    char response[MSG_LEN]; /* "<price> <name>", total, or error text */
} Response;

/* common.c */
int send_all(int fd, const void *buf, size_t len); /* 0 = ok, -1 = fail */
int recv_all(int fd, void *buf, size_t len);       /* 0 = ok, -1 = fail or closed */

/* db.c */
/* returns 1 if found, 0 if not found */
int lookup_product(const char *dbfile, int upc, double *price, char *name, size_t namelen);

/* handler.c */
/* returns 1 if the connection should close after this reply, else 0 */
int handle_request(const Request *req, Response *resp, double *total);

#endif /* PROTOCOL_H */
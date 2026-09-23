#include <string.h>
#include <stdio.h>
#include "protocol.h"

extern int lookup_product(const char *dbfile, int upc, double *price, char *name, size_t namelen);

int handle_request(const Request *req, Response *resp, double *total) {
    memset(resp, 0, sizeof(*resp));

    if (req->request_type == REQ_ITEM) {
        if (req->number <= 0 || req->upc < 100 || req->upc > 999) {
            resp->response_type = RESP_ERR;
            strcpy(resp->response, "Protocol Error");
            return 0; 
        }

        double price;
        char name[MSG_LEN];
        if (lookup_product("products.txt", req->upc, &price, name, sizeof(name)) == 0) {
            resp->response_type = RESP_ERR;
            strcpy(resp->response, "UPC is not found in database");
            return 0; 
        }
        
        *total += price * req->number;
        resp->response_type = RESP_OK;
        snprintf(resp->response, MSG_LEN, "%.2f %s", price, name);
        return 0;
        
    } else if (req->request_type == REQ_CLOSE) {
        resp->response_type = RESP_OK;
        snprintf(resp->response, MSG_LEN, "%.2f", *total);
        return 1;
    }

    resp->response_type = RESP_ERR;
    strcpy(resp->response, "Protocol Error");
    return 0;
}
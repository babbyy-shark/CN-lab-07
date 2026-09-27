#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "protocol.h"

static void clear_input_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./client <server_ip> <port>\n");
        return 1;
    }

    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    while (1) {
        printf("\n1) Buy item\n2) Close and get total\nChoice: ");
        fflush(stdout); 

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        int max_fd = (STDIN_FILENO > sock) ? STDIN_FILENO : sock;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            break;
        }

        if (FD_ISSET(sock, &readfds)) {
            char dummy;
            if (recv(sock, &dummy, 1, MSG_PEEK) <= 0) {
                printf("\nServer disconnected\n");
                close(sock);
                return 1;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            int choice;
            int matched = scanf("%d", &choice);

            if (matched == EOF) {
                break;
            }

            if (matched != 1) {
                clear_input_line();
                printf("Invalid choice. Please enter 1 or 2.\n");
                continue;
            }

            Request req;
            memset(&req, 0, sizeof(req));

            if (choice == 1) {
                int upc, quantity;
                while (1) {
                    printf("Enter UPC code and quantity: ");
                    fflush(stdout);

                    fd_set inner_readfds;
                    FD_ZERO(&inner_readfds);
                    FD_SET(STDIN_FILENO, &inner_readfds);
                    FD_SET(sock, &inner_readfds);

                    int inner_max_fd = (STDIN_FILENO > sock) ? STDIN_FILENO : sock;

                    if (select(inner_max_fd + 1, &inner_readfds, NULL, NULL, NULL) < 0) {
                        perror("select failed");
                        break;
                    }

                    if (FD_ISSET(sock, &inner_readfds)) {
                        char dummy;
                        if (recv(sock, &dummy, 1, MSG_PEEK) <= 0) {
                            printf("\nServer disconnected\n");
                            close(sock);
                            return 1;
                        }
                    }

                    if (FD_ISSET(STDIN_FILENO, &inner_readfds)) {
                        int ret = scanf("%d %d", &upc, &quantity);
                        if (ret == EOF) {
                            close(sock);
                            return 0;
                        }
                        if (ret == 2) {
                            clear_input_line();
                            break;
                        }
                        printf("Invalid input. Please enter numeric values.\n");
                        clear_input_line();
                    }
                }

                req.request_type = REQ_ITEM;
                req.upc = upc;
                req.number = quantity;
            } else if (choice == 2) {
                clear_input_line();
                req.request_type = REQ_CLOSE;
                req.upc = 0;
                req.number = 0;
            } else {
                clear_input_line();
                printf("Invalid choice. Please enter 1 or 2.\n");
                continue;
            }

            Request wire_req;
            wire_req.request_type = htonl(req.request_type);
            wire_req.upc = htonl(req.upc);
            wire_req.number = htonl(req.number);

            if (send_all(sock, &wire_req, sizeof(wire_req)) != 0) {
                printf("Server disconnected\n");
                close(sock);
                return 1;
            }

            Response resp;
            if (recv_all(sock, &resp, sizeof(resp)) != 0) {
                printf("Server disconnected\n");
                close(sock);
                return 1;
            }

            resp.response_type = ntohl(resp.response_type);
            resp.response[MSG_LEN - 1] = '\0';

            if (req.request_type == REQ_ITEM) {
                if (resp.response_type == RESP_OK) {
                    printf("Price and item: %s\n", resp.response);
                } else {
                    printf("%s\n", resp.response);
                }
            } else if (req.request_type == REQ_CLOSE) {
                if (resp.response_type == RESP_OK) {
                    printf("Total amount: %s\n", resp.response);
                } else {
                    printf("%s\n", resp.response);
                }
                close(sock);
                return 0;
            }
        }
    }

    close(sock);
    return 0;
}
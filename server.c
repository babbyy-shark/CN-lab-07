#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "protocol.h" 

int listenfd = -1;

void handle_sigint(int sig) {
    (void)sig;
    if (listenfd != -1) {
        close(listenfd); 
    }
    const char msg[] = "\nServer stopped by user. Socket released.\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    _exit(0); 
}

void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./server <port>\n");
        exit(1);
    }
    int port = atoi(argv[1]);

    printf("Starting server on port %d...\n", port);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(port); 

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(listenfd, 10) == -1) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("Server listening on port %d\n", port);

    struct sigaction sa_int, sa_chld;

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sa_chld.sa_flags = SA_RESTART; 
    sigaction(SIGCHLD, &sa_chld, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd == -1) {
            if (errno == EINTR) {
                continue; 
            }
            perror("Accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        pid_t pid = fork();
        
        if (pid == -1) {
            perror("Fork failed");
            close(connfd);
            continue;
        }

        if (pid == 0) {
            close(listenfd); 
            
            double total = 0.0;
            Request req;
            Response resp;

            while (1) {
                if (recv_all(connfd, &req, sizeof(req)) != 0) {
                    printf("Client %s disconnected abruptly.\n", client_ip);
                    break;
                }

                req.request_type = ntohl(req.request_type);
                req.upc = ntohl(req.upc);
                req.number = ntohl(req.number);

                int done = handle_request(&req, &resp, &total);

                resp.response_type = htonl(resp.response_type);

                if (send_all(connfd, &resp, sizeof(resp)) != 0) {
                    printf("Failed to send response to %s.\n", client_ip);
                    break;
                }

                if (done == 1) {
                    printf("Client %s closed normally.\n", client_ip);
                    break; 
                }
            }
            
            close(connfd);
            exit(0); 

        } else {
            close(connfd); 
        }
    }

    return 0;
}
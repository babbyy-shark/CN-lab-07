#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "protocol.h" // Adithyan's shared file

// 5. Keep the listening fd in a global variable so the signal handler can close it.
int listenfd = -1;

// A temporary fake handler until Harshit writes the real one
int handle_request(const Request *req, Response *resp, double *total) {
    resp->response_type = RESP_OK;
    strcpy(resp->response, "Dummy response from server");
    if (req->request_type == REQ_CLOSE) return 1;
    return 0;
}

// Handler for Ctrl+C (SIGINT)
void handle_sigint(int sig) {
    if (listenfd != -1) {
        close(listenfd); // Close the global listening socket
    }
    // We use write() instead of printf() because it is safer inside signal handlers
    const char msg[] = "\nServer stopped by user. Socket released.\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    _exit(0); // Exit the program
}

// Handler for when a child process finishes (SIGCHLD)
void handle_sigchld(int sig) {
    // waitpid() cleans up the zombie. 
    // -1 means "wait for any child", WNOHANG means "don't pause the server if no child is ready"
    // We loop because multiple children might finish at the exact same time.
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Do nothing, just loop until waitpid stops returning > 0
    }
}

int main(int argc, char **argv) {
    // 1. Check argc == 2. If not, print Usage and exit. Read the port.
    if (argc != 2) {
        printf("Usage: ./server <port>\n");
        exit(1);
    }
    int port = atoi(argv[1]);

    printf("Starting server on port %d...\n", port);

        // 2. Create the listening socket: socket(AF_INET, SOCK_STREAM, 0)
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // 3. Call setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, ...)
    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        exit(1);
    }

    // 4. Fill a sockaddr_in and call bind(), then listen()
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // Clear the struct
    server_addr.sin_family = AF_INET;             // IPv4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Any IP address on this machine
    server_addr.sin_port = htons(port);           // The port they typed in

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(listenfd, 10) == -1) {
        perror("Listen failed");
        exit(1);
    }
    
    printf("Server listening on port %d\n", port);

        // 6. Register handlers with sigaction
    struct sigaction sa_int, sa_chld;

    // Register Ctrl+C handler
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    // Register Zombie cleanup handler
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = handle_sigchld;
    sa_chld.sa_flags = SA_RESTART; // Important! Prevents interrupted system calls
    sigaction(SIGCHLD, &sa_chld, NULL);

        // 7. Main loop to accept clients
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Wait for a client to connect
        int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd == -1) {
            if (errno == EINTR) {
                continue; // We were interrupted by a signal (like a dying child), just retry
            }
            perror("Accept failed");
            continue;
        }

        // 8. Print the client's IP and port for debugging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // 9. Fork: Create a clone to handle this specific client
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("Fork failed");
            close(connfd);
            continue;
        }

        if (pid == 0) {
            // --- WE ARE IN THE CHILD PROCESS ---
            close(listenfd); // The child doesn't need to listen for new clients
            
            double total = 0.0; // 9. Each child has its own separate running total
            Request req;
            Response resp;

            // 10. Child loop: Talk to the client until they leave or send a CLOSE request
            while (1) {
                // Read a Request struct. recv_all returns <= 0 if the client left or an error happened.
                if (recv_all(connfd, &req, sizeof(req)) <= 0) {
                    printf("Client %s disconnected abruptly.\n", client_ip);
                    break;
                }

                // Convert the integers from Network byte order to Host byte order
                req.request_type = ntohl(req.request_type);
                req.upc = ntohl(req.upc);
                req.number = ntohl(req.number);

                // Call the business logic! (Currently our fake stub, later Harshit's real one)
                int done = handle_request(&req, &resp, &total);

                // Convert the response integer back to Network byte order before sending
                resp.response_type = htonl(resp.response_type);
                // Note: We don't convert resp.response because it's a char array (plain text), not an int.

                // Send the Response struct back to the client
                if (send_all(connfd, &resp, sizeof(resp)) <= 0) {
                    printf("Failed to send response to %s.\n", client_ip);
                    break;
                }

                // If handle_request returned 1, it was a CLOSE request. We break the loop.
                if (done == 1) {
                    printf("Client %s closed normally.\n", client_ip);
                    break; 
                }
            }
            
            // Child is done talking. Close the socket and terminate the child process.
            close(connfd);
            exit(0); 

        } else {
            // --- WE ARE IN THE PARENT PROCESS ---
            // The parent doesn't talk to the client, so it closes its copy of the connected socket
            close(connfd); 
            // The loop repeats and goes back to accept() to wait for the next client!
        }
    }

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 4096 // buffer size KiB

void handle_client(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];

    while (1) {
        // receive packet from client
        ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
        if (bytes_received == -1) {
            fprintf(stderr, "Receive failed\n");
            continue;
        }

        if (bytes_received == 0) {
            fprintf(stderr, "Client disconnected\n");
            break;
        }

        // echo bytes back to client
        sendto(sockfd, buffer, bytes_received, 0, (struct sockaddr *)client_addr, addr_len);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Port Number>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    printf("port = %d\n", port);

    // create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Socket creation failed\n");
        exit(1);
    }

    // initialize server address structure & bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Socket bind failed\n");
        close(sockfd);
        exit(1);
    }

    printf("Server listening on port: %d\n", port);

    // receive packet and echo back to client
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        handle_client(sockfd, &client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}

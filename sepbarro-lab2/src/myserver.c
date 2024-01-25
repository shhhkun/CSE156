#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

void handle_client(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];

    while (1) {
        // Receive packet from client
        ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
        if (bytes_received == -1) {
            perror("Receive failed");
            continue;
        }

        if (bytes_received == 0) {
            printf("Client disconnected\n");
            break;
        }

        // Echo the received payload back to the client
        sendto(sockfd, buffer, bytes_received, 0, (struct sockaddr *)client_addr, addr_len);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Port Number>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    // Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Initialize server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(sockfd);
        exit(1);
    }

    printf("Server listening on port %d\n", port);

    // Receive and echo packets
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        handle_client(sockfd, &client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}

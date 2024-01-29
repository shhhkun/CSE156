#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

void send_file(const char *server_ip, int server_port, int mtu, const char *infile_path, const char *outfile_path) {
    char buffer[mtu];

    // open infile in read bytes mode
    FILE *infile = fopen(infile_path, "rb");
    if (infile == NULL) {
        fprintf(stderr, "Error opening input file\n");
        exit(1);
    }

    // open outfile in write bytes mode
    FILE *outfile = fopen(outfile_path, "wb");
    if (outfile == NULL) {
        fprintf(stderr, "Error opening output file\n");
        fclose(infile);
        exit(1);
    }

    // create socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Socket creation failed\n");
        fclose(infile);
        fclose(outfile);
        exit(1);
    }

    // initialize server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    while (1) {
        size_t bytes_read = fread(buffer, 1, mtu, infile); // read file in sizes of max MTU
        if (bytes_read == 0) {
            break; // no more bytes/eof, exit infinite loop
        }

        // send packet to server
        if (sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            fprintf(stderr, "sendto() failed\n");
            fclose(infile);
            fclose(outfile);
            close(sockfd);
            exit(1);
        }

        // wait for ACK
        struct timeval timeout = {10, 0};  // 10 sec timeout
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        int ack_sn;

        ssize_t bytes_received = recvfrom(sockfd, &ack_sn, sizeof(ack_sn), 0, NULL, NULL);
        if (bytes_received == -1) {
            fprintf(stderr, "Packet loss detected\n"); // handle timeout (assume packet loss)
            fclose(infile);
            fclose(outfile);
            close(sockfd);
            exit(1);
        }
        
        fwrite(buffer, 1, bytes_read, outfile);
    }

    fclose(infile);
    fclose(outfile);
    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <Server IP> <Port Number> <MTU> <Infile Path> <Outfile Path>\n", argv[0]);
        exit(1);
    }

    // get CLI input
    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int mtu = atoi(argv[3]);
    const char *infile_path = argv[4];
    const char *outfile_path = argv[5];

    // send file to server in packets
    send_file(server_ip, server_port, mtu, infile_path, outfile_path);

    return 0;
}

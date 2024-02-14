#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 1024

char* get_timestamp() {
    time_t rawtime;
    struct tm *timeinfo;
    static char timestamp[30];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

    return timestamp;
}

void send_file(const char *server_ip, int server_port, int mtu, int winsz, const char *infile_path, const char *outfile_path) {
    char buffer[BUFFER_SIZE];
    FILE *infile = fopen(infile_path, "rb");
    FILE *outfile = fopen(outfile_path, "wb");
    if (infile == NULL || outfile == NULL) {
        perror("Error opening input/output file");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        fclose(infile);
        fclose(outfile);
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    size_t base = 0;
    size_t nextsn = 0;

    while (1) {
        // Send packets up to the window size
        for (size_t i = base; i < base + winsz && i < nextsn; i++) {
            // Read data from infile
            size_t bytes_read = fread(buffer, 1, mtu, infile);
            if (bytes_read == 0) {
                break; // No more data to send
            }

            // Send packet to server
            ssize_t bytes_sent = sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
            if (bytes_sent == -1) {
                perror("sendto() failed");
                fclose(infile);
                fclose(outfile);
                close(sockfd);
                exit(1);
            }

            // Log sequence number and window state
            printf("%s, DATA, %zu, %zu, %zu, %zu\n", get_timestamp(), nextsn, base, nextsn, base + winsz);

            nextsn++;
        }

        // Receive acknowledgements
        for (size_t i = 0; i < winsz; i++) {
            int ack_sn;
            ssize_t bytes_received = recv(sockfd, &ack_sn, sizeof(ack_sn), 0);
            if (bytes_received == -1) {
                perror("recv() failed");
                fclose(infile);
                fclose(outfile);
                close(sockfd);
                exit(1);
            }

            // Log acknowledgement numbers
            printf("%s, ACK, %d, %zu, %zu, %zu\n", get_timestamp(), ack_sn, base, nextsn, base + winsz);

            if (ack_sn == base) {
                base++;
            }
        }

        // Check for end of file and window
        if (feof(infile) && base == nextsn) {
            break;
        }
    }

    fclose(infile);
    fclose(outfile);
    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port> <MTU> <Window Size> <Infile Path> <Outfile Path>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int mtu = atoi(argv[3]);
    int winsz = atoi(argv[4]);
    const char *infile_path = argv[5];
    const char *outfile_path = argv[6];

    if (server_port <= 1023 || server_port > 65535) {
        fprintf(stderr, "Invalid server port number. Port must be in the range 1024-65535.\n");
        exit(1);
    }

    if (mtu <= 0 || winsz <= 0) {
        fprintf(stderr, "MTU and Window Size must be positive integers.\n");
        exit(1);
    }

    send_file(server_ip, server_port, mtu, winsz, infile_path, outfile_path);

    return 0;
}

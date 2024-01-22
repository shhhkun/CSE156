#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096
#define OUTPUT_FILE "output.dat"
#define DEFAULT_PORT "80"
#define DEFAULT_PATH "/"

void parse_server_address(const char *serv_addr, char **ip_address, char **port, char **path) {
    // Initialize outputs to default values
    *ip_address = strdup(serv_addr);
    *port = strdup(DEFAULT_PORT);
    *path = strdup(DEFAULT_PATH);

    // check if ":" is present in the server address
    char *temp = strdup(serv_addr);
    char *ip_port = strtok(temp, ":");
    if (ip_port != NULL) {
        free(*ip_address);
        *ip_address = strdup(ip_port);

        // check if ":" is present in the remaining part
        char *temp_port_path = strtok(NULL, "");
        if (temp_port_path != NULL) {
            // check if "/" is present in the remaining part
            char *temp_path = strchr(temp_port_path, '/');
            if (temp_path != NULL) {
                free(*path);
                *path = strdup(temp_path);

                // remove the path part from the remaining part
                *temp_path = '\0';
            }

            // assign the remaining part to port
            free(*port);
            *port = strdup(temp_port_path);
        }
    } else {
        // No ":" present, treat the whole address as IP and check for path
        char *temp_path = strchr(*ip_address, '/');
        if (temp_path != NULL) {
            free(*path);
            *path = strdup(temp_path);

            // remove the path part from the IP address
            *temp_path = '\0';
        }
    }

    // remove trailing '/' from IP address
    size_t len_ip = strlen(*ip_address);
    if (len_ip > 0 && (*ip_address)[len_ip - 1] == '/') {
        (*ip_address)[len_ip - 1] = '\0';
    }

    // Check if there is a "/" in the ip_address and update path accordingly
    char *temp_path_after_slash = strchr(*ip_address, '/');
    if (temp_path_after_slash != NULL) {
        free(*path);
        *path = malloc(strlen(temp_path_after_slash) + 2);  // +2 for including the leading "/"
        sprintf(*path, "/%s", temp_path_after_slash + 1);

        // remove the path part from the IP address
        *temp_path_after_slash = '\0';
    }

    free(temp);
}


void send_request(const char *hostname, const char *ip_address, const char *port, const char *path, int head_req) {
    int sockfd; // socket file descriptor
    int resp_header = 0; // flag to exclude HTTP response headers in output.dat file write
    struct sockaddr_in addr;
    char request[BUFFER_SIZE]; // request buffer
    FILE *output_file = NULL; // for output.dat file
    char buffer[BUFFER_SIZE]; // receiver buffer
    ssize_t bytes_read;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { // create socket
        fprintf(stderr, "Socket creation failed\n");
        exit(1);
    }

    // Initialize server address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port)); // Convert port to integer

    if (inet_pton(AF_INET, ip_address, &addr.sin_addr) <= 0) { // convert host (IPv4/IPv6) from text to binary
        fprintf(stderr, "Invalid/Unsupported address\n");
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) { // connect to server
        fprintf(stderr, "Connection failed\n");
        exit(1);
    }

    // format GET or HEAD HTTP requests
    if (head_req == 1) {
        snprintf(request, BUFFER_SIZE, "HEAD %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);
    } else {
        snprintf(request, BUFFER_SIZE, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);
    }

    if (send(sockfd, request, strlen(request), 0) == -1) { // send HTTP request
        fprintf(stderr, "Send failed\n");
        exit(1);
    }

    // client receives and prints according to GET or HEAD
    if (head_req == 0) { // if GET, open output file in write bytes mode
        output_file = fopen(OUTPUT_FILE, "wb");
    }

    while ((bytes_read = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0'; // null terminate string

        if (head_req == 1) { // if HEAD, print contents to stdout
            printf("%s", buffer);
        } else { // otherwise write contents to output file

            if (resp_header == 0) {
                char *end_header = strstr(buffer, "\r\n\r\n"); // check if buffer contains headers
                if (end_header != NULL) {
                    int offset = end_header - buffer + 4; // calculate offset relative to start of buffer and add 4 to account for "\r\n\r\n"
                    //printf("%ld\n", end_header - buffer);
                    fwrite(buffer + offset, 1, bytes_read - offset, output_file);
                }
                resp_header = 1;
            } else {
                fwrite(buffer, 1, bytes_read, output_file);
            }
        }
    }

    if (head_req == 0) { // if GET, close output file
        fclose(output_file);
    }

    close(sockfd); // close socket
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <Hostname> <Server Address> [-h]\n", argv[0]);
        exit(1);
    }

    const char *hostname = argv[1];
    const char *serv_addr = argv[2];
    int head_req;

    if (argc == 4 && strcmp(argv[3], "-h") == 0) {
        head_req = 1;
    } else {
        head_req = 0;
    }

    // parse server address as IP, port, and path
    char *ip_address, *port, *path;
    parse_server_address(serv_addr, &ip_address, &port, &path);

    printf("hostname: %s\n", hostname);
    printf("ip_address: %s\n", ip_address);
    printf("port: %s\n", port);
    printf("path: %s\n", path);

    send_request(hostname, ip_address, port, path, head_req);
    free(ip_address); // free dynamically allocated memory
    free(port);
    free(path);

    return 0;
}

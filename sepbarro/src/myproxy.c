#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 8192

// Function to handle SIGINT signal (Control-C)
void handle_sigint(int sig) {
    (void)sig; // Suppress unused parameter warning
    // Reload the forbidden sites file here
    // For now, let's just print a message
    printf("Reloading forbidden sites file...\n");
}

// Function to parse HTTP request and extract hostname and port
int parse_http_request(const char *request, char *hostname, char *path, int *port) {
    char method[10], url[2048], version[10];
    if (sscanf(request, "%s %s %s", method, url, version) != 3) {
        return -1;
    }

    if (strncmp(method, "GET", 3) != 0 && strncmp(method, "HEAD", 4) != 0) {
        return -1;
    }

    char *host_start = strstr(url, "//");
    if (host_start == NULL) {
        return -1;
    }

    host_start += 2; // Move past "//"

    char *host_end = strchr(host_start, '/');
    if (host_end == NULL) {
        return -1;
    }

    int len = host_end - host_start;
    strncpy(hostname, host_start, len);
    hostname[len] = '\0';

    char *port_start = strchr(hostname, ':');
    if (port_start != NULL) {
        *port = atoi(port_start + 1);
        *port_start = '\0';
    } else {
        *port = 80; // Default port
    }

    strcpy(path, host_end);

    return 0;
}

// Function to send HTTP response
void send_response(int client_sock, const char *status, const char *headers, const char *body) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%s\r\n%s\r\n%s\r\n\r\n", status, headers, body);
    send(client_sock, response, strlen(response), 0);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s listen_port forbidden_sites_file access_log_file\n", argv[0]);
        return 1;
    }

    // Set up SIGINT handler
    signal(SIGINT, handle_sigint);

    // Parse command line arguments
    int listen_port = atoi(argv[1]);
    // const char *forbidden_sites_file = argv[2]; // Unused variable
    const char *access_log_file = argv[3];

    // Load forbidden sites from file
    // Implement this part yourself

    // Set up socket for listening
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        return 1;
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listening failed");
        return 1;
    }

    printf("Proxy server listening on port %d...\n", listen_port);

    while (1) {
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accepting connection failed");
            continue;
        }

        // Handle client request
        char request_buffer[BUFFER_SIZE];
        recv(client_sock, request_buffer, sizeof(request_buffer), 0);

        // Parse HTTP request
        char hostname[2048], path[2048];
        int port;
        if (parse_http_request(request_buffer, hostname, path, &port) != 0) {
            // Invalid request
            send_response(client_sock, "HTTP/1.1 400 Bad Request", "", "");
            close(client_sock);
            continue;
        }

        // Check if hostname is forbidden
        // Implement this part yourself

        // Forward request to destination server
        int dest_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (dest_sock < 0) {
            perror("Socket creation failed");
            close(client_sock);
            continue;
        }

        struct hostent *dest_host = gethostbyname(hostname);
        if (dest_host == NULL) {
            perror("Error resolving hostname");
            close(client_sock);
            close(dest_sock);
            continue;
        }

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr = *((struct in_addr *)dest_host->h_addr);
        dest_addr.sin_port = htons(port);

        if (connect(dest_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("Connection to destination server failed");
            close(client_sock);
            close(dest_sock);
            continue;
        }

        send(dest_sock, request_buffer, strlen(request_buffer), 0);

        // Receive response from destination server
        char response_buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(dest_sock, response_buffer, sizeof(response_buffer), 0);
        if (bytes_received < 0) {
            perror("Error receiving response from destination server");
            close(client_sock);
            close(dest_sock);
            continue;
        }

        // Send response back to client
        send(client_sock, response_buffer, bytes_received, 0);

        // Log the request
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
        char log_entry[BUFFER_SIZE];
        snprintf(log_entry, sizeof(log_entry), "%s %s \"%s\" %s %zd\n", time_str,
                 inet_ntoa(client_addr.sin_addr), request_buffer, "200 OK", bytes_received);
        FILE *access_log = fopen(access_log_file, "a");
        if (access_log != NULL) {
            fprintf(access_log, "%s", log_entry);
            fclose(access_log);
        }

        // Close connections
        close(client_sock);
        close(dest_sock);
    }

    close(server_sock);
    return 0;
}

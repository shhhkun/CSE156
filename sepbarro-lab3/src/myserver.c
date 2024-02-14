// Include necessary libraries
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Define buffer size
#define BUFFER_SIZE 1024

// Function to get timestamp in RFC 3339 format
char *get_timestamp() {
  time_t rawtime;
  struct tm *timeinfo;
  static char timestamp[30];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  return timestamp;
}

// Function to process received packets
void process_packet(int sockfd, struct sockaddr_in *client_addr,
                    socklen_t addr_len, int droppc, char *outfile_path) {
  char buffer[BUFFER_SIZE];
  char log_message[100];

  // Simulate packet drop based on droppc
  int should_drop = (rand() % 100) < droppc;

  ssize_t bytes_received =
      recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr,
               &addr_len); // Receive packet
  if (bytes_received == -1) {
    perror("recvfrom() failed");
    return;
  }

  if (should_drop) {
    // Log dropped packet
    snprintf(log_message, sizeof(log_message), "# %s, DROP ACK, -\n",
             get_timestamp());
    printf("%s", log_message);
    return;
  }

  // Process received packet
  if (outfile_path[0] == '\0') {
    // First packet contains the outfile path
    strncpy(outfile_path, buffer, bytes_received);
    printf("Output file path received: %s\n", outfile_path);
  } else {
    // Subsequent packets contain file data
    FILE *outfile = fopen(outfile_path, "wb"); // "wb" write bytes mode
    if (outfile == NULL) {
      perror("Error opening output file");
      return;
    }
    fwrite(buffer, 1, bytes_received, outfile);
    fclose(outfile);
  }
}

// Function to start the server
void start_server(int port, int droppc) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // Create UDP socket
  if (sockfd == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("Socket bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", port);

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  char outfile_path[256] = ""; // Initialize to empty string

  while (1) {
    process_packet(sockfd, &client_addr, addr_len, droppc,
                   outfile_path); // Process incoming packets
  }

  close(sockfd);
}

// Main function
int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <Port Number> <Drop Percentage>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int port = atoi(argv[1]);
  int droppc = atoi(argv[2]);

  if (port <= 1023 || port > 65535) {
    fprintf(stderr,
            "Invalid port number. Port must be in the range 1024-65535.\n");
    exit(EXIT_FAILURE);
  }

  if (droppc < 0 || droppc > 100) {
    fprintf(
        stderr,
        "Invalid drop percentage. Percentage must be in the range 0-100.\n");
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));          // Initialize random seed
  start_server(port, droppc); // Start the server

  return 0;
}

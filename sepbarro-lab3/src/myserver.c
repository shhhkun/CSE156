#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

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
                    socklen_t addr_len, int droppc, char *outfile_path,
                    int *pktsn) {
  char buffer[BUFFER_SIZE];
  // char log_message[100];

  // Simulate packet drop based on droppc
  int should_drop = (rand() % 100) < droppc;

  ssize_t bytes_received =
      recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr,
               &addr_len); // Receive packet
  if (bytes_received == -1) {
    perror("recvfrom() failed");
    return;
  }

  // Log received packet
  printf("%s, DATA, %d\n", get_timestamp(), *pktsn);

  if (should_drop) {
    // Log dropped packet
    if (buffer[0] == 'A') {
      printf("%s, DROP ACK, %d\n", get_timestamp(), *pktsn);
    } else {
      printf("%s, DROP DATA, %d\n", get_timestamp(), *pktsn);
    }
    return;
  }

  // Process received packet
  if (outfile_path[0] == '\0') {
    // First packet contains the outfile path
    strncpy(outfile_path, buffer, bytes_received);
    printf("Output file path received: %s\n", outfile_path);
  } else {
    if (strcmp(buffer, outfile_path) != 0) {
      // Subsequent packets contain file data, write only if it's not the
      // outfile path
      FILE *outfile = fopen(outfile_path, "ab"); // "ab" append bytes mode
      if (outfile == NULL) {
        perror("Error opening output file");
        return;
      }

      // Check if it's not the first packet
      if (*pktsn > 0) {
        fwrite(buffer, 1, bytes_received, outfile);
      }
      fclose(outfile);
    }
  }

  // Send ACK for the received packet
  ssize_t bytes_sent = sendto(sockfd, pktsn, sizeof(*pktsn), 0,
                              (struct sockaddr *)client_addr, addr_len);
  if (bytes_sent == -1) {
    perror("sendto() failed");
    return;
  }

  // Log ACK packet
  printf("%s, ACK, %d\n", get_timestamp(), *pktsn);

  // Increment packet sequence number
  (*pktsn)++;
}

void start_server(int port, int droppc) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

  char outfile_path[256] = "";
  int pktsn = 0;

  while (1) {
    process_packet(sockfd, &client_addr, addr_len, droppc, outfile_path,
                   &pktsn);
    pktsn++;
  }

  close(sockfd);
}

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

  srand(time(NULL));
  start_server(port, droppc);

  return 0;
}

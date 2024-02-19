#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

void validport(int port) {
  if (0 <= port && port <= 1023) {
    fprintf(stderr, "Port number cannot be well-known: 0-1023\n");
    exit(1);
  } else if (port < 0) {
    fprintf(stderr, "Port number cannot be negative\n");
    exit(1);
  } else if (port > 65535) {
    fprintf(stderr, "Port number out of range, must be within: 1024-65535\n");
    exit(1);
  }
  return;
}

char *get_timestamp() {
  time_t rawtime;
  struct tm *timeinfo;
  static char timestamp[30];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  return timestamp;
}

void process_packet(int sockfd, struct sockaddr_in *client_addr,
                    socklen_t addr_len, int droppc, char *outfile_path,
                    int *pktsn) {
  char buffer[BUFFER_SIZE];

  // receive packet from client
  ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                    (struct sockaddr *)client_addr, &addr_len);
  if (bytes_received == -1) {
    fprintf(stderr, "recvfrom() failed\n");
    return;
  }

  // log received packet
  printf("%s, DATA, %d\n", get_timestamp(), *pktsn);

  printf("Received data: %s\n", buffer); // debug message

  // process first packet (contains outfile path)
  if (*pktsn == 0) {
    strncpy(outfile_path, buffer, bytes_received);
    printf("Output file path received: %s\n", outfile_path);
  } else {
    // droppc is applied for subsequent packets
    srand(time(NULL));
    int chance = rand() % 100;
    int should_drop = (chance) < droppc;
    printf("rand() %% 100 = %d\n", chance); // debug message

    // log dropped packet
    if (should_drop) {
      if (buffer[0] == 'A') {
        printf("%s, DROP ACK, %d\n", get_timestamp(), *pktsn);
      } else {
        printf("%s, DROP DATA, %d\n", get_timestamp(), *pktsn);
      }
      return;
    }

    // write buffered data to outfile path (but not first packet)
    if (strcmp(buffer, outfile_path) != 0) {
      FILE *outfile = fopen(outfile_path, "ab"); // append bytes mode
      if (outfile == NULL) {
        fprintf(stderr, "Error opening output file\n");
        return;
      }
      fwrite(buffer, 1, bytes_received, outfile);
      fclose(outfile);
    }
  }

  // send ACK to client for the received packet
  ssize_t bytes_sent = sendto(sockfd, pktsn, sizeof(*pktsn), 0,
                              (struct sockaddr *)client_addr, addr_len);
  if (bytes_sent == -1) {
    fprintf(stderr, "sendto() failed\n");
    return;
  }

  // log ACK packet
  printf("%s, ACK, %d\n", get_timestamp(), *pktsn);

  (*pktsn)++;
}

void start_server(int port, int droppc) {
  // create socket file descriptor
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    fprintf(stderr, "Socket creation failed\n");
    exit(1);
  }

  // initialize server address structure
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // bind socket
  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    fprintf(stderr, "Socket bind failed\n");
    close(sockfd);
    exit(1);
  }

  printf("Server listening on port %d\n", port);

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  char outfile_path[256] = "";
  int pktsn = 0;

  // process incoming packets one at a time
  while (1) {
    process_packet(sockfd, &client_addr, addr_len, droppc, outfile_path,
                   &pktsn);
    pktsn++; // is this necessary?
  }

  close(sockfd);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <Port Number> <Drop Percentage>\n", argv[0]);
    exit(1);
  }

  int port = atoi(argv[1]);
  int droppc = atoi(argv[2]);

  validport(port);
  if (droppc < 0 || droppc > 100) {
    fprintf(stderr, "Drop percentage must be in the range: 0-100.\n");
    exit(1);
  }

  srand(time(NULL));
  start_server(port, droppc);

  return 0;
}

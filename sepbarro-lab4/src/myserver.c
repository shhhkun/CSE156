#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096 // KiB

// global vars
char *outfile_name;
char *outfile_path;
int *pktsn;

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

int file_exists(const char *filename) {
  FILE *file;
  if ((file = fopen(filename, "r"))) {
    // printf("file: %s exists\n", filename); // debug message
    fclose(file);
    return 1;
  }
  return 0;
}

char *timestamp() {
  time_t rawtime;
  struct tm *timeinfo;
  static char timestamp[30];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  return timestamp;
}

void process_packet(int sockfd, struct sockaddr_in *client_addr,
                    socklen_t addr_len, int droppc, char *root_folder) {
  char buffer[BUFFER_SIZE];

  // receive packet from client
  ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                    (struct sockaddr *)client_addr, &addr_len);
  if (bytes_received == -1) {
    fprintf(stderr, "recvfrom() failed\n");
    return;
  }

  // log received packet
  printf("%s, %d, %s, %d, DATA, %d\n", timestamp(),
         ntohs(client_addr->sin_port), inet_ntoa(client_addr->sin_addr),
         ntohs(client_addr->sin_port), *pktsn);

  // process first packet (contains outfile path)
  if (*pktsn == 0) {
    strncpy(outfile_name, buffer, bytes_received);
    snprintf(outfile_path, BUFFER_SIZE, "%s/%s", root_folder, buffer);
    // printf("Outfile name: %s\n", outfile_name); // debug message
    // printf("Output file path received: %s\n", outfile_path); // debug message
  } else {
    // droppc is applied for subsequent packets
    srand(time(NULL));
    int chance = rand() % 100;
    int should_drop = (chance) < droppc;
    // printf("rand() %% 100 = %d\n", chance); // debug message

    // log dropped packet
    if (should_drop) {
      if (buffer[0] == 'A') {
        printf("%s, %d, %s, %d, DROP ACK, %d\n", timestamp(),
               ntohs(client_addr->sin_port), inet_ntoa(client_addr->sin_addr),
               ntohs(client_addr->sin_port), *pktsn);
      } else {
        printf("%s, %d, %s, %d, DROP DATA, %d\n", timestamp(),
               ntohs(client_addr->sin_port), inet_ntoa(client_addr->sin_addr),
               ntohs(client_addr->sin_port), *pktsn);
      }
      return;
    }

    // truncate outfile before server appends to it
    if (strcmp(buffer, outfile_name) == 0 && file_exists(outfile_path)) {
      // printf("output file: %s\n", outfile_path); // debug message
      if (truncate(outfile_path, 0) == -1) {
        fprintf(stderr, "Error truncating output file\n");
        exit(1);
      }
    }

    // write buffered data to outfile path (but not first packet)
    if (strcmp(buffer, outfile_name) != 0 && strstr(buffer, "ACK") == NULL) {
      // printf("buffer: %s\n", buffer); // debug message
      // printf("outfile path: %s\n", outfile_path); // debug message
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
  printf("%s, %d, %s, %d, ACK, %d\n", timestamp(), ntohs(client_addr->sin_port),
         inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
         *pktsn);

  (*pktsn)++;
}

void start_server(int port, int droppc, char *root_folder) {
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

  printf("Server listening on port: %d\n", port);

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  // process incoming packets one at a time
  while (1) {
    process_packet(sockfd, &client_addr, addr_len, droppc, root_folder);
  }

  close(sockfd);
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr,
            "Usage: %s <Port Number> <Drop Percentage> <Root Folder Path>\n",
            argv[0]);
    exit(1);
  }

  int port = atoi(argv[1]);
  int droppc = atoi(argv[2]);
  char *root_folder = argv[3];

  validport(port);
  if (droppc < 0 || droppc > 100) {
    fprintf(stderr, "Drop percentage must be in the range: 0-100.\n");
    exit(1);
  }

  mkdir(root_folder, 0777); // make root directory
  srand(time(NULL));        // set random seed

  // allocate memory for global vars
  outfile_path = malloc(BUFFER_SIZE);
  outfile_name = malloc(BUFFER_SIZE);
  pktsn = malloc(sizeof(int));
  *pktsn = 0;

  start_server(port, droppc, root_folder);

  // free memory for global vars
  free(outfile_path);
  free(outfile_name);
  free(pktsn);

  return 0;
}

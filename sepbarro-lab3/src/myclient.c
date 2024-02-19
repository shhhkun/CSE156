#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h> // for timeval structure
#include <time.h>
#include <unistd.h>

#define RECV_TIMEOUT_SECONDS 5
#define MAX_RETRANSMISSIONS 5
#define RETRANSMISSION_TIMEOUT_SECONDS 5
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

void log_packet(const char *type, int pktsn, size_t base, size_t nextsn,
                int winsz) {
  printf("%s, %s, %d, %zu, %zu, %zu\n", get_timestamp(), type, pktsn, base,
         nextsn, base + winsz);
}

void send_file(const char *server_ip, int server_port, int mtu, int winsz,
               const char *infile_path, const char *outfile_path) {
  char buffer[BUFFER_SIZE];
  FILE *infile = fopen(infile_path, "rb");

  if (infile == NULL) {
    perror("Error opening input file");
    exit(EXIT_FAILURE);
  }

  // Truncate the output file to empty it
  if (truncate(outfile_path, 0) == -1) {
    perror("Error truncating output file");
    fclose(infile);
    exit(EXIT_FAILURE);
  }

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("Socket creation failed");
    fclose(infile);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_port = htons(server_port);

  ssize_t bytes_sent =
      sendto(sockfd, outfile_path, strlen(outfile_path) + 1, 0,
             (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (bytes_sent == -1) {
    perror("sendto() failed");
    fclose(infile);
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Sent outfile_path: %s\n", outfile_path); // Debug message

  size_t base = 0;
  size_t nextsn = 0;

  int retransmissions = 0;

  while (1) {
    size_t i;
    for (i = base; i < base + winsz; i++) {
      size_t bytes_read = fread(buffer, 1, mtu, infile);
      if (bytes_read == 0) {
        break;
      }

      ssize_t bytes_sent =
          sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr,
                 sizeof(server_addr));
      if (bytes_sent == -1) {
        perror("sendto() failed");
        fclose(infile);
        close(sockfd);
        exit(EXIT_FAILURE);
      }

      printf("%s, Sent DATA packet %zu\n", get_timestamp(),
             nextsn); // Debug message
      nextsn++;

      // Wait for ACK
      struct timeval timeout;
      timeout.tv_sec = RETRANSMISSION_TIMEOUT_SECONDS;
      timeout.tv_usec = 0;

      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(sockfd, &readfds);

      int select_result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
      if (select_result == -1) {
        perror("select() failed");
        fclose(infile);
        close(sockfd);
        exit(EXIT_FAILURE);
      } else if (select_result == 0) {
        // Timeout occurred
        printf("%s, Packet loss detected.\n", get_timestamp());
        fseek(infile, i, SEEK_SET);
        retransmissions++;
      } else {
        // ACK received, check if it corresponds to the sent packet
        int ack_sn;
        ssize_t bytes_received = recv(sockfd, &ack_sn, sizeof(ack_sn), 0);
        if (bytes_received == -1) {
          perror("recv() failed");
          fclose(infile);
          close(sockfd);
          exit(EXIT_FAILURE);
        }

        printf("%s, Received ACK: %d\n", get_timestamp(),
               ack_sn); // Debug message

        if (ack_sn == (int)i) {
          // ACK received for the current packet, update base
          base++;
        }
      }

      if (retransmissions >= MAX_RETRANSMISSIONS) {
        fprintf(stderr, "Reached max re-transmission limit\n");
        fclose(infile);
        close(sockfd);
        exit(EXIT_FAILURE);
      }
    }

    if (feof(infile)) { // base == nextsn
      break;
    }
  }

  fclose(infile);
  close(sockfd);
}

int main(int argc, char *argv[]) {
  if (argc != 7) {
    fprintf(stderr,
            "Usage: %s <Server IP> <Server Port> <MTU> <Window Size> <Infile "
            "Path> <Outfile Path>\n",
            argv[0]);
    exit(EXIT_FAILURE);
  }

  const char *server_ip = argv[1];
  int server_port = atoi(argv[2]);
  int mtu = atoi(argv[3]);
  int winsz = atoi(argv[4]);
  const char *infile_path = argv[5];
  const char *outfile_path = argv[6];

  if (server_port <= 1023 || server_port > 65535) {
    fprintf(
        stderr,
        "Invalid server port number. Port must be in the range 1024-65535.\n");
    exit(EXIT_FAILURE);
  }

  if (mtu <= 0 || winsz <= 0) {
    fprintf(stderr, "MTU and Window Size must be positive integers.\n");
    exit(EXIT_FAILURE);
  }

  send_file(server_ip, server_port, mtu, winsz, infile_path, outfile_path);

  return 0;
}

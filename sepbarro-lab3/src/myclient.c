#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define RECV_TIMEOUT_SECONDS 5
#define MAX_RETRANSMISSIONS 5
#define RETRANSMISSION_TIMEOUT_SECONDS 5
#define BUFFER_SIZE 1024

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

void log_packet(const char *type, int pktsn, size_t base, size_t nextsn,
                int winsz) {
  printf("%s, %s, %d, %zu, %zu, %zu\n", get_timestamp(), type, pktsn, base,
         nextsn, base + winsz);
}

void send_file(const char *server_ip, int server_port, int mtu, int winsz,
               const char *infile_path, const char *outfile_path) {
  char buffer[BUFFER_SIZE];

  // open infile in read bytes mode
  FILE *infile = fopen(infile_path, "rb");
  if (infile == NULL) {
    fprintf(stderr, "Error opening input file\n");
    exit(1);
  }

  // truncate outfile before server appends to it
  if (truncate(outfile_path, 0) == -1) {
    fprintf(stderr, "Error truncating output file\n");
    fclose(infile);
    exit(1);
  }

  // create socket file descriptor
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    fprintf(stderr, "Socket creation failed\n");
    fclose(infile);
    exit(1);
  }

  // initialize server address structure
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(server_ip);
  server_addr.sin_port = htons(server_port);

  // first packet sent contains outfile path
  ssize_t bytes_sent =
      sendto(sockfd, outfile_path, strlen(outfile_path) + 1, 0,
             (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (bytes_sent == -1) {
    fprintf(stderr, "sendto() failed\n");
    fclose(infile);
    close(sockfd);
    exit(1);
  }

  // printf("Sent outfile_path: %s\n", outfile_path); // debug message

  size_t base = 0;
  size_t nextsn = 0;
  int retransmissions = 0;

  while (1) {
    for (size_t i = base; i < base + winsz; i++) {
      size_t bytes_read =
          fread(buffer, 1, mtu, infile); // read packets up to size mtu
      if (bytes_read == 0) {
        break;
      }

      // send packet to server
      ssize_t bytes_sent =
          sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr,
                 sizeof(server_addr));
      if (bytes_sent == -1) {
        fprintf(stderr, "sendto() failed\n");
        fclose(infile);
        close(sockfd);
        exit(1);
      }

      // printf("%s, Sent DATA packet %zu\n", get_timestamp(), nextsn); // debug
      // message
      log_packet("DATA", nextsn, base, nextsn, winsz); // log DATA packet

      nextsn++;

      // wait for ACK
      struct timeval timeout;
      timeout.tv_sec = RETRANSMISSION_TIMEOUT_SECONDS;
      timeout.tv_usec = 0;

      fd_set readfds;
      FD_ZERO(&readfds);        // clear set of file descriptors
      FD_SET(sockfd, &readfds); // add sockfd to file descriptor set

      int select_result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
      if (select_result == -1) {
        fprintf(stderr, "select() failed\n");
        fclose(infile);
        close(sockfd);
        exit(1);
      } else if (select_result == 0) { // timeout occurred
        fprintf(stderr, "%s, Packet loss detected.\n", get_timestamp());
        fseek(infile, i,
              SEEK_SET); // move pointer back to retransmit packet content
        retransmissions++;
      } else { // ACK received check if matches packet
        int ack_sn;
        ssize_t bytes_received = recv(sockfd, &ack_sn, sizeof(ack_sn), 0);
        if (bytes_received == -1) {
          fprintf(stderr, "recv() failed");
          fclose(infile);
          close(sockfd);
          exit(1);
        }

        // printf("%s, Received ACK: %d\n", get_timestamp(), ack_sn); // debug
        // message
        log_packet("ACK", ack_sn, base, nextsn, winsz); // log ACK packet

        if (ack_sn == (int)i) { // ACK matches current packet
          base++;
        }
      }

      if (retransmissions >= MAX_RETRANSMISSIONS) {
        fprintf(stderr, "Reached max re-transmission limit\n");
        fclose(infile);
        close(sockfd);
        exit(1);
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
    exit(1);
  }

  const char *server_ip = argv[1];
  int server_port = atoi(argv[2]);
  int mtu = atoi(argv[3]);
  int winsz = atoi(argv[4]);
  const char *infile_path = argv[5];
  const char *outfile_path = argv[6];

  validport(server_port);
  if (mtu < 1) {
    fprintf(stderr, "MTU must be at least 1\n");
    exit(1);
  }
  if (winsz < 1) {
    fprintf(stderr, "Window size must be at least 1");
    exit(1);
  }

  send_file(server_ip, server_port, mtu, winsz, infile_path, outfile_path);

  return 0;
}

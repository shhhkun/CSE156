#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

void send_file(const char *server_ip, int server_port, int mtu,
               const char *infile_path, const char *outfile_path) {
  char buffer[mtu];

  // open infile in read bytes mode
  FILE *infile = fopen(infile_path, "rb");
  if (infile == NULL) {
    fprintf(stderr, "Error opening input filepath\n");
    exit(1);
  }

  // open outfile in write bytes mode
  FILE *outfile = fopen(outfile_path, "wb");
  if (outfile == NULL) {
    fprintf(stderr, "Error opening output filepath\n");
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

  // Set timeout for receiving ACKs
  struct timeval timeout = {10, 0}; // 10 sec timeout
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
             sizeof(timeout));

  // Timestamp to track server response
  time_t start_time = time(NULL);

  while (1) {
    size_t bytes_read =
        fread(buffer, 1, mtu, infile); // read file in sizes of max MTU
    if (bytes_read == 0) {
      break; // no more bytes/eof, exit infinite loop
    }

    // send packet to server
    if (sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&server_addr,
               sizeof(server_addr)) == -1) {
      fprintf(stderr, "sendto() failed\n");
      fclose(infile);
      fclose(outfile);
      close(sockfd);
      exit(1);
    }

    // wait for ACK
    int ack_sn;
    ssize_t bytes_received =
        recvfrom(sockfd, &ack_sn, sizeof(ack_sn), 0, NULL, NULL);
    if (bytes_received == -1) {
      fprintf(stderr,
              "Packet loss detected\n"); // handle timeout (assume packet loss)

      // Check if server is down (60 seconds without any response)
      time_t current_time = time(NULL);
      if (current_time - start_time >= 60) {
        fprintf(stderr, "Cannot detect server\n");
        fclose(infile);
        fclose(outfile);
        close(sockfd);
        exit(1);
      }
      continue;
    }

    // Reset timestamp on successful server response
    start_time = time(NULL);

    fwrite(buffer, 1, bytes_read, outfile);
  }

  fclose(infile);
  fclose(outfile);
  close(sockfd);
}

int main(int argc, char *argv[]) {
  if (argc != 6) {
    fprintf(stderr,
            "Usage: %s <Server IP> <Port Number> <MTU> <Infile Path> <Outfile "
            "Path>\n",
            argv[0]);
    exit(1);
  }

  // get CLI input
  const char *server_ip = argv[1];
  int server_port = atoi(argv[2]);
  int mtu = atoi(argv[3]);
  const char *infile_path = argv[4];
  const char *outfile_path = argv[5];

  validport(server_port);
  if (mtu < 1) {
    fprintf(stderr, "MTU must at least be 1\n");
    exit(1);
  }

  // send file to server in packets
  send_file(server_ip, server_port, mtu, infile_path, outfile_path);

  return 0;
}

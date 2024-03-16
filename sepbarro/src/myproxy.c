#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096 // KiB

pthread_mutex_t forbidden_mutex = PTHREAD_MUTEX_INITIALIZER;
char *forbidden_file;   // global forbidden site file
char *access_log_file;  // global access log file
char **forbidden_sites; // forbidden sites array
int num_sites = 0;

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

void load_forbidden_sites() {
  FILE *file = fopen(forbidden_file, "r");
  char **new_forbidden_sites = NULL;
  int new_num_sites = 0;
  int num_lines = 0;

  if (file == NULL) {
    fprintf(stderr, "Error opening forbidden sites file\n");
    exit(1);
  }

  // count number of lines (to allocate memory for)
  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    num_lines += 1;
  }

  fseek(file, 0, SEEK_SET); // reset pointer to start of file

  // allocate memory for new forbidden sites array
  new_forbidden_sites = (char **)malloc(num_lines * sizeof(char *));
  if (new_forbidden_sites == NULL) {
    fprintf(stderr, "Error allocating memory for forbidden sites array\n");
    fclose(file);
    return;
  }

  int i = 0;
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    size_t len = strlen(buffer);
    if (len > 0 &&
        buffer[len - 1] == '\n') { // remove newline character (at end)
      buffer[len - 1] = '\0';
    }
    new_forbidden_sites[i] = strdup(buffer);
    i += 1;
  }
  fclose(file);

  new_num_sites = num_lines;

  // free memory of old forbidden sites array
  for (i = 0; i < num_sites; i += 1) {
    free(forbidden_sites[i]);
  }
  free(forbidden_sites);

  pthread_mutex_lock(&forbidden_mutex);
  forbidden_sites = new_forbidden_sites; // update sites
  num_sites = new_num_sites;             // update number sites
  pthread_mutex_unlock(&forbidden_mutex);

  return;
}

void handle_sigint(int sig) {
  (void)sig;
  load_forbidden_sites();
  printf("\nForbidden sites reloaded.\n");
  return;
}

int is_forbidden(const char *hostname_or_ip) {
  for (int i = 0; i < num_sites; i += 1) {
    if (strstr(hostname_or_ip, forbidden_sites[i]) != NULL) {
      return 1;
    }
  }
  return 0;
}

int parse_http_request(const char *request, char *method, char *hostname,
                       char *ip, char *path, int *port) {
  char url[2048];

  if (sscanf(request, "%s %s", method, url) != 2) { // check if invalid format
    return -1;
  }
  if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) { // 501 error
    return -1;
  }

  char *host_start = strstr(url, "//");
  if (host_start == NULL) { // invalid URL
    return -1;
  }
  host_start += 2;

  // find the position of the first slash (if present)
  char *proxy_end = strchr(host_start, '/');
  if (proxy_end == NULL) { // invalid URL
    return -1;
  }
  // calculate distance
  int len = proxy_end - host_start;
  strncpy(hostname, host_start, len);
  hostname[len] = '\0';

  // get port (if part of URL)
  char *port_start = strchr(hostname, ':');
  if (port_start != NULL) {
    *port = atoi(port_start + 1);
    *port_start = '\0';
  } else {
    *port = 80; // default port
  }
  strcpy(ip, hostname);
  strcpy(path, proxy_end);

  return 0;
}

void send_response(int client_sock, const char *status, const char *headers,
                   const char *body) {
  char response[BUFFER_SIZE];
  snprintf(response, sizeof(response), "%s\r\n%s\r\n%s\r\n\r\n", status,
           headers, body);
  send(client_sock, response, strlen(response), 0);
  return;
}

void log_request(const struct sockaddr_in *dest_addr, const char *method,
                 const char *uri, const char *version, int status_code,
                 ssize_t bytes_received) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);

  char ip_str[INET_ADDRSTRLEN]; // IP address

  if (dest_addr != NULL) { // get IP
    inet_ntop(AF_INET, &(dest_addr->sin_addr), ip_str, INET_ADDRSTRLEN);
  } else { // dest_addr is NULL, IP is unknown
    strcpy(ip_str, "Unknown");
  }

  // format log entry
  char log_entry[BUFFER_SIZE];
  snprintf(log_entry, sizeof(log_entry), "%s %s \"%s %s %s\" %d %zd\n",
           time_str, ip_str, method, uri, version, status_code, bytes_received);

  // write log entry to access log file
  FILE *access_log = fopen(access_log_file, "a");
  if (access_log != NULL) {
    fprintf(access_log, "%s", log_entry);
    fclose(access_log);
  }

  return;
}

void *handle_client(void *arg) {
  int client_sock = *((int *)arg);
  free(arg);

  // receive client request
  char request_buffer[BUFFER_SIZE];
  recv(client_sock, request_buffer, sizeof(request_buffer), 0);

  // parse incoming HTTP request
  char method[10], hostname[2048], uri[2048], ip[INET_ADDRSTRLEN];
  int port;
  if (parse_http_request(request_buffer, method, hostname, ip, uri, &port) !=
      0) {
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
      send_response(client_sock, "HTTP/1.1 501 Not Implemented", "", "");
      log_request(NULL, method, hostname, "HTTP/1.1", 501, -1);
    } else {
      send_response(client_sock, "HTTP/1.1 400 Bad Request", "", "");
      log_request(NULL, method, hostname, "HTTP/1.1", 400, -1);
    }
    close(client_sock);
    return NULL;
  }

  // check if hostname or IP is in forbidden array
  pthread_mutex_lock(&forbidden_mutex);
  if (is_forbidden(hostname) || is_forbidden(ip)) {
    pthread_mutex_unlock(&forbidden_mutex);
    send_response(client_sock, "HTTP/1.1 403 Forbidden", "", "");
    log_request(NULL, method, hostname, "HTTP/1.1", 403, -1);
    close(client_sock);
    return NULL;
  }
  pthread_mutex_unlock(&forbidden_mutex);

  // forward request to destination server
  int dest_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (dest_sock < 0) {
    fprintf(stderr, "Socket creation failed\n");
    send_response(client_sock, "HTTP/1.1 502 Bad Gateway", "", "");
    log_request(NULL, method, hostname, "HTTP/1.1", 502, -1);
    close(client_sock);
    return NULL;
  }

  // domain name resolution
  struct hostent *dest_host = gethostbyname(hostname);
  if (dest_host == NULL) {
    fprintf(stderr, "Error resolving hostname\n");
    send_response(client_sock, "HTTP/1.1 502 Bad Gateway", "", "");
    log_request(NULL, method, hostname, "HTTP/1.1", 502, -1);
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  // initialize client address structure
  struct sockaddr_in dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr = *((struct in_addr *)dest_host->h_addr);
  dest_addr.sin_port = htons(port);

  if (connect(dest_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <
      0) {
    fprintf(stderr, "Connection to destination server failed\n");
    send_response(client_sock, "HTTP/1.1 504 Gateway Timeout", "", "");
    log_request(NULL, method, hostname, "HTTP/1.1", 504, -1);
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  // send request to destination server
  send(dest_sock, request_buffer, strlen(request_buffer), 0);

  // receive response from destination server
  char response_buffer[BUFFER_SIZE];
  ssize_t bytes_received =
      recv(dest_sock, response_buffer, sizeof(response_buffer), 0);
  if (bytes_received < 0) {
    fprintf(stderr, "Error receiving response from destination server\n");
    send_response(client_sock, "HTTP/1.1 502 Bad Gateway", "", "");
    log_request(NULL, method, hostname, "HTTP/1.1", 502, -1);
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  // send response to client
  send(client_sock, response_buffer, bytes_received, 0);

  // log request
  log_request(&dest_addr, method, hostname, "HTTP/1.1", 200, bytes_received);

  close(client_sock);
  close(dest_sock);

  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(
        stderr,
        "Usage: %s <Port Number> <Forbidden Sites File> <Access Log File>\n",
        argv[0]);
    exit(1);
  }

  int listen_port = atoi(argv[1]);
  forbidden_file = argv[2];
  access_log_file = argv[3];

  signal(SIGINT, handle_sigint); // SIGINT handler

  validport(listen_port); // check if port is within valid range

  load_forbidden_sites(); // load forbidden sites (initial load)

  int server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock < 0) {
    fprintf(stderr, "Socket creation failed\n");
    exit(1);
  }
  // initialize proxy server address structure
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(listen_port);

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    fprintf(stderr, "Binding failed\n");
    exit(1);
  }
  if (listen(server_sock, 10) < 0) {
    fprintf(stderr, "Listening failed\n");
    exit(1);
  }

  printf("Proxy server listening on port: %d\n", listen_port);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int *client_sock = malloc(sizeof(int));
    *client_sock =
        accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
    if (*client_sock < 0) {
      fprintf(stderr, "Accepting connection failed\n");
      continue;
    }

    // thread for handling client requests
    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client, (void *)client_sock) != 0) {
      fprintf(stderr, "Failed to create thread\n");
      close(*client_sock);
      free(client_sock);
      continue;
    }

    pthread_detach(tid); // detach thread
  }

  close(server_sock);
  return 0;
}

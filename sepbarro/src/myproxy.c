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

#define BUFFER_SIZE 8192

// Mutex for protecting access to forbidden sites list
pthread_mutex_t mutex_forbidden_sites = PTHREAD_MUTEX_INITIALIZER;

// Global variables for forbidden sites and access log file paths
char *forbidden_sites_file;
char *access_log_file;
char **forbidden_sites; // Array to store forbidden sites
int num_forbidden_sites = 0;

// Function to load forbidden sites from file
void load_forbidden_sites() {
  FILE *file = fopen(forbidden_sites_file, "r");
  if (file == NULL) {
    perror("Error opening forbidden sites file");
    return;
  }

  printf("Loading forbidden sites from file: %s\n", forbidden_sites_file);

  // Count the number of lines in the file
  int num_lines = 0;
  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    num_lines++;
  }

  printf("Number of lines in the file: %d\n", num_lines);

  // Allocate memory for forbidden_sites array
  forbidden_sites = (char **)malloc(num_lines * sizeof(char *));
  if (forbidden_sites == NULL) {
    perror("Error allocating memory for forbidden sites array");
    fclose(file);
    return;
  }

  printf("Memory allocated for forbidden_sites array\n");

  // Reset file pointer to beginning
  fseek(file, 0, SEEK_SET);

  // Read lines from the file and store them in forbidden_sites array
  int i = 0;
  while (fgets(buffer, sizeof(buffer), file) != NULL) {
    // Remove newline character from the end
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }
    forbidden_sites[i] = strdup(buffer);
    printf("Loaded forbidden site: %s\n", forbidden_sites[i]);
    i++;
  }

  num_forbidden_sites = num_lines;
  fclose(file);
  printf("Forbidden sites loaded successfully\n");
}

// Function to handle SIGINT signal (Control-C)
void handle_sigint(int sig) {
  (void)sig; // Suppress unused parameter warning
  // Reload the forbidden sites file here
  load_forbidden_sites();
  printf("Forbidden sites file reloaded.\n");

  exit(1); // remove later
}

// Function to check if a given hostname or IP address is forbidden
int is_forbidden(const char *hostname_or_ip) {
  pthread_mutex_lock(&mutex_forbidden_sites);
  for (int i = 0; i < num_forbidden_sites; i++) {
    if (strstr(hostname_or_ip, forbidden_sites[i]) != NULL) {
      pthread_mutex_unlock(&mutex_forbidden_sites);
      return 1; // Forbidden
    }
  }
  pthread_mutex_unlock(&mutex_forbidden_sites);
  return 0; // Not forbidden
}

// Function to parse HTTP request and extract hostname, path, and port
int parse_http_request(const char *request, char *hostname, char *path,
                       int *port) {
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
void send_response(int client_sock, const char *status, const char *headers,
                   const char *body) {
  char response[BUFFER_SIZE];
  snprintf(response, sizeof(response), "%s\r\n%s\r\n%s\r\n\r\n", status,
           headers, body);
  send(client_sock, response, strlen(response), 0);
}

// Function to handle client request
void *handle_client(void *arg) {
  int client_sock = *((int *)arg);
  free(arg);

  // Handle client request
  char request_buffer[BUFFER_SIZE];
  recv(client_sock, request_buffer, sizeof(request_buffer), 0);

  printf("parsing...\n"); //
  // Parse HTTP request
  char hostname[2048], path[2048];
  int port;
  if (parse_http_request(request_buffer, hostname, path, &port) != 0) {
    // Invalid request
    send_response(client_sock, "HTTP/1.1 400 Bad Request", "", "");
    close(client_sock);
    return NULL;
  }

  printf("checking if forbidden...\n"); //
  // Check if hostname is forbidden
  pthread_mutex_lock(&mutex_forbidden_sites);
  if (is_forbidden(hostname)) {
    pthread_mutex_unlock(&mutex_forbidden_sites);
    send_response(client_sock, "HTTP/1.1 403 Forbidden", "", "");
    close(client_sock);
    return NULL;
  }
  pthread_mutex_unlock(&mutex_forbidden_sites);

  printf("dest server\n");
  // Forward request to destination server
  int dest_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (dest_sock < 0) {
    perror("Socket creation failed");
    close(client_sock);
    return NULL;
  }

  printf("dest host\n");
  struct hostent *dest_host = gethostbyname(hostname);
  if (dest_host == NULL) {
    perror("Error resolving hostname");
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  struct sockaddr_in dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_addr = *((struct in_addr *)dest_host->h_addr);
  dest_addr.sin_port = htons(port);

  printf("connecting...\n"); //
  // Connect to destination server
  if (connect(dest_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <
      0) {
    perror("Connection to destination server failed");
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  send(dest_sock, request_buffer, strlen(request_buffer), 0);

  printf("receive resp from dest serv\n");
  // Receive response from destination server
  char response_buffer[BUFFER_SIZE];
  ssize_t bytes_received =
      recv(dest_sock, response_buffer, sizeof(response_buffer), 0);
  if (bytes_received < 0) {
    perror("Error receiving response from destination server");
    close(client_sock);
    close(dest_sock);
    return NULL;
  }

  printf("sending back to client\n");
  // Send response back to client
  send(client_sock, response_buffer, bytes_received, 0);

  printf("logging\n");
  // Log the request
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_str[64];
  strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
  char log_entry[BUFFER_SIZE];
  snprintf(log_entry, sizeof(log_entry), "%s %s \"%s\" %s %zd\n", time_str,
           inet_ntoa(dest_addr.sin_addr), request_buffer, "200 OK",
           bytes_received);
  FILE *access_log = fopen(access_log_file, "a");
  if (access_log != NULL) {
    fprintf(access_log, "%s", log_entry);
    fclose(access_log);
  }

  printf("handling done.\n");
  // Close connections
  close(client_sock);
  close(dest_sock);

  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr,
            "Usage: %s listen_port forbidden_sites_file access_log_file\n",
            argv[0]);
    return 1;
  }

  // Set up SIGINT handler
  signal(SIGINT, handle_sigint);

  // Parse command line arguments
  int listen_port = atoi(argv[1]);
  forbidden_sites_file = argv[2];
  access_log_file = argv[3];

  // Load forbidden sites from file
  load_forbidden_sites();

  // Set up socket for listening
  int server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock < 0) {
    perror("Socket creation failed");
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(listen_port);

  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Binding failed");
    return 1;
  }

  if (listen(server_sock, 10) < 0) {
    perror("Listening failed");
    return 1;
  }

  printf("Proxy server listening on port %d...\n", listen_port);

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int *client_sock = malloc(sizeof(int));
    *client_sock =
        accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
    if (*client_sock < 0) {
      perror("Accepting connection failed");
      continue;
    }

    // Create a new thread to handle the client request
    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client, (void *)client_sock) != 0) {
      perror("Failed to create thread");
      close(*client_sock);
      free(client_sock);
      continue;
    }

    // Detach the thread to allow it to run independently
    pthread_detach(tid);
  }

  close(server_sock);
  return 0;
}

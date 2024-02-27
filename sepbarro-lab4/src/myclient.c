#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h> // pthread.h only needs to be included once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define RECV_TIMEOUT_SECONDS 10
#define MAX_RETRANSMISSIONS 10
#define RETRANSMISSION_TIMEOUT_SECONDS 10
#define BUFFER_SIZE 1024

typedef struct {
    char *server_ip;
    int server_port;
    int mtu;
    int winsz;
    char *infile_path;
    char *outfile_path;
} ClientConfig;

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    char *outfile_path;
    FILE *infile;
    int mtu;
    int winsz;
    int *pktsn;
} SenderThreadArgs;

char *timestamp() {
    time_t rawtime;
    struct tm *timeinfo;
    static char timestamp[30];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

    return timestamp;
}

void *sender_thread(void *arg) {
    SenderThreadArgs *args = (SenderThreadArgs *)arg;

    size_t base = 0;

    while (!feof(args->infile)) {
        for (size_t i = base; i < base + args->winsz; i++) {
            char buffer[BUFFER_SIZE];
            size_t bytes_read = fread(buffer, 1, args->mtu, args->infile);
            if (bytes_read == 0) {
                break;
            }

            ssize_t bytes_sent = sendto(args->sockfd, buffer, bytes_read, 0,
                                         (struct sockaddr *)&(args->server_addr),
                                         sizeof(args->server_addr));
            if (bytes_sent == -1) {
                fprintf(stderr, "sendto() failed\n");
                fclose(args->infile);
                close(args->sockfd);
                exit(1);
            }

            printf("%s, DATA, %d\n", timestamp(), *args->pktsn);
            (*args->pktsn)++;
        }
    }

    fclose(args->infile);
    close(args->sockfd);
    free(args);

    return NULL;
}

void send_file(ClientConfig *config) {
    FILE *infile = fopen(config->infile_path, "rb");
    if (infile == NULL) {
        fprintf(stderr, "Error opening input file\n");
        exit(1);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Socket creation failed\n");
        fclose(infile);
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(config->server_ip);
    server_addr.sin_port = htons(config->server_port);

    SenderThreadArgs *thread_args = malloc(sizeof(SenderThreadArgs));
    thread_args->sockfd = sockfd;
    thread_args->server_addr = server_addr;
    thread_args->outfile_path = config->outfile_path;
    thread_args->infile = infile;
    thread_args->mtu = config->mtu;
    thread_args->winsz = config->winsz;
    thread_args->pktsn = malloc(sizeof(int));
    *(thread_args->pktsn) = 0;

    pthread_t thread;
    if (pthread_create(&thread, NULL, sender_thread, thread_args) != 0) {
        fprintf(stderr, "Error creating sender thread\n");
        fclose(infile);
        close(sockfd);
        exit(1);
    }

    pthread_join(thread, NULL);
}

void start_client(ClientConfig *config, int num_servers) {
    pthread_t *threads = malloc(num_servers * sizeof(pthread_t));

    for (int i = 0; i < num_servers; i++) {
        if (pthread_create(&(threads[i]), NULL, (void *(*)(void *))send_file,
                           &(config[i])) != 0) {
            fprintf(stderr, "Error creating thread for server %d\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < num_servers; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr,
                "Usage: %s <Number of Servers> <Server Configuration File> <MTU> "
                "<Window Size> <Input File Path> <Output File Path>\n",
                argv[0]);
        exit(1);
    }

    int num_servers = atoi(argv[1]);
    char *server_config_file = argv[2];
    int mtu = atoi(argv[3]);
    int winsz = atoi(argv[4]);
    char *infile_path = argv[5];
    char *outfile_path = argv[6];

    FILE *server_config = fopen(server_config_file, "r");
    if (server_config == NULL) {
        fprintf(stderr, "Error opening server configuration file\n");
        exit(1);
    }

    ClientConfig *config = malloc(num_servers * sizeof(ClientConfig));

    for (int i = 0; i < num_servers; i++) {
        config[i].server_ip = malloc(16 * sizeof(char));
        fscanf(server_config, "%s %d", config[i].server_ip,
               &(config[i].server_port));
        config[i].mtu = mtu;
        config[i].winsz = winsz;
        config[i].infile_path = infile_path;
        config[i].outfile_path = outfile_path;
    }

    fclose(server_config);

    start_client(config, num_servers); // Pass num_servers to the start_client function

    free(config);

    return 0;
}

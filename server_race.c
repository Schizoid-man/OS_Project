//server_race.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define LOG_FILE "server_race.log"

char shared_buffer[BUFFER_SIZE];  // ❌ shared across threads = vulnerable
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void write_log(const char* state, pid_t pid, pid_t tid, const char* req, const char* res) {
    pthread_mutex_lock(&log_mutex);
    FILE* log = fopen(LOG_FILE, "a");
    if (!log) {
        perror("Failed to open log file");
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log, "[%s] [TID %d] [PID %d] [%s] Request: %s | Response: %s\n",
            timestamp, tid, pid, state, req, res);
    fclose(log);
    pthread_mutex_unlock(&log_mutex);
}

void reverse_string(char* str) {
    int n = strlen(str);
    for (int i = 0; i < n / 2; i++) {
        char tmp = str[i];
        str[i] = str[n - i - 1];
        str[n - i - 1] = tmp;
    }
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid);

    memset(shared_buffer, 0, BUFFER_SIZE);
    write_log("CONNECTED", pid, tid, "-", "-");

    ssize_t bytes_read = read(client_socket, shared_buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return NULL;
    }
    shared_buffer[bytes_read] = '\0';

    // 🧯 Force threads to overlap
    usleep(50000);  // 50ms

    write_log("RECEIVED", pid, tid, shared_buffer, "-");

    char response[BUFFER_SIZE];
    strncpy(response, shared_buffer, BUFFER_SIZE);
    reverse_string(response);

    write_log("PROCESSED", pid, tid, shared_buffer, response);
    write(client_socket, response, strlen(response));
    write_log("RESPONDED", pid, tid, shared_buffer, response);

    close(client_socket);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pid_t pid = getpid();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 20);

    printf("[Main PID %d] RACE Server listening on port %d...\n", pid, PORT);

    FILE* log = fopen(LOG_FILE, "w");
    if (log) {
        fprintf(log, "=== RACE Server Started at PID %d ===\n", pid);
        fclose(log);
    }

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        int* pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_t t;
        pthread_create(&t, NULL, handle_client, pclient);
        pthread_detach(t);
    }

    close(server_fd);
    return 0;
}

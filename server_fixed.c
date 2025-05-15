// server_fixed_semaphore.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>
#include <semaphore.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define LOG_FILE "server_fixed_semaphore.log"

sem_t log_semaphore; // Semaphore for logging

void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void write_log(const char* state, pid_t pid, pid_t tid, const char* req, const char* res) {
    sem_wait(&log_semaphore);
    FILE* log = fopen(LOG_FILE, "a");
    if (!log) {
        perror("Failed to open log file");
        sem_post(&log_semaphore);
        return;
    }
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log, "[%s] [TID %d] [PID %d] [%s] Request: %s | Response: %s\n",
            timestamp, tid, pid, state, req, res);
    fclose(log);
    sem_post(&log_semaphore);
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

    char local_buffer[BUFFER_SIZE];
    memset(local_buffer, 0, BUFFER_SIZE);
    write_log("CONNECTED", pid, tid, "-", "-");

    ssize_t bytes_read = read(client_socket, local_buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return NULL;
    }
    local_buffer[bytes_read] = '\0';

    usleep(50000); // Optional: mimic same timing as broken version

    write_log("RECEIVED", pid, tid, local_buffer, "-");

    char response[BUFFER_SIZE];
    strncpy(response, local_buffer, BUFFER_SIZE);
    reverse_string(response);

    write_log("PROCESSED", pid, tid, local_buffer, response);
    write(client_socket, response, strlen(response));
    write_log("RESPONDED", pid, tid, local_buffer, response);

    close(client_socket);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pid_t pid = getpid();

    // Initialize the semaphore
    if (sem_init(&log_semaphore, 0, 1) != 0) {
        perror("Semaphore init failed");
        exit(1);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 20);

    printf("[Main PID %d] ✅ FIXED Server with SEMAPHORE listening on port %d...\n", pid, PORT);

    FILE* log = fopen(LOG_FILE, "w");
    if (log) {
        fprintf(log, "=== FIXED Server (Semaphore) Started at PID %d ===\n", pid);
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
    sem_destroy(&log_semaphore); // Clean up the semaphore
    return 0;
}

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

// Hardcoded port for race server
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
            timestamp, (int)tid, (int)pid, state, req ? req : "-", res ? res : "-");
    fflush(log); // Ensure data is written to disk
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
    pid_t tid = (pid_t)pthread_self(); // Use pthread_self instead of syscall for better macOS compatibility

    memset(shared_buffer, 0, BUFFER_SIZE);
    write_log("CONNECTED", pid, tid, "-", "-");

    ssize_t bytes_read = read(client_socket, shared_buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        write_log("ERROR_READ", pid, tid, "-", "-");
        close(client_socket);
        return NULL;
    }
    shared_buffer[bytes_read] = '\0';

    // Add delay to make race conditions more likely
    usleep(100000);  // 100ms delay

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

    // Initialize log file
    FILE* log = fopen(LOG_FILE, "w");
    if (!log) {
        perror("Failed to create log file");
        exit(1);
    }
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log, "=== RACE Server Started at PID %d (%s) on PORT %d ===\n", pid, timestamp, PORT);
    fflush(log);
    fclose(log);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Allow socket reuse to avoid "address already in use" errors
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if (listen(server_fd, 20) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("[Main PID %d] 🔥 RACE Server listening on port %d...\n", pid, PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        int* pclient = malloc(sizeof(int));
        if (!pclient) {
            close(new_socket);
            continue;
        }
        
        *pclient = new_socket;
        pthread_t t;
        if (pthread_create(&t, NULL, handle_client, pclient) != 0) {
            perror("Thread creation failed");
            free(pclient);
            close(new_socket);
            continue;
        }
        pthread_detach(t);
    }

    close(server_fd);
    return 0;
}

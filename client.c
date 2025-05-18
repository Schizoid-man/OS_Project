// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <pthread.h>

// If PORT is not defined through compiler flags, use default
#ifndef PORT
#define PORT 8080
#endif

#define BUFFER_SIZE 1024
#define LOG_FILE "clients.log"

const char* words[] = {"apple", "loss", "cherry", "dragon", "win"};
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

void write_log(const char* state, pid_t pid, const char* req, const char* res) {
    pthread_mutex_lock(&log_mutex);
    FILE* log = fopen(LOG_FILE, "a");
    if (!log) {
        perror("Log open failed");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    fprintf(log, "[%s] [PID %d] [%s] Request: %s | Response: %s\n",
            timestamp, pid, state, req, res);
    fflush(log);
    fclose(log);
    pthread_mutex_unlock(&log_mutex);
}

int main(int argc, char* argv[]) {
    srand(time(NULL) ^ getpid());
    pid_t pid = getpid();

    // Make sure log exists (create if first process)
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&init_mutex);
    if (access(LOG_FILE, F_OK) != 0) {
        FILE* log = fopen(LOG_FILE, "w");
        if (log) {
            fprintf(log, "=== Client Logs Started (connecting to port %d) ===\n", PORT);
            fclose(log);
        }
    }
    pthread_mutex_unlock(&init_mutex);

    const char* word;
    if (argc > 1) {
        int index = atoi(argv[1]) % 5;
        word = words[index];
    } else {
        word = words[rand() % 5]; // fallback
    }

    write_log("INIT", pid, word, "-");

    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        write_log("ERROR_SOCKET", pid, word, "-");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    write_log("CONNECTING", pid, word, "-");

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        write_log("ERROR_CONNECT", pid, word, "-");
        close(sock);
        exit(1);
    }

    write_log("SENDING", pid, word, "-");

    if (write(sock, word, strlen(word)) < 0) {
        perror("Write failed");
        write_log("ERROR_SEND", pid, word, "-");
        close(sock);
        exit(1);
    }

    memset(buffer, 0, BUFFER_SIZE);
    ssize_t received = read(sock, buffer, BUFFER_SIZE - 1);
    if (received < 0) {
        perror("Read failed");
        write_log("ERROR_RECEIVE", pid, word, "-");
        close(sock);
        exit(1);
    }

    buffer[received] = '\0';
    write_log("RECEIVED", pid, word, buffer);

    printf("[Client PID %d] Sent: %s | Received: %s\n", pid, word, buffer);
    fflush(stdout);

    close(sock);
    return 0;
}

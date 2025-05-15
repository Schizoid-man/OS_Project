// run_clients.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_CLIENTS 100

int main() {
    for (int i = 0; i < NUM_CLIENTS; i++) {
        printf("Spawning client #%d...\n", i + 1);
        pid_t pid = fork();

        if (pid == 0) {
            printf("Client #%d (PID %d) executing ./client with argument %d\n", i + 1, getpid(), i);
            char index_arg[4];
            snprintf(index_arg, sizeof(index_arg), "%d", i);
            char *args[] = {"./client", index_arg, NULL};
            execv(args[0], args);
            perror("execv failed");
            exit(1);
        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        wait(NULL);
    }

    printf("✅ All clients have completed.\n");
    return 0;
}

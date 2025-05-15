int main(int argc, char* argv[]) {
    srand(time(NULL) ^ getpid());
    pid_t pid = getpid();

    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "client_%d.log", pid);
    FILE* log = fopen(log_filename, "w");
    if (!log) {
        perror("Log open failed");
        exit(1);
    }

    const char* word;
    if (argc > 1) {
        int index = atoi(argv[1]) % 5;
        word = words[index];
    } else {
        word = words[rand() % 5]; // fallback
    }

#include "model.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

ShmBuf *buf_init() {
    int fd = shm_open(SHARED_FILE_PATH, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return NULL;
    }

    if (ftruncate(fd, BUF_SIZE) == -1) {
        perror("ftruncate");
        close(fd);
        return NULL;
    }

    ShmBuf *shmp = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmp == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return NULL;
    }

    shmp->fd = fd;
    if (sem_init(&shmp->sem, 1, 1) == -1) {
        perror("sem_init");
        munmap(shmp, BUF_SIZE);
        close(fd);
        return NULL;
    }
    
    shmp->cnt = 0;
    return shmp;
}

void model_send_message(ShmBuf *shmp, const char *msg) {
    if (shmp == NULL || msg == NULL) return;

    sem_wait(&shmp->sem);
    size_t len = strlen(msg);
    size_t available = BUF_SIZE - sizeof(ShmBuf) - shmp->cnt - 1;

    if (len > available) {
        len = available;
    }

    if (len > 0) {
        memcpy(&shmp->msgbuf[shmp->cnt], msg, len);
        shmp->cnt += len;
        shmp->msgbuf[shmp->cnt++] = '\n';
    }
    sem_post(&shmp->sem);
}

char *model_read_messages(ShmBuf *shmp) {
    if (shmp == NULL) return NULL;

    sem_wait(&shmp->sem);
    char *buf = malloc(shmp->cnt + 1);
    if (buf) {
        memcpy(buf, shmp->msgbuf, shmp->cnt);
        buf[shmp->cnt] = '\0';
    }
    sem_post(&shmp->sem);
    return buf;
}

void handle_redirection(char *cmd) {
    char *redirect_out = strchr(cmd, '>');
    char *redirect_in = strchr(cmd, '<');

    if (redirect_out) {
        *redirect_out = '\0';
        char *filename = redirect_out + 1;
        while (isspace(*filename)) filename++;

        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (redirect_in) {
        *redirect_in = '\0';
        char *filename = redirect_in + 1;
        while (isspace(*filename)) filename++;

        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
}

void execute_pipeline(char **commands, int num_commands) {
    int pipes[num_commands-1][2];
    pid_t pids[num_commands];

    // Create pipes
    for (int i = 0; i < num_commands-1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Execute commands
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // Set up input
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
                close(pipes[i-1][0]);
            }

            // Set up output
            if (i < num_commands-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][1]);
            }

            // Close all pipes
            for (int j = 0; j < num_commands-1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Parse and execute command
            char *args[64];
            char *token = strtok(commands[i], " ");
            int j = 0;
            while (token && j < 63) {
                args[j++] = token;
                token = strtok(NULL, " ");
            }
            args[j] = NULL;

            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipes in parent
    for (int i = 0; i < num_commands-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

char *execute_command(const char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) {
        return strdup("");
    }

    // Check for pipes
    if (strstr(cmd, "|")) {
        // Handle pipeline
        char *commands[16];
        char *cmd_copy = strdup(cmd);
        char *token = strtok(cmd_copy, "|");
        int num_commands = 0;

        while (token && num_commands < 16) {
            commands[num_commands++] = token;
            token = strtok(NULL, "|");
        }

        // Create pipe for capturing output
        int capture_pipe[2];
        if (pipe(capture_pipe) == -1) {
            perror("pipe");
            free(cmd_copy);
            return strdup("Error: pipe creation failed");
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process - execute pipeline
            close(capture_pipe[0]);
            dup2(capture_pipe[1], STDOUT_FILENO);
            close(capture_pipe[1]);

            execute_pipeline(commands, num_commands);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process - read output
            close(capture_pipe[1]);
            char *output = malloc(MAX_OUTPUT_LENGTH);
            ssize_t bytes_read = read(capture_pipe[0], output, MAX_OUTPUT_LENGTH - 1);
            close(capture_pipe[0]);

            if (bytes_read < 0) {
                perror("read");
                free(output);
                output = strdup("Error: reading command output failed");
            } else {
                output[bytes_read] = '\0';
            }

            waitpid(pid, NULL, 0);
            free(cmd_copy);
            return output;
        }
    } else {
        // No pipes, handle single command with possible redirection
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("pipe");
            return strdup("Error: pipe creation failed");
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            // Handle redirection
            char *cmd_copy = strdup(cmd);
            handle_redirection(cmd_copy);

            // Parse and execute command
            char *args[64];
            char *token = strtok(cmd_copy, " ");
            int i = 0;
            while (token && i < 63) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;

            execvp(args[0], args);
            perror("execvp");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            close(pipefd[1]);
            char *output = malloc(MAX_OUTPUT_LENGTH);
            ssize_t bytes_read = read(pipefd[0], output, MAX_OUTPUT_LENGTH - 1);
            close(pipefd[0]);

            if (bytes_read < 0) {
                perror("read");
                free(output);
                output = strdup("Error: reading command output failed");
            } else {
                output[bytes_read] = '\0';
            }

            waitpid(pid, NULL, 0);
            return output;
        }
    }
}

void cleanup(ShmBuf *shmp) {
    if (shmp) {
        sem_destroy(&shmp->sem);
        munmap(shmp, BUF_SIZE);
        close(shmp->fd);
        shm_unlink(SHARED_FILE_PATH);
    }
}

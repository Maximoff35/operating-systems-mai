#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void remove_line_ending(char* s) {
    size_t n = strlen(s);
    while ((n > 0) && ((s[n - 1] == '\n') || (s[n - 1] == '\r'))) {
        s[n - 1] = '\0';
        n--;
    }
}

static size_t char_count_utf8(const char* s, size_t bytes) {
    size_t count = 0;
    for (size_t i = 0; i < bytes; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

static int write_all(int fd, const char* data, size_t length) {
    while (length > 0) {
        ssize_t n = write(fd, data, length);
        if ((n == -1) && (errno == EINTR)) {
            continue;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }
        if (n < 0) {
            return -1;
        }
        data += n;
        length -= (size_t)n; 
    }
    return 0;
}

int main(void) {
    char* names[2] = {NULL, NULL};
    size_t name_capacities[2] = {0, 0};
    for (int i = 0; i < 2; i++) {
        printf("Output filename for child%d: ", i + 1);
        if (fflush(stdout) == EOF) {
            perror("parent: fflush stdout");
            free(names[0]);
            free(names[1]);
            return EXIT_FAILURE;
        }
        if (getline(&names[i], &name_capacities[i], stdin) == -1) {
            if (ferror(stdin)) {
                perror("parent: getline filename");
            } else {
                fprintf(stderr, "Unexpected EOF while reading filename %d\n", i + 1);
            }
            free(names[0]);
            free(names[1]);
            return EXIT_FAILURE;
        }
        remove_line_ending(names[i]);
        if (names[i][0] == '\0') {
            fprintf(stderr, "The file name cannot be empty\n");
            free(names[0]);
            free(names[1]);
            return EXIT_FAILURE;
        }
    }

    if (strcmp(names[0], names[1]) == 0) {
        fprintf(stderr, "Different files are needed for the processes\n");
        free(names[0]);
        free(names[1]);
        return EXIT_FAILURE;
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        free(names[0]);
        free(names[1]);
        return EXIT_FAILURE;
    }

    int pipes[2][2];
    if (pipe(pipes[0]) == -1) {
        perror("pipe1");
        free(names[0]);
        free(names[1]);
        return EXIT_FAILURE;
    }
    if (pipe(pipes[1]) == -1) {
        perror("pipe2");
        if (close(pipes[0][0]) == -1) {
            perror("cleanup: close pipe1 read");
        }
        if (close(pipes[0][1]) == -1) {
            perror("cleanup: close pipe1 write");
        }
        free(names[0]);
        free(names[1]);
        return EXIT_FAILURE;
    }

    pid_t children[2];
    int started = 0;
    int failure = 0;

    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            failure = 1;
            break;
        }
        if (pid == 0) {
            if (dup2(pipes[i][0], STDIN_FILENO) == -1) {
                perror("dup2 stdin");
                _exit(1);
            }
            for (int j = 0; j < 2; j++) {
                if (close(pipes[j][0]) == -1) {
                    perror("child: close read");
                    _exit(1);
                }
                if (close(pipes[j][1]) == -1) {
                    perror("child: close write");
                    _exit(1);
                }
            }
            int output = open(names[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output == -1) {
                perror("open output");
                _exit(1);
            }
            if (dup2(output, STDOUT_FILENO) == -1) {
                perror("dup2 stdout");
                close(output);
                _exit(1);
            }
            if (output != STDOUT_FILENO) {
                if (close(output) == -1) {
                    perror("child: close output");
                    _exit(1);
                }
            }
            execl("./build/child", "child", (char*)NULL);
            
            perror("execl child");
            _exit(127);
        }
        fprintf(stderr, "Started child%d: PID=%ld\n", i + 1, (long)pid);
        children[started] = pid;
        started++;
    }

    for (int j = 0; j < 2; j++) {
        if (close(pipes[j][0]) == -1) {
            fprintf(stderr, "close pipe%d read: %s\n", j + 1, strerror(errno));
            failure = 1;
        }
    }

    char* line = NULL;
    size_t capacity = 0;
    ssize_t length;
    if (!failure) {
        while ((length = getline(&line, &capacity, stdin)) != -1) {
            size_t content_length = (size_t)length;
            while ((content_length > 0) && ((line[content_length - 1] == '\n') || (line[content_length - 1] == '\r'))) {
                content_length--;
            }
            int target = char_count_utf8(line, content_length) > 10 ? 1 : 0;
            if (write_all(pipes[target][1], line, (size_t)length) == -1) {
                perror("write pipe");
                failure = 1;
                break;
            }
        }
        if (ferror(stdin)) {
            perror("read stdin");
            failure = 1;
        }
    }

    free(line);
    
    for (int j = 0; j < 2; j++) {
        if (close(pipes[j][1]) == -1) {
            fprintf(stderr, "close pipe%d write: %s\n", j + 1, strerror(errno));
            failure = 1;
        }
    }

    for (int i = 0; i < started; i++) {
        int status;
        pid_t result;
        do {
            result = waitpid(children[i], &status, 0);
        } while ((result == -1) && (errno == EINTR));

        if (result == -1) {
            perror("waitpid");
            failure = 1;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child%d killed by signal %d\n", i + 1, WTERMSIG(status));
            failure = 1;
        } else if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code != 0) {
                fprintf(stderr, "child%d exited with code %d\n", i + 1, code);
                failure = 1;
            }
        } else {
            fprintf(stderr, "child%d terminated unexpectedly\n", i + 1);
            failure = 1;
        }
        
        
    }

    free(names[0]);
    free(names[1]);

    return failure ? EXIT_FAILURE : EXIT_SUCCESS;
}
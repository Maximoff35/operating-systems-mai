#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Read an entire line, even if it is longer than a fixed-size buffer.
// Return 1 for a line, 0 for EOF, -1 for an error.
static int read_line(char **out, unsigned long line_number) {
    size_t capacity = 64;
    size_t length = 0;
    char *line = malloc(capacity);
    if (line == NULL) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    int ch;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (ch == '\0' || ch == 0x1a) {
            fprintf(stderr, "Invalid control byte 0x%02X on line %lu\n",
                    (unsigned int)ch, line_number);
            free(line);
            return -1;
        }
        if (length + 1 >= capacity) {
            if (capacity > SIZE_MAX / 2) {
                fprintf(stderr, "Input line is too long\n");
                free(line);
                return -1;
            }
            size_t new_capacity = capacity * 2;
            char *larger = realloc(line, new_capacity);
            if (larger == NULL) {
                fprintf(stderr, "realloc failed\n");
                free(line);
                return -1;
            }
            line = larger;
            capacity = new_capacity;
        }
        line[length++] = (char)ch;
    }
    if (ferror(stdin)) {
        fprintf(stderr, "Cannot read standard input\n");
        free(line);
        return -1;
    }
    if (ch == EOF && length == 0) {
        free(line);
        *out = NULL;
        return 0;
    }
    line[length] = '\0';
    *out = line;
    return 1;
}

static int sum_line(const char *line, unsigned long line_number) {
    const char *p = line;
    long long sum = 0;
    int found = 0;

    while (*p != '\0') {
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '\0') break;

        char *end;
        errno = 0;
        long value = strtol(p, &end, 10);
        if (end == p || errno == ERANGE || value < INT_MIN || value > INT_MAX ||
            (*end != '\0' && !isspace((unsigned char)*end))) {
            fprintf(stderr, "Invalid int on line %lu\n", line_number);
            return 0;
        }
        if ((value > 0 && sum > LLONG_MAX - value) ||
            (value < 0 && sum < LLONG_MIN - value)) {
            fprintf(stderr, "Sum overflow on line %lu\n", line_number);
            return 0;
        }
        sum += value;
        found = 1;
        p = end;
    }
    if (found) {
        if (printf("Line %lu: %lld\n", line_number, sum) < 0) {
            fprintf(stderr, "Cannot write child stdout\n");
            return 0;
        }
        // Keep earlier results ahead of diagnostics written to the same pipe.
        if (fflush(stdout) == EOF) {
            fprintf(stderr, "Cannot flush child stdout\n");
            return 0;
        }
    }
    return 1;
}

int main(void) {
    // Read actual bytes: text mode would interpret Ctrl+Z as an early EOF.
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        perror("_setmode(stdin)");
        return 1;
    }

    unsigned long line_number = 0;
    for (;;) {
        char *line = NULL;
        int state = read_line(&line, line_number + 1);
        if (state == 0) break;
        if (state < 0) return 1;

        ++line_number;
        int ok = sum_line(line, line_number);
        free(line);
        if (!ok) return 1;
    }
    if (fflush(stdout) == EOF) {
        fprintf(stderr, "Cannot flush child stdout\n");
        return 1;
    }
    return 0;
}

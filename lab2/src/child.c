#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

static int is_vowel(char c) {
    switch (c) {
        case 'a': case 'e': case 'i': case 'u': case 'y': case 'o':
        case 'A': case 'E': case 'I': case 'O': case 'U': case 'Y':
            return 1;
        default:
            return 0;
    }
}

int main(void) {
    char* line = NULL;
    size_t capacity = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, stdin)) != -1) {
        size_t j = 0;
        for (ssize_t i = 0; i < length; i++) {
            if (!is_vowel(line[i])) {
                line[j] = line[i];
                j++;
            }
        }
        if (fwrite(line, 1, j, stdout) != j) {
            perror("child: write");
            free(line);
            return EXIT_FAILURE;
        }
    }
    if (ferror(stdin)) {
        perror("child: read");
        free(line);
        return EXIT_FAILURE;
    }
    free(line);
    if (fclose(stdout) == EOF) {
        perror("child:fclose stdout");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
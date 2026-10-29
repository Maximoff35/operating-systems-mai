#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "minmax.h"

int main(int argc, char* argv[]) {
    int verbose = ((argc == 6) && (strcmp(argv[5], "--verbose") == 0));
    int demo = (argc == 6 && strcmp(argv[5], "--demo") == 0);
    if (((argc != 6) && (argc != 5)) || (strcmp(argv[1], "--threads") != 0) || (strcmp(argv[3], "--size") != 0) || ((argc == 6) && (!verbose) && (!demo))) {
        fprintf(stderr, "Usage: %s --threads N --size M [--verbose|--demo]\n", argv[0]);
        return 1;
    }
    char* end;
    errno = 0;
    long requested = strtol(argv[2], &end, 10);
    if ((errno != 0) || (end == argv[2]) || (*end != '\0') || (requested <= 0)) {
        fprintf(stderr, "Invalid thread count\n");
        return 1;
    }
    errno = 0;
    unsigned long long requested_size = strtoull(argv[4], &end, 10);
    if ((argv[4][0] == '-') || (errno != 0) || (end == argv[4]) || (*end != '\0') || (requested_size == 0) || (requested_size > SIZE_MAX / sizeof(int))) {
        fprintf(stderr, "invalid array size\n");
        return 1;
    }
    size_t size = (size_t)requested_size;
    int* array = malloc(size * sizeof(int));
    if (array == NULL) {
        perror("malloc array");
        return 1;
    }
    srand(42);
    for (size_t i = 0; i < size; i++) {
        array[i] = rand() - RAND_MAX / 2;
    }

    size_t thread_count = (size_t)requested;
    if (thread_count > size) {
        thread_count = size;
    }
    pthread_t* threads = calloc(thread_count, sizeof(*threads));
    MinMaxTask* tasks = calloc(thread_count, sizeof(*tasks));
    if ((threads == NULL) || (tasks == NULL)) {
        perror("calloc");
        free(threads);
        free(tasks);
        free(array);
        return 1;
    }
    size_t base = size / thread_count;
    size_t remainder = size % thread_count;
    size_t offset = 0;
    size_t launched = 0;
    int failed = 0;
    struct timespec start, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        free(tasks);
        free(threads);
        free(array);
        return 1;
    }
    for (size_t i = 0; i < thread_count; i++) {
        size_t count = base + (i < remainder ? 1 : 0);
        tasks[i].array = array + offset;
        tasks[i].size = count;
        tasks[i].status = -1;
        tasks[i].record_tid = verbose || demo;
        tasks[i].demo = demo;
        int rc = pthread_create(&threads[i], NULL, minmax_worker, &tasks[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            failed = 1;
            break;
        }
        launched++;
        offset += count;
    }
    MinMaxResult overall = {0};
    int initialized = 0;
    for (size_t i = 0; i < launched; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(rc));
            _Exit(EXIT_FAILURE);
        }
        if (tasks[i].status != 0) {
            fprintf(stderr, "Worker %zu failed\n", i);
            failed = 1;
            continue;
        }
        if (!initialized) {
            overall = tasks[i].result;
            initialized = 1;
        } else {
            if (tasks[i].result.min < overall.min) {
                overall.min = tasks[i].result.min;
            }
            if (tasks[i].result.max > overall.max) {
                overall.max = tasks[i].result.max;
            }
        }
    }
    double elapsed = 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        perror("clock_gettime");
        failed = 1;
    } else {
        elapsed = (double)(end_time.tv_sec - start.tv_sec) + (double)(end_time.tv_nsec - start.tv_nsec) / 1e9;
    }
    int exit_code = ((failed) || (!initialized) ? 1 : 0);
    if (exit_code == 0) {
        if ((verbose) || (demo)) {
            for (size_t i = 0; i < launched; i++) {
                printf("PID=%ld TID=%ld | size=%zu | min=%d max=%d\n", (long)getpid(), tasks[i].tid, tasks[i].size, tasks[i].result.min, tasks[i].result.max);
            }
        }
        printf("Array size: %zu\n", size);
        printf("Worker threads: %zu\n", thread_count);
        printf("Minimum: %d\n", overall.min);
        printf("Maximum: %d\n", overall.max);
        if (!demo) printf("Elapsed time: %.6f seconds\n", elapsed);
        else printf("Demo mode: timing ommitted\n");
    }
    free(tasks);
    free(threads);
    free(array);

    return exit_code;
}
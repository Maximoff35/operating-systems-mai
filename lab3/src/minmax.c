#define _GNU_SOURCE

#include "minmax.h"

#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

int find_minmax(const int* array, size_t size, MinMaxResult* result) {
    if ((array == NULL) || (result == NULL) || (size == 0)) {
        return -1;
    }
    int min = array[0];
    int max = array[0];
    for (size_t i = 1; i < size; i++) {
        if (array[i] < min) {
            min = array[i];
        }
        if (array[i] > max) {
            max = array[i];
        }
    }
    result->min = min;
    result->max = max;
    return 0;
}

void* minmax_worker(void* arg) {
    MinMaxTask* task = (MinMaxTask*)arg;
    if (task->record_tid) {
        task->tid = syscall(SYS_gettid);
        if (task->tid == -1) {
            perror("syscall gettid");
            task->status = -1;
            return NULL;
        }
    }
    task->status = find_minmax(task->array, task->size, &task->result);

    if (task->status != 0) {
        return NULL;
    }

    if (task->demo) {
        struct timespec remaining = {12, 0};
        while (nanosleep(&remaining, &remaining) == -1) {
            if (errno != EINTR) {
                perror("nanosleep");
                task->status = -1;
                return NULL;
            }
        }
    }

    return NULL;
}
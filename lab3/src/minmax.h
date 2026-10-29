#ifndef MINMAX_H
#define MINMAX_H

#include <stddef.h>

typedef struct {
    int min;
    int max;
} MinMaxResult;

typedef struct {
    const int* array;
    size_t size;
    MinMaxResult result;
    int status;
    long tid;
    int record_tid;
    int demo;
} MinMaxTask;

int find_minmax(const int* array, size_t size, MinMaxResult* result);
void* minmax_worker(void* arg);

#endif
#ifndef LAB_COMMON_H
#define LAB_COMMON_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <wchar.h>
void win_error(const char *operation);
int close_handle(HANDLE *handle);
int read_line(char **line);
wchar_t *decode_utf8(const char *text);
size_t character_count(const wchar_t *text);
int write_all(HANDLE pipe, const char *data, size_t length);
#endif

#include "common.h"
void win_error(const char *operation) {
    DWORD error = GetLastError();
    fprintf(stderr, "%s failed (Windows error %lu)\n", operation, (unsigned long)error);
}
int close_handle(HANDLE *handle) {
    if (*handle == NULL || *handle == INVALID_HANDLE_VALUE) return 1;
    if (!CloseHandle(*handle)) { win_error("CloseHandle"); return 0; }
    *handle = NULL;
    return 1;
}
/* No fixed limit on line length. LF/CRLF are excluded from the result. */
int read_line(char **out) {
    size_t size = 128, used = 0;
    char *line = malloc(size);
    *out = NULL;
    if (!line) { perror("malloc"); return -1; }
    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        if (c == 0) { fprintf(stderr, "NUL is not valid text\n"); free(line); return -1; }
        if (used + 1 >= size) {
            if (size > SIZE_MAX / 2) { fprintf(stderr, "Line too long\n"); free(line); return -1; }
            char *next = realloc(line, size * 2);
            if (!next) { perror("realloc"); free(line); return -1; }
            line = next; size *= 2;
        }
        line[used++] = (char)c;
    }
    if (ferror(stdin)) { perror("stdin"); free(line); return -1; }
    if (c == EOF && used == 0) { free(line); return 0; }
    if (used && line[used - 1] == '\r') --used;
    line[used] = 0;
    *out = line;
    return 1;
}
wchar_t *decode_utf8(const char *text) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!n) { win_error("MultiByteToWideChar (input must be UTF-8)"); return NULL; }
    wchar_t *wide = malloc((size_t)n * sizeof(*wide));
    if (!wide) { perror("malloc"); return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, n)) {
        win_error("MultiByteToWideChar"); free(wide); return NULL;
    }
    return wide;
}
size_t character_count(const wchar_t *text) {
    size_t n = 0;
    for (; *text; ++text) {
        unsigned c = (unsigned)*text;
        if (c < 0xDC00 || c > 0xDFFF) ++n; /* Count a surrogate pair once. */
    }
    return n;
}
int write_all(HANDLE pipe, const char *data, size_t length) {
    while (length) {
        DWORD written = 0;
        DWORD part = length > 65536 ? 65536 : (DWORD)length;
        if (!WriteFile(pipe, data, part, &written, NULL)) { win_error("WriteFile"); return 0; }
        if (!written) { fprintf(stderr, "WriteFile made no progress\n"); return 0; }
        data += written; length -= written;
    }
    return 1;
}

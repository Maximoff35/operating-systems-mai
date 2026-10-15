#include "common.h"
#include <fcntl.h>
#include <io.h>

static int is_vowel(wchar_t c) {
    if (wcschr(L"AEIOUaeiou", c)) return 1;
    /* Russian vowels, both cases. Hex escapes keep source encoding portable. */
    return wcschr(L"\x0410\x0415\x0401\x0418\x041E\x0423\x042B\x042D\x042E\x042F"
                  L"\x0430\x0435\x0451\x0438\x043E\x0443\x044B\x044D\x044E\x044F", c) != NULL;
}
int main(void) {
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 ||
        _setmode(_fileno(stdout), _O_BINARY) == -1) { perror("_setmode"); return 1; }
    for (;;) {
        char *line = NULL;
        int state = read_line(&line);
        if (state == 0) break;
        if (state < 0) return 1;
        wchar_t *wide = decode_utf8(line);
        free(line);
        if (!wide) return 1;
        size_t output = 0;
        for (size_t i = 0; wide[i]; ++i)
            if (!is_vowel(wide[i])) wide[output++] = wide[i];
        wide[output] = 0;
        int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
        if (!size) { win_error("WideCharToMultiByte"); free(wide); return 1; }
        char *utf8 = malloc((size_t)size);
        if (!utf8) { perror("malloc"); free(wide); return 1; }
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, utf8, size, NULL, NULL)) {
            win_error("WideCharToMultiByte"); free(utf8); free(wide); return 1;
        }
        free(wide);
        int ok = fwrite(utf8, 1, (size_t)size - 1, stdout) == (size_t)size - 1;
        free(utf8);
        if (!ok || fputc('\n', stdout) == EOF || fflush(stdout) == EOF) {
            fprintf(stderr, "Cannot write child output\n"); return 1;
        }
    }
    return 0;
}

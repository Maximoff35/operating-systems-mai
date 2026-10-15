#include "common.h"

typedef struct {
    HANDLE write_pipe;
    PROCESS_INFORMATION process;
} Child;

static int start_child(Child *child, const wchar_t *program, const wchar_t *filename) {
    HANDLE input = NULL, output = INVALID_HANDLE_VALUE, errors = NULL;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    STARTUPINFOW si = {0};
    int ok = 0;
    output = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) { win_error("CreateFileW(output)"); goto cleanup; }
    if (!CreatePipe(&input, &child->write_pipe, &sa, 0)) {
        win_error("CreatePipe"); input = NULL; child->write_pipe = NULL; goto cleanup;
    }
    /* Only the read end is inherited. Each child is started before the next pipe exists. */
    if (!SetHandleInformation(child->write_pipe, HANDLE_FLAG_INHERIT, 0)) {
        win_error("SetHandleInformation"); goto cleanup;
    }
    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),
                         GetCurrentProcess(), &errors, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        win_error("DuplicateHandle(stderr)"); goto cleanup;
    }
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input;
    si.hStdOutput = output;
    si.hStdError = errors;
    if (!CreateProcessW(program, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si, &child->process)) {
        win_error("CreateProcessW"); goto cleanup;
    }
    ok = 1;
cleanup:
    if (!close_handle(&input)) ok = 0;
    if (!close_handle(&output)) ok = 0;
    if (!close_handle(&errors)) ok = 0;
    return ok;
}

static int child_path(wchar_t *path, const wchar_t *name) {
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!n || n >= MAX_PATH) { win_error("GetModuleFileNameW"); return 0; }
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash || (size_t)(slash + 1 - path) + wcslen(name) + 1 > MAX_PATH) {
        fprintf(stderr, "Child path too long\n"); return 0;
    }
    wcscpy(slash + 1, name);
    return 1;
}

int main(void) {
    Child children[2] = {0};
    wchar_t *filenames[2] = {NULL, NULL};
    wchar_t programs[2][MAX_PATH];
    int result = 1;
    DWORD mode;
    UINT old_input_cp = 0, old_output_cp = 0;
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)) {
        old_input_cp = GetConsoleCP();
        if (!SetConsoleCP(CP_UTF8)) { win_error("SetConsoleCP"); goto cleanup; }
    }
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode)) {
        old_output_cp = GetConsoleOutputCP();
        if (!SetConsoleOutputCP(CP_UTF8)) { win_error("SetConsoleOutputCP"); goto cleanup; }
    }
    for (int i = 0; i < 2; ++i) {
        if (printf("Output filename for child%d: ", i + 1) < 0 || fflush(stdout) == EOF) goto cleanup;
        char *line = NULL;
        int state = read_line(&line);
        if (state != 1 || !line[0]) { fprintf(stderr, "Output filename is required\n"); free(line); goto cleanup; }
        filenames[i] = decode_utf8(line); free(line);
        if (!filenames[i]) goto cleanup;
    }
    /* Prevent accidental truncation of one output by the other child. */
    wchar_t full[2][MAX_PATH];
    for (int i = 0; i < 2; ++i) {
        DWORD n = GetFullPathNameW(filenames[i], MAX_PATH, full[i], NULL);
        if (!n || n >= MAX_PATH) { win_error("GetFullPathNameW"); goto cleanup; }
    }
    if (_wcsicmp(full[0], full[1]) == 0) { fprintf(stderr, "Output files must be different\n"); goto cleanup; }
    if (!child_path(programs[0], L"child1.exe") || !child_path(programs[1], L"child2.exe")) goto cleanup;
    for (int i = 0; i < 2; ++i)
        if (!start_child(&children[i], programs[i], filenames[i])) goto cleanup;
    if (printf("Parent PID: %lu\nChild1 PID: %lu\nChild2 PID: %lu\n"
               "Enter text lines. Finish with Ctrl+Z then Enter (EOF).\n",
               (unsigned long)GetCurrentProcessId(), (unsigned long)children[0].process.dwProcessId,
               (unsigned long)children[1].process.dwProcessId) < 0 || fflush(stdout) == EOF) goto cleanup;
    size_t number = 0;
    for (;;) {
        char *line = NULL;
        int state = read_line(&line);
        if (state == 0) break;
        if (state < 0) goto cleanup;
        wchar_t *wide = decode_utf8(line);
        if (!wide) { free(line); goto cleanup; }
        size_t length = character_count(wide); free(wide);
        int target = length > 10 ? 1 : 0;
        if (!write_all(children[target].write_pipe, line, strlen(line)) ||
            !write_all(children[target].write_pipe, "\n", 1)) { free(line); goto cleanup; }
        free(line);
        if (printf("Line %zu: %zu characters -> child%d\n", ++number, length, target + 1) < 0 ||
            fflush(stdout) == EOF) goto cleanup;
    }
    result = 0;
cleanup:
    /* Close BOTH writers before waiting: each child must receive EOF. */
    for (int i = 0; i < 2; ++i)
        if (!close_handle(&children[i].write_pipe)) result = 1;
    for (int i = 0; i < 2; ++i) {
        if (children[i].process.hProcess) {
            if (WaitForSingleObject(children[i].process.hProcess, INFINITE) != WAIT_OBJECT_0) {
                win_error("WaitForSingleObject"); result = 1;
            } else {
                DWORD code;
                if (!GetExitCodeProcess(children[i].process.hProcess, &code)) {
                    win_error("GetExitCodeProcess"); result = 1;
                } else if (code != 0) {
                    fprintf(stderr, "Child%d exited with code %lu\n", i + 1, (unsigned long)code); result = 1;
                }
            }
        }
        if (!close_handle(&children[i].process.hThread)) result = 1;
        if (!close_handle(&children[i].process.hProcess)) result = 1;
        free(filenames[i]);
    }
    if (old_input_cp && !SetConsoleCP(old_input_cp)) { win_error("Restore console input CP"); result = 1; }
    if (old_output_cp && !SetConsoleOutputCP(old_output_cp)) { win_error("Restore console output CP"); result = 1; }
    return result;
}

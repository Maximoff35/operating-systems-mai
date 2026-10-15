#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

static void print_win_error(const char *what) {
    fprintf(stderr, "%s failed (Windows error %lu)\n", what,
            (unsigned long)GetLastError());
}

static int close_checked(HANDLE *handle) {
    if (*handle == NULL || *handle == INVALID_HANDLE_VALUE) {
        return 1;
    }
    if (!CloseHandle(*handle)) {
        print_win_error("CloseHandle");
        *handle = NULL;
        return 0;
    }
    *handle = NULL;
    return 1;
}

int main(void) {
    char filename[MAX_PATH];
    char child_path[MAX_PATH];
    char buffer[4096];
    HANDLE input_file = INVALID_HANDLE_VALUE;
    HANDLE pipe_read = NULL;
    HANDLE pipe_write = NULL;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    int result = 1;
    DWORD bytes_read = 0;

    // Copy bytes from the pipe without translating the child's CRLF again.
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        perror("_setmode(stdout)");
        return 1;
    }

    printf("Enter input filename (for example, input.txt): ");
    fflush(stdout);
    if (!fgets(filename, sizeof(filename), stdin)) {
        fprintf(stderr, "Cannot read filename.\n");
        return 1;
    }
    if (!strchr(filename, '\n') && !feof(stdin)) {
        fprintf(stderr, "Filename is too long.\n");
        return 1;
    }
    filename[strcspn(filename, "\r\n")] = '\0';
    if (filename[0] == '\0') {
        fprintf(stderr, "Filename is empty.\n");
        return 1;
    }

    // The input file is inherited as child's STDIN.
    input_file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, &sa,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input_file == INVALID_HANDLE_VALUE) {
        print_win_error("CreateFileA");
        return 1;
    }

    // Child writes to the pipe; parent reads from it.
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0)) {
        print_win_error("CreatePipe");
        goto cleanup;
    }
    // Child does not read this pipe and must not keep an extra reader alive.
    // The parent sees EOF only after all WRITE handles have been closed.
    if (!SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
        print_win_error("SetHandleInformation");
        goto cleanup;
    }

    // Find child.exe beside parent.exe, independently of current directory.
    DWORD path_len = GetModuleFileNameA(NULL, child_path, MAX_PATH);
    if (path_len == 0 || path_len >= MAX_PATH) {
        print_win_error("GetModuleFileNameA");
        goto cleanup;
    }
    char *slash = strrchr(child_path, '\\');
    if (slash == NULL || (size_t)(slash - child_path) +
                             sizeof("\\child.exe") > sizeof(child_path)) {
        fprintf(stderr, "Cannot build child.exe path.\n");
        goto cleanup;
    }
    strcpy(slash + 1, "child.exe");

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input_file; // File -> child stdin
    si.hStdOutput = pipe_write; // Child stdout -> pipe
    si.hStdError = pipe_write; // Errors also go to the parent

    if (!CreateProcessA(child_path, NULL, NULL, NULL, TRUE, 0,
                        NULL, NULL, &si, &pi)) {
        print_win_error("CreateProcessA");
        goto cleanup;
    }
    printf("Parent PID: %lu\r\nChild PID: %lu\r\nResults:\r\n",
           (unsigned long)GetCurrentProcessId(),
           (unsigned long)pi.dwProcessId);
    fflush(stdout);

    // Close parent's copies: otherwise ReadFile can wait forever for EOF.
    if (!close_checked(&pipe_write) || !close_checked(&input_file)) {
        goto cleanup;
    }

    for (;;) {
        BOOL ok = ReadFile(pipe_read, buffer, sizeof(buffer), &bytes_read, NULL);
        if (!ok) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                break; // All write handles are closed: normal EOF.
            }
            print_win_error("ReadFile");
            goto cleanup;
        }
        if (bytes_read == 0) {
            break;
        }
        if (fwrite(buffer, 1, bytes_read, stdout) != bytes_read) {
            fprintf(stderr, "Cannot write to parent stdout.\n");
            goto cleanup;
        }
    }
    if (fflush(stdout) == EOF) {
        fprintf(stderr, "Cannot flush parent stdout.\n");
        goto cleanup;
    }
    if (!close_checked(&pipe_read)) {
        goto cleanup;
    }
    if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
        print_win_error("WaitForSingleObject");
        goto cleanup;
    }
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        print_win_error("GetExitCodeProcess");
        goto cleanup;
    }
    if (exit_code != 0) {
        fprintf(stderr, "Child process exited with code %lu.\n",
                (unsigned long)exit_code);
        goto cleanup;
    }
    result = 0;

cleanup:
    // Close the pipe before waiting: a failed reader must not block its child.
    if (!close_checked(&pipe_read)) result = 1;
    if (!close_checked(&pipe_write)) result = 1;
    if (input_file != INVALID_HANDLE_VALUE && !close_checked(&input_file)) result = 1;
    if (pi.hProcess != NULL) {
        if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
            print_win_error("WaitForSingleObject");
            result = 1;
        }
    }
    if (!close_checked(&pi.hThread)) result = 1;
    if (!close_checked(&pi.hProcess)) result = 1;
    return result;
}

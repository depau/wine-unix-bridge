#define UNICODE
#define _UNICODE
#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static char *wstr_to_utf8_alloc(const wchar_t *ws) {
    if (!ws) return NULL;

    const int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;

    char *out = malloc((size_t) n);
    if (!out) return NULL;

    const int n2 = WideCharToMultiByte(CP_UTF8, 0, ws, -1, out, n, NULL, NULL);
    if (n2 <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

struct proxy_args {
    HANDLE hWin;
    int unix_fd;
    int to_unix;
};

static DWORD WINAPI proxy_thread(LPVOID lpParam) {
    struct proxy_args *args = lpParam;
    char buf[4096];
    DWORD dwRead, dwWritten;

    if (args->to_unix) {
        while (ReadFile(args->hWin, buf, sizeof(buf), &dwRead, NULL) && dwRead > 0) {
            if (write(args->unix_fd, buf, dwRead) <= 0) break;
        }
        close(args->unix_fd);
    } else {
        ssize_t n;
        while ((n = read(args->unix_fd, buf, sizeof(buf))) > 0) {
            if (!WriteFile(args->hWin, buf, (DWORD) n, &dwWritten, NULL)) break;
        }
    }
    free(args);
    return 0;
}

int __cdecl unix_bridge_exec(const int argc, wchar_t **argv_w, const wchar_t *target_name_w) {
    char *target_name = wstr_to_utf8_alloc(target_name_w);
    const char *env_target = getenv("UNIX_BRIDGE_TARGET");
    const char *exec_path = env_target && *env_target
                                ? env_target
                                : target_name;

    if (argc <= 0 || !argv_w || !exec_path) {
        fprintf(stderr, "unixbridge: invalid arguments\n");
        free(target_name);
        return 111;
    }

    /* Build argv for exec: argv0 is exec_path, then forward wrapper args[1..]. */
    const int out_argc = argc;
    char **argv = calloc((size_t) out_argc + 1, sizeof(char *));
    if (!argv) {
        fprintf(stderr, "unixbridge: out of memory\n");
        free(target_name);
        return 111;
    }

    argv[0] = strdup(exec_path);
    if (!argv[0]) {
        fprintf(stderr, "unixbridge: out of memory\n");
        free(target_name);
        free(argv);
        return 111;
    }

    for (int i = 1; i < argc; i++) {
        argv[i] = wstr_to_utf8_alloc(argv_w[i]);
        if (!argv[i]) {
            fprintf(stderr, "unixbridge: argument conversion failed\n");
            for (int j = 0; j < i; j++) free(argv[j]);
            free(target_name);
            free(argv);
            return 111;
        }
    }
    argv[out_argc] = NULL;

    fflush(NULL);

    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        perror("unixbridge: pipe failed");
        for (int i = 0; i < out_argc; i++) free(argv[i]);
        free(target_name);
        free(argv);
        return 111;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("unixbridge: fork failed");
        for (int i = 0; i < out_argc; i++) free(argv[i]);
        free(target_name);
        free(argv);
        return 111;
    } else if (pid == 0) {
        /* Child process */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        execvp(exec_path, argv);
        /* If execvp returns, it failed */
        if (errno == ENOENT) {
            fprintf(stderr,
                    "unixbridge: could not find '%s' in the native PATH.\n\n"
                    "Please make sure that:\n"
                    " - The native executable is installed and in your PATH, or\n"
                    " - Rename this .exe file to match the target the native executable (i.e. `ffmpeg.exe`), or\n"
                    " - Set the UNIX_BRIDGE_TARGET environment variable (i.e. `export UNIX_BRIDGE_TARGET=/usr/bin/ffmpeg`) to point to the native executable.\n\n",
                    exec_path);
        } else {
            perror("unixbridge: execvp failed");
        }
        _exit(127);
    } else {
        /* Parent process */
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        HANDLE threads[3];
        struct proxy_args *a;

        a = malloc(sizeof(*a));
        a->hWin = GetStdHandle(STD_INPUT_HANDLE);
        a->unix_fd = stdin_pipe[1];
        a->to_unix = 1;
        threads[0] = CreateThread(NULL, 0, proxy_thread, a, 0, NULL);

        a = malloc(sizeof(*a));
        a->hWin = GetStdHandle(STD_OUTPUT_HANDLE);
        a->unix_fd = stdout_pipe[0];
        a->to_unix = 0;
        threads[1] = CreateThread(NULL, 0, proxy_thread, a, 0, NULL);

        a = malloc(sizeof(*a));
        a->hWin = GetStdHandle(STD_ERROR_HANDLE);
        a->unix_fd = stderr_pipe[0];
        a->to_unix = 0;
        threads[2] = CreateThread(NULL, 0, proxy_thread, a, 0, NULL);

        int status;
        int exit_code = 127;

        if (waitpid(pid, &status, 0) == -1) {
            perror("unixbridge: waitpid failed");
            exit_code = 111;
        } else if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exit_code = 128 + WTERMSIG(status);
        }

        /* Signal threads to exit if they haven't already */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        WaitForMultipleObjects(3, threads, TRUE, 1000); // Wait a bit for threads to flush
        for (int i = 0; i < 3; i++) CloseHandle(threads[i]);

        for (int i = 0; i < out_argc; i++) free(argv[i]);
        free(target_name);
        free(argv);
        return exit_code;
    }
}

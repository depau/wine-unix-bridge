#include <windows.h>
#include <shellapi.h>

#include <stdio.h>
#include <wchar.h>
#include <string.h>

typedef int (__cdecl *unix_bridge_exec_fn)(int argc, wchar_t **argv, const wchar_t *target_name);

static void get_target_name(const wchar_t *path, wchar_t *out, size_t out_cap) {
    const wchar_t *filename = path;
    for (const wchar_t *p = path; *p; p++) {
        if (*p == L'\\' || *p == L'/') filename = p + 1;
    }
    
    wcsncpy(out, filename, out_cap - 1);
    out[out_cap - 1] = L'\0';
    
    /* Strip .exe extension if present (case-insensitive) */
    const size_t len = wcslen(out);
    if (len > 4 && _wcsicmp(out + len - 4, L".exe") == 0) {
        out[len - 4] = L'\0';
    }
}

static void die_last_error(const wchar_t *msg, int exit_code) {
    const DWORD e = GetLastError();
    wchar_t buf[512];

    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, e, 0, buf, (DWORD) (sizeof(buf) / sizeof(buf[0])), NULL);

    if (!n) buf[0] = L'\0';

    fwprintf(stderr, L"%ls: (%lu) %ls\n", msg, (unsigned long) e, buf);
    ExitProcess(exit_code);
}

static void build_sibling_path(wchar_t *out, const size_t out_cap,
                               const wchar_t *sibling_name) {
    DWORD n = GetModuleFileNameW(NULL, out, (DWORD) out_cap);
    if (n == 0 || n >= out_cap) {
        die_last_error(L"GetModuleFileNameW failed", 100);
    }

    /* Strip filename, keep directory (including trailing backslash). */
    for (DWORD i = n; i > 0; i--) {
        wchar_t c = out[i - 1];
        if (c == L'\\' || c == L'/') {
            out[i] = L'\0';
            break;
        }
        if (i == 1) {
            /* No separator found; unexpected but handle it. */
            out[0] = L'\0';
        }
    }

    if (wcslen(out) + wcslen(sibling_name) + 1 > out_cap) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        die_last_error(L"Path buffer too small", 101);
    }

    wcscat(out, sibling_name);
}

int wmain(void) {
    int argc = 0;

    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) die_last_error(L"CommandLineToArgvW failed", 102);

    wchar_t target_name[MAX_PATH];
    get_target_name(argv[0], target_name, MAX_PATH);

    wchar_t dll_path[MAX_PATH];
    build_sibling_path(dll_path, (sizeof(dll_path) / sizeof(dll_path[0])),
                       L"unixbridge.dll");

    const HMODULE dll = LoadLibraryW(dll_path);
    if (!dll) die_last_error(L"LoadLibraryW(unixbridge.dll next to exe) failed", 103);

    const FARPROC p = GetProcAddress(dll, "unix_bridge_exec");
    if (!p) die_last_error(L"GetProcAddress(unix_bridge_exec) failed", 104);

    unix_bridge_exec_fn unix_bridge_exec = NULL;
    _Static_assert(sizeof(unix_bridge_exec) == sizeof(p), "function pointer size mismatch");
    memcpy(&unix_bridge_exec, &p, sizeof(unix_bridge_exec));

    const int rc = unix_bridge_exec(argc, argv, target_name);

    LocalFree(argv);
    return rc;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_ARGS 16

__declspec(dllimport) int __cdecl ParseCommand(char *line, char *args[], int limit)
{
    int count = 0;
    char *readp = line;
    char *writep = line;

    if (!line || !args || limit <= 0 || limit > MAX_ARGS)
        return -1;

    while (*readp) {
        int quoted = 0;
        while (isspace((unsigned char)*readp))
            ++readp;
        if (!*readp)
            break;
        if (count >= limit)
            return -1;

        args[count++] = writep;
        while (*readp) {
            if (*readp == '"') {
                quoted = !quoted;
                ++readp;
            } else if (!quoted && isspace((unsigned char)*readp)) {
                ++readp;
                break;
            } else {
                *writep++ = *readp++;
            }
        }
        if (quoted)
            return -1;
        *writep++ = '\0';
    }
    return count;
}

__declspec(dllexport) int __cdecl ParseCommand_Export(char *line, char *args[], int limit)
{
    return ParseCommand(line, args, limit);
}

static int approved_verb(const char *verb)
{
    static const char *const verbs[] = {
        "dir", "type", "copy", "move", "del", "mkdir", "rmdir", "echo"
    };
    size_t i;
    if (!verb)
        return 0;
    for (i = 0; i < sizeof(verbs) / sizeof(verbs[0]); ++i)
        if (strcmp(verb, verbs[i]) == 0)
            return 1;
    return 0;
}

static int safe_argument(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    if (!p || !*p)
        return 0;
    while (*p) {
        if (*p < 32 || *p == 127 || strchr("&|<>^*%!?()\"", *p))
            return 0;
        ++p;
    }
    return 1;
}

static int append_token(char *out, size_t capacity, const char *token)
{
    size_t used = strlen(out);
    size_t len = strlen(token);
    if (used + len + 4 >= capacity)
        return 0;
    out[used++] = ' ';
    out[used++] = '"';
    memcpy(out + used, token, len);
    used += len;
    out[used++] = '"';
    out[used] = '\0';
    return 1;
}

__declspec(dllexport) int __stdcall ExecuteCommand(
    const char *windows_verb, const char *const args[], int argc)
{
    char command[MAX_LINE * 2] = {0};
    char cmdline[MAX_LINE * 2 + 64];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    int i;
    int n;

    if (!approved_verb(windows_verb) || argc < 0 || argc >= MAX_ARGS ||
        (argc > 0 && !args)) {
        puts("Invalid command.");
        return 1;
    }
    strcpy(command, windows_verb);
    for (i = 0; i < argc; ++i) {
        if (!safe_argument(args[i]) ||
            !append_token(command, sizeof(command), args[i])) {
            puts("Invalid or unsupported argument.");
            return 1;
        }
    }

    n = snprintf(cmdline, sizeof(cmdline), "cmd.exe /D /C %s", command);
    if (n < 0 || n >= (int)sizeof(cmdline))
        return 1;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0,
                        NULL, NULL, &si, &pi)) {
        fprintf(stderr, "Command execution failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &code))
        code = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static __declspec(dllexport) int __cdecl ExecuteCommandStatic(const char *windows_verb, const char *const args[], int argc)
{
    return 0;
}

__declspec(dllexport) int ExecuteCommand = 42;

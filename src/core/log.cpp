#include "log.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>

static FILE* g_log = nullptr;

void LogInit(const char* path) {
    if (path) g_log = fopen(path, "w");
}

void Log(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "%s\n", buf);
    if (g_log) {
        fprintf(g_log, "%s\n", buf);
        fflush(g_log);
    }
}

void FatalError(const char* title, const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Log("FATAL: %s", buf);
    if (!SDL_getenv("HOPMON_HEADLESS")) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, buf, nullptr);
    exit(1);
}

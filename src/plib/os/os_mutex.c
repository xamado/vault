#include "plib/os/os_mutex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
// POSIX backend
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#endif

struct os_mutex {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
    char path[256];
#endif
};

os_mutex* os_mutex_create(const char* name)
{
    os_mutex* mutex = malloc(sizeof(os_mutex));
    if (!mutex) return NULL;
    
#ifdef _WIN32
    mutex->handle = CreateMutexA(NULL, FALSE, name);
#else
    snprintf(mutex->path, sizeof(mutex->path), "/tmp/fallout1-re-%s.lock", name);
    
    mutex->fd = open(mutex->path, O_CREAT | O_RDWR, 0666);
    if (mutex->fd < 0) {
        free(mutex);
        return NULL;
    }
#endif
    
    return mutex;
}

bool os_mutex_try_lock(os_mutex* mutex)
{
    if (!mutex) return false;
    
#ifdef _WIN32
    // In Win32, if GetLastError() == ERROR_ALREADY_EXISTS, another process holds it.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return false;
    }
#else
    if (mutex->fd < 0) return false;
    
    // Attempt to lock exclusively without blocking
    if (flock(mutex->fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            return false; // Already locked by another process
        }
        return false;
    }
#endif
    
    return true;
}

void os_mutex_destroy(os_mutex* mutex)
{
    if (mutex) {
#ifdef _WIN32
        if (mutex->handle) {
            CloseHandle(mutex->handle);
        }
#else
        if (mutex->fd >= 0) {
            flock(mutex->fd, LOCK_UN);
            close(mutex->fd);
            unlink(mutex->path);
        }
#endif
        free(mutex);
    }
}

#include "plib/xfile/xsys_find.h"
#include "plib/os/os_string.h"
#include "plib/os/os_filesystem.h"

#include <string.h>

#if !defined(_WIN32) && !defined(__WATCOMC__)
#include <fnmatch.h>
#endif

static bool advance_to_match(DirectoryFileFindData* findData)
{
#if !defined(_WIN32) && !defined(__WATCOMC__)
    for (;;) {
        findData->entry = readdir(findData->dir);
        if (findData->entry == NULL) {
            return false;
        }
        if (fnmatch(findData->pattern, findData->entry->d_name, 0) == 0) {
            return true;
        }
    }
#else
    (void)findData;
    return false;
#endif
}

bool xsys_findfirst(const char* path, DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    findData->hFind = FindFirstFileA(path, &(findData->ffd));
    if (findData->hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
#elif defined(__WATCOMC__)
    findData->dir = opendir(path);
    if (findData->dir == NULL) {
        return false;
    }

    findData->entry = readdir(findData->dir);
    if (findData->entry == NULL) {
        closedir(findData->dir);
        return false;
    }
#else
    // Path may contain a glob pattern (e.g. "MAPS/*.SAV"). Split into
    // directory and pattern, then iterate manually with fnmatch.
    const char* lastSep = strrchr(path, '/');
#ifndef _WIN32
    const char* backSep = strrchr(path, '\\');
    if (backSep != NULL && (lastSep == NULL || backSep > lastSep)) {
        lastSep = backSep;
    }
#endif
    if (lastSep == NULL) {
        strncpy(findData->dirPath, ".", sizeof(findData->dirPath) - 1);
        findData->dirPath[sizeof(findData->dirPath) - 1] = '\0';
        strncpy(findData->pattern, path, sizeof(findData->pattern) - 1);
        findData->pattern[sizeof(findData->pattern) - 1] = '\0';
    } else {
        size_t dirLen = (size_t)(lastSep - path);
        if (dirLen == 0) dirLen = 1; // root
        
        char rawDirPath[1024];
        if (dirLen >= sizeof(rawDirPath)) dirLen = sizeof(rawDirPath) - 1;
        memcpy(rawDirPath, path, dirLen);
        rawDirPath[dirLen] = '\0';
        
        os_fs_normalize_path(findData->dirPath, sizeof(findData->dirPath), rawDirPath);

        strncpy(findData->pattern, lastSep + 1, sizeof(findData->pattern) - 1);
        findData->pattern[sizeof(findData->pattern) - 1] = '\0';
    }
    
    // Normalize pattern to uppercase if we are normalizing paths to uppercase
#if defined(__linux__)
    os_strupr(findData->pattern);
#endif
    if (findData->pattern[0] == '\0') {
        strncpy(findData->pattern, "*", sizeof(findData->pattern) - 1);
        findData->pattern[sizeof(findData->pattern) - 1] = '\0';
    }

    findData->dir = opendir(findData->dirPath);
    if (findData->dir == NULL) {
        return false;
    }

    if (!advance_to_match(findData)) {
        closedir(findData->dir);
        findData->dir = NULL;
        return false;
    }
#endif

    return true;
}

bool xsys_findnext(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    if (!FindNextFileA(findData->hFind, &(findData->ffd))) {
        return false;
    }
#elif defined(__WATCOMC__)
    findData->entry = readdir(findData->dir);
    if (findData->entry == NULL) {
        closedir(findData->dir);
        return false;
    }
#else
    if (!advance_to_match(findData)) {
        return false;
    }
#endif

    return true;
}

bool xsys_findclose(DirectoryFileFindData* findData)
{
#if defined(_MSC_VER)
    FindClose(findData->hFind);
#elif defined(__WATCOMC__)
    if (closedir(findData->dir) != 0) {
        return false;
    }
#else
    if (findData->dir != NULL) {
        if (closedir(findData->dir) != 0) {
            return false;
        }
        findData->dir = NULL;
    }
#endif

    return true;
}

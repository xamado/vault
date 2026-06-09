#ifndef FALLOUT_PLIB_XFILE_XSYS_FIND_H_
#define FALLOUT_PLIB_XFILE_XSYS_FIND_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

typedef struct DirectoryFileFindData {
#if defined(_WIN32)
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
#else
    DIR* dir;
    struct dirent* entry;
    char dirPath[1024];
    char pattern[256];
#endif
} DirectoryFileFindData;

bool xsys_findfirst(const char* path, DirectoryFileFindData* findData);
bool xsys_findnext(DirectoryFileFindData* findData);
bool xsys_findclose(DirectoryFileFindData* findData);

static inline bool fileFindIsDirectory(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    return (findData->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#elif defined(__WATCOMC__)
    return (findData->entry->d_attr & _A_SUBDIR) != 0;
#else
    char fullPath[1280];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", findData->dirPath, findData->entry->d_name);
    struct stat st;
    if (stat(fullPath, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
#endif
}

static inline char* fileFindGetName(DirectoryFileFindData* findData)
{
#if defined(_WIN32)
    return findData->ffd.cFileName;
#else
    return findData->entry->d_name;
#endif
}

#endif /* FALLOUT_PLIB_XFILE_XSYS_FIND_H_ */

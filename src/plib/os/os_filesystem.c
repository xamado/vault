#include "plib/os/os_filesystem.h"
#include "plib/os/os_string.h"

#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

void os_fs_normalize_path(char* dst, size_t dstSize, const char* src)
{
#if defined(__linux__)
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
    os_strupr(dst);
    char* p = dst;
    while (*p) {
        if (*p == '\\') *p = '/';
        p++;
    }
#else
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
#endif
}

FILE* os_fs_fopen(const char* path, const char* mode) 
{   
#if defined(__linux__)
    char linux_path[1024];
    os_fs_normalize_path(linux_path, sizeof(linux_path), path);
    return fopen(linux_path, mode);
#else
    return fopen(path, mode);
#endif
}

int os_fs_remove(const char* path) 
{
#if defined(__linux__)
    char linux_path[1024];
    os_fs_normalize_path(linux_path, sizeof(linux_path), path);
    return remove(linux_path);
#else
    return remove(path);
#endif
}

int os_fs_rename(const char* oldpath, const char* newpath) 
{
#if defined(__linux__)
    char linux_oldpath[1024];
    os_fs_normalize_path(linux_oldpath, sizeof(linux_oldpath), oldpath);
    
    char linux_newpath[1024];
    os_fs_normalize_path(linux_newpath, sizeof(linux_newpath), newpath);
    
    return rename(linux_oldpath, linux_newpath);
#else
    return rename(oldpath, newpath);
#endif
}

int os_fs_mkdir(const char* path) 
{
#if defined(__linux__)
    char linux_path[1024];
    os_fs_normalize_path(linux_path, sizeof(linux_path), path);
    return mkdir(linux_path, 0755);
#else
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0755);
#endif
#endif
}

long os_filesystem_file_size(int fd) {
#if defined(_WIN32)
    return _filelength(fd);
#else
    struct stat st;
    fstat(fd, &st);
    return st.st_size;
#endif
}

long os_filesystem_tell(int fd) {
#if defined(_WIN32)
    return _tell(fd);
#else
    return lseek(fd, 0, SEEK_CUR);
#endif
}

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <strings.h>
#endif

struct os_fs_find_data {
#if defined(_WIN32)
    HANDLE hFind;
    WIN32_FIND_DATAA fd;
#else
    DIR* dir;
    struct dirent* dirent;
    char linux_pattern[256];
#endif
};

os_fs_find_data* os_fs_find_first(const char* path)
{
    os_fs_find_data* find_data = (os_fs_find_data*)malloc(sizeof(os_fs_find_data));
    if (!find_data) return NULL;

#if defined(_WIN32)
    find_data->hFind = FindFirstFileA(path, &(find_data->fd));
    if (find_data->hFind == INVALID_HANDLE_VALUE) {
        free(find_data);
        return NULL;
    }
#else
    char dir_path[1024];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    char* p = dir_path;
    while (*p) {
        if (*p == '\\') *p = '/';
        p++;
    }

    // Isolate the directory and the wildcard
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        strcpy(find_data->linux_pattern, last_slash + 1);
        if (last_slash == dir_path) {
            strcpy(dir_path, "/");
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(find_data->linux_pattern, dir_path);
        strcpy(dir_path, ".");
    }
    
    // If pattern is *.*, treat it as *
    if (strcmp(find_data->linux_pattern, "*.*") == 0) {
        strcpy(find_data->linux_pattern, "*");
    }

    if (strlen(dir_path) == 0) {
        strcpy(dir_path, ".");
    }

    find_data->dir = opendir(dir_path);

    if (find_data->dir == NULL) {
        free(find_data);
        return NULL;
    }

    while ((find_data->dirent = readdir(find_data->dir)) != NULL) {
        if (strcmp(find_data->linux_pattern, "*") == 0) {
            return find_data;
        } else if (find_data->linux_pattern[0] == '*' && find_data->linux_pattern[1] == '.') {
            const char* ext = find_data->linux_pattern + 1;
            size_t ext_len = strlen(ext);
            size_t name_len = strlen(find_data->dirent->d_name);
            if (name_len >= ext_len) {
                if (os_stricmp(find_data->dirent->d_name + name_len - ext_len, ext) == 0) {
                    return find_data;
                }
            }
        } else {
            if (os_stricmp(find_data->dirent->d_name, find_data->linux_pattern) == 0) {
                return find_data;
            }
        }
    }

    closedir(find_data->dir);
    free(find_data);
    return NULL;
#endif

    return find_data;
}

int os_fs_find_next(os_fs_find_data* find_data)
{
    if (!find_data) return -1;

#if defined(_WIN32)
    if (!FindNextFileA(find_data->hFind, &(find_data->fd))) {
        return -1;
    }
#else
    while ((find_data->dirent = readdir(find_data->dir)) != NULL) {
        if (strcmp(find_data->linux_pattern, "*") == 0) {
            return 0;
        } else if (find_data->linux_pattern[0] == '*' && find_data->linux_pattern[1] == '.') {
            const char* ext = find_data->linux_pattern + 1;
            size_t ext_len = strlen(ext);
            size_t name_len = strlen(find_data->dirent->d_name);
            if (name_len >= ext_len) {
                if (os_stricmp(find_data->dirent->d_name + name_len - ext_len, ext) == 0) {
                    return 0;
                }
            }
        } else {
            if (os_stricmp(find_data->dirent->d_name, find_data->linux_pattern) == 0) {
                return 0;
            }
        }
    }
    return -1;
#endif

    return 0;
}

void os_fs_find_close(os_fs_find_data* find_data)
{
    if (!find_data) return;

#if defined(_WIN32)
    FindClose(find_data->hFind);
#else
    if (find_data->dir) {
        closedir(find_data->dir);
    }
#endif

    free(find_data);
}

bool os_fs_find_is_directory(os_fs_find_data* find_data)
{
    if (!find_data) return false;

#if defined(_WIN32)
    return (find_data->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    return find_data->dirent->d_type == DT_DIR;
#endif
}

const char* os_fs_find_get_name(os_fs_find_data* find_data)
{
    if (!find_data) return NULL;

#if defined(_WIN32)
    return find_data->fd.cFileName;
#else
    return find_data->dirent->d_name;
#endif
}

void os_filesystem_split_path(const char* path, char* drive, char* dir, char* fname, char* ext) {
#if defined(_WIN32)
    _splitpath(path, drive, dir, fname, ext);
#else
    if (drive) drive[0] = '\0';
    if (dir) dir[0] = '\0';
    if (fname) fname[0] = '\0';
    if (ext) ext[0] = '\0';
    
    if (!path) return;

    const char* p = path;

    if (p[0] != '\0' && p[1] == ':') {
        if (drive) {
            drive[0] = p[0];
            drive[1] = p[1];
            drive[2] = '\0';
        }
        p += 2;
    }

    const char* last_slash = strrchr(p, '\\');
    const char* last_fslash = strrchr(p, '/');
    if (last_fslash > last_slash) last_slash = last_fslash;

    if (last_slash) {
        if (dir) {
            size_t dir_len = last_slash - p + 1;
            strncpy(dir, p, dir_len);
            dir[dir_len] = '\0';
        }
        p = last_slash + 1;
    }

    const char* last_dot = strrchr(p, '.');
    if (last_dot) {
        if (fname) {
            size_t fname_len = last_dot - p;
            strncpy(fname, p, fname_len);
            fname[fname_len] = '\0';
        }
        if (ext) {
            strcpy(ext, last_dot);
        }
    } else {
        if (fname) {
            strcpy(fname, p);
        }
    }
#endif
}

void os_filesystem_make_path(char* path, const char* drive, const char* dir, const char* fname, const char* ext) {
#if defined(_WIN32)
    _makepath(path, drive, dir, fname, ext);
#else
    if (path == NULL) return;
    path[0] = '\0';
    if (drive != NULL && drive[0] != '\0') strcat(path, drive);
    if (dir != NULL && dir[0] != '\0') strcat(path, dir);
    if (fname != NULL && fname[0] != '\0') strcat(path, fname);
    if (ext != NULL && ext[0] != '\0') strcat(path, ext);
#endif
}

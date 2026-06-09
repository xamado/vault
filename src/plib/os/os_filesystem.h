#ifndef FALLOUT_PLIB_DB_FILESYSTEM_H_
#define FALLOUT_PLIB_DB_FILESYSTEM_H_

#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "plib/math.h"
#endif

#define MAX_PATH 260
#define _MAX_PATH 260
#define _MAX_DRIVE 3
#define _MAX_DIR 256
#define _MAX_FNAME 256
#define _MAX_EXT 256

typedef struct os_fs_find_data os_fs_find_data;

void os_fs_normalize_path(char* dst, size_t dstSize, const char* src);

FILE* os_fs_fopen(const char* path, const char* mode);
int os_fs_remove(const char* path);
int os_fs_rename(const char* oldpath, const char* newpath);
int os_fs_mkdir(const char* path);
long os_filesystem_file_size(int fd);
long os_filesystem_tell(int fd);

os_fs_find_data* os_fs_find_first(const char* path);
int os_fs_find_next(os_fs_find_data* find_data);
void os_fs_find_close(os_fs_find_data* find_data);
bool os_fs_find_is_directory(os_fs_find_data* find_data);
const char* os_fs_find_get_name(os_fs_find_data* find_data);

void os_filesystem_split_path(const char* path, char* drive, char* dir, char* fname, char* ext);
void os_filesystem_make_path(char* path, const char* drive, const char* dir, const char* fname, const char* ext);


#endif /* FALLOUT_PLIB_DB_FILESYSTEM_H_ */

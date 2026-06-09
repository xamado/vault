#include "game/gz.h"

#include <stdbool.h>
#include <stdio.h>
#include <zlib.h>

#include "plib/db/db.h"
#include "plib/os/os_filesystem.h"

static int gz_plain_copy(const char* src, const char* dst)
{
    char srcPathNorm[1024];
    os_fs_normalize_path(srcPathNorm, sizeof(srcPathNorm), src);
    FILE* in = fopen(srcPathNorm, "rb");
    if (in == NULL) return -1;

    char newPathNorm[1024];
    os_fs_normalize_path(newPathNorm, sizeof(newPathNorm), dst);
    FILE* out = fopen(newPathNorm, "wb");
    if (out == NULL) { db_fclose(in); return -1; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in); fclose(out); return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

// NOTE: Not present in debug symbols in `mapper2.exe`, but can be seen in OS X
// binary.
//
// 0x452740
int gzRealUncompressCopyReal_file(const char* existingFilePath, const char* newFilePath)
{
    char existingPathNorm[1024];
    os_fs_normalize_path(existingPathNorm, sizeof(existingPathNorm), existingFilePath);

    FILE* stream = fopen(existingPathNorm, "rb");
    if (stream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    rewind(stream);

    char newPathNorm[1024];
    os_fs_normalize_path(newPathNorm, sizeof(newPathNorm), newFilePath);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        fclose(stream);
        
        gzFile inStream = gzopen(existingPathNorm, "rb");
        FILE* outStream = fopen(newPathNorm, "wb");

        if (inStream != NULL && outStream != NULL) {
            for (;;) {
                int ch = gzgetc(inStream);
                if (ch == -1) {
                    break;
                }

                fputc(ch, outStream);
            }

            gzclose(inStream);
            fclose(outStream);
        } else {
            if (inStream != NULL) {
                gzclose(inStream);
            }

            if (outStream != NULL) {
                fclose(outStream);
            }

            return -1;
        }
    } else {
        gz_plain_copy(existingFilePath, newFilePath);
    }

    return 0;
}

// 0x47BD14
int gzcompress_file(const char* existingFilePath, const char* newFilePath)
{
    char existingPathNorm[1024];
    os_fs_normalize_path(existingPathNorm, sizeof(existingPathNorm), existingFilePath);

    FILE* inStream = fopen(existingPathNorm, "rb");
    if (inStream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(inStream);
    magic[1] = fgetc(inStream);
    rewind(inStream);

    char newPathNorm[1024];
    os_fs_normalize_path(newPathNorm, sizeof(newPathNorm), newFilePath);

    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        // Source file is already gzipped, there is no need to do anything
        // besides copying.
        fclose(inStream);
        gz_plain_copy(existingFilePath, newFilePath);
    } else {
        gzFile outStream = gzopen(newPathNorm, "wb");
        if (outStream == NULL) {
            fclose(inStream);
            return -1;
        }

        // Copy byte-by-byte.
        for (;;) {
            int ch = fgetc(inStream);
            if (ch == -1) {
                break;
            }

            gzputc(outStream, ch);
        }

        fclose(inStream);
        gzclose(outStream);
    }

    return 0;
}

// TODO: Check, implementation looks odd.
//
// 0x47BBA4
int gzdecompress_file(const char* existingFilePath, const char* newFilePath)
{
    char existingPathNorm[1024];
    os_fs_normalize_path(existingPathNorm, sizeof(existingPathNorm), existingFilePath);

    char newPathNorm[1024];
    os_fs_normalize_path(newPathNorm, sizeof(newPathNorm), newFilePath);

    FILE* stream = fopen(existingPathNorm, "rb");
    if (stream == NULL) {
        return -1;
    }

    int magic[2];
    magic[0] = fgetc(stream);
    magic[1] = fgetc(stream);
    fclose(stream);

    // Fixed inverted logic: if it IS a gzip file, decompress it.
    if (magic[0] == 0x1F && magic[1] == 0x8B) {
        gzFile gzstream = gzopen(existingPathNorm, "rb");
        if (gzstream == NULL) {
            return -1;
        }

        stream = fopen(newPathNorm, "wb");
        if (stream == NULL) {
            gzclose(gzstream);
            return -1;
        }

        while (1) {
            int ch = gzgetc(gzstream);
            if (ch == -1) {
                break;
            }

            fputc(ch, stream);
        }

        gzclose(gzstream);
        fclose(stream);
    } else {
        gz_plain_copy(existingFilePath, newFilePath);
    }

    return 0;
}

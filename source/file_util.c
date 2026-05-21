#define _XOPEN_SOURCE 500

#include "file_util.h"

#include <errno.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
    #include <direct.h>
#endif

static int File_MkdirOne(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0775);
#endif
}

int File_ReadAll(const char *path, u8 **out_data, size_t *out_size)
{
    FILE *f;
    long size;
    u8 *data;

    *out_data = NULL;
    *out_size = 0;

    f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return -1;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    data = malloc((size_t)size);
    if (data == NULL) {
        fclose(f);
        return -1;
    }

    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return -1;
    }

    fclose(f);

    *out_data = data;
    *out_size = (size_t)size;

    return 0;
}

int File_WriteAll(const char *path, const u8 *data, size_t size)
{
    FILE *f;

    if (path == NULL || data == NULL) {
        return -1;
    }

    f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int File_MkdirRecursive(const char *path)
{
    char tmp[1024];
    size_t len;
    char *p;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    len = strlen(path);

    if (len >= sizeof(tmp)) {
        return -1;
    }

    strcpy(tmp, path);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';

            if (File_MkdirOne(tmp) != 0 && errno != EEXIST) {
                perror(tmp);
                return -1;
            }

            *p = '/';
        }
    }

    if (File_MkdirOne(tmp) != 0 && errno != EEXIST) {
        perror(tmp);
        return -1;
    }

    return 0;
}

static int File_RemoveRecursiveCallback(
    const char *path,
    const struct stat *st,
    int type_flag,
    struct FTW *ftw_buffer
)
{
    (void)st;
    (void)type_flag;
    (void)ftw_buffer;
    return remove(path);
}

int File_RemoveRecursive(const char *path)
{
    struct stat st;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    if (stat(path, &st) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    (void)st;

    return nftw(path, File_RemoveRecursiveCallback, 16, FTW_DEPTH | FTW_PHYS);
}

u16 ReadU16LE(const u8 *p)
{
    return (u16)p[0] | ((u16)p[1] << 8);
}

int CopyBytes(const u8 *data, size_t size, u8 **out_data, size_t *out_size)
{
    u8 *copy;

    if (out_data == NULL || out_size == NULL) {
        return -1;
    }

    copy = malloc(size == 0 ? 1 : size);
    if (copy == NULL) {
        return -1;
    }

    if (size > 0) {
        memcpy(copy, data, size);
    }

    *out_data = copy;
    *out_size = size;

    return 0;
}

u32 ReadU32LE(const u8 *p)
{
    return (u32)p[0]
        | ((u32)p[1] << 8)
        | ((u32)p[2] << 16)
        | ((u32)p[3] << 24);
}

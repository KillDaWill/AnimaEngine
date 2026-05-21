#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include "common.h"

int File_ReadAll(const char *path, u8 **out_data, size_t *out_size);
int File_WriteAll(const char *path, const u8 *data, size_t size);
int File_MkdirRecursive(const char *path);
int File_RemoveRecursive(const char *path);

u16 ReadU16LE(const u8 *p);
u32 ReadU32LE(const u8 *p);

int CopyBytes(const u8 *data, size_t size, u8 **out_data, size_t *out_size);

#endif

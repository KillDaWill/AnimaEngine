/**
 * @file file_util.h
 * @brief File system utilities and light binary reading helper functions.
 */

#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include "common.h"

/**
 * @brief Reads the entire contents of a file into a newly allocated buffer.
 * @param path Path to the file to read.
 * @param out_data Pointer to the destination buffer pointer, allocated inside this function.
 * @param out_size Pointer to store the size of the read file in bytes.
 * @return 0 on success; negative value on read failure.
 */
int File_ReadAll(const char *path, u8 **out_data, size_t *out_size);

/**
 * @brief Writes a buffer to a file, creating or overwriting it.
 * @param path Path to the file to write.
 * @param data Pointer to the buffer containing data to write.
 * @param size Number of bytes to write.
 * @return 0 on success; negative value on write failure.
 */
int File_WriteAll(const char *path, const u8 *data, size_t size);

/**
 * @brief Creates a directory structure recursively (equivalent to mkdir -p).
 * @param path Path of directory structure to create.
 * @return 0 on success; negative value on failure.
 */
int File_MkdirRecursive(const char *path);

/**
 * @brief Deletes a file or recursively deletes a directory.
 * @param path Path of target file or directory.
 * @return 0 on success; negative value on deletion failure.
 */
int File_RemoveRecursive(const char *path);

/**
 * @brief Helper to read a little-endian unsigned 16-bit value.
 * @param p Pointer to the start bytes.
 * @return The parsed u16 value.
 */
u16 ReadU16LE(const u8 *p);

/**
 * @brief Helper to read a little-endian unsigned 32-bit value.
 * @param p Pointer to the start bytes.
 * @return The parsed u32 value.
 */
u32 ReadU32LE(const u8 *p);

/**
 * @brief Allocates and duplicates a byte array.
 * @param data Source data to copy.
 * @param size Number of bytes to copy.
 * @param out_data Pointer to the destination buffer pointer.
 * @param out_size Pointer to store the size of the duplicated buffer.
 * @return 0 on success; negative value on memory failure.
 */
int CopyBytes(const u8 *data, size_t size, u8 **out_data, size_t *out_size);

#endif


#include "nds_fnt.h"
#include "file_util.h"

typedef struct NdsFntDir {
    u32 subtable_offset;
    u16 first_file_id;
    u16 parent_or_count;
} NdsFntDir;

static int NdsFnt_GetDir(
    const u8 *fnt,
    size_t fnt_size,
    u16 dir_id,
    NdsFntDir *out_dir
)
{
    u16 dir_index;
    u16 dir_count;
    size_t entry_offset;

    if (fnt == NULL || out_dir == NULL || fnt_size < 8) {
        return -1;
    }

    dir_count = ReadU16LE(fnt + 6);
    dir_index = dir_id & 0x0FFF;

    if (dir_index >= dir_count) {
        return -1;
    }

    entry_offset = (size_t)dir_index * 8;

    if (entry_offset + 8 > fnt_size) {
        return -1;
    }

    out_dir->subtable_offset = ReadU32LE(fnt + entry_offset + 0);
    out_dir->first_file_id   = ReadU16LE(fnt + entry_offset + 4);
    out_dir->parent_or_count = ReadU16LE(fnt + entry_offset + 6);

    if (out_dir->subtable_offset >= fnt_size) {
        return -1;
    }

    return 0;
}

static int NameMatches(
    const u8 *name,
    size_t name_len,
    const char *component,
    size_t component_len
)
{
    if (name_len != component_len) {
        return 0;
    }

    return memcmp(name, component, name_len) == 0;
}

static int NdsFnt_FindInDir(
    const u8 *fnt,
    size_t fnt_size,
    u16 dir_id,
    const char *component,
    size_t component_len,
    int is_last_component,
    int *out_file_id,
    u16 *out_dir_id
)
{
    NdsFntDir dir;
    size_t pos;
    int file_index;

    if (NdsFnt_GetDir(fnt, fnt_size, dir_id, &dir) != 0) {
        return -1;
    }

    pos = dir.subtable_offset;
    file_index = 0;

    while (pos < fnt_size) {
        u8 control;
        int is_dir;
        size_t name_len;
        const u8 *name;

        control = fnt[pos++];

        if (control == 0x00) {
            break;
        }

        is_dir = (control & 0x80) != 0;
        name_len = control & 0x7F;

        if (name_len == 0 || pos + name_len > fnt_size) {
            return -1;
        }

        name = fnt + pos;

        if (is_dir) {
            u16 child_dir_id;

            if (pos + name_len + 2 > fnt_size) {
                return -1;
            }

            child_dir_id = ReadU16LE(fnt + pos + name_len);

            if (!is_last_component &&
                NameMatches(name, name_len, component, component_len)) {
                *out_dir_id = child_dir_id;
                return 1;
            }

            pos += name_len + 2;
        } else {
            int file_id;

            file_id = dir.first_file_id + file_index;

            if (is_last_component &&
                NameMatches(name, name_len, component, component_len)) {
                *out_file_id = file_id;
                return 1;
            }

            file_index++;
            pos += name_len;
        }
    }

    return 0;
}

int NdsFnt_FindFileId(
    const u8 *fnt,
    size_t fnt_size,
    const char *path,
    int *out_file_id
)
{
    const char *p;
    u16 current_dir;

    if (fnt == NULL || path == NULL || out_file_id == NULL) {
        return -1;
    }

    *out_file_id = -1;

    p = path;
    while (*p == '/') {
        p++;
    }

    if (*p == '\0') {
        return -1;
    }

    current_dir = NDS_FNT_ROOT_DIR_ID;

    while (*p != '\0') {
        const char *component_start;
        size_t component_len;
        const char *after_component;
        int is_last_component;
        int found;
        int file_id;
        u16 next_dir;

        component_start = p;

        while (*p != '\0' && *p != '/') {
            p++;
        }

        component_len = (size_t)(p - component_start);

        after_component = p;
        while (*after_component == '/') {
            after_component++;
        }

        is_last_component = (*after_component == '\0');

        file_id = -1;
        next_dir = 0;

        found = NdsFnt_FindInDir(
            fnt,
            fnt_size,
            current_dir,
            component_start,
            component_len,
            is_last_component,
            &file_id,
            &next_dir
        );

        if (found < 0) {
            return -1;
        }

        if (found == 0) {
            return -1;
        }

        if (is_last_component) {
            *out_file_id = file_id;
            return 0;
        }

        current_dir = next_dir;
        p = after_component;
    }

    return -1;
}
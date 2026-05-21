#include "nmar.h"
#include "file_util.h"
#include "nitro_util.h"

static void DumpSectionHex(const u8 *data, size_t offset, u32 size)
{
    u32 i;

    printf("  section at 0x%zX (%u bytes): ", offset, size);

    for (i = 0; i < size && i < 32; i++) {
        if (i > 0 && (i % 16) == 0) printf(" ");
        printf("%02X ", data[offset + i]);
    }

    if (size > 32) printf("...");
    printf("\n");
}

static int ReadLabel(
    const u8 *data,
    size_t section_end,
    size_t string_offset,
    char out_label[32]
)
{
    size_t i;

    if (string_offset >= section_end) {
        out_label[0] = '\0';
        return -1;
    }

    for (i = 0; i + 1 < 32 && string_offset + i < section_end; i++) {
        char c;

        c = (char)data[string_offset + i];
        out_label[i] = c;

        if (c == '\0') {
            return 0;
        }
    }

    out_label[i] = '\0';
    return 0;
}

static void ApplyLabels(
    const u8 *data,
    size_t size,
    NmarFile *out_nmar
)
{
    size_t lbal_offset;
    u32 lbal_size;
    size_t data_start;
    size_t string_base;
    size_t section_end;
    int i;

    if (out_nmar == NULL || out_nmar->entry_count <= 0 ||
        out_nmar->entries == NULL) {
        return;
    }

    if (Nitro_FindSection(data, size, "LBAL", &lbal_offset, &lbal_size) != 0) {
        return;
    }

    data_start = lbal_offset + 8;
    section_end = lbal_offset + lbal_size;
    string_base = data_start + ((size_t)out_nmar->entry_count * 4);

    if (lbal_size < 8 ||
        string_base > section_end ||
        section_end > size) {
        return;
    }

    for (i = 0; i < out_nmar->entry_count; i++) {
        u32 label_offset;

        label_offset = ReadU32LE(data + data_start + ((size_t)i * 4));
        ReadLabel(
            data,
            section_end,
            string_base + label_offset,
            out_nmar->entries[i].label
        );
        if (out_nmar->animations != NULL && i < out_nmar->animation_count) {
            strncpy(out_nmar->animations[i].label, out_nmar->entries[i].label,
                    sizeof(out_nmar->animations[i].label) - 1);
            out_nmar->animations[i].label[sizeof(out_nmar->animations[i].label) - 1] = '\0';
        }
    }
}

static void NmarFrame_SetIdentityTransform(NmarFrame *frame)
{
    frame->transform_type = 0;
    frame->rotation = 0;
    frame->scale_x = 4096;
    frame->scale_y = 4096;
    frame->translate_x = 0;
    frame->translate_y = 0;
}

static void Nmar_ReadFrameElement(
    const u8 *element,
    int frame_type,
    NmarFrame *frame
)
{
    frame->map_index = (int)ReadU16LE(element + 0);
    NmarFrame_SetIdentityTransform(frame);

    if (frame_type == 1) {
        frame->transform_type = 1;
        frame->rotation = Nitro_ReadS16LE(element + 2);
        frame->scale_x = Nitro_ReadS32LE(element + 4);
        frame->scale_y = Nitro_ReadS32LE(element + 8);
        frame->translate_x = Nitro_ReadS16LE(element + 12);
        frame->translate_y = Nitro_ReadS16LE(element + 14);
    } else if (frame_type == 2) {
        frame->transform_type = 2;
        frame->translate_x = Nitro_ReadS16LE(element + 4);
        frame->translate_y = Nitro_ReadS16LE(element + 6);
    } else if (frame_type == 3) {
        frame->transform_type = 3;
    }
}

static u32 Nmar_FrameElementSizeForType(int frame_type)
{
    if (frame_type == 0) return 2;
    if (frame_type == 1) return 16;
    if (frame_type == 2) return 8;
    if (frame_type == 3) return 4;
    return 0;
}

static int TryParseKnbaTiming(
    const u8 *data,
    size_t size,
    size_t section_offset,
    u32 section_size,
    NmarFile *out_nmar
)
{
    int animation_count;
    u32 animation_table_offset;
    u32 frame_data_offset;
    u32 frame_element_offset;
    size_t animation_table;
    size_t frame_data_base;
    size_t frame_element_base;
    int i;

    if (section_size < 0x18) {
        return -1;
    }

    animation_count = ReadU16LE(data + section_offset + 0x08);
    animation_table_offset = ReadU32LE(data + section_offset + 0x0C);
    frame_data_offset = ReadU32LE(data + section_offset + 0x10);
    frame_element_offset = ReadU32LE(data + section_offset + 0x14);

    if (animation_count <= 0 || animation_count > 2048) {
        return -1;
    }

    animation_table = section_offset + 8 + animation_table_offset;
    frame_data_base = section_offset + 8 + frame_data_offset;
    frame_element_base = section_offset + 8 + frame_element_offset;

    if (animation_table + ((size_t)animation_count * 16) > section_offset + section_size ||
        frame_data_base > section_offset + section_size ||
        frame_element_base > section_offset + section_size) {
        return -1;
    }

    out_nmar->entries = calloc((size_t)animation_count, sizeof(NmarEntry));
    out_nmar->animations = calloc((size_t)animation_count, sizeof(NmarAnimation));
    if (out_nmar->entries == NULL || out_nmar->animations == NULL) {
        free(out_nmar->entries);
        free(out_nmar->animations);
        out_nmar->entries = NULL;
        out_nmar->animations = NULL;
        return -1;
    }

    out_nmar->entry_count = animation_count;
    out_nmar->animation_count = animation_count;

    for (i = 0; i < animation_count; i++) {
        size_t entry_offset;
        size_t frames_offset;
        int frame_count;
        int frame_type;
        u32 raw_frame_offset;
        int j;

        entry_offset = animation_table + ((size_t)i * 16);
        frame_count = (int)ReadU16LE(data + entry_offset + 0);
        frame_type = (int)ReadU16LE(data + entry_offset + 4);
        raw_frame_offset = ReadU32LE(data + entry_offset + 12);

        if (frame_count <= 0 || frame_count > 4096) {
            Nmar_Free(out_nmar);
            return -1;
        }

        frames_offset = frame_data_base + raw_frame_offset;
        if (frames_offset + ((size_t)frame_count * 8) > section_offset + section_size) {
            Nmar_Free(out_nmar);
            return -1;
        }

        out_nmar->animations[i].frame_count = frame_count;
        out_nmar->animations[i].loop_start = ReadU16LE(data + entry_offset + 2);
        out_nmar->animations[i].format = frame_type;
        out_nmar->animations[i].playback_type = (int)ReadU32LE(data + entry_offset + 8);
        out_nmar->animations[i].raw_frame_offset = raw_frame_offset;
        if (out_nmar->animations[i].loop_start < 0 ||
            out_nmar->animations[i].loop_start >= frame_count) {
            out_nmar->animations[i].loop_start = 0;
        }

        out_nmar->animations[i].frames = calloc((size_t)frame_count, sizeof(NmarFrame));
        if (out_nmar->animations[i].frames == NULL) {
            Nmar_Free(out_nmar);
            return -1;
        }

        for (j = 0; j < frame_count; j++) {
            size_t frame_offset;
            size_t element_offset;
            u32 raw_element_offset;
            u32 required_size;

            frame_offset = frames_offset + ((size_t)j * 8);
            raw_element_offset = ReadU32LE(data + frame_offset + 0);
            element_offset = frame_element_base + raw_element_offset;
            required_size = Nmar_FrameElementSizeForType(frame_type);
            if (required_size == 0) required_size = 2;

            if (element_offset + required_size > section_offset + section_size) {
                Nmar_Free(out_nmar);
                return -1;
            }

            out_nmar->animations[i].frames[j].raw_element_offset = raw_element_offset;
            Nmar_ReadFrameElement(
                data + element_offset,
                frame_type,
                &out_nmar->animations[i].frames[j]
            );
            out_nmar->animations[i].frames[j].duration = ReadU16LE(data + frame_offset + 4);
            out_nmar->animations[i].frames[j].marker = ReadU16LE(data + frame_offset + 6);
        }

        out_nmar->entries[i].map_index = out_nmar->animations[i].frames[0].map_index;
        out_nmar->entries[i].flags = (u16)ReadU32LE(data + entry_offset + 8);
        out_nmar->entries[i].label[0] = '\0';
    }

    ApplyLabels(data, size, out_nmar);
    return 0;
}

static int TryParseSectionData(
    const u8 *data,
    size_t section_offset,
    u32 section_size,
    NmarFile *out_nmar,
    int use_hdr_entry_count
)
{
    size_t data_start;
    size_t data_len;
    u16 hdr_entry_count;
    u32 hdr_table_offset;
    int entry_count;
    int i;

    data_start = section_offset + 8;
    data_len   = section_size - 8;

    if (data_len < 4) return -1;

    hdr_entry_count  = ReadU16LE(data + data_start + 0x00);
    hdr_table_offset = ReadU32LE(data + data_start + 0x04);

    if (use_hdr_entry_count && hdr_entry_count > 0 && hdr_entry_count <= 2048 &&
        hdr_table_offset < data_len &&
        data_start + hdr_table_offset + ((size_t)hdr_entry_count * 4) <= section_offset + section_size) {

        entry_count = (int)hdr_entry_count;

        out_nmar->entries = calloc((size_t)entry_count, sizeof(NmarEntry));
        if (out_nmar->entries == NULL) return -1;

        out_nmar->entry_count = entry_count;

        for (i = 0; i < entry_count; i++) {
            size_t ent = data_start + hdr_table_offset + ((size_t)i * 4);
            out_nmar->entries[i].map_index = (int)ReadU16LE(data + ent + 0);
            out_nmar->entries[i].flags     = ReadU16LE(data + ent + 2);
        }

        return 0;
    }

    if (use_hdr_entry_count && hdr_entry_count > 0 && hdr_entry_count <= 2048 &&
        hdr_table_offset < data_len &&
        data_start + hdr_table_offset + ((size_t)hdr_entry_count * 8) <= section_offset + section_size) {

        entry_count = (int)hdr_entry_count;

        out_nmar->entries = calloc((size_t)entry_count, sizeof(NmarEntry));
        if (out_nmar->entries == NULL) return -1;

        out_nmar->entry_count = entry_count;

        for (i = 0; i < entry_count; i++) {
            size_t ent = data_start + hdr_table_offset + ((size_t)i * 8);
            out_nmar->entries[i].map_index = (int)ReadU16LE(data + ent + 0);
            out_nmar->entries[i].flags     = ReadU16LE(data + ent + 2);
        }

        return 0;
    }

    if (data_len >= 4 && (data_len % 4) == 0) {
        entry_count = (int)(data_len / 4);

        if (entry_count > 0 && entry_count <= 2048) {
            out_nmar->entries = calloc((size_t)entry_count, sizeof(NmarEntry));
            if (out_nmar->entries == NULL) return -1;

            out_nmar->entry_count = entry_count;

            for (i = 0; i < entry_count; i++) {
                size_t ent = data_start + ((size_t)i * 4);
                out_nmar->entries[i].map_index = (int)ReadU16LE(data + ent + 0);
                out_nmar->entries[i].flags     = ReadU16LE(data + ent + 2);
            }

            return 0;
        }
    }

    return -1;
}

int Nmar_Parse(
    const u8 *data,
    size_t size,
    NmarFile *out_nmar
)
{
    size_t section_offset;
    u32 section_size;

    if (data == NULL || out_nmar == NULL) {
        return -1;
    }

    memset(out_nmar, 0, sizeof(*out_nmar));

    if (size < 0x20 || !Nitro_HasMagic(data, size, "RAMN")) {
        return -1;
    }

    if (Nitro_FindSection(data, size, "KNMA", &section_offset, &section_size) == 0) {
        DumpSectionHex(data, section_offset, section_size);
        return TryParseSectionData(data, section_offset, section_size, out_nmar, 1);
    }

    if (Nitro_FindSection(data, size, "KNAM", &section_offset, &section_size) == 0) {
        DumpSectionHex(data, section_offset, section_size);
        return TryParseSectionData(data, section_offset, section_size, out_nmar, 1);
    }

    if (Nitro_FindSection(data, size, "KNBA", &section_offset, &section_size) == 0) {
        return TryParseKnbaTiming(data, size, section_offset, section_size, out_nmar);
    }

    {
        size_t pos;
        int section_index;
        int parsed;

        pos = 0x10;
        section_index = 0;
        parsed = 0;

        while (pos + 8 <= size) {
            u32 sec_size;

            sec_size = ReadU32LE(data + pos + 4);
            if (sec_size < 8 || pos + sec_size > size) break;

            if (sec_size >= 0x10) {
                char magic[5];

                magic[0] = (char)data[pos + 0];
                magic[1] = (char)data[pos + 1];
                magic[2] = (char)data[pos + 2];
                magic[3] = (char)data[pos + 3];
                magic[4] = '\0';

                printf("NMAR section %d: magic='%s' offset=0x%zX size=%u\n",
                       section_index, magic, pos, sec_size);
                DumpSectionHex(data, pos, sec_size);

                if (TryParseSectionData(data, pos, sec_size, out_nmar, 1) == 0) {
                    printf("NMAR: parsed via section %d magic '%s'\n",
                           section_index, magic);
                    parsed = 1;
                    break;
                }

                if (TryParseSectionData(data, pos, sec_size, out_nmar, 0) == 0) {
                    printf("NMAR: parsed via section %d magic '%s' (raw entries)\n",
                           section_index, magic);
                    parsed = 1;
                    break;
                }
            }

            pos += sec_size;
            section_index++;
        }

        if (parsed) return 0;
    }

    return -1;
}

int Nmar_GetIdleMapIndex(const NmarFile *nmar)
{
    int i;

    if (nmar == NULL || nmar->entry_count <= 0 || nmar->entries == NULL) {
        return -1;
    }

    for (i = 0; i < nmar->entry_count; i++) {
        if (strcmp(nmar->entries[i].label, "stay") == 0) {
            return nmar->entries[i].map_index;
        }
    }

    return nmar->entries[0].map_index;
}

int Nmar_GetIdleBreakMapIndex(
    const NmarFile *nmar,
    int idle_map,
    int map_count
)
{
    const char *labels[] = { "wait", "break", "special" };
    int num_labels = (int)(sizeof(labels) / sizeof(labels[0]));
    int i;
    int j;

    if (nmar == NULL || nmar->entry_count <= 0 || nmar->entries == NULL) {
        return -1;
    }

    (void)idle_map;
    (void)map_count;

    for (j = 0; j < num_labels; j++) {
        for (i = 0; i < nmar->entry_count; i++) {
            if (strcmp(nmar->entries[i].label, labels[j]) == 0) {
                return nmar->entries[i].map_index;
            }
        }
    }

    /* "stop" label: return the map it points to, but only if it differs
     * from idle.  Callers should validate this with scoring when NMCR
     * data is available. */
    for (i = 0; i < nmar->entry_count; i++) {
        if (strcmp(nmar->entries[i].label, "stop") == 0) {
            return nmar->entries[i].map_index;
        }
    }

    /* No explicit label found: NMAR alone cannot pick a good break.
     * Return -1 so callers can fall back to NMCR-based scoring. */
    return -1;
}

static int Nmar_FrameDuration(const NmarFrame *frame)
{
    if (frame == NULL || frame->duration <= 0) return 0;
    return frame->duration;
}

static int Nmar_FrameDurationRange(
    const NmarAnimation *animation,
    int start_frame,
    int end_frame,
    int step
)
{
    int total = 0;
    int i;

    if (animation == NULL || animation->frames == NULL || step == 0) return 0;

    for (i = start_frame;
         step > 0 ? i <= end_frame : i >= end_frame;
         i += step) {
        if (i >= 0 && i < animation->frame_count) {
            total += Nmar_FrameDuration(&animation->frames[i]);
        }
    }

    return total;
}

static int Nmar_SelectFrameInRange(
    const NmarAnimation *animation,
    int start_frame,
    int end_frame,
    int step,
    int tick
)
{
    int i;
    int last_valid = start_frame;

    if (animation == NULL || animation->frames == NULL || step == 0) return 0;

    for (i = start_frame;
         step > 0 ? i <= end_frame : i >= end_frame;
         i += step) {
        int dur;

        if (i < 0 || i >= animation->frame_count) continue;
        last_valid = i;
        dur = Nmar_FrameDuration(&animation->frames[i]);
        if (dur <= 0) continue;
        if (tick < dur) return i;
        tick -= dur;
    }

    return last_valid;
}

static int Nmar_SelectFrameForward(
    const NmarAnimation *animation,
    int tick,
    int loop
)
{
    int total_duration;
    int loop_start_tick;
    int loop_duration;

    total_duration = Nmar_FrameDurationRange(animation, 0, animation->frame_count - 1, 1);
    if (total_duration <= 0) return 0;

    if (!loop) {
        if (tick >= total_duration) tick = total_duration - 1;
        return Nmar_SelectFrameInRange(animation, 0, animation->frame_count - 1, 1, tick);
    }

    loop_start_tick = Nmar_FrameDurationRange(animation, 0, animation->loop_start - 1, 1);
    loop_duration = Nmar_FrameDurationRange(animation, animation->loop_start, animation->frame_count - 1, 1);
    if (tick >= total_duration && loop_duration > 0) {
        tick = loop_start_tick + (tick - loop_start_tick) % loop_duration;
    } else if (tick >= total_duration) {
        tick = tick % total_duration;
    }

    return Nmar_SelectFrameInRange(animation, 0, animation->frame_count - 1, 1, tick);
}

int Nmar_GetFrameAtTick(
    const NmarFile *nmar,
    int animation_index,
    int tick,
    NmarFrame *out_frame
)
{
    const NmarAnimation *animation;
    int frame_index;

    if (nmar == NULL || out_frame == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count ||
        nmar->animations == NULL) {
        return -1;
    }

    animation = &nmar->animations[animation_index];
    if (animation->frame_count <= 0 || animation->frames == NULL) return -1;
    if (tick < 0) tick = 0;

    if (animation->playback_type == 2) {
        frame_index = Nmar_SelectFrameForward(animation, tick, 1);
    } else {
        frame_index = Nmar_SelectFrameForward(animation, tick, 0);
    }

    if (frame_index < 0 || frame_index >= animation->frame_count) frame_index = 0;
    *out_frame = animation->frames[frame_index];
    return out_frame->map_index;
}

int Nmar_GetTotalDuration(
    const NmarFile *nmar,
    int animation_index
)
{
    if (nmar == NULL || nmar->animations == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count) {
        return 0;
    }

    return Nmar_FrameDurationRange(
        &nmar->animations[animation_index],
        0,
        nmar->animations[animation_index].frame_count - 1,
        1
    );
}

void Nmar_Free(NmarFile *nmar)
{
    int i;

    if (nmar == NULL) return;

    free(nmar->entries);
    if (nmar->animations != NULL) {
        for (i = 0; i < nmar->animation_count; i++) {
            free(nmar->animations[i].frames);
        }
        free(nmar->animations);
    }
    nmar->entries = NULL;
    nmar->animations = NULL;
    nmar->entry_count = 0;
    nmar->animation_count = 0;
}

void Nmar_PrintInfo(const NmarFile *nmar)
{
    int i;

    if (nmar == NULL) return;

    printf("NMAR file:\n");
    printf("  entry count: %d\n", nmar->entry_count);
    printf("  animation count: %d\n", nmar->animation_count);

    for (i = 0; i < nmar->entry_count && i < 16; i++) {
        printf("  [%02d] map_index=%d flags=0x%04X label='%s'\n",
               i,
               nmar->entries[i].map_index,
               nmar->entries[i].flags,
               nmar->entries[i].label);
    }

    if (nmar->entry_count > 16) {
        printf("  ...\n");
    }

    for (i = 0; i < nmar->animation_count && i < 4; i++) {
        printf("  anim[%02d] frames=%d loop=%d playback=%d format=%d label='%s'\n",
               i,
               nmar->animations[i].frame_count,
               nmar->animations[i].loop_start,
               nmar->animations[i].playback_type,
               nmar->animations[i].format,
               nmar->animations[i].label);
    }

    printf("\n");
}

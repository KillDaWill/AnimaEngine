#include "nanr.h"
#include "file_util.h"
#include "nitro_util.h"

static void NanrFrame_SetIdentityTransform(NanrFrame *frame)
{
    frame->transform_type = 0;
    frame->rotation = 0;
    frame->scale_x = NANR_SCALE_ONE;
    frame->scale_y = NANR_SCALE_ONE;
    frame->translate_x = 0;
    frame->translate_y = 0;
}

static void Nanr_ReadFrameElement(
    const u8 *element,
    int frame_type,
    NanrFrame *frame
)
{
    frame->cell_id = ReadU16LE(element + 0);
    NanrFrame_SetIdentityTransform(frame);

    if (frame_type == 0) {
        frame->transform_type = 0;
    } else if (frame_type == 1) {
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
    } else {
        frame->transform_type = 0;
    }
}

static u32 Nanr_FrameElementSizeForType(int frame_type)
{
    if (frame_type == 0) return 2;
    if (frame_type == 1) return 16;
    if (frame_type == 2) return 8;
    if (frame_type == 3) return 4;
    return 0;
}

static int CompareU32(const void *a, const void *b)
{
    u32 val_a = *(const u32 *)a;
    u32 val_b = *(const u32 *)b;
    return (val_a > val_b) - (val_a < val_b);
}

static int Nanr_FrameDuration(const NanrFrame *frame)
{
    if (frame == NULL || frame->duration <= 0) {
        return 0;
    }

    return frame->duration;
}

static int Nanr_FrameDurationRange(
    const NanrAnimation *animation,
    int start_frame,
    int end_frame,
    int step
)
{
    int total;
    int i;

    total = 0;

    if (animation == NULL || animation->frames == NULL || step == 0) {
        return 0;
    }

    for (i = start_frame;
         step > 0 ? i <= end_frame : i >= end_frame;
         i += step) {
        if (i >= 0 && i < animation->frame_count) {
            total += Nanr_FrameDuration(&animation->frames[i]);
        }
    }

    return total;
}

static int Nanr_SelectFrameInRange(
    const NanrAnimation *animation,
    int start_frame,
    int end_frame,
    int step,
    int tick
)
{
    int i;
    int last_valid;

    last_valid = start_frame;

    if (animation == NULL || animation->frames == NULL || step == 0) {
        return 0;
    }

    for (i = start_frame;
         step > 0 ? i <= end_frame : i >= end_frame;
         i += step) {
        int dur;

        if (i < 0 || i >= animation->frame_count) {
            continue;
        }

        last_valid = i;
        dur = Nanr_FrameDuration(&animation->frames[i]);
        if (dur <= 0) {
            continue;
        }

        if (tick < dur) {
            return i;
        }

        tick -= dur;
    }

    return last_valid;
}

static int Nanr_SelectFrameForward(
    const NanrAnimation *animation,
    int tick,
    int loop
)
{
    int total_duration;
    int loop_start_tick;
    int loop_duration;

    total_duration = Nanr_FrameDurationRange(animation, 0, animation->frame_count - 1, 1);
    if (total_duration <= 0) {
        return 0;
    }

    if (!loop) {
        if (tick >= total_duration) {
            tick = total_duration - 1;
        }
        return Nanr_SelectFrameInRange(animation, 0, animation->frame_count - 1, 1, tick);
    }

    loop_start_tick = Nanr_FrameDurationRange(animation, 0, animation->loop_start - 1, 1);
    loop_duration = Nanr_FrameDurationRange(animation, animation->loop_start, animation->frame_count - 1, 1);

    if (tick >= total_duration && loop_duration > 0) {
        tick = loop_start_tick + (tick - loop_start_tick) % loop_duration;
    } else if (tick >= total_duration) {
        tick = tick % total_duration;
    }

    return Nanr_SelectFrameInRange(animation, 0, animation->frame_count - 1, 1, tick);
}

static int Nanr_SelectFramePingPong(
    const NanrAnimation *animation,
    int tick,
    int loop
)
{
    int forward_duration;
    int reverse_start;
    int reverse_end;
    int reverse_duration;
    int sequence_duration;

    forward_duration = Nanr_FrameDurationRange(animation, 0, animation->frame_count - 1, 1);
    if (forward_duration <= 0) {
        return 0;
    }

    reverse_start = animation->frame_count - 2;
    reverse_end = loop ? animation->loop_start : 0;
    reverse_duration = Nanr_FrameDurationRange(animation, reverse_start, reverse_end, -1);
    sequence_duration = forward_duration + reverse_duration;

    if (sequence_duration <= 0) {
        return Nanr_SelectFrameForward(animation, tick, 0);
    }

    if (!loop) {
        if (tick >= sequence_duration) {
            tick = sequence_duration - 1;
        }
    } else {
        int loop_start_tick;
        int loop_forward_duration;
        int loop_duration;

        loop_start_tick = Nanr_FrameDurationRange(animation, 0, animation->loop_start - 1, 1);
        loop_forward_duration = Nanr_FrameDurationRange(animation, animation->loop_start, animation->frame_count - 1, 1);
        loop_duration = loop_forward_duration + reverse_duration;

        if (tick >= sequence_duration && loop_duration > 0) {
            tick = loop_start_tick + (tick - loop_start_tick) % loop_duration;
        } else if (tick >= sequence_duration) {
            tick = tick % sequence_duration;
        }
    }

    if (tick < forward_duration) {
        return Nanr_SelectFrameInRange(animation, 0, animation->frame_count - 1, 1, tick);
    }

    return Nanr_SelectFrameInRange(
        animation,
        reverse_start,
        reverse_end,
        -1,
        tick - forward_duration
    );
}

int Nanr_Parse(const u8 *data, size_t size, NanrFile *out_nanr)
{
    size_t knba_offset;
    u32 knba_size;
    int animation_count;
    u32 animation_table_offset;
    u32 frame_data_offset;
    u32 frame_element_offset;
    size_t animation_table;
    size_t frame_data_base;
    size_t frame_element_base;
    int i;
    int total_frames = 0;
    u32 *offsets = NULL;
    int offset_count = 0;
    int unique_count = 0;
    u32 end_offset;

    if (data == NULL || out_nanr == NULL) {
        return -1;
    }

    memset(out_nanr, 0, sizeof(*out_nanr));

    if (size < 0x20 || !Nitro_HasMagic(data, size, "RNAN")) {
        return -1;
    }

    if (Nitro_FindSection(data, size, "KNBA", &knba_offset, &knba_size) != 0) {
        return -1;
    }

    if (knba_size < 0x18) {
        return -1;
    }

    animation_count = ReadU16LE(data + knba_offset + 0x08);
    animation_table_offset = ReadU32LE(data + knba_offset + 0x0C);
    frame_data_offset = ReadU32LE(data + knba_offset + 0x10);
    frame_element_offset = ReadU32LE(data + knba_offset + 0x14);

    if (animation_count <= 0 || animation_count > 2048) {
        return -1;
    }

    animation_table = knba_offset + 8 + animation_table_offset;
    frame_data_base = knba_offset + 8 + frame_data_offset;
    frame_element_base = knba_offset + 8 + frame_element_offset;

    if (animation_table + ((size_t)animation_count * 16) > knba_offset + knba_size ||
        frame_data_base > knba_offset + knba_size ||
        frame_element_base > knba_offset + knba_size) {
        return -1;
    }

    /* Pass 1: Validate animation frame counts and sum total_frames */
    for (i = 0; i < animation_count; i++) {
        size_t entry_offset = animation_table + ((size_t)i * 16);
        int frame_count = (int)ReadU16LE(data + entry_offset + 0);
        if (frame_count < 0 || frame_count > 4096) {
            return -1;
        }
        total_frames += frame_count;
    }

    /* Allocate offsets array (adding 1 for the sentinel end_offset) */
    offsets = malloc(((size_t)total_frames + 1) * sizeof(u32));
    if (offsets == NULL) {
        return -1;
    }

    /* Collect all raw_cell_value offsets */
    for (i = 0; i < animation_count; i++) {
        size_t entry_offset = animation_table + ((size_t)i * 16);
        int frame_count = (int)ReadU16LE(data + entry_offset + 0);
        u32 raw_frame_offset = ReadU32LE(data + entry_offset + 12);
        size_t frames_offset = frame_data_base + raw_frame_offset;
        int j;

        for (j = 0; j < frame_count; j++) {
            size_t frame_offset = frames_offset + ((size_t)j * 8);
            if (frame_offset + 8 > knba_offset + knba_size) {
                free(offsets);
                return -1;
            }
            offsets[offset_count++] = ReadU32LE(data + frame_offset + 0);
        }
    }

    /* Sort offsets and remove duplicates */
    if (offset_count > 0) {
        qsort(offsets, (size_t)offset_count, sizeof(u32), CompareU32);
        for (i = 0; i < offset_count; i++) {
            if (unique_count == 0 || offsets[i] != offsets[unique_count - 1]) {
                offsets[unique_count++] = offsets[i];
            }
        }
    }

    /* Append sentinel end offset representing the end of element segment */
    end_offset = (u32)(knba_size - 8 - frame_element_offset);
    if (unique_count == 0 || offsets[unique_count - 1] < end_offset) {
        offsets[unique_count++] = end_offset;
    }

    out_nanr->animations = calloc((size_t)animation_count, sizeof(NanrAnimation));
    if (out_nanr->animations == NULL) {
        free(offsets);
        return -1;
    }

    out_nanr->animation_count = animation_count;

    for (i = 0; i < animation_count; i++) {
        size_t entry_offset;
        size_t frames_offset;
        int j;

        entry_offset = animation_table + ((size_t)i * 16);

        out_nanr->animations[i].frame_count = (int)ReadU16LE(data + entry_offset + 0);
        out_nanr->animations[i].loop_start = ReadU16LE(data + entry_offset + 2);
        out_nanr->animations[i].format = ReadU16LE(data + entry_offset + 4);
        out_nanr->animations[i].playback_type = (int)ReadU32LE(data + entry_offset + 8);
        out_nanr->animations[i].raw_frame_offset = ReadU32LE(data + entry_offset + 12);

        if (out_nanr->animations[i].loop_start < 0 ||
            out_nanr->animations[i].loop_start >= out_nanr->animations[i].frame_count) {
            out_nanr->animations[i].loop_start = 0;
        }

        frames_offset = frame_data_base + out_nanr->animations[i].raw_frame_offset;

        out_nanr->animations[i].frames = calloc(
            (size_t)out_nanr->animations[i].frame_count,
            sizeof(NanrFrame)
        );
        if (out_nanr->animations[i].frames == NULL) {
            free(offsets);
            Nanr_Free(out_nanr);
            return -1;
        }

        for (j = 0; j < out_nanr->animations[i].frame_count; j++) {
            size_t frame_offset;
            size_t element_offset;
            size_t element_available;
            u32 raw_cell_value;
            u32 element_size;
            u32 size_limit;

            frame_offset = frames_offset + ((size_t)j * 8);
            raw_cell_value = ReadU32LE(data + frame_offset + 0);
            element_offset = frame_element_base + raw_cell_value;

            if (element_offset + 2 > knba_offset + knba_size) {
                free(offsets);
                Nanr_Free(out_nanr);
                return -1;
            }

            out_nanr->animations[i].frames[j].raw_cell_value = raw_cell_value;

            size_limit = (u32)(knba_size - 8 - frame_element_offset - raw_cell_value);
            element_size = size_limit;

            /* Binary search to find raw_cell_value in unique sorted offsets */
            int low = 0;
            int high = unique_count - 1;
            int found_idx = -1;
            while (low <= high) {
                int mid = low + (high - low) / 2;
                if (offsets[mid] == raw_cell_value) {
                    found_idx = mid;
                    break;
                } else if (offsets[mid] < raw_cell_value) {
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }
            if (found_idx != -1 && found_idx + 1 < unique_count) {
                u32 next_offset = offsets[found_idx + 1];
                if (next_offset > raw_cell_value) {
                    element_size = next_offset - raw_cell_value;
                }
            }

            element_available = element_size;
            if (element_available > size_limit) {
                element_available = size_limit;
            }

            int resolved_format = out_nanr->animations[i].format;
            u32 required_size = Nanr_FrameElementSizeForType(resolved_format);

            if (element_size == 4 && element_available >= 4 &&
                ReadU16LE(data + element_offset + 2) == 0xCCCC) {
                resolved_format = 3;
            } else if (required_size == 0 || element_available < required_size) {
                if (element_size == 2 || element_size == 4) {
                    resolved_format = 0;
                } else if (element_size == 8) {
                    resolved_format = 2;
                } else if (element_size >= 16) {
                    resolved_format = 1;
                } else {
                    resolved_format = 0;
                }
            }

            Nanr_ReadFrameElement(
                data + element_offset,
                resolved_format,
                &out_nanr->animations[i].frames[j]
            );
            out_nanr->animations[i].frames[j].duration = ReadU16LE(data + frame_offset + 4);
            out_nanr->animations[i].frames[j].marker = ReadU16LE(data + frame_offset + 6);
        }
    }

    free(offsets);
    return 0;
}

void Nanr_Free(NanrFile *nanr)
{
    int i;

    if (nanr == NULL) {
        return;
    }

    if (nanr->animations != NULL) {
        for (i = 0; i < nanr->animation_count; i++) {
            free(nanr->animations[i].frames);
        }

        free(nanr->animations);
    }

    nanr->animation_count = 0;
    nanr->animations = NULL;
}

int Nanr_GetResolvedCellId(
    const NanrFile *nanr,
    int animation_index,
    int frame_index,
    NanrFrame *out_frame
)
{
    const NanrAnimation *animation;
    int wrapped_frame;

    if (nanr == NULL || out_frame == NULL ||
        animation_index < 0 || animation_index >= nanr->animation_count) {
        return -1;
    }

    animation = &nanr->animations[animation_index];

    if (animation->frame_count <= 0 || animation->frames == NULL) {
        return -1;
    }

    wrapped_frame = frame_index % animation->frame_count;
    *out_frame = animation->frames[wrapped_frame];

    return out_frame->cell_id;
}

int Nanr_GetResolvedCellIdAtTick(
    const NanrFile *nanr,
    int animation_index,
    int tick,
    NanrFrame *out_frame
)
{
    const NanrAnimation *animation;
    int frame_index;

    if (nanr == NULL || out_frame == NULL ||
        animation_index < 0 || animation_index >= nanr->animation_count) {
        return -1;
    }

    animation = &nanr->animations[animation_index];

    if (animation->frame_count <= 0 || animation->frames == NULL) {
        return -1;
    }

    if (tick < 0) {
        tick = 0;
    }

    if (animation->playback_type == 2) {
        frame_index = Nanr_SelectFrameForward(animation, tick, 1);
    } else if (animation->playback_type == 3) {
        frame_index = Nanr_SelectFramePingPong(animation, tick, 0);
    } else if (animation->playback_type == 4) {
        frame_index = Nanr_SelectFramePingPong(animation, tick, 1);
    } else {
        frame_index = Nanr_SelectFrameForward(animation, tick, 0);
    }

    if (frame_index < 0 || frame_index >= animation->frame_count) {
        frame_index = 0;
    }

    *out_frame = animation->frames[frame_index];
    return out_frame->cell_id;
}

void Nanr_PrintInfo(const NanrFile *nanr)
{
    int i;

    if (nanr == NULL) return;

    printf("NANR file:\n");
    printf("  animation count: %d\n", nanr->animation_count);

    for (i = 0; i < nanr->animation_count && i < 8; i++) {
        printf("  [%02d] frames=%d loop=%d playback=%d\n",
               i,
               nanr->animations[i].frame_count,
               nanr->animations[i].loop_start,
               nanr->animations[i].playback_type);
    }

    if (nanr->animation_count > 8) {
        printf("  ...\n");
    }

    printf("\n");
}

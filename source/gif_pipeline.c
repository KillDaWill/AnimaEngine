#include "gif_pipeline.h"
#include "file_util.h"
#include "gif_writer.h"
#include "sprite_composer.h"

#define GIF_DIR_IDLE "animated_idle_gif"
#define GIF_DIR_BREAK "idle_break_gif"
#define GIF_DIR_OTHER "animation_gif"

void GifExportOptions_Init(GifExportOptions *options)
{
    memset(options, 0, sizeof(*options));
    options->enabled = 1;
    options->side = GIF_SIDE_FRONT;
    options->palette = GIF_PALETTE_NORMAL;
    options->eye_mode = GIF_EYE_OPEN;
    options->scale = 4;
    options->delay_cs = 5;
    options->playback_delay_cs = 0;
    options->loop_count = 0;
    options->start_frame = 0;
    options->frame_count = -1;
    options->map_is_idle = 1;
    options->map_index = 0;
    options->nmar_animation_index = -1;
}

static const char *GifDirectoryForAnimation(const char *animation_name)
{
    if (animation_name != NULL && strcmp(animation_name, "idle") == 0) {
        return GIF_DIR_IDLE;
    }
    if (animation_name != NULL && strcmp(animation_name, "break") == 0) {
        return GIF_DIR_BREAK;
    }
    return GIF_DIR_OTHER;
}

static int ResolveGifPlaybackDelayCs(const GifExportOptions *options)
{
    if (options == NULL) return 5;
    if (options->playback_delay_cs > 0) return options->playback_delay_cs;
    return options->delay_cs;
}

static int ResolveGifMapIndex(
    const GifExportOptions *options,
    int default_idle_map,
    int map_count
)
{
    int map_index;

    map_index = options->map_is_idle ? default_idle_map : options->map_index;

    if (map_index < 0 || map_index >= map_count) {
        map_index = 0;
    }

    return map_index;
}

static int ResolveNmarTimelineIndex(
    const GifExportOptions *options,
    const NmarFile *nmar,
    const char *animation_name
)
{
    if (options == NULL || nmar == NULL || nmar->animation_count <= 0 ||
        nmar->animations == NULL) {
        return -1;
    }

    if (options->nmar_animation_index >= 0 &&
        options->nmar_animation_index < nmar->animation_count) {
        return options->nmar_animation_index;
    }

    if (animation_name == NULL || strcmp(animation_name, "idle") != 0) {
        return -1;
    }

    if (!options->map_is_idle) {
        return -1;
    }

    return 0;
}

static int NmarFrameCountForGif(
    const NmarFile *nmar,
    int animation_index,
    int delay_cs
)
{
    int total_ticks;
    int frame_count;

    total_ticks = Nmar_GetTotalDuration(nmar, animation_index);
    if (total_ticks <= 0 || delay_cs <= 0) {
        return 0;
    }

    frame_count = (total_ticks * 100 + (delay_cs * 60) - 1) / (delay_cs * 60);
    if (frame_count <= 0) frame_count = 1;
    return frame_count;
}

static void ComposerTransform_FromNmarFrame(
    const NmarFrame *frame,
    ComposerTransform *out_transform
)
{
    if (out_transform == NULL) return;

    out_transform->rotation = 0;
    out_transform->scale_x = NANR_SCALE_ONE;
    out_transform->scale_y = NANR_SCALE_ONE;
    out_transform->translate_x = 0;
    out_transform->translate_y = 0;

    if (frame == NULL) return;

    out_transform->rotation = frame->rotation;
    out_transform->scale_x = frame->scale_x;
    out_transform->scale_y = frame->scale_y;
    out_transform->translate_x = frame->translate_x;
    out_transform->translate_y = frame->translate_y;
}

static void ComputeNmarBoundsRange(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    int animation_index,
    int start_frame,
    int frame_count,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    int delay_cs
)
{
    int min_x = 999999;
    int min_y = 999999;
    int max_x = -999999;
    int max_y = -999999;
    int i;

    for (i = 0; i < frame_count; i++) {
        int tick = ((start_frame + i) * delay_cs * 60) / 100;
        NmarFrame nmar_frame;
        ComposerTransform parent_transform;
        int map_index;
        int frame_min_x;
        int frame_min_y;
        int frame_max_x;
        int frame_max_y;

        map_index = Nmar_GetFrameAtTick(nmar, animation_index, tick, &nmar_frame);
        if (map_index < 0 || map_index >= nmcr->map_count) continue;
        ComposerTransform_FromNmarFrame(&nmar_frame, &parent_transform);
        Composer_ComputeBoundsWithTransform(
            ncer,
            nanr,
            &nmcr->maps[map_index],
            tick,
            &frame_min_x,
            &frame_min_y,
            &frame_max_x,
            &frame_max_y,
            coords,
            &parent_transform
        );

        if (frame_min_x < min_x) min_x = frame_min_x;
        if (frame_min_y < min_y) min_y = frame_min_y;
        if (frame_max_x > max_x) max_x = frame_max_x;
        if (frame_max_y > max_y) max_y = frame_max_y;
    }

    if (min_x == 999999) {
        min_x = 0;
        min_y = 0;
        max_x = 1;
        max_y = 1;
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}

int GifPipeline_ExportIdle(
    const char *out_dir,
    const char *side_name,
    const char *animation_name,
    const char *palette_name,
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int default_idle_map,
    const GifExportOptions *options,
    int tile_stride,
    int margin,
    const int *union_min_x,
    const int *union_min_y,
    const int *union_max_x,
    const int *union_max_y,
    const CoordFile *coords
)
{
    char gif_dir[GIF_PATH_BUFFER_SIZE];
    char gif_path[GIF_PATH_BUFFER_SIZE];
    const char *eye_name;
    const NmcrMap *map;
    int map_index;
    int auto_frame_count;
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    int frame_index;
    u8 *frames;
    u8 *cropped_frames;
    int cropped_width;
    int cropped_height;
    int nmar_timeline_index;
    int playback_delay_cs;

    if (options == NULL || !options->enabled ||
        ncer == NULL || nanr == NULL || nmcr == NULL ||
        ncgr == NULL || palette == NULL || nmcr->map_count <= 0) {
        return -1;
    }

    nmar_timeline_index = ResolveNmarTimelineIndex(options, nmar, animation_name);
    playback_delay_cs = ResolveGifPlaybackDelayCs(options);
    map_index = ResolveGifMapIndex(options, default_idle_map, nmcr->map_count);
    map = &nmcr->maps[map_index];
    auto_frame_count = nmar_timeline_index >= 0 ?
        NmarFrameCountForGif(nmar, nmar_timeline_index, options->delay_cs) :
        Nmcr_MaxFrameCount(map, nanr);
    frame_count = options->frame_count < 0 ? auto_frame_count : options->frame_count;

    if (frame_count <= 0) {
        frame_count = auto_frame_count;
    }

    if (nmar_timeline_index >= 0 && strcmp(animation_name, "idle") == 0) {
        int break_map_index = Nmar_GetIdleBreakMapIndex(nmar, map_index, nmcr->map_count);
        int first_break_frame = -1;
        for (int f = 0; f < frame_count; f++) {
            int tick = ((options->start_frame + f) * options->delay_cs * 60) / 100;
            NmarFrame nmar_frame;
            int frame_map_index = Nmar_GetFrameAtTick(nmar, nmar_timeline_index, tick, &nmar_frame);
            if (frame_map_index >= 0) {
                if (break_map_index >= 0 && frame_map_index == break_map_index) {
                    first_break_frame = f;
                    break;
                } else if (break_map_index < 0 && frame_map_index != map_index) {
                    first_break_frame = f;
                    break;
                }
            }
        }
        if (first_break_frame > 0) {
            frame_count = first_break_frame;
        }
    }

    if (frame_count <= 0 || frame_count > 4096) {
        return -1;
    }

    if (nmar_timeline_index >= 0) {
        ComputeNmarBoundsRange(
            ncer,
            nanr,
            nmcr,
            nmar,
            nmar_timeline_index,
            options->start_frame,
            frame_count,
            &min_x,
            &min_y,
            &max_x,
            &max_y,
            coords,
            options->delay_cs
        );
    } else if (union_min_x != NULL && union_min_y != NULL &&
        union_max_x != NULL && union_max_y != NULL) {
        min_x = *union_min_x;
        min_y = *union_min_y;
        max_x = *union_max_x;
        max_y = *union_max_y;
    } else {
        Composer_ComputeBoundsRange(
            ncer,
            nanr,
            map,
            options->start_frame,
            frame_count,
            &min_x,
            &min_y,
            &max_x,
            &max_y,
            coords,
            options->delay_cs
        );
    }

    width = (max_x - min_x) + margin * 2;
    height = (max_y - min_y) + margin * 2;

    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return -1;
    }

    frames = calloc(
        (size_t)frame_count *
        (size_t)width *
        (size_t)height,
        1
    );
    if (frames == NULL) {
        return -1;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        u8 *frame;
        int tick = ((options->start_frame + frame_index) * options->delay_cs * 60) / 100;
        const NmcrMap *render_map;
        ComposerTransform parent_transform;
        ComposerTransform *parent_transform_ptr;

        frame = frames + ((size_t)frame_index * (size_t)width * (size_t)height);
        render_map = map;
        parent_transform_ptr = NULL;

        if (nmar_timeline_index >= 0) {
            NmarFrame nmar_frame;
            int frame_map_index;

            frame_map_index = Nmar_GetFrameAtTick(nmar, nmar_timeline_index, tick, &nmar_frame);
            if (frame_map_index < 0 || frame_map_index >= nmcr->map_count) {
                free(frames);
                return -1;
            }

            render_map = &nmcr->maps[frame_map_index];
            ComposerTransform_FromNmarFrame(&nmar_frame, &parent_transform);
            parent_transform_ptr = &parent_transform;
        }

        if (Composer_RenderFrameIndexedWithTransform(
                ncer,
                nanr,
                render_map,
                ncgr,
                palette,
                tick,
                tile_stride,
                min_x,
                min_y,
                width,
                height,
                margin,
                frame,
                coords,
                parent_transform_ptr
            ) != 0) {
            free(frames);
            return -1;
        }
    }

    cropped_frames = NULL;
    cropped_width = 0;
    cropped_height = 0;

    if (Composer_CropIndexedFrames(
            frames,
            frame_count,
            width,
            height,
            &cropped_frames,
            &cropped_width,
            &cropped_height
        ) != 0) {
        free(frames);
        return -1;
    }

    snprintf(gif_dir, sizeof(gif_dir), "%s/%s", out_dir, GifDirectoryForAnimation(animation_name));
    if (File_MkdirRecursive(gif_dir) != 0) {
        free(frames);
        free(cropped_frames);
        return -1;
    }

    eye_name = options->eye_mode == GIF_EYE_ALL ? "all" : "open";
    snprintf(
        gif_path,
        sizeof(gif_path),
        "%s/%s_%s_%s_%s.gif",
        gif_dir,
        side_name,
        animation_name,
        eye_name,
        palette_name
    );

    if (Gif_WriteIndexed(
            gif_path,
            cropped_frames,
            frame_count,
            cropped_width,
            cropped_height,
            palette->colors,
            palette->color_count,
            0,
            playback_delay_cs,
            options->loop_count,
            options->scale
        ) != 0) {
        free(frames);
        free(cropped_frames);
        return -1;
    }

    printf("Generated idle GIF:\n");
    printf("  %s\n", gif_path);
    printf("  map=%d frames=%d size=%dx%d scale=%d delay=%dcs%s\n",
           map_index,
           frame_count,
           cropped_width,
           cropped_height,
           options->scale,
           playback_delay_cs,
           nmar_timeline_index >= 0 ? " nmar=on" : "");

    free(frames);
    free(cropped_frames);
    return 0;
}

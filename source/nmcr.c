#include "nmcr.h"
#include "file_util.h"
#include "nitro_util.h"

int Nmcr_Parse(const u8 *data, size_t size, NmcrFile *out_nmcr)
{
    size_t kbcm_offset;
    u32 kbcm_size;
    int map_count;
    u32 map_table_offset;
    size_t map_table;
    size_t record_base;
    int i;

    if (data == NULL || out_nmcr == NULL) {
        return -1;
    }

    memset(out_nmcr, 0, sizeof(*out_nmcr));

    if (size < 0x20 || !Nitro_HasMagic(data, size, "RCMN")) {
        return -1;
    }

    if (Nitro_FindSection(data, size, "KBCM", &kbcm_offset, &kbcm_size) != 0) {
        return -1;
    }

    if (kbcm_size < 0x18) {
        return -1;
    }

    map_count = ReadU16LE(data + kbcm_offset + 0x08);
    map_table_offset = ReadU32LE(data + kbcm_offset + 0x0C);

    if (map_count <= 0 || map_count > 2048) {
        return -1;
    }

    map_table = kbcm_offset + 8 + map_table_offset;
    record_base = map_table + ((size_t)map_count * 8);

    if (map_table + ((size_t)map_count * 8) > kbcm_offset + kbcm_size ||
        record_base > kbcm_offset + kbcm_size) {
        return -1;
    }

    out_nmcr->maps = calloc((size_t)map_count, sizeof(NmcrMap));
    if (out_nmcr->maps == NULL) {
        return -1;
    }

    out_nmcr->map_count = map_count;

    for (i = 0; i < map_count; i++) {
        size_t entry_offset;
        size_t records_offset;
        int j;

        entry_offset = map_table + ((size_t)i * 8);
        out_nmcr->maps[i].record_count = ReadU16LE(data + entry_offset + 0);
        out_nmcr->maps[i].raw_record_offset = ReadU32LE(data + entry_offset + 4);

        if (out_nmcr->maps[i].record_count > 4096) {
            Nmcr_Free(out_nmcr);
            return -1;
        }

        records_offset = record_base + out_nmcr->maps[i].raw_record_offset;
        if (records_offset > kbcm_offset + kbcm_size) {
            Nmcr_Free(out_nmcr);
            return -1;
        }

        if (out_nmcr->maps[i].record_count == 0) {
            continue;
        }

        if (records_offset + ((size_t)out_nmcr->maps[i].record_count * 8) >
            kbcm_offset + kbcm_size) {
            Nmcr_Free(out_nmcr);
            return -1;
        }

        out_nmcr->maps[i].records = calloc(
            (size_t)out_nmcr->maps[i].record_count,
            sizeof(NmcrRecord)
        );
        if (out_nmcr->maps[i].records == NULL) {
            Nmcr_Free(out_nmcr);
            return -1;
        }

        for (j = 0; j < out_nmcr->maps[i].record_count; j++) {
            size_t record_offset;

            record_offset = records_offset + ((size_t)j * 8);

            out_nmcr->maps[i].records[j].animation_index = ReadU16LE(data + record_offset + 0);
            out_nmcr->maps[i].records[j].x = Nitro_ReadS16LE(data + record_offset + 2);
            out_nmcr->maps[i].records[j].y = Nitro_ReadS16LE(data + record_offset + 4);
            out_nmcr->maps[i].records[j].flags = Nitro_ReadS16LE(data + record_offset + 6);
        }
    }

    return 0;
}

void Nmcr_Free(NmcrFile *nmcr)
{
    int i;

    if (nmcr == NULL) {
        return;
    }

    if (nmcr->maps != NULL) {
        for (i = 0; i < nmcr->map_count; i++) {
            free(nmcr->maps[i].records);
        }

        free(nmcr->maps);
    }

    nmcr->map_count = 0;
    nmcr->maps = NULL;
}

static int Gcd(int a, int b)
{
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static int Lcm(int a, int b)
{
    if (a == 0 || b == 0) return 0;
    int g = Gcd(a, b);
    if (g == 0) return 0;
    return (a / g) * b;
}

int Nmcr_MaxFrameCount(const NmcrMap *map, const NanrFile *nanr)
{
    int i;
    int lcm_ticks = 0;
    int ticks_per_frame = 3; // delay_cs = 5 maps to (5 * 60)/100 = 3 ticks per frame

    if (map == NULL || nanr == NULL) {
        return 1;
    }

    for (i = 0; i < map->record_count; i++) {
        int anim_idx = map->records[i].animation_index;
        if (anim_idx >= 0 && anim_idx < nanr->animation_count) {
            const NanrAnimation *anim = &nanr->animations[anim_idx];
            int anim_dur = 0;
            int j;
            for (j = 0; j < anim->frame_count; j++) {
                anim_dur += anim->frames[j].duration;
            }
            if (anim_dur > 0) {
                if (lcm_ticks == 0) {
                    lcm_ticks = anim_dur;
                } else {
                    lcm_ticks = Lcm(lcm_ticks, anim_dur);
                }
                if (lcm_ticks > 240) {
                    lcm_ticks = 240;
                }
            }
        }
    }

    if (lcm_ticks <= 0) {
        return 1;
    }

    // Calculate the LCM of lcm_ticks and ticks_per_frame so they align perfectly
    int aligned_lcm = Lcm(lcm_ticks, ticks_per_frame);
    if (aligned_lcm > 240) {
        aligned_lcm = (240 / ticks_per_frame) * ticks_per_frame;
    }

    return aligned_lcm / ticks_per_frame;
}

int Nmcr_CountValidRecords(
    const NmcrMap *map,
    const NanrFile *nanr,
    const NcerFile *ncer,
    int frame_index
)
{
    int valid;
    int i;

    valid = 0;

    for (i = 0; i < map->record_count; i++) {
        NanrFrame frame;
        int cell_id;

        cell_id = Nanr_GetResolvedCellId(nanr, map->records[i].animation_index, frame_index, &frame);

        if (cell_id >= 0 && cell_id < ncer->cell_count) {
            valid++;
        }
    }

    return valid;
}

int Nmcr_ComputeBreakScore(
    const NmcrMap *idle_map,
    const NmcrMap *candidate_map,
    const NanrFile *nanr,
    const NcerFile *ncer
)
{
    int score;
    int min_count;
    int i;
    int idle_anim_count;
    int cand_anim_count;
    int has_any_overlap;
    int has_any_diff;
    int idle_valid;
    int cand_valid;

    if (idle_map == NULL || candidate_map == NULL) {
        return 0;
    }

    /* Same map → not a break candidate */
    if (idle_map == candidate_map) {
        return 0;
    }

    score = 0;

    /* --- structural divergence --- */

    /* 1. Different record count */
    if (idle_map->record_count != candidate_map->record_count) {
        score += 2;
    }

    /* 2. Count unique animation indices in each map */
    {
        int seen_idle[4096];
        int seen_cand[4096];
        int idle_idx;
        int cand_idx;
        int r;

        memset(seen_idle, 0, sizeof(seen_idle));
        memset(seen_cand, 0, sizeof(seen_cand));
        idle_anim_count = 0;
        cand_anim_count = 0;

        for (r = 0; r < idle_map->record_count; r++) {
            idle_idx = idle_map->records[r].animation_index;
            if (idle_idx >= 0 && idle_idx < 4096 && !seen_idle[idle_idx]) {
                seen_idle[idle_idx] = 1;
                idle_anim_count++;
            }
        }

        for (r = 0; r < candidate_map->record_count; r++) {
            cand_idx = candidate_map->records[r].animation_index;
            if (cand_idx >= 0 && cand_idx < 4096 && !seen_cand[cand_idx]) {
                seen_cand[cand_idx] = 1;
                cand_anim_count++;
            }
        }

        /* Different unique animation count */
        if (idle_anim_count != cand_anim_count) {
            score += 1;
        }

        /* Check for overlap between animation sets */
        has_any_overlap = 0;
        for (r = 0; r < 4096; r++) {
            if (seen_idle[r] && seen_cand[r]) {
                has_any_overlap = 1;
                break;
            }
        }

        /* Completely disjoint animation sets → strong break signal */
        if (!has_any_overlap && cand_anim_count > 0) {
            score += 5;
        }
    }

    /* 3. Per-record comparison */
    min_count = idle_map->record_count;
    if (candidate_map->record_count < min_count) {
        min_count = candidate_map->record_count;
    }

    has_any_diff = 0;
    for (i = 0; i < min_count; i++) {
        if (candidate_map->records[i].animation_index !=
            idle_map->records[i].animation_index) {
            score += 3;
            has_any_diff = 1;
        } else {
            if (candidate_map->records[i].x != idle_map->records[i].x) {
                score += 1;
                has_any_diff = 1;
            }
            if (candidate_map->records[i].y != idle_map->records[i].y) {
                score += 1;
                has_any_diff = 1;
            }
            if (candidate_map->records[i].flags != idle_map->records[i].flags) {
                score += 1;
                has_any_diff = 1;
            }
        }
    }

    /* 4. Validate that at least some records resolve to valid NCER cells */
    idle_valid = Nmcr_CountValidRecords(idle_map, nanr, ncer, 0);
    cand_valid = Nmcr_CountValidRecords(candidate_map, nanr, ncer, 0);

    /* Penalty if candidate has far fewer valid records than idle */
    if (cand_valid < idle_valid / 2) {
        score -= 10;
    }

    /* No differences at all → score should be 0 */
    if (!has_any_diff && idle_map->record_count == candidate_map->record_count) {
        score = 0;
    }

    return score;
}

void Nmcr_PrintInfo(const NmcrFile *nmcr)
{
    int i;

    if (nmcr == NULL) return;

    printf("NMCR file:\n");
    printf("  map count: %d\n", nmcr->map_count);

    for (i = 0; i < nmcr->map_count && i < 8; i++) {
        printf("  [%02d] records=%d\n",
               i,
               nmcr->maps[i].record_count);
    }

    if (nmcr->map_count > 8) {
        printf("  ...\n");
    }

    printf("\n");
}

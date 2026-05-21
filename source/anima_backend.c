#include "anima_backend.h"
#include "common.h"
#include "file_util.h"
#include "nds_header.h"
#include "nds_fnt.h"
#include "nds_fat.h"
#include "narc.h"
#include "lz.h"
#include "nitro_guess.h"
#include "nitro_util.h"
#include "nclr.h"
#include "ncgr.h"
#include "ncer.h"
#include "nanr.h"
#include "nmcr.h"
#include "nmar.h"
#include "ppm.h"
#include "png_writer.h"
#include "png_pipeline.h"
#include "gif_writer.h"
#include "gif_pipeline.h"
#include "json_export.h"
#include "sprite_composer.h"
#include "coords.h"

static int s_game_is_sequel = 0;

#define PATH_BUFFER_SIZE ANIMA_PATH_BUFFER_SIZE
#define BW_TILE_STRIDE ANIMA_BW_TILE_STRIDE
#define COMPOSITE_MARGIN ANIMA_COMPOSITE_MARGIN
#define ANIMA_DIR_RAW_MEMBERS "raw_narc_members"
#define ANIMA_DIR_NDS_FILES "nds_files"
#define ANIMA_DIR_JSON "reconstruction_json"
#define ANIMA_DIR_SPRITESHEETS "spritesheet_png"
#define ANIMA_DIR_STATIC "static_png"
#define ANIMA_DIR_IDLE_GIF "animated_idle_gif"
#define ANIMA_DIR_BREAK_GIF "idle_break_gif"
#define ANIMA_DIR_COMPOSED_GIF "composed_gif"

typedef enum PokemonMemberRole {
    ROLE_FRONT_STATIC,
    ROLE_FRONT_STATIC_EMPTY,
    ROLE_FRONT_SHEET,
    ROLE_FRONT_SHEET_EMPTY,
    ROLE_FRONT_CELLS,
    ROLE_FRONT_ANIM,
    ROLE_FRONT_MAP,
    ROLE_FRONT_TIMING,
    ROLE_FRONT_COORDS,
    ROLE_BACK_STATIC,
    ROLE_BACK_STATIC_EMPTY,
    ROLE_BACK_SHEET,
    ROLE_BACK_SHEET_EMPTY,
    ROLE_BACK_CELLS,
    ROLE_BACK_ANIM,
    ROLE_BACK_MAP,
    ROLE_BACK_TIMING,
    ROLE_BACK_COORDS,
    ROLE_PALETTE_NORMAL,
    ROLE_PALETTE_SHINY
} PokemonMemberRole;

typedef struct PokemonMemberSpec {
    PokemonMemberRole role;
    const char *logical_type;
    const char *semantic_name;
    const char *extension;
    int is_coordinate_file;
} PokemonMemberSpec;

static const PokemonMemberSpec g_member_specs[20] = {
    { ROLE_FRONT_STATIC,      "NCGR_STATIC",   "front_static",       "ncgr", 0 },
    { ROLE_FRONT_STATIC_EMPTY,"EMPTY",         "front_static_empty", "bin",  0 },
    { ROLE_FRONT_SHEET,       "NCGR_SHEET",    "front_sheet",        "ncgr", 0 },
    { ROLE_FRONT_SHEET_EMPTY, "EMPTY",         "front_sheet_empty",  "bin",  0 },
    { ROLE_FRONT_CELLS,       "NCER_CELLS",    "front_cells",        "ncer", 0 },
    { ROLE_FRONT_ANIM,        "NANR_ANIM",     "front_anim",         "nanr", 0 },
    { ROLE_FRONT_MAP,         "NMCR_MAP",      "front_map",          "nmcr", 0 },
    { ROLE_FRONT_TIMING,      "NMAR_TIMING",   "front_timing",       "nmar", 0 },
    { ROLE_FRONT_COORDS,      "COORDS",        "front_coords",       "bin",  1 },
    { ROLE_BACK_STATIC,       "NCGR_STATIC",   "back_static",        "ncgr", 0 },
    { ROLE_BACK_STATIC_EMPTY, "EMPTY",         "back_static_empty",  "bin",  0 },
    { ROLE_BACK_SHEET,        "NCGR_SHEET",    "back_sheet",         "ncgr", 0 },
    { ROLE_BACK_SHEET_EMPTY,  "EMPTY",         "back_sheet_empty",   "bin",  0 },
    { ROLE_BACK_CELLS,        "NCER_CELLS",    "back_cells",         "ncer", 0 },
    { ROLE_BACK_ANIM,         "NANR_ANIM",     "back_anim",          "nanr", 0 },
    { ROLE_BACK_MAP,          "NMCR_MAP",      "back_map",           "nmcr", 0 },
    { ROLE_BACK_TIMING,       "NMAR_TIMING",   "back_timing",        "nmar", 0 },
    { ROLE_BACK_COORDS,       "COORDS",        "back_coords",        "bin",  1 },
    { ROLE_PALETTE_NORMAL,    "NCLR_PALETTE",  "palette_normal",     "nclr", 0 },
    { ROLE_PALETTE_SHINY,     "NCLR_PALETTE",  "palette_shiny",      "nclr", 0 }
};

static int ResolveBreakMapValidated(
    const NmarFile *nmar,
    int idle_map,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer
);
static int FindIdleNmarAnimationIndex(const NmarFile *nmar);
static int FindBreakNmarAnimationIndex(
    const NmarFile *nmar,
    int idle_map,
    int break_map,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer
);
static int RenderComposedAnimationSmart(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    int idle_map,
    int break_map,
    int preferred_break_animation_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
);

static void PrintFirstFourBytes(const u8 *data, size_t size)
{
    if (size < 4) {
        printf("First four bytes: <too small>\n");
        return;
    }

    printf("First four bytes: %c%c%c%c\n",
           data[0],
           data[1],
           data[2],
           data[3]);
}

static int ExtractDecodedMember(
    const NarcArchive *narc,
    int member_id,
    const PokemonMemberSpec *spec,
    u8 **out_data,
    size_t *out_size
)
{
    u8 *member_data;
    size_t member_size;
    CompressionType compression;

    if (narc == NULL || spec == NULL || out_data == NULL || out_size == NULL) {
        return -1;
    }

    *out_data = NULL;
    *out_size = 0;
    member_data = NULL;
    member_size = 0;

    if (Narc_ExtractMember(narc, member_id, &member_data, &member_size) != 0) {
        return -1;
    }

    compression = Lz_Detect(member_data, member_size);

    if (spec->is_coordinate_file) {
        if (CopyBytes(member_data, member_size, out_data, out_size) != 0) {
            free(member_data);
            return -1;
        }
    } else if (Lz_Decompress(member_data, member_size, out_data, out_size, &compression) != 0) {
        if (CopyBytes(member_data, member_size, out_data, out_size) != 0) {
            free(member_data);
            return -1;
        }
    }

    free(member_data);
    return 0;
}

static int AnimaBackend_GetFormBlockIndex(int species, int form_index)
{
    if (form_index <= 0) {
        return species;
    }

    if (s_game_is_sequel) {
        switch (species) {
            case 201: // Unown
                return 685 + (form_index - 1);
            case 351: // Castform
                return 712 + (form_index - 1);
            case 386: // Deoxys
                return 715 + (form_index - 1);
            case 412: // Burmy
                return 718 + (form_index - 1);
            case 413: // Wormadam
                return 720 + (form_index - 1);
            case 421: // Cherrim
                return 722;
            case 422: // Shellos
                return 723;
            case 423: // Gastrodon
                return 724;
            case 479: // Rotom
                return 725 + (form_index - 1);
            case 487: // Giratina
                return 730;
            case 492: // Shaymin
                return 731;
            case 550: // Basculin
                return 732;
            case 555: // Darmanitan
                return 733;
            case 585: // Deerling
                return 734 + (form_index - 1);
            case 586: // Sawsbuck
                return 737 + (form_index - 1);
            case 641: // Tornadus
                return 740;
            case 642: // Thundurus
                return 741;
            case 645: // Landorus
                return 742;
            case 646: // Kyurem
                if (form_index == 1) return 744; // Black Kyurem
                if (form_index == 2) return 743; // White Kyurem
                break;
            case 647: // Keldeo
                return 745;
            case 648: // Meloetta
                return 746;
            case 649: // Genesect
                return 747 + (form_index - 1);
            default:
                break;
        }
    } else {
        switch (species) {
            case 201: // Unown
                return 652 + (form_index - 1);
            case 351: // Castform
                return 679 + (form_index - 1);
            case 386: // Deoxys
                return 682 + (form_index - 1);
            case 412: // Burmy
                return 685 + (form_index - 1);
            case 413: // Wormadam
                return 687 + (form_index - 1);
            case 421: // Cherrim
                return 689;
            case 422: // Shellos
                return 690;
            case 423: // Gastrodon
                return 691;
            case 479: // Rotom
                return 692 + (form_index - 1);
            case 487: // Giratina
                return 697;
            case 492: // Shaymin
                return 698;
            case 550: // Basculin
                return 699;
            case 555: // Darmanitan
                return 700;
            case 585: // Deerling
                return 701 + (form_index - 1);
            case 586: // Sawsbuck
                return 704 + (form_index - 1);
            case 648: // Meloetta
                return 707;
            case 649: // Genesect
                return 708 + (form_index - 1);
            default:
                break;
        }
    }

    return species;
}

static PokemonMemberRole ResolveMemberRole(PokemonMemberRole role, const AnimaPreviewOptions *opts)
{
    int is_female = (opts != NULL) && (opts->gender == 1);
    int is_shiny = (opts != NULL) && (opts->is_shiny == 1);
    int is_back = (opts != NULL) && (opts->is_back == 1);

    if (is_back) {
        switch (role) {
            case ROLE_FRONT_STATIC:
                return is_female ? ROLE_BACK_STATIC_EMPTY : ROLE_BACK_STATIC;
            case ROLE_FRONT_SHEET:
                return is_female ? ROLE_BACK_SHEET_EMPTY : ROLE_BACK_SHEET;
            case ROLE_FRONT_CELLS:
                return ROLE_BACK_CELLS;
            case ROLE_FRONT_ANIM:
                return ROLE_BACK_ANIM;
            case ROLE_FRONT_MAP:
                return ROLE_BACK_MAP;
            case ROLE_FRONT_TIMING:
                return ROLE_BACK_TIMING;
            case ROLE_FRONT_COORDS:
                return ROLE_BACK_COORDS;
            default:
                break;
        }
    } else {
        switch (role) {
            case ROLE_FRONT_STATIC:
                return is_female ? ROLE_FRONT_STATIC_EMPTY : ROLE_FRONT_STATIC;
            case ROLE_FRONT_SHEET:
                return is_female ? ROLE_FRONT_SHEET_EMPTY : ROLE_FRONT_SHEET;
            default:
                break;
        }
    }

    if (role == ROLE_PALETTE_NORMAL) {
        return is_shiny ? ROLE_PALETTE_SHINY : ROLE_PALETTE_NORMAL;
    }

    return role;
}

static int ExtractDecodedPokemonRoleExt(
    const NarcArchive *narc,
    int species,
    PokemonMemberRole role,
    const AnimaPreviewOptions *opts,
    u8 **out_data,
    size_t *out_size
)
{
    int form_index = (opts != NULL) ? opts->form_index : 0;
    int block_index = AnimaBackend_GetFormBlockIndex(species, form_index);
    PokemonMemberRole resolved_role = ResolveMemberRole(role, opts);
    int base = block_index * 20;
    int member_id = base + (int)resolved_role;

    // Check if we need to fall back from female to male
    if (opts != NULL && opts->gender == 1 && (role == ROLE_FRONT_STATIC || role == ROLE_FRONT_SHEET)) {
        NarcMemberRange range;
        if (Narc_GetMemberRange(narc, member_id, &range) != 0 || range.size == 0) {
            // Fall back to male
            AnimaPreviewOptions male_opts = *opts;
            male_opts.gender = 0;
            resolved_role = ResolveMemberRole(role, &male_opts);
            member_id = base + (int)resolved_role;
        }
    }

    int offset = (int)resolved_role;
    if (offset < 0 || offset >= 20) {
        return -1;
    }

    return ExtractDecodedMember(
        narc,
        member_id,
        &g_member_specs[offset],
        out_data,
        out_size
    );
}

static int ExtractDecodedPokemonRole(
    const NarcArchive *narc,
    int species,
    PokemonMemberRole role,
    u8 **out_data,
    size_t *out_size
)
{
    return ExtractDecodedPokemonRoleExt(narc, species, role, NULL, out_data, out_size);
}

static int InitPokegraFromRom(
    const char *rom_path,
    u8 **out_rom,
    size_t *out_rom_size,
    NarcArchive *out_pokegra
)
{
    u8 *rom;
    size_t rom_size;
    NdsHeader header;
    const u8 *fnt;
    int file_id;
    NdsFatRange range;

    if (rom_path == NULL || out_rom == NULL ||
        out_rom_size == NULL || out_pokegra == NULL) {
        return -1;
    }

    *out_rom = NULL;
    *out_rom_size = 0;
    rom = NULL;
    rom_size = 0;

    if (File_ReadAll(rom_path, &rom, &rom_size) != 0) {
        return -1;
    }

    if (NdsHeader_Parse(rom, rom_size, &header) != 0) {
        free(rom);
        return -1;
    }

    if (!NdsHeader_IsValidGame(&header)) {
        fprintf(stderr, "Error: The provided NDS ROM is not a valid Pokémon Black, White, Black 2, or White 2 game.\n");
        free(rom);
        return -1;
    }
    s_game_is_sequel = NdsHeader_IsSequel(&header);

    fnt = rom + header.fnt_offset;
    if (NdsFnt_FindFileId(fnt, header.fnt_size, ANIMA_POKEGRA_PATH, &file_id) != 0) {
        free(rom);
        return -1;
    }

    if (NdsFat_GetRange(rom, rom_size, &header, file_id, &range) != 0) {
        free(rom);
        return -1;
    }

    if (Narc_Init(out_pokegra, rom + range.start, range.size) != 0) {
        free(rom);
        return -1;
    }

    *out_rom = rom;
    *out_rom_size = rom_size;
    return 0;
}

static int WritePokemonBlockMembers(
    const NarcArchive *narc,
    int species,
    int block_index,
    const char *out_dir
)
{
    int base;
    int i;
    char raw_dir[PATH_BUFFER_SIZE];
    char nds_dir[PATH_BUFFER_SIZE];
    char json_dir[PATH_BUFFER_SIZE];
    JsonMemberInfo exports[20];

    base = block_index * 20;

    snprintf(raw_dir, sizeof(raw_dir), "%s/%s", out_dir, ANIMA_DIR_RAW_MEMBERS);
    snprintf(nds_dir, sizeof(nds_dir), "%s/%s", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(json_dir, sizeof(json_dir), "%s/%s", out_dir, ANIMA_DIR_JSON);

    if (File_MkdirRecursive(raw_dir) != 0) {
        fprintf(stderr, "Error: could not create raw output directory: %s\n", raw_dir);
        return -1;
    }

    if (File_MkdirRecursive(nds_dir) != 0 ||
        File_MkdirRecursive(json_dir) != 0) {
        fprintf(stderr, "Error: could not create structured output directories.\n");
        return -1;
    }

    memset(exports, 0, sizeof(exports));

    printf("Extracting Pokemon block:\n");
    printf("  species: %d\n", species);
    printf("  form block: %d\n", block_index);
    printf("  base member: %d\n", base);
    printf("  block: %d..%d\n", base, base + 19);
    printf("\n");

    for (i = 0; i < 20; i++) {
        int member_id;
        u8 *member_data;
        size_t member_size;

        u8 *decoded_data;
        size_t decoded_size;
        CompressionType compression;
        NitroFileType nitro_type;
        const PokemonMemberSpec *spec;
        const char *compression_name;

        char raw_path[PATH_BUFFER_SIZE];
        char nds_path[PATH_BUFFER_SIZE];
        char decoded_magic[5];

        member_id = base + i;
        member_data = NULL;
        member_size = 0;
        decoded_data = NULL;
        decoded_size = 0;
        spec = &g_member_specs[i];

        if (Narc_ExtractMember(narc, member_id, &member_data, &member_size) != 0) {
            fprintf(stderr, "Error: could not extract NARC member %d\n", member_id);
            return -1;
        }

        snprintf(raw_path, sizeof(raw_path), "%s/%05d_%02d.raw.bin",
                 raw_dir, member_id, i);

        if (File_WriteAll(raw_path, member_data, member_size) != 0) {
            fprintf(stderr, "Error: could not write %s\n", raw_path);
            free(member_data);
            return -1;
        }

        compression = Lz_Detect(member_data, member_size);

        if (spec->is_coordinate_file) {
            if (CopyBytes(member_data, member_size, &decoded_data, &decoded_size) != 0) {
                free(member_data);
                return -1;
            }

            compression_name = compression == COMPRESSION_NONE ? "none" : "not_lz_special";
        } else if (Lz_Decompress(
                       member_data,
                       member_size,
                       &decoded_data,
                       &decoded_size,
                       &compression
                   ) != 0) {
            if (CopyBytes(member_data, member_size, &decoded_data, &decoded_size) != 0) {
                free(member_data);
                return -1;
            }

            compression_name = compression == COMPRESSION_NONE
                ? "none"
                : "invalid_lz";
        } else {
            compression_name = Lz_CompressionName(compression);
        }

        nitro_type = NitroGuess_Detect(decoded_data, decoded_size, i);
        (void)nitro_type;

        snprintf(nds_path, sizeof(nds_path), "%s/%s.%s",
                 nds_dir, spec->semantic_name, spec->extension);

        if (File_WriteAll(nds_path, decoded_data, decoded_size) != 0) {
            fprintf(stderr, "Error: could not write %s\n", nds_path);
            free(member_data);
            free(decoded_data);
            return -1;
        }

        Nitro_GetPrintableMagic(decoded_data, decoded_size, decoded_magic);

        printf(
            "  [%02d] member %05d raw=%zu decoded=%zu comp=%s magic=%s type=%s\n",
            i,
            member_id,
            member_size,
            decoded_size,
            compression_name,
            decoded_magic,
            spec->logical_type
        );

        exports[i].offset = i;
        exports[i].member_id = member_id;
        exports[i].raw_size = member_size;
        exports[i].decoded_size = decoded_size;
        snprintf(exports[i].compression, sizeof(exports[i].compression), "%s", compression_name);
        snprintf(exports[i].magic, sizeof(exports[i].magic), "%s", decoded_magic);
        snprintf(exports[i].type, sizeof(exports[i].type), "%s", spec->logical_type);
        snprintf(exports[i].raw_path, sizeof(exports[i].raw_path), "%s", raw_path);
        snprintf(exports[i].nds_path, sizeof(exports[i].nds_path), "%s", nds_path);

        free(member_data);
        free(decoded_data);
    }

    if (Json_WriteManifest(json_dir, species, base, exports) != 0) {
        return -1;
    }

    printf("\n");
    printf("Pokemon block extracted successfully.\n");
    printf("Manifest written to:\n");
    printf("  %s/manifest.json\n", json_dir);

    return 0;
}

static void GeneratePokemonTilePreviews(int species, const char *out_dir)
{
    char preview_dir[PATH_BUFFER_SIZE];

    char front_ncgr[PATH_BUFFER_SIZE];
    char back_ncgr[PATH_BUFFER_SIZE];
    char normal_nclr[PATH_BUFFER_SIZE];
    char shiny_nclr[PATH_BUFFER_SIZE];

    char out_front_normal[PATH_BUFFER_SIZE];
    char out_front_shiny[PATH_BUFFER_SIZE];
    char out_back_normal[PATH_BUFFER_SIZE];
    char out_back_shiny[PATH_BUFFER_SIZE];

    (void)species;

    snprintf(preview_dir, sizeof(preview_dir), "%s/%s", out_dir, ANIMA_DIR_SPRITESHEETS);

    if (File_MkdirRecursive(preview_dir) != 0) {
        fprintf(stderr, "Warning: could not create preview directory: %s\n", preview_dir);
        return;
    }

    snprintf(front_ncgr, sizeof(front_ncgr), "%s/%s/front_sheet.ncgr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_ncgr, sizeof(back_ncgr), "%s/%s/back_sheet.ncgr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(normal_nclr, sizeof(normal_nclr), "%s/%s/palette_normal.nclr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(shiny_nclr, sizeof(shiny_nclr), "%s/%s/palette_shiny.nclr", out_dir, ANIMA_DIR_NDS_FILES);

    snprintf(out_front_normal, sizeof(out_front_normal), "%s/front_sheet_normal.png", preview_dir);
    snprintf(out_front_shiny, sizeof(out_front_shiny), "%s/front_sheet_shiny.png", preview_dir);
    snprintf(out_back_normal, sizeof(out_back_normal), "%s/back_sheet_normal.png", preview_dir);
    snprintf(out_back_shiny, sizeof(out_back_shiny), "%s/back_sheet_shiny.png", preview_dir);

    PngPipeline_TilePreview(front_ncgr, normal_nclr, out_front_normal);
    PngPipeline_TilePreview(front_ncgr, shiny_nclr, out_front_shiny);
    PngPipeline_TilePreview(back_ncgr, normal_nclr, out_back_normal);
    PngPipeline_TilePreview(back_ncgr, shiny_nclr, out_back_shiny);
}

static void GeneratePokemonCellPreviews(int species, const char *out_dir)
{
    (void)species;
    (void)out_dir;
}

static int GenerateAssembledStaticAndLeaves(
    const char *out_dir,
    const GifExportOptions *gif_options,
    int export_idle_break_gifs,
    int export_composed_gifs
)
{
    char front_ncgr_path[PATH_BUFFER_SIZE];
    char front_ncer_path[PATH_BUFFER_SIZE];
    char front_nanr_path[PATH_BUFFER_SIZE];
    char front_nmcr_path[PATH_BUFFER_SIZE];
    char front_nmar_path[PATH_BUFFER_SIZE];
    char back_ncgr_path[PATH_BUFFER_SIZE];
    char back_ncer_path[PATH_BUFFER_SIZE];
    char back_nanr_path[PATH_BUFFER_SIZE];
    char back_nmcr_path[PATH_BUFFER_SIZE];
    char back_nmar_path[PATH_BUFFER_SIZE];
    char nclr_path[PATH_BUFFER_SIZE];
    char shiny_nclr_path[PATH_BUFFER_SIZE];
    char front_coords_path[PATH_BUFFER_SIZE];
    char back_coords_path[PATH_BUFFER_SIZE];

    snprintf(front_ncgr_path, sizeof(front_ncgr_path), "%s/%s/front_sheet.ncgr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(front_ncer_path, sizeof(front_ncer_path), "%s/%s/front_cells.ncer", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(front_nanr_path, sizeof(front_nanr_path), "%s/%s/front_anim.nanr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(front_nmcr_path, sizeof(front_nmcr_path), "%s/%s/front_map.nmcr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(front_nmar_path, sizeof(front_nmar_path), "%s/%s/front_timing.nmar", out_dir, ANIMA_DIR_NDS_FILES);

    snprintf(back_ncgr_path, sizeof(back_ncgr_path), "%s/%s/back_sheet.ncgr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_ncer_path, sizeof(back_ncer_path), "%s/%s/back_cells.ncer", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_nanr_path, sizeof(back_nanr_path), "%s/%s/back_anim.nanr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_nmcr_path, sizeof(back_nmcr_path), "%s/%s/back_map.nmcr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_nmar_path, sizeof(back_nmar_path), "%s/%s/back_timing.nmar", out_dir, ANIMA_DIR_NDS_FILES);

    snprintf(nclr_path, sizeof(nclr_path), "%s/%s/palette_normal.nclr", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(shiny_nclr_path, sizeof(shiny_nclr_path), "%s/%s/palette_shiny.nclr", out_dir, ANIMA_DIR_NDS_FILES);

    snprintf(front_coords_path, sizeof(front_coords_path), "%s/%s/front_coords.bin", out_dir, ANIMA_DIR_NDS_FILES);
    snprintf(back_coords_path, sizeof(back_coords_path), "%s/%s/back_coords.bin", out_dir, ANIMA_DIR_NDS_FILES);

    char out_front_idle_normal[PATH_BUFFER_SIZE];
    char out_back_idle_normal[PATH_BUFFER_SIZE];
    char out_front_idle_shiny[PATH_BUFFER_SIZE];
    char out_back_idle_shiny[PATH_BUFFER_SIZE];

    snprintf(out_front_idle_normal, sizeof(out_front_idle_normal), "%s/%s/front_idle_normal.png", out_dir, ANIMA_DIR_STATIC);
    snprintf(out_back_idle_normal, sizeof(out_back_idle_normal), "%s/%s/back_idle_normal.png", out_dir, ANIMA_DIR_STATIC);
    snprintf(out_front_idle_shiny, sizeof(out_front_idle_shiny), "%s/%s/front_idle_shiny.png", out_dir, ANIMA_DIR_STATIC);
    snprintf(out_back_idle_shiny, sizeof(out_back_idle_shiny), "%s/%s/back_idle_shiny.png", out_dir, ANIMA_DIR_STATIC);

    {
        char static_dir[PATH_BUFFER_SIZE];
        snprintf(static_dir, sizeof(static_dir), "%s/%s", out_dir, ANIMA_DIR_STATIC);
        File_MkdirRecursive(static_dir);
    }

    u8 *front_ncgr_data = NULL; size_t front_ncgr_size = 0;
    u8 *front_ncer_data = NULL; size_t front_ncer_size = 0;
    u8 *front_nanr_data = NULL; size_t front_nanr_size = 0;
    u8 *front_nmcr_data = NULL; size_t front_nmcr_size = 0;
    u8 *front_nmar_data = NULL; size_t front_nmar_size = 0;

    u8 *back_ncgr_data = NULL; size_t back_ncgr_size = 0;
    u8 *back_ncer_data = NULL; size_t back_ncer_size = 0;
    u8 *back_nanr_data = NULL; size_t back_nanr_size = 0;
    u8 *back_nmcr_data = NULL; size_t back_nmcr_size = 0;
    u8 *back_nmar_data = NULL; size_t back_nmar_size = 0;

    u8 *nclr_data = NULL; size_t nclr_size = 0;
    u8 *shiny_nclr_data = NULL; size_t shiny_nclr_size = 0;

    u8 *front_coords_data = NULL; size_t front_coords_size = 0;
    u8 *back_coords_data = NULL; size_t back_coords_size = 0;
    CoordFile front_coords; CoordFile back_coords;
    memset(&front_coords, 0, sizeof(front_coords));
    memset(&back_coords, 0, sizeof(back_coords));

    NcgrImage front_ncgr; memset(&front_ncgr, 0, sizeof(front_ncgr));
    NcerFile front_ncer; memset(&front_ncer, 0, sizeof(front_ncer));
    NanrFile front_nanr; memset(&front_nanr, 0, sizeof(front_nanr));
    NmcrFile front_nmcr; memset(&front_nmcr, 0, sizeof(front_nmcr));
    NmarFile front_nmar; memset(&front_nmar, 0, sizeof(front_nmar));

    NcgrImage back_ncgr; memset(&back_ncgr, 0, sizeof(back_ncgr));
    NcerFile back_ncer; memset(&back_ncer, 0, sizeof(back_ncer));
    NanrFile back_nanr; memset(&back_nanr, 0, sizeof(back_nanr));
    NmcrFile back_nmcr; memset(&back_nmcr, 0, sizeof(back_nmcr));
    NmarFile back_nmar; memset(&back_nmar, 0, sizeof(back_nmar));

    NclrPalette palette; memset(&palette, 0, sizeof(palette));
    NclrPalette shiny_palette; memset(&shiny_palette, 0, sizeof(shiny_palette));

    RgbaColor *front_idle = NULL; int front_idle_w = 0, front_idle_h = 0;
    RgbaColor *back_idle = NULL; int back_idle_w = 0, back_idle_h = 0;
    RgbaColor *combined_static = NULL;
    (void)combined_static;

    int front_idle_map = 0;
    int back_idle_map = 0;
    int front_break_map = -1;
    int back_break_map = -1;
    int front_union_min_x = 0, front_union_min_y = 0, front_union_max_x = 0, front_union_max_y = 0;
    int back_union_min_x = 0, back_union_min_y = 0, back_union_max_x = 0, back_union_max_y = 0;
    int have_front_union = 0, have_back_union = 0;
    int success = -1;

    if (File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        printf("DEBUG: Failed at line %d\n", __LINE__);
        goto cleanup;
    }

    if (File_ReadAll(front_ncgr_path, &front_ncgr_data, &front_ncgr_size) != 0) { printf("fail front_ncgr file\n"); goto cleanup; }
    if (Ncgr_Parse(front_ncgr_data, front_ncgr_size, &front_ncgr) != 0) { printf("fail front_ncgr parse\n"); goto cleanup; }
    if (File_ReadAll(front_ncer_path, &front_ncer_data, &front_ncer_size) != 0) { printf("fail front_ncer file\n"); goto cleanup; }
    if (Ncer_Parse(front_ncer_data, front_ncer_size, &front_ncer) != 0) { printf("fail front_ncer parse\n"); goto cleanup; }
    if (File_ReadAll(front_nanr_path, &front_nanr_data, &front_nanr_size) != 0) { printf("fail front_nanr file\n"); goto cleanup; }
    if (Nanr_Parse(front_nanr_data, front_nanr_size, &front_nanr) != 0) { printf("fail front_nanr parse\n"); goto cleanup; }
    if (File_ReadAll(front_nmcr_path, &front_nmcr_data, &front_nmcr_size) != 0) { printf("fail front_nmcr file\n"); goto cleanup; }
    if (Nmcr_Parse(front_nmcr_data, front_nmcr_size, &front_nmcr) != 0) { printf("fail front_nmcr parse\n"); goto cleanup; }

    if (File_ReadAll(back_ncgr_path, &back_ncgr_data, &back_ncgr_size) != 0 ||
        Ncgr_Parse(back_ncgr_data, back_ncgr_size, &back_ncgr) != 0 ||
        File_ReadAll(back_ncer_path, &back_ncer_data, &back_ncer_size) != 0 ||
        Ncer_Parse(back_ncer_data, back_ncer_size, &back_ncer) != 0 ||
        File_ReadAll(back_nanr_path, &back_nanr_data, &back_nanr_size) != 0 ||
        Nanr_Parse(back_nanr_data, back_nanr_size, &back_nanr) != 0 ||
        File_ReadAll(back_nmcr_path, &back_nmcr_data, &back_nmcr_size) != 0 ||
        Nmcr_Parse(back_nmcr_data, back_nmcr_size, &back_nmcr) != 0) {
        printf("DEBUG: Failed at line %d\n", __LINE__);
        goto cleanup;
    }

    if (File_ReadAll(front_nmar_path, &front_nmar_data, &front_nmar_size) == 0 &&
        Nmar_Parse(front_nmar_data, front_nmar_size, &front_nmar) == 0) {
        front_idle_map = Nmar_GetIdleMapIndex(&front_nmar);
        if (front_idle_map < 0) front_idle_map = 0;
        printf("Front idle map from NMAR: %d\n", front_idle_map);
    }

    if (File_ReadAll(back_nmar_path, &back_nmar_data, &back_nmar_size) == 0 &&
        Nmar_Parse(back_nmar_data, back_nmar_size, &back_nmar) == 0) {
        back_idle_map = Nmar_GetIdleMapIndex(&back_nmar);
        if (back_idle_map < 0) back_idle_map = 0;
        printf("Back idle map from NMAR: %d\n", back_idle_map);
    }

    if (front_idle_map >= front_nmcr.map_count) front_idle_map = 0;
    if (back_idle_map >= back_nmcr.map_count) back_idle_map = 0;

    front_break_map = ResolveBreakMapValidated(&front_nmar, front_idle_map, &front_nmcr, &front_nanr, &front_ncer);
    back_break_map = ResolveBreakMapValidated(&back_nmar, back_idle_map, &back_nmcr, &back_nanr, &back_ncer);
    if (front_break_map >= 0 && front_break_map < front_nmcr.map_count) {
        printf("Front idle break map from NMAR: %d\n", front_break_map);
    }
    if (back_break_map >= 0 && back_break_map < back_nmcr.map_count) {
        printf("Back idle break map from NMAR: %d\n", back_break_map);
    }

    if (File_ReadAll(front_coords_path, &front_coords_data, &front_coords_size) == 0) {
        if (Coord_Parse(front_coords_data, front_coords_size, &front_coords) != 0) {
            memset(&front_coords, 0, sizeof(front_coords));
        }
    }

    if (File_ReadAll(back_coords_path, &back_coords_data, &back_coords_size) == 0) {
        if (Coord_Parse(back_coords_data, back_coords_size, &back_coords) != 0) {
            memset(&back_coords, 0, sizeof(back_coords));
        }
    }

    if (front_nmar.entry_count == 0 && front_nmcr.map_count > 1) {
        int best_map;
        int best_count;
        int m;

        best_map = 0;
        best_count = 0;
        printf("NMAR fallback: scanning %d front maps for best idle pose\n", front_nmcr.map_count);

        for (m = 0; m < front_nmcr.map_count; m++) {
            int count = Nmcr_CountValidRecords(&front_nmcr.maps[m], &front_nanr, &front_ncer, 0);
            printf("  front map %d: %d valid records\n", m, count);
            if (count > best_count) {
                best_count = count;
                best_map = m;
            }
        }

        front_idle_map = best_map;
        printf("  selected front map %d (%d valid records)\n", best_map, best_count);
    }

    if (back_nmar.entry_count == 0 && back_nmcr.map_count > 1) {
        int best_map;
        int best_count;
        int m;

        best_map = 0;
        best_count = 0;
        printf("NMAR fallback: scanning %d back maps for best idle pose\n", back_nmcr.map_count);

        for (m = 0; m < back_nmcr.map_count; m++) {
            int count = Nmcr_CountValidRecords(&back_nmcr.maps[m], &back_nanr, &back_ncer, 0);
            printf("  back map %d: %d valid records\n", m, count);
            if (count > best_count) {
                best_count = count;
                best_map = m;
            }
        }

        back_idle_map = best_map;
        printf("  selected back map %d (%d valid records)\n", best_map, best_count);
    }

    if (front_nmcr.map_count > 0 &&
        Composer_RenderFrameRgba(&front_ncer, &front_nanr, &front_nmcr.maps[front_idle_map], &front_ncgr, &palette, 0, BW_TILE_STRIDE, "front idle", &front_idle, &front_idle_w, &front_idle_h, &front_coords) != 0) {
        front_idle = NULL;
    }

    if (back_nmcr.map_count > 0 &&
        Composer_RenderFrameRgba(&back_ncer, &back_nanr, &back_nmcr.maps[back_idle_map], &back_ncgr, &palette, 0, BW_TILE_STRIDE, "back idle", &back_idle, &back_idle_w, &back_idle_h, &back_coords) != 0) {
        back_idle = NULL;
    }

    if (front_idle != NULL) {
        Png_WriteRgbaImageScaled(out_front_idle_normal, front_idle, front_idle_w, front_idle_h, 4);
    }
    if (back_idle != NULL) {
        Png_WriteRgbaImageScaled(out_back_idle_normal, back_idle, back_idle_w, back_idle_h, 4);
    }

    int delay_cs = (gif_options != NULL && gif_options->enabled) ? gif_options->delay_cs : 5;
    int playback_delay_cs = (
        gif_options != NULL &&
        gif_options->enabled &&
        gif_options->playback_delay_cs > 0
    ) ? gif_options->playback_delay_cs : delay_cs;

    if (front_break_map >= 0 && front_nmcr.map_count > 0) {
        int idle_fc = Nmcr_MaxFrameCount(&front_nmcr.maps[front_idle_map], &front_nanr);
        int break_fc = Nmcr_MaxFrameCount(&front_nmcr.maps[front_break_map], &front_nanr);
        if (idle_fc > 512) idle_fc = 512;
        if (break_fc > 512) break_fc = 512;
        Composer_ComputeUnionBounds(
            &front_ncer, &front_nanr,
            &front_nmcr.maps[front_idle_map], idle_fc,
            &front_nmcr.maps[front_break_map], break_fc,
            &front_union_min_x, &front_union_min_y,
            &front_union_max_x, &front_union_max_y,
            &front_coords,
            delay_cs
        );
        have_front_union = 1;
    }

    if (back_break_map >= 0 && back_nmcr.map_count > 0) {
        int idle_fc = Nmcr_MaxFrameCount(&back_nmcr.maps[back_idle_map], &back_nanr);
        int break_fc = Nmcr_MaxFrameCount(&back_nmcr.maps[back_break_map], &back_nanr);
        if (idle_fc > 512) idle_fc = 512;
        if (break_fc > 512) break_fc = 512;
        Composer_ComputeUnionBounds(
            &back_ncer, &back_nanr,
            &back_nmcr.maps[back_idle_map], idle_fc,
            &back_nmcr.maps[back_break_map], break_fc,
            &back_union_min_x, &back_union_min_y,
            &back_union_max_x, &back_union_max_y,
            &back_coords,
            delay_cs
        );
        have_back_union = 1;
    }

    if (gif_options != NULL && gif_options->enabled &&
        (gif_options->palette == GIF_PALETTE_NORMAL ||
         gif_options->palette == GIF_PALETTE_BOTH)) {
        if ((gif_options->side == GIF_SIDE_FRONT ||
             gif_options->side == GIF_SIDE_BOTH) &&
            front_nmcr.map_count > 0) {
            GifPipeline_ExportIdle(
                out_dir,
                "front",
                "idle",
                "normal",
                &front_ncer,
                &front_nanr,
                &front_nmcr,
                &front_nmar,
                &front_ncgr,
                &palette,
                front_idle_map,
                gif_options,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                have_front_union ? &front_union_min_x : NULL,
                have_front_union ? &front_union_min_y : NULL,
                have_front_union ? &front_union_max_x : NULL,
                have_front_union ? &front_union_max_y : NULL,
                &front_coords
            );
        }

        if ((gif_options->side == GIF_SIDE_BACK ||
             gif_options->side == GIF_SIDE_BOTH) &&
            back_nmcr.map_count > 0) {
            GifPipeline_ExportIdle(
                out_dir,
                "back",
                "idle",
                "normal",
                &back_ncer,
                &back_nanr,
                &back_nmcr,
                &back_nmar,
                &back_ncgr,
                &palette,
                back_idle_map,
                gif_options,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                have_back_union ? &back_union_min_x : NULL,
                have_back_union ? &back_union_min_y : NULL,
                have_back_union ? &back_union_max_x : NULL,
                have_back_union ? &back_union_max_y : NULL,
                &back_coords
            );
        }

        if (export_idle_break_gifs && front_break_map >= 0 &&
            (gif_options->side == GIF_SIDE_FRONT ||
             gif_options->side == GIF_SIDE_BOTH) &&
            front_nmcr.map_count > 0) {
            GifPipeline_ExportIdle(
                out_dir,
                "front",
                "break",
                "normal",
                &front_ncer,
                &front_nanr,
                &front_nmcr,
                &front_nmar,
                &front_ncgr,
                &palette,
                front_break_map,
                gif_options,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                have_front_union ? &front_union_min_x : NULL,
                have_front_union ? &front_union_min_y : NULL,
                have_front_union ? &front_union_max_x : NULL,
                have_front_union ? &front_union_max_y : NULL,
                &front_coords
            );
        }

        if (export_idle_break_gifs && back_break_map >= 0 &&
            (gif_options->side == GIF_SIDE_BACK ||
             gif_options->side == GIF_SIDE_BOTH) &&
            back_nmcr.map_count > 0) {
            GifPipeline_ExportIdle(
                out_dir,
                "back",
                "break",
                "normal",
                &back_ncer,
                &back_nanr,
                &back_nmcr,
                &back_nmar,
                &back_ncgr,
                &palette,
                back_break_map,
                gif_options,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                have_back_union ? &back_union_min_x : NULL,
                have_back_union ? &back_union_min_y : NULL,
                have_back_union ? &back_union_max_x : NULL,
                have_back_union ? &back_union_max_y : NULL,
                &back_coords
            );
        }
    }

    if (File_ReadAll(shiny_nclr_path, &shiny_nclr_data, &shiny_nclr_size) == 0 &&
        Nclr_Parse(shiny_nclr_data, shiny_nclr_size, &shiny_palette) == 0) {
        RgbaColor *front_shiny = NULL; int fsw = 0, fsh = 0;
        RgbaColor *back_shiny = NULL; int bsw = 0, bsh = 0;
        (void)fsw; (void)fsh; (void)bsw; (void)bsh;

        if (front_nmcr.map_count > 0 &&
            Composer_RenderFrameRgba(&front_ncer, &front_nanr, &front_nmcr.maps[front_idle_map], &front_ncgr, &shiny_palette, 0, BW_TILE_STRIDE, NULL, &front_shiny, &fsw, &fsh, &front_coords) == 0) {
        }

        if (back_nmcr.map_count > 0 &&
            Composer_RenderFrameRgba(&back_ncer, &back_nanr, &back_nmcr.maps[back_idle_map], &back_ncgr, &shiny_palette, 0, BW_TILE_STRIDE, NULL, &back_shiny, &bsw, &bsh, &back_coords) == 0) {
        }

        if (front_shiny != NULL) {
            Png_WriteRgbaImageScaled(out_front_idle_shiny, front_shiny, fsw, fsh, 4);
        }
        if (back_shiny != NULL) {
            Png_WriteRgbaImageScaled(out_back_idle_shiny, back_shiny, bsw, bsh, 4);
        }

        if (gif_options != NULL && gif_options->enabled &&
            (gif_options->palette == GIF_PALETTE_SHINY ||
             gif_options->palette == GIF_PALETTE_BOTH)) {
            if ((gif_options->side == GIF_SIDE_FRONT ||
                 gif_options->side == GIF_SIDE_BOTH) &&
                front_nmcr.map_count > 0) {
                GifPipeline_ExportIdle(
                    out_dir,
                    "front",
                    "idle",
                     "shiny",
                     &front_ncer,
                      &front_nanr,
                      &front_nmcr,
                      &front_nmar,
                      &front_ncgr,
                     &shiny_palette,
                     front_idle_map,
                     gif_options,
                     BW_TILE_STRIDE,
                     COMPOSITE_MARGIN,
                     have_front_union ? &front_union_min_x : NULL,
                     have_front_union ? &front_union_min_y : NULL,
                     have_front_union ? &front_union_max_x : NULL,
                     have_front_union ? &front_union_max_y : NULL,
                     &front_coords
                 );
             }

             if ((gif_options->side == GIF_SIDE_BACK ||
                  gif_options->side == GIF_SIDE_BOTH) &&
                 back_nmcr.map_count > 0) {
                 GifPipeline_ExportIdle(
                     out_dir,
                     "back",
                     "idle",
                     "shiny",
                     &back_ncer,
                      &back_nanr,
                      &back_nmcr,
                      &back_nmar,
                      &back_ncgr,
                     &shiny_palette,
                     back_idle_map,
                     gif_options,
                     BW_TILE_STRIDE,
                     COMPOSITE_MARGIN,
                     have_back_union ? &back_union_min_x : NULL,
                     have_back_union ? &back_union_min_y : NULL,
                     have_back_union ? &back_union_max_x : NULL,
                     have_back_union ? &back_union_max_y : NULL,
                     &back_coords
                 );
            }

        if (export_idle_break_gifs && front_break_map >= 0 &&
                (gif_options->side == GIF_SIDE_FRONT ||
                 gif_options->side == GIF_SIDE_BOTH) &&
                front_nmcr.map_count > 0) {
                GifPipeline_ExportIdle(
                    out_dir,
                    "front",
                    "break",
                    "shiny",
                    &front_ncer,
                    &front_nanr,
                    &front_nmcr,
                    &front_nmar,
                    &front_ncgr,
                    &shiny_palette,
                    front_break_map,
                    gif_options,
                    BW_TILE_STRIDE,
                    COMPOSITE_MARGIN,
                    have_front_union ? &front_union_min_x : NULL,
                    have_front_union ? &front_union_min_y : NULL,
                    have_front_union ? &front_union_max_x : NULL,
                    have_front_union ? &front_union_max_y : NULL,
                    &front_coords
                );
            }

        if (export_idle_break_gifs && back_break_map >= 0 &&
                (gif_options->side == GIF_SIDE_BACK ||
                 gif_options->side == GIF_SIDE_BOTH) &&
                back_nmcr.map_count > 0) {
                GifPipeline_ExportIdle(
                    out_dir,
                    "back",
                    "break",
                    "shiny",
                    &back_ncer,
                    &back_nanr,
                    &back_nmcr,
                    &back_nmar,
                    &back_ncgr,
                    &shiny_palette,
                    back_break_map,
                    gif_options,
                    BW_TILE_STRIDE,
                    COMPOSITE_MARGIN,
                    have_back_union ? &back_union_min_x : NULL,
                    have_back_union ? &back_union_min_y : NULL,
                    have_back_union ? &back_union_max_x : NULL,
                    have_back_union ? &back_union_max_y : NULL,
                    &back_coords
                );
            }
        }

        free(front_shiny);
        free(back_shiny);
    }

    if (export_composed_gifs && front_break_map >= 0 &&
        gif_options != NULL && gif_options->enabled &&
        (gif_options->palette == GIF_PALETTE_NORMAL ||
         gif_options->palette == GIF_PALETTE_BOTH)) {
        int reps;
        int composed_frame_count;
        int composed_width;
        int composed_height;
        u8 *composed_frames;
        u8 *cropped;
        int cropped_w, cropped_h;
        char gif_path[PATH_BUFFER_SIZE];
        char gif_subdir[PATH_BUFFER_SIZE];

        reps = -1;

        if (RenderComposedAnimationSmart(
                &front_ncer, &front_nanr, &front_nmcr, &front_nmar,
                front_idle_map, front_break_map, -1,
                &front_ncgr, &palette,
                reps,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                &composed_frames,
                &composed_frame_count,
                &composed_width,
                &composed_height,
                &front_coords,
                gif_options->delay_cs
            ) == 0) {

            cropped = NULL;
            cropped_w = 0;
            cropped_h = 0;
            if (Composer_CropIndexedFrames(
                    composed_frames, composed_frame_count,
                    composed_width, composed_height,
                    &cropped, &cropped_w, &cropped_h
                ) == 0) {

                snprintf(gif_subdir, sizeof(gif_subdir), "%s/%s", out_dir, ANIMA_DIR_COMPOSED_GIF);
                File_MkdirRecursive(gif_subdir);
                snprintf(gif_path, sizeof(gif_path),
                         "%s/front_composed_%s_%s.gif",
                         gif_subdir,
                         gif_options->eye_mode == GIF_EYE_ALL ? "all" : "open",
                         "normal");

                Gif_WriteIndexed(
                    gif_path,
                    cropped, composed_frame_count,
                    cropped_w, cropped_h,
                    palette.colors, palette.color_count,
                    0, playback_delay_cs,
                    gif_options->loop_count,
                    gif_options->scale
                );
                printf("Generated composed GIF:\n  %s\n", gif_path);
                free(cropped);
            }
            free(composed_frames);
        }
    }

    if (export_composed_gifs && back_break_map >= 0 &&
        gif_options != NULL && gif_options->enabled &&
        (gif_options->side == GIF_SIDE_BACK ||
         gif_options->side == GIF_SIDE_BOTH) &&
        (gif_options->palette == GIF_PALETTE_NORMAL ||
         gif_options->palette == GIF_PALETTE_BOTH)) {
        int reps;
        int composed_frame_count;
        int composed_width;
        int composed_height;
        u8 *composed_frames;
        u8 *cropped;
        int cropped_w, cropped_h;
        char gif_path[PATH_BUFFER_SIZE];
        char gif_subdir[PATH_BUFFER_SIZE];

        reps = -1;

        if (RenderComposedAnimationSmart(
                &back_ncer, &back_nanr, &back_nmcr, &back_nmar,
                back_idle_map, back_break_map, -1,
                &back_ncgr, &palette,
                reps,
                BW_TILE_STRIDE,
                COMPOSITE_MARGIN,
                &composed_frames,
                &composed_frame_count,
                &composed_width,
                &composed_height,
                &back_coords,
                gif_options->delay_cs
            ) == 0) {

            cropped = NULL;
            cropped_w = 0;
            cropped_h = 0;
            if (Composer_CropIndexedFrames(
                    composed_frames, composed_frame_count,
                    composed_width, composed_height,
                    &cropped, &cropped_w, &cropped_h
                ) == 0) {

                snprintf(gif_subdir, sizeof(gif_subdir), "%s/%s", out_dir, ANIMA_DIR_COMPOSED_GIF);
                File_MkdirRecursive(gif_subdir);
                snprintf(gif_path, sizeof(gif_path),
                         "%s/back_composed_%s_%s.gif",
                         gif_subdir,
                         gif_options->eye_mode == GIF_EYE_ALL ? "all" : "open",
                         "normal");

                Gif_WriteIndexed(
                    gif_path,
                    cropped, composed_frame_count,
                    cropped_w, cropped_h,
                    palette.colors, palette.color_count,
                    0, playback_delay_cs,
                    gif_options->loop_count,
                    gif_options->scale
                );
                printf("Generated composed GIF:\n  %s\n", gif_path);
                free(cropped);
            }
            free(composed_frames);
        }
    }


    success = 0;

cleanup:
    free(front_idle);
    free(back_idle);
    free(combined_static);


    Ncgr_Free(&front_ncgr);
    Nanr_Free(&front_nanr);
    Nmcr_Free(&front_nmcr);
    Nmar_Free(&front_nmar);
    Ncer_Free(&front_ncer);
    free(front_ncgr_data);
    free(front_ncer_data);
    free(front_nanr_data);
    free(front_nmcr_data);
    free(front_nmar_data);

    Ncgr_Free(&back_ncgr);
    Nanr_Free(&back_nanr);
    Nmcr_Free(&back_nmcr);
    Nmar_Free(&back_nmar);
    Ncer_Free(&back_ncer);
    free(back_ncgr_data);
    free(back_ncer_data);
    free(back_nanr_data);
    free(back_nmcr_data);
    free(back_nmar_data);

    free(nclr_data);
    free(shiny_nclr_data);
    Coord_Free(&front_coords);
    Coord_Free(&back_coords);
    free(front_coords_data);
    free(back_coords_data);
    return success;
}

void AnimaExtractOptions_Init(AnimaExtractOptions *options)
{
    if (options == NULL) {
        return;
    }

    memset(options, 0, sizeof(*options));
    options->export_spritesheets = 1;
    options->export_static_previews = 1;
    options->export_gifs = 1;
    options->export_idle_break_gifs = 0;
    options->export_composed_gifs = 0;
    options->export_json = 1;
    options->form_index = 0;
    GifExportOptions_Init(&options->gif_options);
}

int AnimaBackend_BuildFrontNormalGifPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
)
{
    if (out_dir == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s/%s/front_idle_open_normal.gif",
        out_dir,
        ANIMA_DIR_IDLE_GIF
    );
    return 0;
}

int AnimaBackend_BuildFrontNormalIdleBreakGifPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
)
{
    if (out_dir == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s/%s/front_break_open_normal.gif",
        out_dir,
        ANIMA_DIR_BREAK_GIF
    );
    return 0;
}

int AnimaBackend_BuildFrontNormalStaticPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
)
{
    if (out_dir == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s/%s/front_idle_normal.png",
        out_dir,
        ANIMA_DIR_STATIC
    );
    return 0;
}

static void RemoveChildDirectory(const char *out_dir, const char *child)
{
    char path[PATH_BUFFER_SIZE];

    if (out_dir == NULL || child == NULL) {
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", out_dir, child);
    File_RemoveRecursive(path);
}

static int OptionsRequestOnlyDsFiles(const AnimaExtractOptions *options)
{
    return options != NULL &&
        !options->export_spritesheets &&
        !options->export_static_previews &&
        !options->export_gifs &&
        !options->export_idle_break_gifs &&
        !options->export_composed_gifs;
}

static int ResolveIdleMapIndex(
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer,
    const NmarFile *nmar
)
{
    int idle_map;

    idle_map = 0;
    if (nmar != NULL && nmar->entry_count > 0) {
        idle_map = Nmar_GetIdleMapIndex(nmar);
        if (idle_map < 0) {
            idle_map = 0;
        }
    }

    if (nmcr == NULL || nmcr->map_count <= 0) {
        return -1;
    }

    if (idle_map >= nmcr->map_count) {
        idle_map = 0;
    }

    if ((nmar == NULL || nmar->entry_count == 0) && nmcr->map_count > 1) {
        int best_map;
        int best_count;
        int i;

        best_map = 0;
        best_count = 0;
        for (i = 0; i < nmcr->map_count; i++) {
            int count;

            count = Nmcr_CountValidRecords(&nmcr->maps[i], nanr, ncer, 0);
            if (count > best_count) {
                best_count = count;
                best_map = i;
            }
        }

        idle_map = best_map;
    }

    return idle_map;
}

static int ResolveBreakMapValidated(
    const NmarFile *nmar,
    int idle_map,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer
)
{
    int nmcr_candidate;
    int best_map;
    int best_score;
    int scores[256];
    int i;

    if (nmcr == NULL || nmcr->map_count <= 1 || idle_map < 0) {
        return -1;
    }

    if (nmcr->map_count > 256) {
        return -1;
    }

    /* 1. Get the NMAR label-based candidate */
    nmcr_candidate = Nmar_GetIdleBreakMapIndex(nmar, idle_map, nmcr->map_count);

    /* 2. Score every non-idle map */
    best_map = -1;
    best_score = 0;

    for (i = 0; i < nmcr->map_count; i++) {
        if (i == idle_map) {
            scores[i] = -1;
            continue;
        }

        scores[i] = Nmcr_ComputeBreakScore(
            &nmcr->maps[idle_map],
            &nmcr->maps[i],
            nanr,
            ncer
        );

        printf("  break candidate map %d: score=%d\n", i, scores[i]);

        if (scores[i] > best_score) {
            best_score = scores[i];
            best_map = i;
        }
    }

    /* 3. If the NMAR candidate is valid, check whether it came from a
     *    strong explicit label.  "wait", "break", and "special" describe
     *    actual idle-break animations in BW data and should win over
     *    structural scoring.  "stop" is weaker: several species use it for
     *    an idle-like terminal pose, so keep it behind score validation. */
    if (nmcr_candidate >= 0 && nmcr_candidate < nmcr->map_count &&
        nmcr_candidate != idle_map &&
        scores[nmcr_candidate] > 0) {

        int has_strong_explicit_label = 0;
        if (nmar != NULL && nmar->entry_count > 0 && nmar->entries != NULL) {
            int k;
            for (k = 0; k < nmar->entry_count; k++) {
                if (nmar->entries[k].map_index == nmcr_candidate) {
                    const char *label = nmar->entries[k].label;
                    if (strcmp(label, "wait") == 0 ||
                        strcmp(label, "break") == 0 ||
                        strcmp(label, "special") == 0) {
                        has_strong_explicit_label = 1;
                        break;
                    }
                }
            }
        }

        if (has_strong_explicit_label) {
            printf("Break map: strong NMAR label candidate %d (score=%d)\n",
                   nmcr_candidate, scores[nmcr_candidate]);
            return nmcr_candidate;
        }

        if (best_map >= 0 && best_score > 0) {
            int ratio = (scores[nmcr_candidate] * 100) / best_score;

            if (ratio >= 80) {
                printf("Break map: NMAR candidate %d (score=%d, %d%% of best=%d)\n",
                       nmcr_candidate, scores[nmcr_candidate], ratio, best_score);
                return nmcr_candidate;
            }

            printf("Break map: NMAR candidate %d overridden (score=%d, only %d%% of best=%d)\n",
                   nmcr_candidate, scores[nmcr_candidate], ratio, best_score);
        } else {
            printf("Break map: NMAR candidate %d (score=%d, only candidate)\n",
                   nmcr_candidate, scores[nmcr_candidate]);
            return nmcr_candidate;
        }
    }

    /* 4. Return the highest-scoring non-idle map */
    if (best_map >= 0 && best_score >= 2) {
        printf("Break map selected by scoring: map %d (score=%d)\n",
               best_map, best_score);
        return best_map;
    }

    /* 5. No valid break found */
    printf("No valid break map found (best score=%d)\n", best_score);
    return -1;
}

typedef struct PreviewResourceSet {
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;
} PreviewResourceSet;

static void PreviewResources_Free(PreviewResourceSet *res)
{
    if (res == NULL) return;

    Ncgr_Free(&res->ncgr);
    Ncer_Free(&res->ncer);
    Nanr_Free(&res->nanr);
    Nmcr_Free(&res->nmcr);
    Nmar_Free(&res->nmar);
    Coord_Free(&res->coords);
    free(res->ncgr_data);
    free(res->ncer_data);
    free(res->nanr_data);
    free(res->nmcr_data);
    free(res->nmar_data);
    free(res->nclr_data);
    free(res->coords_data);
    free(res->rom);
    memset(res, 0, sizeof(*res));
}

static int PreviewResources_Load(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    PreviewResourceSet *res
)
{
    if (rom_path == NULL || species < 0 || res == NULL) {
        return -1;
    }

    memset(res, 0, sizeof(*res));

    if (InitPokegraFromRom(rom_path, &res->rom, &res->rom_size, &res->pokegra) != 0) {
        goto fail;
    }

    if (ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_SHEET, opts, &res->ncgr_data, &res->ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_CELLS, opts, &res->ncer_data, &res->ncer_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_ANIM, opts, &res->nanr_data, &res->nanr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_MAP, opts, &res->nmcr_data, &res->nmcr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_PALETTE_NORMAL, opts, &res->nclr_data, &res->nclr_size) != 0) {
        goto fail;
    }

    if (Ncgr_Parse(res->ncgr_data, res->ncgr_size, &res->ncgr) != 0 ||
        Ncer_Parse(res->ncer_data, res->ncer_size, &res->ncer) != 0 ||
        Nanr_Parse(res->nanr_data, res->nanr_size, &res->nanr) != 0 ||
        Nmcr_Parse(res->nmcr_data, res->nmcr_size, &res->nmcr) != 0 ||
        Nclr_Parse(res->nclr_data, res->nclr_size, &res->palette) != 0) {
        goto fail;
    }

    if (ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_TIMING, opts, &res->nmar_data, &res->nmar_size) == 0) {
        if (Nmar_Parse(res->nmar_data, res->nmar_size, &res->nmar) != 0) {
            Nmar_Free(&res->nmar);
            memset(&res->nmar, 0, sizeof(res->nmar));
        }
    }

    if (ExtractDecodedPokemonRoleExt(&res->pokegra, species, ROLE_FRONT_COORDS, opts, &res->coords_data, &res->coords_size) == 0) {
        if (Coord_Parse(res->coords_data, res->coords_size, &res->coords) != 0) {
            memset(&res->coords, 0, sizeof(res->coords));
        }
    }

    return 0;

fail:
    PreviewResources_Free(res);
    return -1;
}

static void ComposerTransformFromNmarFrame(
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

static int NmcrMapIsUsable(
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer,
    int map_index
)
{
    if (nmcr == NULL || nanr == NULL || ncer == NULL ||
        map_index < 0 || map_index >= nmcr->map_count) {
        return 0;
    }

    return Nmcr_CountValidRecords(&nmcr->maps[map_index], nanr, ncer, 0) > 0;
}

static int NmarAnimationFirstMap(
    const NmarFile *nmar,
    int animation_index
)
{
    if (nmar == NULL || nmar->animations == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count ||
        nmar->animations[animation_index].frame_count <= 0 ||
        nmar->animations[animation_index].frames == NULL) {
        return -1;
    }

    return nmar->animations[animation_index].frames[0].map_index;
}

static int NmarAnimationIsUsable(
    const NmarFile *nmar,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer,
    int animation_index
)
{
    const NmarAnimation *animation;
    int i;

    if (nmar == NULL || nmar->animations == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count) {
        return 0;
    }

    animation = &nmar->animations[animation_index];
    if (animation->frame_count <= 0 || animation->frames == NULL) {
        return 0;
    }

    for (i = 0; i < animation->frame_count; i++) {
        int map_index = animation->frames[i].map_index;
        if (!NmcrMapIsUsable(nmcr, nanr, ncer, map_index)) {
            return 0;
        }
    }

    return 1;
}

static int NmarAnimationReferencesMap(
    const NmarFile *nmar,
    int map_index
)
{
    int i;

    if (nmar == NULL || nmar->animations == NULL) {
        return 0;
    }

    for (i = 0; i < nmar->animation_count; i++) {
        if (NmarAnimationFirstMap(nmar, i) == map_index) {
            return 1;
        }
    }

    return 0;
}

static int NmarAnimationContainsMapOtherThan(
    const NmarFile *nmar,
    int animation_index,
    int map_index
)
{
    const NmarAnimation *animation;
    int i;

    if (nmar == NULL || nmar->animations == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count) {
        return 0;
    }

    animation = &nmar->animations[animation_index];
    if (animation->frame_count <= 0 || animation->frames == NULL) {
        return 0;
    }

    for (i = 0; i < animation->frame_count; i++) {
        if (animation->frames[i].map_index != map_index) {
            return 1;
        }
    }

    return 0;
}

static int NmarAnimationFrameCountForPreview(
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
    if (frame_count > 512) frame_count = 512;
    return frame_count;
}

static int FindIdleNmarAnimationIndex(const NmarFile *nmar)
{
    int i;

    if (nmar == NULL || nmar->animation_count <= 0 || nmar->animations == NULL) {
        return -1;
    }

    for (i = 0; i < nmar->animation_count; i++) {
        if (strcmp(nmar->animations[i].label, "stay") == 0) {
            return i;
        }
    }

    return 0;
}

static int IsStrongBreakLabel(const char *label)
{
    return label != NULL &&
        (strcmp(label, "wait") == 0 ||
         strcmp(label, "break") == 0 ||
         strcmp(label, "special") == 0);
}

static int FindBreakNmarAnimationIndex(
    const NmarFile *nmar,
    int idle_map,
    int break_map,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    const NcerFile *ncer
)
{
    int i;
    int best_stop = -1;
    int best_stop_score = 0;
    int best_score = 0;
    int idle_animation;

    if (nmar == NULL || nmar->animation_count <= 0 || nmar->animations == NULL ||
        nmcr == NULL || break_map < 0 || break_map >= nmcr->map_count ||
        idle_map < 0 || idle_map >= nmcr->map_count) {
        return -1;
    }

    best_score = Nmcr_ComputeBreakScore(&nmcr->maps[idle_map], &nmcr->maps[break_map], nanr, ncer);
    idle_animation = FindIdleNmarAnimationIndex(nmar);

    for (i = 0; i < nmar->animation_count; i++) {
        int map_index;
        int score;
        const char *label;
        const NmarAnimation *animation;
        int j;

        if (!NmarAnimationIsUsable(nmar, nmcr, nanr, ncer, i)) {
            continue;
        }
        if (i == idle_animation) {
            continue;
        }

        animation = &nmar->animations[i];
        label = animation->label;
        if (strcmp(label, "stay") == 0) {
            continue;
        }

        map_index = -1;
        score = 0;

        for (j = 0; j < animation->frame_count; j++) {
            int frame_map;
            int frame_score;

            frame_map = animation->frames[j].map_index;
            if (frame_map == idle_map || frame_map < 0 || frame_map >= nmcr->map_count) {
                continue;
            }

            frame_score = Nmcr_ComputeBreakScore(&nmcr->maps[idle_map], &nmcr->maps[frame_map], nanr, ncer);
            if (frame_map == break_map) {
                map_index = frame_map;
                score = frame_score;
                break;
            }
            if (frame_score > score) {
                map_index = frame_map;
                score = frame_score;
            }
        }

        if (map_index < 0 || map_index >= nmcr->map_count) {
            continue;
        }

        if (map_index == break_map && IsStrongBreakLabel(label)) {
            return i;
        }

        if (IsStrongBreakLabel(label) && score > 0) {
            return i;
        }

        if (map_index == break_map) {
            return i;
        }

        if (strcmp(label, "stop") == 0 && score > best_stop_score) {
            best_stop = i;
            best_stop_score = score;
        }
    }

    if (best_stop >= 0 && best_score > 0 &&
        (best_stop_score * 100) / best_score >= 80) {
        return best_stop;
    }

    return -1;
}

static int RenderComposedAnimationSmart(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    int idle_map,
    int break_map,
    int preferred_break_animation_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
)
{
    int idle_anim;
    int break_anim;

    if (nmcr == NULL || idle_map < 0 || idle_map >= nmcr->map_count ||
        break_map < 0 || break_map >= nmcr->map_count) {
        return -1;
    }

    idle_anim = FindIdleNmarAnimationIndex(nmar);
    if (!NmarAnimationIsUsable(nmar, nmcr, nanr, ncer, idle_anim)) {
        idle_anim = -1;
    }

    break_anim = -1;
    if (preferred_break_animation_index >= 0 &&
        NmarAnimationIsUsable(nmar, nmcr, nanr, ncer, preferred_break_animation_index)) {
        break_anim = preferred_break_animation_index;
    }
    if (break_anim < 0) {
        break_anim = FindBreakNmarAnimationIndex(nmar, idle_map, break_map, nmcr, nanr, ncer);
        if (!NmarAnimationIsUsable(nmar, nmcr, nanr, ncer, break_anim)) {
            break_anim = -1;
        }
    }

    if (idle_repetitions <= 0) {
        if (idle_anim >= 0) {
            idle_repetitions = Composer_ComputeNmarIdleRepetitions(nmar, idle_anim);
        } else {
            idle_repetitions = Composer_ComputeIdleRepetitions(nanr, &nmcr->maps[idle_map]);
        }
    }

    if (Composer_RenderComposedAnimationTimeline(
            ncer,
            nanr,
            nmcr,
            nmar,
            &nmcr->maps[idle_map],
            &nmcr->maps[break_map],
            idle_anim,
            break_anim,
            ncgr,
            palette,
            idle_repetitions,
            tile_stride,
            margin,
            out_frames,
            out_frame_count,
            out_width,
            out_height,
            coords,
            delay_cs
        ) == 0) {
        return 0;
    }

    return Composer_RenderComposedAnimation(
        ncer,
        nanr,
        &nmcr->maps[idle_map],
        &nmcr->maps[break_map],
        ncgr,
        palette,
        idle_repetitions,
        tile_stride,
        margin,
        out_frames,
        out_frame_count,
        out_width,
        out_height,
        coords,
        delay_cs
    );
}

static int CopyIndexedFramesToPreview(
    const u8 *indices,
    int frame_count,
    int width,
    int height,
    const NclrPalette *palette,
    int delay_cs,
    AnimaIdlePreview *out_preview
)
{
    size_t pixel_count;
    size_t i;
    unsigned char *rgba;

    if (indices == NULL || palette == NULL || out_preview == NULL ||
        frame_count <= 0 || width <= 0 || height <= 0) {
        return -1;
    }

    pixel_count = (size_t)frame_count * (size_t)width * (size_t)height;
    if (pixel_count == 0) {
        return -1;
    }

    rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        return -1;
    }

    for (i = 0; i < pixel_count; i++) {
        RgbaColor color;
        int color_index;

        color_index = indices[i];
        if (color_index < 0 || color_index >= palette->color_count) {
            color_index = 0;
        }

        color = palette->colors[color_index];
        rgba[i * 4 + 0] = color.r;
        rgba[i * 4 + 1] = color.g;
        rgba[i * 4 + 2] = color.b;
        rgba[i * 4 + 3] = color.a;
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = delay_cs;
    out_preview->rgba = rgba;
    return 0;
}

int AnimaBackend_LoadIdlePreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t pixel_count;
    size_t frame_pixels;
    u8 *indices;
    unsigned char *rgba;
    int frame_index;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_SHEET, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_CELLS, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_ANIM, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_MAP, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_PALETTE_NORMAL, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_TIMING, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_COORDS, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
    if (frame_count <= 0) {
        frame_count = 1;
    }
    if (frame_count > 512) {
        frame_count = 512;
    }

    Composer_ComputeBoundsRange(
        &ncer,
        &nanr,
        &nmcr.maps[idle_map],
        0,
        frame_count,
        &min_x, &min_y, &max_x, &max_y,
        &coords,
        5
    );

    // Force symmetric bounds on the X-axis so the horizontal pivot is perfectly centered
    {
        int abs_x = abs(min_x) > abs(max_x) ? abs(min_x) : abs(max_x);
        min_x = -abs_x;
        max_x = abs_x;
    }

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    rgba = malloc(pixel_count * 4u);
    if (indices == NULL || rgba == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        size_t i;
        int tick = (frame_index * 5 * 60) / 100;

        frame_offset = (size_t)frame_index * frame_pixels;
        if (Composer_RenderFrameIndexed(
                &ncer,
                &nanr,
                &nmcr.maps[idle_map],
                &ncgr,
                &palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &coords
            ) != 0) {
            goto cleanup;
        }

        for (i = 0; i < frame_pixels; i++) {
            RgbaColor color;
            int color_index;
            size_t rgba_offset;

            color_index = indices[frame_offset + i];
            if (color_index < 0 || color_index >= palette.color_count) {
                color_index = 0;
            }

            color = palette.colors[color_index];
            rgba_offset = (frame_offset + i) * 4u;
            rgba[rgba_offset + 0] = color.r;
            rgba[rgba_offset + 1] = color.g;
            rgba[rgba_offset + 2] = color.b;
            rgba[rgba_offset + 3] = color.a;
        }
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadSpritesheetPreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *nclr_data; size_t nclr_size;

    NcgrImage ncgr;
    NclrPalette palette;

    RgbaColor *pixels;
    int width;
    int height;
    size_t pixel_count;
    unsigned char *rgba;
    size_t i;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    nclr_data = NULL; nclr_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&palette, 0, sizeof(palette));
    pixels = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_SHEET, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_PALETTE_NORMAL, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (Ncgr_RenderTilesToImage(&ncgr, &palette, 0, &pixels, &width, &height) != 0) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        goto cleanup;
    }

    for (i = 0; i < pixel_count; i++) {
        rgba[i * 4 + 0] = pixels[i].r;
        rgba[i * 4 + 1] = pixels[i].g;
        rgba[i * 4 + 2] = pixels[i].b;
        rgba[i * 4 + 3] = pixels[i].a;
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = 1;
    out_preview->delay_cs = 0;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(pixels);
    free(rgba);
    Ncgr_Free(&ncgr);
    free(ncgr_data);
    free(nclr_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadIdleBreakPreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int break_map;
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t pixel_count;
    size_t frame_pixels;
    u8 *indices;
    unsigned char *rgba;
    int frame_index;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_SHEET, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_CELLS, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_ANIM, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_MAP, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_PALETTE_NORMAL, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_TIMING, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_COORDS, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    break_map = ResolveBreakMapValidated(&nmar, idle_map, &nmcr, &nanr, &ncer);
    if (break_map < 0 || break_map >= nmcr.map_count) {
        goto cleanup;
    }

    frame_count = Nmcr_MaxFrameCount(&nmcr.maps[break_map], &nanr);
    {
        int idle_frame_count;
        idle_frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
        if (frame_count <= 0) frame_count = 1;
        if (frame_count > 512) frame_count = 512;
        if (idle_frame_count > 512) idle_frame_count = 512;

        Composer_ComputeUnionBounds(
            &ncer,
            &nanr,
            &nmcr.maps[idle_map], idle_frame_count,
            &nmcr.maps[break_map], frame_count,
            &min_x, &min_y, &max_x, &max_y,
            &coords,
            5
        );
    }

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    rgba = malloc(pixel_count * 4u);
    if (indices == NULL || rgba == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        size_t i;
        int tick = (frame_index * 5 * 60) / 100;

        frame_offset = (size_t)frame_index * frame_pixels;
        if (Composer_RenderFrameIndexed(
                &ncer,
                &nanr,
                &nmcr.maps[break_map],
                &ncgr,
                &palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &coords
            ) != 0) {
            goto cleanup;
        }

        for (i = 0; i < frame_pixels; i++) {
            RgbaColor color;
            int color_index;
            size_t rgba_offset;

            color_index = indices[frame_offset + i];
            if (color_index < 0 || color_index >= palette.color_count) {
                color_index = 0;
            }

            color = palette.colors[color_index];
            rgba_offset = (frame_offset + i) * 4u;
            rgba[rgba_offset + 0] = color.r;
            rgba[rgba_offset + 1] = color.g;
            rgba[rgba_offset + 2] = color.b;
            rgba[rgba_offset + 3] = color.a;
        }
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadComposedPreview(
    const char *rom_path,
    int species,
    int idle_repetitions,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int break_map;
    int frame_count;
    int width;
    int height;
    size_t pixel_count;
    u8 *indices;
    unsigned char *rgba;
    size_t i;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_SHEET, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_CELLS, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_ANIM, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_MAP, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRole(&pokegra, species, ROLE_PALETTE_NORMAL, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_TIMING, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRole(&pokegra, species, ROLE_FRONT_COORDS, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    break_map = ResolveBreakMapValidated(&nmar, idle_map, &nmcr, &nanr, &ncer);
    if (break_map < 0 || break_map >= nmcr.map_count) {
        goto cleanup;
    }

    if (RenderComposedAnimationSmart(
            &ncer, &nanr, &nmcr, &nmar,
            idle_map, break_map, -1,
            &ncgr, &palette,
            idle_repetitions,
            BW_TILE_STRIDE,
            COMPOSITE_MARGIN,
            &indices,
            &frame_count,
            &width,
            &height,
            &coords,
            5
        ) != 0) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height * (size_t)frame_count;
    rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        goto cleanup;
    }

    for (i = 0; i < pixel_count; i++) {
        RgbaColor color;
        int color_index;

        color_index = indices[i];
        if (color_index < 0 || color_index >= palette.color_count) {
            color_index = 0;
        }

        color = palette.colors[color_index];
        rgba[i * 4 + 0] = color.r;
        rgba[i * 4 + 1] = color.g;
        rgba[i * 4 + 2] = color.b;
        rgba[i * 4 + 3] = color.a;
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

void AnimaIdlePreview_Free(AnimaIdlePreview *preview)
{
    if (preview == NULL) {
        return;
    }

    free(preview->rgba);
    memset(preview, 0, sizeof(*preview));
}

static int RenderNmcrMapPreviewFromResources(
    const PreviewResourceSet *res,
    int map_index,
    AnimaIdlePreview *out_preview
)
{
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t frame_pixels;
    size_t pixel_count;
    u8 *indices;
    int frame_index;
    int success;

    if (res == NULL || out_preview == NULL ||
        !NmcrMapIsUsable(&res->nmcr, &res->nanr, &res->ncer, map_index)) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    indices = NULL;
    success = -1;

    frame_count = Nmcr_MaxFrameCount(&res->nmcr.maps[map_index], &res->nanr);
    if (frame_count <= 0) frame_count = 1;
    if (frame_count > 512) frame_count = 512;

    Composer_ComputeBoundsRange(
        &res->ncer,
        &res->nanr,
        &res->nmcr.maps[map_index],
        0,
        frame_count,
        &min_x, &min_y, &max_x, &max_y,
        &res->coords,
        5
    );

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    if (indices == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        int tick;

        frame_offset = (size_t)frame_index * frame_pixels;
        tick = (frame_index * 5 * 60) / 100;

        if (Composer_RenderFrameIndexed(
                &res->ncer,
                &res->nanr,
                &res->nmcr.maps[map_index],
                &res->ncgr,
                &res->palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &res->coords
            ) != 0) {
            goto cleanup;
        }
    }

    if (CopyIndexedFramesToPreview(indices, frame_count, width, height, &res->palette, 5, out_preview) != 0) {
        goto cleanup;
    }

    success = 0;

cleanup:
    free(indices);
    return success;
}

static int RenderNmarAnimationPreviewFromResources(
    const PreviewResourceSet *res,
    int animation_index,
    AnimaIdlePreview *out_preview
)
{
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t frame_pixels;
    size_t pixel_count;
    u8 *indices;
    int frame_index;
    int success;

    if (res == NULL || out_preview == NULL ||
        !NmarAnimationIsUsable(&res->nmar, &res->nmcr, &res->nanr, &res->ncer, animation_index)) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    indices = NULL;
    success = -1;

    frame_count = NmarAnimationFrameCountForPreview(&res->nmar, animation_index, 5);
    if (frame_count <= 0) {
        goto cleanup;
    }

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        int tick;
        int map_index;
        int frame_min_x;
        int frame_min_y;
        int frame_max_x;
        int frame_max_y;
        NmarFrame nmar_frame;
        ComposerTransform parent_transform;

        tick = (frame_index * 5 * 60) / 100;
        map_index = Nmar_GetFrameAtTick(&res->nmar, animation_index, tick, &nmar_frame);
        if (map_index < 0 || map_index >= res->nmcr.map_count) {
            goto cleanup;
        }

        ComposerTransformFromNmarFrame(&nmar_frame, &parent_transform);
        Composer_ComputeBoundsWithTransform(
            &res->ncer,
            &res->nanr,
            &res->nmcr.maps[map_index],
            tick,
            &frame_min_x,
            &frame_min_y,
            &frame_max_x,
            &frame_max_y,
            &res->coords,
            &parent_transform
        );

        if (frame_min_x < min_x) min_x = frame_min_x;
        if (frame_min_y < min_y) min_y = frame_min_y;
        if (frame_max_x > max_x) max_x = frame_max_x;
        if (frame_max_y > max_y) max_y = frame_max_y;
    }

    if (min_x == 999999) {
        goto cleanup;
    }

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    if (indices == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        int tick;
        int map_index;
        NmarFrame nmar_frame;
        ComposerTransform parent_transform;

        frame_offset = (size_t)frame_index * frame_pixels;
        tick = (frame_index * 5 * 60) / 100;
        map_index = Nmar_GetFrameAtTick(&res->nmar, animation_index, tick, &nmar_frame);
        if (map_index < 0 || map_index >= res->nmcr.map_count) {
            goto cleanup;
        }

        ComposerTransformFromNmarFrame(&nmar_frame, &parent_transform);
        if (Composer_RenderFrameIndexedWithTransform(
                &res->ncer,
                &res->nanr,
                &res->nmcr.maps[map_index],
                &res->ncgr,
                &res->palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &res->coords,
                &parent_transform
            ) != 0) {
            goto cleanup;
        }
    }

    if (CopyIndexedFramesToPreview(indices, frame_count, width, height, &res->palette, 5, out_preview) != 0) {
        goto cleanup;
    }

    success = 0;

cleanup:
    free(indices);
    return success;
}

static int WritePreviewGifFromPalette(
    const char *out_path,
    const AnimaIdlePreview *preview,
    const NclrPalette *palette
)
{
    size_t pixel_count;
    size_t i;
    u8 *indices;
    int result;

    if (out_path == NULL || preview == NULL || preview->rgba == NULL ||
        palette == NULL || preview->width <= 0 || preview->height <= 0 ||
        preview->frame_count <= 0) {
        return -1;
    }

    pixel_count = (size_t)preview->width * (size_t)preview->height * (size_t)preview->frame_count;
    indices = malloc(pixel_count);
    if (indices == NULL) {
        return -1;
    }

    for (i = 0; i < pixel_count; i++) {
        int c;
        int found;
        unsigned char r = preview->rgba[i * 4 + 0];
        unsigned char g = preview->rgba[i * 4 + 1];
        unsigned char b = preview->rgba[i * 4 + 2];
        unsigned char a = preview->rgba[i * 4 + 3];

        found = 0;
        for (c = 0; c < palette->color_count; c++) {
            if (palette->colors[c].r == r &&
                palette->colors[c].g == g &&
                palette->colors[c].b == b &&
                palette->colors[c].a == a) {
                found = c;
                break;
            }
        }
        indices[i] = (u8)found;
    }

    result = Gif_WriteIndexed(
        out_path,
        indices,
        preview->frame_count,
        preview->width,
        preview->height,
        palette->colors,
        palette->color_count,
        0,
        preview->delay_cs > 0 ? preview->delay_cs : 5,
        0,
        4
    );

    free(indices);
    return result;
}

int AnimaBackend_LoadNmarAnimationPreviewExt(
    const char *rom_path,
    int species,
    int animation_index,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    PreviewResourceSet res;
    int success;

    if (out_preview == NULL) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    if (PreviewResources_Load(rom_path, species, opts, &res) != 0) {
        return -1;
    }

    success = RenderNmarAnimationPreviewFromResources(&res, animation_index, out_preview);
    PreviewResources_Free(&res);
    return success;
}

int AnimaBackend_LoadNmcrMapPreviewExt(
    const char *rom_path,
    int species,
    int map_index,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    PreviewResourceSet res;
    int success;

    if (out_preview == NULL) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    if (PreviewResources_Load(rom_path, species, opts, &res) != 0) {
        return -1;
    }

    success = RenderNmcrMapPreviewFromResources(&res, map_index, out_preview);
    PreviewResources_Free(&res);
    return success;
}

static void FillAssetText(
    AnimaPreviewAssetInfo *asset,
    AnimaPreviewAssetType type,
    int animation_index,
    int map_index,
    int frame_count,
    const char *label,
    const char *display_name
)
{
    if (asset == NULL) return;

    memset(asset, 0, sizeof(*asset));
    asset->type = type;
    asset->animation_index = animation_index;
    asset->map_index = map_index;
    asset->frame_count = frame_count;
    snprintf(asset->label, sizeof(asset->label), "%s", label != NULL ? label : "");
    snprintf(asset->display_name, sizeof(asset->display_name), "%s", display_name != NULL ? display_name : "");
}

static int AddAsset(
    AnimaPreviewAssetInfo *assets,
    int max_assets,
    int *count,
    AnimaPreviewAssetType type,
    int animation_index,
    int map_index,
    int frame_count,
    const char *label,
    const char *display_name
)
{
    if (assets == NULL || count == NULL || *count >= max_assets) {
        return -1;
    }

    FillAssetText(
        &assets[*count],
        type,
        animation_index,
        map_index,
        frame_count,
        label,
        display_name
    );
    (*count)++;
    return 0;
}

int AnimaBackend_ListPreviewAssets(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaPreviewAssetInfo *out_assets,
    int max_assets,
    int *out_count
)
{
    PreviewResourceSet res;
    int count;
    int idle_map;
    int break_map;
    int idle_anim;
    int break_anim;
    int i;

    if (out_count != NULL) {
        *out_count = 0;
    }

    if (out_assets == NULL || max_assets <= 0 || out_count == NULL) {
        return -1;
    }

    memset(out_assets, 0, (size_t)max_assets * sizeof(out_assets[0]));
    if (PreviewResources_Load(rom_path, species, opts, &res) != 0) {
        return -1;
    }

    count = 0;
    idle_map = ResolveIdleMapIndex(&res.nmcr, &res.nanr, &res.ncer, &res.nmar);
    break_map = ResolveBreakMapValidated(&res.nmar, idle_map, &res.nmcr, &res.nanr, &res.ncer);
    idle_anim = FindIdleNmarAnimationIndex(&res.nmar);
    break_anim = FindBreakNmarAnimationIndex(&res.nmar, idle_map, break_map, &res.nmcr, &res.nanr, &res.ncer);

    if (idle_map >= 0 && idle_map < res.nmcr.map_count) {
        int idle_frame_count;

        idle_frame_count = Nmcr_MaxFrameCount(&res.nmcr.maps[idle_map], &res.nanr);
        if (idle_frame_count <= 0) idle_frame_count = 1;
        if (idle_frame_count > 512) idle_frame_count = 512;
        AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_IDLE_GIF, -1, idle_map, idle_frame_count, "", "Animated Idle GIF");
    }

    for (i = 0; i < res.nmar.animation_count && count < max_assets; i++) {
        char name[ANIMA_PREVIEW_ASSET_NAME_SIZE];
        const char *label;
        int first_map;
        int frame_count;

        if (!NmarAnimationIsUsable(&res.nmar, &res.nmcr, &res.nanr, &res.ncer, i)) {
            continue;
        }

        label = res.nmar.animations[i].label;
        first_map = NmarAnimationFirstMap(&res.nmar, i);
        frame_count = NmarAnimationFrameCountForPreview(&res.nmar, i, 5);

        if (i == idle_anim &&
            NmarAnimationContainsMapOtherThan(&res.nmar, i, idle_map)) {
            continue;
        }

        if (i == idle_anim) {
            snprintf(name, sizeof(name), "Animation %02d: %s (Idle)", i, label[0] ? label : "unnamed");
        } else if (i == break_anim) {
            snprintf(name, sizeof(name), "Animation %02d: %s (Idle Break)", i, label[0] ? label : "unnamed");
        } else {
            snprintf(name, sizeof(name), "Animation %02d: %s", i, label[0] ? label : "unnamed");
        }

        AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_NMAR_ANIMATION_GIF, i, first_map, frame_count, label, name);
    }

    for (i = 0; i < res.nmcr.map_count && count < max_assets; i++) {
        char name[ANIMA_PREVIEW_ASSET_NAME_SIZE];
        int frame_count;

        if (!NmcrMapIsUsable(&res.nmcr, &res.nanr, &res.ncer, i)) {
            continue;
        }
        if (NmarAnimationReferencesMap(&res.nmar, i)) {
            continue;
        }

        frame_count = Nmcr_MaxFrameCount(&res.nmcr.maps[i], &res.nanr);
        if (frame_count <= 0) frame_count = 1;
        if (frame_count > 512) frame_count = 512;

        if (i == break_map) {
            snprintf(name, sizeof(name), "Map %02d: Idle Break", i);
        } else if (i == idle_map) {
            snprintf(name, sizeof(name), "Map %02d: Idle", i);
        } else {
            snprintf(name, sizeof(name), "Map %02d", i);
        }

        AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_NMCR_MAP_GIF, -1, i, frame_count, "", name);
    }

    if (idle_map >= 0 && idle_map < res.nmcr.map_count &&
        break_map >= 0 && break_map < res.nmcr.map_count) {
        AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_COMPOSED_GIF, break_anim, break_map, 0, "", "Composed Idle + Break GIF");
    }

    AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_SPRITESHEET_PNG, -1, -1, 1, "", "Spritesheet PNG");
    if (idle_map >= 0 && idle_map < res.nmcr.map_count) {
        AddAsset(out_assets, max_assets, &count, ANIMA_PREVIEW_ASSET_STATIC_IDLE_PNG, -1, idle_map, 1, "", "Static Idle PNG");
    }

    PreviewResources_Free(&res);
    *out_count = count;
    return count > 0 ? 0 : -1;
}

int AnimaBackend_ExtractPokemon(
    const char *rom_path,
    int species,
    const char *out_dir,
    const AnimaExtractOptions *options
)
{
    const char *target_path;
    AnimaExtractOptions local_options;
    GifExportOptions active_gif_options;

    char out_narc_path[PATH_BUFFER_SIZE];
    int block_index;

    u8 *rom;
    size_t rom_size;

    const u8 *fnt;
    NdsHeader header;
    NdsFatRange range;
    NarcArchive pokegra;

    int file_id;
    size_t fat_entry_offset;

    if (rom_path == NULL || out_dir == NULL || species < 0) {
        return -1;
    }

    if (options == NULL) {
        AnimaExtractOptions_Init(&local_options);
        options = &local_options;
    }

    active_gif_options = options->gif_options;
    active_gif_options.enabled = options->export_gifs && options->gif_options.enabled;

    target_path = ANIMA_POKEGRA_PATH;

    if (File_ReadAll(rom_path, &rom, &rom_size) != 0) {
        fprintf(stderr, "Error: no se pudo leer la ROM.\n");
        return -1;
    }

    if (NdsHeader_Parse(rom, rom_size, &header) != 0) {
        fprintf(stderr, "Error: cabecera NDS inválida o corrupta.\n");
        free(rom);
        return -1;
    }

    if (!NdsHeader_IsValidGame(&header)) {
        fprintf(stderr, "Error: The provided NDS ROM is not a valid Pokémon Black, White, Black 2, or White 2 game.\n");
        free(rom);
        return -1;
    }
    s_game_is_sequel = NdsHeader_IsSequel(&header);

    block_index = AnimaBackend_GetFormBlockIndex(species, options->form_index);

    NdsHeader_Print(&header, rom_size);

    fnt = rom + header.fnt_offset;

    printf("Resolving path: %s\n", target_path);

    if (NdsFnt_FindFileId(fnt, header.fnt_size, target_path, &file_id) != 0) {
        fprintf(stderr, "Error: no se pudo encontrar %s en la FNT.\n", target_path);
        free(rom);
        return -1;
    }

    printf("Found %s\n", target_path);
    printf("  file_id: %d\n", file_id);
    printf("\n");

    fat_entry_offset = (size_t)header.fat_offset + ((size_t)file_id * 8);

    printf("Resolving FAT entry:\n");
    printf("  FAT entry offset: 0x%08zX (%zu)\n", fat_entry_offset, fat_entry_offset);

    if (NdsFat_GetRange(rom, rom_size, &header, file_id, &range) != 0) {
        fprintf(stderr, "Error: no se pudo resolver la entrada FAT del file_id %d.\n", file_id);
        free(rom);
        return -1;
    }

    printf("  file start: 0x%08X (%u)\n", range.start, range.start);
    printf("  file end:   0x%08X (%u)\n", range.end, range.end);
    printf("  file size:  0x%08X (%u bytes)\n", range.size, range.size);
    printf("\n");

    PrintFirstFourBytes(rom + range.start, range.size);

    if (File_MkdirRecursive(out_dir) != 0) {
        fprintf(stderr, "Error: no se pudo crear el directorio de salida: %s\n", out_dir);
        free(rom);
        return -1;
    }

    RemoveChildDirectory(out_dir, "editable");
    if (OptionsRequestOnlyDsFiles(options)) {
        RemoveChildDirectory(out_dir, ANIMA_DIR_SPRITESHEETS);
        RemoveChildDirectory(out_dir, ANIMA_DIR_STATIC);
        RemoveChildDirectory(out_dir, ANIMA_DIR_IDLE_GIF);
        RemoveChildDirectory(out_dir, ANIMA_DIR_BREAK_GIF);
        RemoveChildDirectory(out_dir, ANIMA_DIR_COMPOSED_GIF);
    }

    snprintf(out_narc_path, sizeof(out_narc_path), "%s/a_0_0_4.narc", out_dir);
    remove(out_narc_path);
    printf("Using %s as source archive; writing only the selected Pokemon block.\n\n", target_path);

    if (Narc_Init(&pokegra, rom + range.start, range.size) != 0) {
        fprintf(stderr, "Error: no se pudo parsear el NARC extraído.\n");
        free(rom);
        return -1;
    }

    Narc_PrintInfo(&pokegra);

    if (WritePokemonBlockMembers(&pokegra, species, block_index, out_dir) != 0) {
        free(rom);
        return -1;
    }

    if (options->export_spritesheets) {
        GeneratePokemonTilePreviews(species, out_dir);
        GeneratePokemonCellPreviews(species, out_dir);
    }

    if (options->export_json) {
        if (Json_WritePalettes(out_dir) != 0) {
            fprintf(stderr, "Warning: could not write palettes JSON.\n");
        }

        if (Json_WriteCells(out_dir, BW_TILE_STRIDE) != 0) {
            fprintf(stderr, "Warning: could not write cells JSON.\n");
        }

        if (Json_WriteAnimation(out_dir, BW_TILE_STRIDE) != 0) {
            fprintf(stderr, "Warning: could not write animation JSON.\n");
        }
    }

    if ((options->export_static_previews || active_gif_options.enabled) &&
        GenerateAssembledStaticAndLeaves(out_dir, &active_gif_options, options->export_idle_break_gifs, options->export_composed_gifs) != 0) {
        fprintf(stderr, "Warning: could not generate assembled static or leaves PNG.\n");
    }

    free(rom);
    return 0;
}

int AnimaBackend_HasFemaleSprite(const char *rom_path, int species)
{
    u8 *rom = NULL;
    size_t rom_size = 0;
    NarcArchive pokegra;
    int has_female = 0;
    u8 *member_data = NULL;
    size_t member_size = 0;

    if (rom_path == NULL || species < 0) {
        return 0;
    }

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) == 0) {
        if (Narc_ExtractMember(&pokegra, species * 20 + 1, &member_data, &member_size) == 0) {
            if (member_size > 0) {
                has_female = 1;
            }
            free(member_data);
        }
        free(rom);
    }
    return has_female;
}

int AnimaBackend_LoadIdlePreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t pixel_count;
    size_t frame_pixels;
    u8 *indices;
    unsigned char *rgba;
    int frame_index;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_SHEET, opts, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_CELLS, opts, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_ANIM, opts, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_MAP, opts, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_PALETTE_NORMAL, opts, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_TIMING, opts, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_COORDS, opts, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
    if (frame_count <= 0) {
        frame_count = 1;
    }
    if (frame_count > 512) {
        frame_count = 512;
    }

    Composer_ComputeBoundsRange(
        &ncer,
        &nanr,
        &nmcr.maps[idle_map],
        0,
        frame_count,
        &min_x, &min_y, &max_x, &max_y,
        &coords,
        5
    );

    // Force symmetric bounds on the X-axis so the horizontal pivot is perfectly centered
    {
        int abs_x = abs(min_x) > abs(max_x) ? abs(min_x) : abs(max_x);
        min_x = -abs_x;
        max_x = abs_x;
    }

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    rgba = malloc(pixel_count * 4u);
    if (indices == NULL || rgba == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        size_t i;
        int tick = (frame_index * 5 * 60) / 100;

        frame_offset = (size_t)frame_index * frame_pixels;
        if (Composer_RenderFrameIndexed(
                &ncer,
                &nanr,
                &nmcr.maps[idle_map],
                &ncgr,
                &palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &coords
            ) != 0) {
            goto cleanup;
        }

        for (i = 0; i < frame_pixels; i++) {
            RgbaColor color;
            int color_index;
            size_t rgba_offset;

            color_index = indices[frame_offset + i];
            if (color_index < 0 || color_index >= palette.color_count) {
                color_index = 0;
            }

            color = palette.colors[color_index];
            rgba_offset = (frame_offset + i) * 4u;
            rgba[rgba_offset + 0] = color.r;
            rgba[rgba_offset + 1] = color.g;
            rgba[rgba_offset + 2] = color.b;
            rgba[rgba_offset + 3] = color.a;
        }
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadSpritesheetPreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *nclr_data; size_t nclr_size;

    NcgrImage ncgr;
    NclrPalette palette;

    RgbaColor *pixels;
    int width;
    int height;
    size_t pixel_count;
    unsigned char *rgba;
    size_t i;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    nclr_data = NULL; nclr_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&palette, 0, sizeof(palette));
    pixels = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_SHEET, opts, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_PALETTE_NORMAL, opts, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (Ncgr_RenderTilesToImage(&ncgr, &palette, 0, &pixels, &width, &height) != 0) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        goto cleanup;
    }

    for (i = 0; i < pixel_count; i++) {
        rgba[i * 4 + 0] = pixels[i].r;
        rgba[i * 4 + 1] = pixels[i].g;
        rgba[i * 4 + 2] = pixels[i].b;
        rgba[i * 4 + 3] = pixels[i].a;
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = 1;
    out_preview->delay_cs = 0;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(pixels);
    free(rgba);
    Ncgr_Free(&ncgr);
    free(ncgr_data);
    free(nclr_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadIdleBreakPreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int break_map;
    int frame_count;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    size_t pixel_count;
    size_t frame_pixels;
    u8 *indices;
    unsigned char *rgba;
    int frame_index;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_SHEET, opts, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_CELLS, opts, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_ANIM, opts, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_MAP, opts, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_PALETTE_NORMAL, opts, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_TIMING, opts, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_COORDS, opts, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    break_map = ResolveBreakMapValidated(&nmar, idle_map, &nmcr, &nanr, &ncer);
    if (break_map < 0 || break_map >= nmcr.map_count) {
        goto cleanup;
    }

    frame_count = Nmcr_MaxFrameCount(&nmcr.maps[break_map], &nanr);
    {
        int idle_frame_count;
        idle_frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
        if (frame_count <= 0) frame_count = 1;
        if (frame_count > 512) frame_count = 512;
        if (idle_frame_count > 512) idle_frame_count = 512;

        Composer_ComputeBoundsRange(
            &ncer,
            &nanr,
            &nmcr.maps[break_map],
            0,
            frame_count,
            &min_x, &min_y, &max_x, &max_y,
            &coords,
            5
        );
    }

    // Force symmetric bounds on the X-axis so the horizontal pivot is perfectly centered
    {
        int abs_x = abs(min_x) > abs(max_x) ? abs(min_x) : abs(max_x);
        min_x = -abs_x;
        max_x = abs_x;
    }

    width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
    height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        goto cleanup;
    }

    frame_pixels = (size_t)width * (size_t)height;
    pixel_count = frame_pixels * (size_t)frame_count;
    if (frame_pixels == 0 || pixel_count / frame_pixels != (size_t)frame_count) {
        goto cleanup;
    }

    indices = calloc(pixel_count, sizeof(u8));
    rgba = malloc(pixel_count * 4u);
    if (indices == NULL || rgba == NULL) {
        goto cleanup;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        size_t frame_offset;
        size_t i;
        int tick = (frame_index * 5 * 60) / 100;

        frame_offset = (size_t)frame_index * frame_pixels;
        if (Composer_RenderFrameIndexed(
                &ncer,
                &nanr,
                &nmcr.maps[break_map],
                &ncgr,
                &palette,
                tick,
                BW_TILE_STRIDE,
                min_x,
                min_y,
                width,
                height,
                COMPOSITE_MARGIN,
                indices + frame_offset,
                &coords
            ) != 0) {
            goto cleanup;
        }

        for (i = 0; i < frame_pixels; i++) {
            RgbaColor color;
            int color_index;
            size_t rgba_offset;

            color_index = indices[frame_offset + i];
            if (color_index < 0 || color_index >= palette.color_count) {
                color_index = 0;
            }

            color = palette.colors[color_index];
            rgba_offset = (frame_offset + i) * 4u;
            rgba[rgba_offset + 0] = color.r;
            rgba[rgba_offset + 1] = color.g;
            rgba[rgba_offset + 2] = color.b;
            rgba[rgba_offset + 3] = color.a;
        }
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

int AnimaBackend_LoadComposedPreviewExt(
    const char *rom_path,
    int species,
    int idle_repetitions,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
)
{
    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map;
    int break_map;
    int frame_count;
    int width;
    int height;
    size_t pixel_count;
    u8 *indices;
    unsigned char *rgba;
    size_t i;
    int success;

    if (rom_path == NULL || out_preview == NULL || species < 0) {
        return -1;
    }

    memset(out_preview, 0, sizeof(*out_preview));
    rom = NULL;
    rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));
    indices = NULL;
    rgba = NULL;
    success = -1;

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_SHEET, opts, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_CELLS, opts, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_ANIM, opts, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_MAP, opts, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_PALETTE_NORMAL, opts, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_TIMING, opts, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_COORDS, opts, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    break_map = ResolveBreakMapValidated(&nmar, idle_map, &nmcr, &nanr, &ncer);
    if (break_map < 0 || break_map >= nmcr.map_count) {
        goto cleanup;
    }

    if (opts != NULL && opts->map_index >= 0 && opts->map_index < nmcr.map_count &&
        opts->map_index != idle_map) {
        break_map = opts->map_index;
    }

    if (RenderComposedAnimationSmart(
            &ncer, &nanr, &nmcr, &nmar,
            idle_map, break_map,
            opts != NULL ? opts->animation_index : -1,
            &ncgr, &palette,
            idle_repetitions,
            BW_TILE_STRIDE,
            COMPOSITE_MARGIN,
            &indices,
            &frame_count,
            &width,
            &height,
            &coords,
            5
        ) != 0) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height * (size_t)frame_count;
    rgba = malloc(pixel_count * 4u);
    if (rgba == NULL) {
        goto cleanup;
    }

    for (i = 0; i < pixel_count; i++) {
        RgbaColor color;
        int color_index;

        color_index = indices[i];
        if (color_index < 0 || color_index >= palette.color_count) {
            color_index = 0;
        }

        color = palette.colors[color_index];
        rgba[i * 4 + 0] = color.r;
        rgba[i * 4 + 1] = color.g;
        rgba[i * 4 + 2] = color.b;
        rgba[i * 4 + 3] = color.a;
    }

    out_preview->width = width;
    out_preview->height = height;
    out_preview->frame_count = frame_count;
    out_preview->delay_cs = 5;
    out_preview->rgba = rgba;
    rgba = NULL;
    success = 0;

cleanup:
    free(indices);
    free(rgba);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

static int ResolvePreviewGifDelayCs(const AnimaPreviewOptions *opts)
{
    int delay_cs;

    delay_cs = opts != NULL ? opts->gif_delay_cs : 0;
    if (delay_cs <= 0) delay_cs = 5;
    if (delay_cs < 1) delay_cs = 1;
    if (delay_cs > 100) delay_cs = 100;
    return delay_cs;
}

int AnimaBackend_ExportCurrentAsset(
    const char *rom_path,
    int species,
    int preview_mode,
    const AnimaPreviewOptions *opts,
    const char *out_path
)
{
    char parent_dir[PATH_BUFFER_SIZE];
    int gif_delay_cs;

    gif_delay_cs = ResolvePreviewGifDelayCs(opts);

    snprintf(parent_dir, sizeof(parent_dir), "%s", out_path);
    char *last_slash = strrchr(parent_dir, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        File_MkdirRecursive(parent_dir);
    }

    if (preview_mode == 1) { // Spritesheet PNG
        AnimaIdlePreview sheet;
        memset(&sheet, 0, sizeof(sheet));
        if (AnimaBackend_LoadSpritesheetPreviewExt(rom_path, species, opts, &sheet) == 0) {
            int ret = Png_WriteRgbaImageScaled(out_path, (const RgbaColor *)sheet.rgba, sheet.width, sheet.height, 4);
            AnimaIdlePreview_Free(&sheet);
            return ret;
        }
        return -1;
    }

    if (preview_mode == 2) { // Static Idle PNG
        AnimaIdlePreview idle;
        memset(&idle, 0, sizeof(idle));
        if (AnimaBackend_LoadIdlePreviewExt(rom_path, species, opts, &idle) == 0) {
            int ret = Png_WriteRgbaImageScaled(out_path, (const RgbaColor *)idle.rgba, idle.width, idle.height, 4);
            AnimaIdlePreview_Free(&idle);
            return ret;
        }
        return -1;
    }

    if (preview_mode == ANIMA_PREVIEW_ASSET_NMAR_ANIMATION_GIF ||
        preview_mode == ANIMA_PREVIEW_ASSET_NMCR_MAP_GIF) {
        PreviewResourceSet res;
        AnimaIdlePreview preview;
        int ret;

        memset(&preview, 0, sizeof(preview));
        if (PreviewResources_Load(rom_path, species, opts, &res) != 0) {
            return -1;
        }

        if (preview_mode == ANIMA_PREVIEW_ASSET_NMAR_ANIMATION_GIF) {
            int animation_index = opts != NULL ? opts->animation_index : 0;
            ret = RenderNmarAnimationPreviewFromResources(&res, animation_index, &preview);
        } else {
            int map_index = opts != NULL ? opts->map_index : 0;
            ret = RenderNmcrMapPreviewFromResources(&res, map_index, &preview);
        }

        if (ret == 0) {
            preview.delay_cs = gif_delay_cs;
            ret = WritePreviewGifFromPalette(out_path, &preview, &res.palette);
        }

        AnimaIdlePreview_Free(&preview);
        PreviewResources_Free(&res);
        return ret;
    }

    u8 *rom;
    size_t rom_size;
    NarcArchive pokegra;

    u8 *ncgr_data; size_t ncgr_size;
    u8 *ncer_data; size_t ncer_size;
    u8 *nanr_data; size_t nanr_size;
    u8 *nmcr_data; size_t nmcr_size;
    u8 *nmar_data; size_t nmar_size;
    u8 *nclr_data; size_t nclr_size;
    u8 *coords_data; size_t coords_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NanrFile nanr;
    NmcrFile nmcr;
    NmarFile nmar;
    NclrPalette palette;
    CoordFile coords;

    int idle_map = 0;
    int break_map = 0;
    int frame_count = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int width = 0;
    int height = 0;
    size_t pixel_count = 0;
    size_t frame_pixels = 0;
    u8 *indices = NULL;
    int frame_index = 0;
    int success = -1;

    rom = NULL; rom_size = 0;
    ncgr_data = NULL; ncgr_size = 0;
    ncer_data = NULL; ncer_size = 0;
    nanr_data = NULL; nanr_size = 0;
    nmcr_data = NULL; nmcr_size = 0;
    nmar_data = NULL; nmar_size = 0;
    nclr_data = NULL; nclr_size = 0;
    coords_data = NULL; coords_size = 0;
    memset(&ncgr, 0, sizeof(ncgr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&nmar, 0, sizeof(nmar));
    memset(&palette, 0, sizeof(palette));
    memset(&coords, 0, sizeof(coords));

    if (InitPokegraFromRom(rom_path, &rom, &rom_size, &pokegra) != 0) {
        goto cleanup;
    }
    (void)rom_size;

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_SHEET, opts, &ncgr_data, &ncgr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_CELLS, opts, &ncer_data, &ncer_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_ANIM, opts, &nanr_data, &nanr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_MAP, opts, &nmcr_data, &nmcr_size) != 0 ||
        ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_PALETTE_NORMAL, opts, &nclr_data, &nclr_size) != 0) {
        goto cleanup;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        goto cleanup;
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_TIMING, opts, &nmar_data, &nmar_size) == 0) {
        if (Nmar_Parse(nmar_data, nmar_size, &nmar) != 0) {
            Nmar_Free(&nmar);
            memset(&nmar, 0, sizeof(nmar));
        }
    }

    if (ExtractDecodedPokemonRoleExt(&pokegra, species, ROLE_FRONT_COORDS, opts, &coords_data, &coords_size) == 0) {
        if (Coord_Parse(coords_data, coords_size, &coords) != 0) {
            memset(&coords, 0, sizeof(coords));
        }
    }

    idle_map = ResolveIdleMapIndex(&nmcr, &nanr, &ncer, &nmar);
    if (idle_map < 0 || idle_map >= nmcr.map_count) {
        goto cleanup;
    }

    break_map = ResolveBreakMapValidated(&nmar, idle_map, &nmcr, &nanr, &ncer);

    if (preview_mode == 0) { // Animated Idle GIF
        frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
        if (frame_count <= 0) frame_count = 1;
        if (frame_count > 512) frame_count = 512;

        Composer_ComputeBoundsRange(
            &ncer, &nanr,
            &nmcr.maps[idle_map], 0, frame_count,
            &min_x, &min_y, &max_x, &max_y,
            &coords, gif_delay_cs
        );

        width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
        height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
        if (width <= 0 || height <= 0 || width > 1024 || height > 1024) goto cleanup;

        frame_pixels = (size_t)width * (size_t)height;
        pixel_count = frame_pixels * (size_t)frame_count;
        indices = calloc(pixel_count, sizeof(u8));
        if (indices == NULL) goto cleanup;

        for (frame_index = 0; frame_index < frame_count; frame_index++) {
            size_t frame_offset = (size_t)frame_index * frame_pixels;
            int tick = (frame_index * gif_delay_cs * 60) / 100;
            if (Composer_RenderFrameIndexed(
                    &ncer, &nanr, &nmcr.maps[idle_map], &ncgr, &palette,
                    tick, BW_TILE_STRIDE, min_x, min_y, width, height,
                    COMPOSITE_MARGIN, indices + frame_offset, &coords
                ) != 0) {
                goto cleanup;
            }
        }

        if (Gif_WriteIndexed(out_path, indices, frame_count, width, height, palette.colors, palette.color_count, 0, gif_delay_cs, 0, 4) == 0) {
            success = 0;
        }

    } else if (preview_mode == 3) { // Animated Break GIF
        if (break_map < 0 || break_map >= nmcr.map_count) goto cleanup;

        frame_count = Nmcr_MaxFrameCount(&nmcr.maps[break_map], &nanr);
        int idle_frame_count = Nmcr_MaxFrameCount(&nmcr.maps[idle_map], &nanr);
        if (frame_count <= 0) frame_count = 1;
        if (frame_count > 512) frame_count = 512;
        if (idle_frame_count > 512) idle_frame_count = 512;

        Composer_ComputeUnionBounds(
            &ncer, &nanr,
            &nmcr.maps[idle_map], idle_frame_count,
            &nmcr.maps[break_map], frame_count,
            &min_x, &min_y, &max_x, &max_y,
            &coords, gif_delay_cs
        );

        width = (max_x - min_x) + COMPOSITE_MARGIN * 2;
        height = (max_y - min_y) + COMPOSITE_MARGIN * 2;
        if (width <= 0 || height <= 0 || width > 1024 || height > 1024) goto cleanup;

        frame_pixels = (size_t)width * (size_t)height;
        pixel_count = frame_pixels * (size_t)frame_count;
        indices = calloc(pixel_count, sizeof(u8));
        if (indices == NULL) goto cleanup;

        for (frame_index = 0; frame_index < frame_count; frame_index++) {
            size_t frame_offset = (size_t)frame_index * frame_pixels;
            int tick = (frame_index * gif_delay_cs * 60) / 100;
            if (Composer_RenderFrameIndexed(
                    &ncer, &nanr, &nmcr.maps[break_map], &ncgr, &palette,
                    tick, BW_TILE_STRIDE, min_x, min_y, width, height,
                    COMPOSITE_MARGIN, indices + frame_offset, &coords
                ) != 0) {
                goto cleanup;
            }
        }

        if (Gif_WriteIndexed(out_path, indices, frame_count, width, height, palette.colors, palette.color_count, 0, gif_delay_cs, 0, 4) == 0) {
            success = 0;
        }

    } else if (preview_mode == 4) { // Animated Composed GIF
        if (break_map < 0 || break_map >= nmcr.map_count) goto cleanup;

        if (opts != NULL && opts->map_index >= 0 && opts->map_index < nmcr.map_count &&
            opts->map_index != idle_map) {
            break_map = opts->map_index;
        }

        if (RenderComposedAnimationSmart(
                &ncer, &nanr, &nmcr, &nmar,
                idle_map, break_map,
                opts != NULL ? opts->animation_index : -1,
                &ncgr, &palette, -1, BW_TILE_STRIDE,
                COMPOSITE_MARGIN, &indices, &frame_count, &width, &height,
                &coords, gif_delay_cs
            ) != 0) {
                goto cleanup;
        }

        if (Gif_WriteIndexed(out_path, indices, frame_count, width, height, palette.colors, palette.color_count, 0, gif_delay_cs, 0, 4) == 0) {
            success = 0;
        }
    }

cleanup:
    free(indices);
    Ncgr_Free(&ncgr);
    Ncer_Free(&ncer);
    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Coord_Free(&coords);
    free(ncgr_data);
    free(ncer_data);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(nclr_data);
    free(coords_data);
    free(rom);
    return success;
}

#include "anima_backend.h"
#include "common.h"
#include "pokemon_catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintHelp(const char *program)
{
    printf("========================================================================\n");
    printf("                  AnimaEngine CLI - Command Line Utility\n");
    printf("========================================================================\n");
    printf("Usage:\n");
    printf("  %s <rom.nds> <species_id> <out_path> [options]\n\n", program);
    printf("Required Arguments:\n");
    printf("  <rom.nds>            Path to the Nintendo DS Pokemon Black/White ROM file\n");
    printf("  <species_id>         National Dex ID of the Pokemon to extract (1 to 649)\n");
    printf("  <out_path>           Output directory (in full mode) or file path (in single mode)\n\n");
    printf("General Options:\n");
    printf("  -h, --help           Display this comprehensive help page and exit\n");
    printf("  --mode <full|single> Select run mode:\n");
    printf("                         full   - Batch extracts all assets into canonical subfolders (default)\n");
    printf("                         single - Renders and exports a single custom asset to <out_path>\n");
    printf("  --form <index>       Alternate form index within Pokegra (0 = base form, 1, 2, ...)\n");
    printf("  --gender <0|1>       Select gender (0 = male/unisex [default], 1 = female)\n");
    printf("  --male               Shortcut for --gender 0\n");
    printf("  --female             Shortcut for --gender 1\n");
    printf("  --shiny              Enable shiny palette bank (default uses normal palette)\n");
    printf("  --back               Set camera perspective to back view (default uses front view)\n");
    printf("  --scale <n>          Multiplier scale (1 to 32) for PNGs and GIFs (default: 4)\n");
    printf("  --delay-cs <n>       Custom frame delay in centiseconds (<=0 to use default ROM timing)\n\n");
    printf("Single Mode Asset Types (used with --mode single):\n");
    printf("  --asset <type>       Select the specific asset type to export:\n");
    printf("                         idle        - Battle idle animation GIF (default)\n");
    printf("                         spritesheet - Reconstructed tile sheet PNG\n");
    printf("                         static      - First frame idle preview PNG\n");
    printf("                         break       - Idle break candidates animation GIF\n");
    printf("                         composed    - Composed idle + break sequence loop GIF\n");
    printf("                         animation   - Specific timeline NMAR animation index GIF\n");
    printf("                         map         - Specific static layout NMCR map index GIF\n");
    printf("  --animation-idx <n>  Timeline NMAR index to target for '--asset animation' (0, 1, ...)\n");
    printf("  --map-idx <n>        Layout NMCR map index to target for '--asset map' (0, 1, ...)\n\n");
    printf("Backwards Compatibility & Full-Mode Batch Export Controls:\n");
    printf("  --no-gif             Disable automatic GIF generation during full batch extraction\n");
    printf("  --gif-side <f|b|both> Limit batch GIF side exports (front, back, both)\n");
    printf("  --gif-palette <n|s|b> Limit batch GIF palettes (normal, shiny, both)\n");
    printf("  --gif-break          Force export of standalone break GIFs in batch extraction\n");
    printf("  --gif-composed       Force export of composed idle+break loops in batch extraction\n");
    printf("  --gif-scale <n>      Scale multiplier override for batch exports\n");
    printf("  --gif-delay-cs <n>   Duration override for batch exports\n");
    printf("  --gif-loop <n>       Loop iteration count for batch exports\n");
    printf("========================================================================\n");
}

static int ParseNonNegativeInt(const char *text, int *out_value)
{
    char *end;
    long value;

    if (text == NULL || out_value == NULL || *text == '\0') {
        return -1;
    }

    value = strtol(text, &end, 10);
    if (*end != '\0' || value < 0 || value > 1000000L) {
        return -1;
    }

    *out_value = (int)value;
    return 0;
}

static int ParsePositiveInt(const char *text, int *out_value)
{
    if (ParseNonNegativeInt(text, out_value) != 0 || *out_value <= 0) {
        return -1;
    }

    return 0;
}

static void SanitizeOutputName(const char *name, char *out, size_t out_size)
{
    size_t i;
    size_t j;
    int last_sep;

    if (out == NULL || out_size == 0) {
        return;
    }

    j = 0;
    last_sep = 1;
    for (i = 0; name != NULL && name[i] != '\0' && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) {
            out[j++] = (char)c;
            last_sep = 0;
        } else if (!last_sep) {
            out[j++] = '_';
            last_sep = 1;
        }
    }

    if (j > 0 && out[j - 1] == '_') {
        j--;
    }
    if (j == 0 && out_size > 1) {
        out[j++] = 'P';
    }
    out[j] = '\0';
}

static void BuildCanonicalOutputDir(int species, const char *out_root, char *out_dir, size_t out_dir_size)
{
    const PokemonCatalogEntry *entry;
    char name[128];

    entry = PokemonCatalog_FindByDexId(species);
    SanitizeOutputName(entry != NULL ? entry->name : "Pokemon", name, sizeof(name));
    snprintf(out_dir, out_dir_size, "%s/pokedex%03d_%s", out_root, species, name);
}

int main(int argc, char **argv)
{
    // Help trigger
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        PrintHelp(argv[0]);
        return 0;
    }

    if (argc < 4) {
        fprintf(stderr, "Error: Insufficient arguments.\n");
        fprintf(stderr, "Run '%s --help' to display available command line options.\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    int species = atoi(argv[2]);
    const char *out_path_arg = argv[3];

    // Core options mapping CLI controls
    int run_mode_single = 0; // 0 = full (default), 1 = single
    int opt_form = 0;
    int opt_gender = 0;
    int opt_shiny = 0;
    int opt_back = 0;
    int opt_scale = 4;
    int opt_delay_cs = 0;

    int opt_asset_type = 0; // matching AnimaPreviewAssetType
    int opt_animation_idx = 0;
    int opt_map_idx = 0;

    // Full extraction options with backwards-compatible defaults
    AnimaExtractOptions batch_options;
    AnimaExtractOptions_Init(&batch_options);

    // Parse options from index 4 onwards
    for (int i = 4; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--mode") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --mode requires a value (full|single)\n");
                return 1;
            }
            const char *mode = argv[++i];
            if (strcmp(mode, "single") == 0) {
                run_mode_single = 1;
            } else if (strcmp(mode, "full") == 0) {
                run_mode_single = 0;
            } else {
                fprintf(stderr, "Error: Invalid mode: %s\n", mode);
                return 1;
            }
        } else if (strcmp(arg, "--form") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &opt_form) != 0) {
                fprintf(stderr, "Error: --form requires a valid non-negative integer\n");
                return 1;
            }
            batch_options.form_index = opt_form;
        } else if (strcmp(arg, "--gender") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &opt_gender) != 0) {
                fprintf(stderr, "Error: --gender requires an integer (0 = male, 1 = female)\n");
                return 1;
            }
        } else if (strcmp(arg, "--male") == 0) {
            opt_gender = 0;
        } else if (strcmp(arg, "--female") == 0) {
            opt_gender = 1;
        } else if (strcmp(arg, "--shiny") == 0) {
            opt_shiny = 1;
        } else if (strcmp(arg, "--back") == 0) {
            opt_back = 1;
        } else if (strcmp(arg, "--scale") == 0) {
            if (i + 1 >= argc || ParsePositiveInt(argv[++i], &opt_scale) != 0) {
                fprintf(stderr, "Error: --scale requires a positive integer (1 to 32)\n");
                return 1;
            }
            batch_options.gif_options.scale = opt_scale;
        } else if (strcmp(arg, "--delay-cs") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &opt_delay_cs) != 0) {
                fprintf(stderr, "Error: --delay-cs requires a non-negative integer\n");
                return 1;
            }
            batch_options.gif_options.delay_cs = opt_delay_cs;
        } else if (strcmp(arg, "--asset") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --asset requires a type value\n");
                return 1;
            }
            const char *type = argv[++i];
            if (strcmp(type, "idle") == 0) {
                opt_asset_type = 0;
            } else if (strcmp(type, "spritesheet") == 0) {
                opt_asset_type = 1;
            } else if (strcmp(type, "static") == 0) {
                opt_asset_type = 2;
            } else if (strcmp(type, "break") == 0) {
                opt_asset_type = 3;
            } else if (strcmp(type, "composed") == 0) {
                opt_asset_type = 4;
            } else if (strcmp(type, "animation") == 0) {
                opt_asset_type = 5;
            } else if (strcmp(type, "map") == 0) {
                opt_asset_type = 6;
            } else {
                fprintf(stderr, "Error: Unknown asset type: %s\n", type);
                return 1;
            }
        } else if (strcmp(arg, "--animation-idx") == 0 || strcmp(arg, "--animation-index") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &opt_animation_idx) != 0) {
                fprintf(stderr, "Error: --animation-idx requires a valid integer\n");
                return 1;
            }
        } else if (strcmp(arg, "--map-idx") == 0 || strcmp(arg, "--map-index") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &opt_map_idx) != 0) {
                fprintf(stderr, "Error: --map-idx requires a valid integer\n");
                return 1;
            }
        }
        // ------------------ Backwards Compatibility Options ------------------
        else if (strcmp(arg, "--no-gif") == 0) {
            batch_options.gif_options.enabled = 0;
            batch_options.export_gifs = 0;
        } else if (strcmp(arg, "--gif-side") == 0) {
            if (i + 1 >= argc) return 1;
            const char *val = argv[++i];
            if (strcmp(val, "front") == 0) batch_options.gif_options.side = GIF_SIDE_FRONT;
            else if (strcmp(val, "back") == 0) batch_options.gif_options.side = GIF_SIDE_BACK;
            else if (strcmp(val, "both") == 0) batch_options.gif_options.side = GIF_SIDE_BOTH;
            else { fprintf(stderr, "Error: Invalid --gif-side value\n"); return 1; }
        } else if (strcmp(arg, "--gif-palette") == 0) {
            if (i + 1 >= argc) return 1;
            const char *val = argv[++i];
            if (strcmp(val, "normal") == 0) batch_options.gif_options.palette = GIF_PALETTE_NORMAL;
            else if (strcmp(val, "shiny") == 0) batch_options.gif_options.palette = GIF_PALETTE_SHINY;
            else if (strcmp(val, "both") == 0) batch_options.gif_options.palette = GIF_PALETTE_BOTH;
            else { fprintf(stderr, "Error: Invalid --gif-palette value\n"); return 1; }
        } else if (strcmp(arg, "--gif-eye-mode") == 0) {
            if (i + 1 >= argc) return 1;
            const char *val = argv[++i];
            if (strcmp(val, "open") == 0) batch_options.gif_options.eye_mode = GIF_EYE_OPEN;
            else if (strcmp(val, "all") == 0) batch_options.gif_options.eye_mode = GIF_EYE_ALL;
            else { fprintf(stderr, "Error: Invalid --gif-eye-mode value\n"); return 1; }
        } else if (strcmp(arg, "--gif-scale") == 0) {
            if (i + 1 >= argc || ParsePositiveInt(argv[++i], &batch_options.gif_options.scale) != 0) {
                fprintf(stderr, "Error: Invalid --gif-scale\n");
                return 1;
            }
        } else if (strcmp(arg, "--gif-delay-cs") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &batch_options.gif_options.delay_cs) != 0) {
                fprintf(stderr, "Error: Invalid --gif-delay-cs\n");
                return 1;
            }
        } else if (strcmp(arg, "--gif-loop") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &batch_options.gif_options.loop_count) != 0) {
                fprintf(stderr, "Error: Invalid --gif-loop\n");
                return 1;
            }
        } else if (strcmp(arg, "--gif-start-frame") == 0) {
            if (i + 1 >= argc || ParseNonNegativeInt(argv[++i], &batch_options.gif_options.start_frame) != 0) {
                fprintf(stderr, "Error: Invalid --gif-start-frame\n");
                return 1;
            }
        } else if (strcmp(arg, "--gif-frame-count") == 0) {
            if (i + 1 >= argc) return 1;
            const char *val = argv[++i];
            if (strcmp(val, "auto") == 0) batch_options.gif_options.frame_count = -1;
            else if (ParsePositiveInt(val, &batch_options.gif_options.frame_count) != 0) {
                fprintf(stderr, "Error: Invalid --gif-frame-count\n");
                return 1;
            }
        } else if (strcmp(arg, "--gif-map") == 0) {
            if (i + 1 >= argc) return 1;
            const char *val = argv[++i];
            if (strcmp(val, "idle") == 0) {
                batch_options.gif_options.map_is_idle = 1;
                batch_options.gif_options.map_index = 0;
            } else {
                batch_options.gif_options.map_is_idle = 0;
                if (ParseNonNegativeInt(val, &batch_options.gif_options.map_index) != 0) {
                    fprintf(stderr, "Error: Invalid --gif-map index\n");
                    return 1;
                }
            }
        } else if (strcmp(arg, "--gif-break") == 0) {
            batch_options.export_idle_break_gifs = 1;
        } else if (strcmp(arg, "--gif-composed") == 0) {
            batch_options.export_composed_gifs = 1;
        } else {
            fprintf(stderr, "Error: Unknown command line option: %s\n", arg);
            fprintf(stderr, "Run '%s --help' to display available command line options.\n", argv[0]);
            return 1;
        }
    }

    if (run_mode_single) {
        // Build rendering configuration targeting specified variant
        AnimaPreviewOptions preview_opts;
        memset(&preview_opts, 0, sizeof(preview_opts));
        preview_opts.gender = opt_gender;
        preview_opts.is_shiny = opt_shiny;
        preview_opts.form_index = opt_form;
        preview_opts.is_back = opt_back;
        preview_opts.animation_index = opt_animation_idx;
        preview_opts.map_index = opt_map_idx;
        preview_opts.gif_delay_cs = opt_delay_cs;

        // Force scaling override in GIF options structure
        batch_options.gif_options.scale = opt_scale;

        printf("------------------------------------------------------------------------\n");
        printf("Executing AnimaEngine in SINGLE EXPORT MODE:\n");
        printf("  Species:       %d\n", species);
        printf("  Asset Type:    %d\n", opt_asset_type);
        printf("  Gender:        %s\n", opt_gender ? "Female" : "Male/Unisex");
        printf("  Palette:       %s\n", opt_shiny ? "Shiny" : "Normal");
        printf("  View:          %s\n", opt_back ? "Back Perspective" : "Front Perspective");
        printf("  Form Index:    %d\n", opt_form);
        if (opt_asset_type == 5) printf("  Animation Idx: %d\n", opt_animation_idx);
        if (opt_asset_type == 6) printf("  Map Index:     %d\n", opt_map_idx);
        printf("  Destination:   %s\n", out_path_arg);
        printf("------------------------------------------------------------------------\n");

        if (AnimaBackend_ExportCurrentAsset(rom_path, species, opt_asset_type, &preview_opts, out_path_arg) != 0) {
            fprintf(stderr, "Error: Single asset rendering or export failed.\n");
            return 1;
        }
        printf("[SUCCESS] Asset exported correctly to: %s\n", out_path_arg);
    } else {
        char out_dir[ANIMA_PATH_BUFFER_SIZE];
        BuildCanonicalOutputDir(species, out_path_arg, out_dir, sizeof(out_dir));

        printf("------------------------------------------------------------------------\n");
        printf("Executing AnimaEngine in FULL BATCH EXTRACTION MODE:\n");
        printf("  Species:       %d\n", species);
        printf("  Form Index:    %d\n", opt_form);
        printf("  Output Dir:    %s\n", out_dir);
        printf("------------------------------------------------------------------------\n");

        // Forward compatibility overrides
        batch_options.export_gifs = batch_options.gif_options.enabled;

        if (AnimaBackend_ExtractPokemon(rom_path, species, out_dir, &batch_options) != 0) {
            fprintf(stderr, "Error: Batch Pokemon extraction failed.\n");
            return 1;
        }
        printf("[SUCCESS] Batch extraction finished correctly under: %s\n", out_dir);
    }

    return 0;
}

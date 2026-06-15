#include "test_harness.h"
#include "test_main.h"

#include "pokemon_catalog.h"

#include <stddef.h>
#include <stdlib.h>

/*
 * The catalog should cover the full Gen V national dex (1..649) which is
 * what Pokemon Black, White, Black 2, and White 2 read. Spot-check a few
 * well-known species to guard against accidental truncation or renumbering.
 */
static void test_entries_are_nonempty(void)
{
    int count = 0;
    const PokemonCatalogEntry *entries = PokemonCatalog_GetEntries(&count);
    ANIMA_ASSERT(entries != NULL);
    ANIMA_ASSERT(count >= 649);
}

static void test_dex_ids_are_sequential_starting_at_one(void)
{
    int count = 0;
    const PokemonCatalogEntry *entries = PokemonCatalog_GetEntries(&count);
    ANIMA_ASSERT(entries != NULL);
    ANIMA_ASSERT(count >= 649);
    for (int i = 0; i < 649; i++) {
        if (entries[i].dex_id != i + 1) {
            printf("    expected dex_id %d at index %d, got %d\n",
                   i + 1, i, entries[i].dex_id);
            g_anima_test_state.current_failed = 1;
            return;
        }
    }
}

static void test_find_known_species(void)
{
    const PokemonCatalogEntry *p = PokemonCatalog_FindByDexId(25);
    ANIMA_ASSERT(p != NULL);
    if (p != NULL) {
        ANIMA_ASSERT_STR_EQ(p->name, "Pikachu");
    }

    p = PokemonCatalog_FindByDexId(7);
    ANIMA_ASSERT(p != NULL);
    if (p != NULL) {
        ANIMA_ASSERT_STR_EQ(p->name, "Squirtle");
    }

    p = PokemonCatalog_FindByDexId(649);
    ANIMA_ASSERT(p != NULL);
    if (p != NULL) {
        ANIMA_ASSERT_STR_EQ(p->name, "Genesect");
    }
}

static void test_find_unknown_dex_id_returns_null(void)
{
    int count = 0;
    const PokemonCatalogEntry *entries = PokemonCatalog_GetEntries(&count);
    ANIMA_ASSERT(entries != NULL);

    int out_of_range = count + 1;
    const PokemonCatalogEntry *p = PokemonCatalog_FindByDexId(out_of_range);
    ANIMA_ASSERT(p == NULL);

    p = PokemonCatalog_FindByDexId(-1);
    ANIMA_ASSERT(p == NULL);

    p = PokemonCatalog_FindByDexId(0);
    ANIMA_ASSERT(p == NULL);
}

static void test_get_entries_null_outparam_is_safe(void)
{
    const PokemonCatalogEntry *entries = PokemonCatalog_GetEntries(NULL);
    ANIMA_ASSERT(entries != NULL);
}

void anima_test_pokemon_catalog_register(void)
{
    ANIMA_RUN(test_entries_are_nonempty);
    ANIMA_RUN(test_dex_ids_are_sequential_starting_at_one);
    ANIMA_RUN(test_find_known_species);
    ANIMA_RUN(test_find_unknown_dex_id_returns_null);
    ANIMA_RUN(test_get_entries_null_outparam_is_safe);
}

/**
 * @file pokemon_catalog.h
 * @brief Pokemon catalog structure and query database functions.
 */

#ifndef POKEMON_CATALOG_H
#define POKEMON_CATALOG_H

/**
 * @brief Represents a single species entry in the local database catalog.
 */
typedef struct PokemonCatalogEntry {
    int dex_id;        /**< National Dex ID of the Pokemon. */
    const char *name;  /**< Read-only name string. */
} PokemonCatalogEntry;

/**
 * @brief Retrieves the entire catalog list of Pokemon entries.
 * @param out_count Destination pointer to store number of entries in the catalog.
 * @return Pointer to the read-only catalog array.
 */
const PokemonCatalogEntry *PokemonCatalog_GetEntries(int *out_count);

/**
 * @brief Finds a Pokemon catalog entry matching a specific National Dex ID.
 * @param dex_id National Dex ID.
 * @return Catalog entry pointer, or NULL if not found.
 */
const PokemonCatalogEntry *PokemonCatalog_FindByDexId(int dex_id);

#endif


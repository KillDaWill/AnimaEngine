#ifndef POKEMON_CATALOG_H
#define POKEMON_CATALOG_H

typedef struct PokemonCatalogEntry {
    int dex_id;
    const char *name;
} PokemonCatalogEntry;

const PokemonCatalogEntry *PokemonCatalog_GetEntries(int *out_count);
const PokemonCatalogEntry *PokemonCatalog_FindByDexId(int dex_id);

#endif

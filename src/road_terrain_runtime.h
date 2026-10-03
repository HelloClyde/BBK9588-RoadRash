#ifndef ROAD_RASH_TERRAIN_RUNTIME_H
#define ROAD_RASH_TERRAIN_RUNTIME_H

#include "road_core.h"
#include "cel_runtime.h"

#define ROAD_HILL_TILE_WIDTH 128u
#define ROAD_HILL_TILE_HEIGHT 16u
#define ROAD_HILL_TILE_PIXELS (ROAD_HILL_TILE_WIDTH * ROAD_HILL_TILE_HEIGHT)
/* Main and alternate routes together use at most 30 unique RHIL tiles. */
#define ROAD_HILL_TILE_LIMIT 32u
#define ROAD_HILL_MIP_PIXELS (ROAD_HILL_TILE_WIDTH * (8u + 4u + 2u + 1u))
/* Widths 16, 32 and 64 at heights 8, 4, 2 and 1. */
#define ROAD_HILL_NARROW_MIP_PIXELS ((16u + 32u + 64u) * (8u + 4u + 2u + 1u))
#define ROAD_ROADSIDE_TILE_WIDTH 32u
#define ROAD_ROADSIDE_TILE_HEIGHT 32u
#define ROAD_ROADSIDE_TILE_PIXELS 1024u
#define ROAD_ROADSIDE_TILE_LIMIT 192u
#define ROAD_ROADSIDE_REPEAT_LEVELS 4u
#define ROAD_MARGIN_TILE_LIMIT 16u
/* Shared RGB565 storage for unique CLGP dimension selections. */
#define ROAD_MARGIN_PIXEL_POOL_PIXELS 65536u

/* Resolve each RHIL strip through RRSM/FAM/CLGP and cache a compact tile.
 * Missing or unsupported CELs keep their solid-colour fallback. */
unsigned int road_terrain_load_hill_tiles(
    const rr_rsrc_file_t *families, road_hill_profile_t *profiles,
    unsigned int profile_count, road_sprite_t *tiles,
    road_sprite_t *mip_tiles, road_sprite_t *narrow_tiles,
    road_pixel_t *tile_pixels, road_pixel_t *mip_pixels,
    road_pixel_t *narrow_pixels,
    unsigned short *family_keys, unsigned char *selector_keys,
    unsigned char *band_keys,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity);

unsigned int road_terrain_load_roadside_tiles(
    const rr_rsrc_file_t *families, road_surface_sample_t *samples,
    const unsigned char *terrain_mode, unsigned int sample_count,
    road_sprite_t *tiles,
    unsigned short *tile_heights, unsigned char *repeat_indices,
    road_sprite_t *repeat_mips,
    road_pixel_t *tile_pixels,
    unsigned short *family_keys,
    unsigned char *selector_keys,
    unsigned char *child_keys,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity);

/* Append RSLD family-index-5 child 2/3 CELs to a shared cache so the
 * selected and preview routes can use the same decoded margin textures. */
unsigned int road_terrain_load_margin_tiles(
    const rr_rsrc_file_t *families,
    const rr_u8 *left_selector, const rr_u8 *right_selector,
    const unsigned short *left_family,
    const unsigned short *right_family,
    unsigned int sample_count,
    road_margin_tile_t *tiles, road_pixel_t *pixel_pool,
    unsigned int *pixel_used, unsigned int pixel_capacity,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity);

#endif

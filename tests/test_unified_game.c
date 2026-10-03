#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "course_catalog.h"
#include "road_course_runtime.h"
#include "road_course_graph.h"
#include "road_terrain_runtime.h"
#include "cel_runtime.h"
#include "local-data/road_bike_data.h"

static signed char curvature[10000], elevation[10000];
static unsigned short left_width[10000], right_width[10000];
static rr_u8 left_margin[10000], right_margin[10000];
static rr_u8 left_depth[10000], right_depth[10000];
static rr_u8 left_edge_resource[10000], right_edge_resource[10000];
static unsigned short left_edge_family[10000], right_edge_family[10000];
static rr_u8 terrain_mode[10000];
static rr_u8 surface_flags[10000];
static road_surface_sample_t surface_samples[10000];
static road_surface_sample_t fork_surface_samples[10000];
static signed char fork_curvature[10000];
static signed char fork_elevation[10000];
static rr_course_graph_t junction_graph;
static unsigned short fork_left_width[10000], fork_right_width[10000];
static rr_u8 fork_left_margin[10000], fork_right_margin[10000];
static rr_u8 fork_left_depth[10000], fork_right_depth[10000];
static rr_u8 fork_terrain_mode[10000], fork_surface_flags[10000];
static rr_u8 fork_left_edge_resource[10000];
static rr_u8 fork_right_edge_resource[10000];
static unsigned short fork_left_edge_family[10000];
static unsigned short fork_right_edge_family[10000];
static road_scenery_placement_t objects[8000];
static road_static_placement_t hazards[1000];
static road_effect_placement_t effects[1000];
static rr_u32 families[32], hazard_families[64];
static rr_u8 hazard_frames[64];
static rr_u8 backdrop[360u * 94u];
static road_pixel_t sky_pixels[80u * 145u];
static road_sprite_t sky_sprite;
static road_pixel_t palette[256], screen[ROAD_PIXELS];
static signed char straight[512];
static signed char crest_elevation[512];
static signed char crest_curvature[512];
static unsigned short test_left_width[512], test_right_width[512];
static rr_u8 test_left_margin[512], test_right_margin[512];
static rr_u8 test_left_depth[512], test_right_depth[512];
static rr_u8 test_terrain_mode[512];
static road_hill_profile_t hill_profiles[512];
static road_hill_profile_t fork_hill_profiles[512];
static road_hill_sample_t hill_samples[10000];
static road_hill_sample_t fork_hill_samples[10000];
static unsigned int fork_hill_profile_count;
static unsigned short hill_family_keys[ROAD_HILL_TILE_LIMIT];
static unsigned char hill_selector_keys[ROAD_HILL_TILE_LIMIT];
static unsigned char hill_band_keys[ROAD_HILL_TILE_LIMIT];
static unsigned int hill_cache_count;
static road_edge_profile_t edge_profiles[2][256];
static short edge_outer_samples[2][10000];
static road_pixel_t hill_tile_pixels[ROAD_HILL_TILE_LIMIT]
    [ROAD_HILL_TILE_PIXELS];
static road_sprite_t hill_tiles[ROAD_HILL_TILE_LIMIT];
static road_pixel_t hill_mip_pixels[ROAD_HILL_TILE_LIMIT]
    [ROAD_HILL_MIP_PIXELS];
static road_sprite_t hill_mips[ROAD_HILL_TILE_LIMIT * 4u];
static road_pixel_t hill_narrow_pixels[ROAD_HILL_TILE_LIMIT]
    [ROAD_HILL_NARROW_MIP_PIXELS];
static road_sprite_t hill_narrow_mips[ROAD_HILL_TILE_LIMIT * 12u];
static road_pixel_t roadside_tile_pixels[ROAD_ROADSIDE_TILE_LIMIT]
    [ROAD_ROADSIDE_TILE_PIXELS];
static road_sprite_t roadside_tiles[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned short roadside_family_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char roadside_selector_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char roadside_child_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned int roadside_cache_count;
static unsigned short roadside_tile_heights[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char roadside_repeat_indices[ROAD_ROADSIDE_TILE_LIMIT];
static road_sprite_t roadside_repeat_mips
    [ROAD_ROADSIDE_TILE_LIMIT * ROAD_ROADSIDE_REPEAT_LEVELS];
static road_margin_tile_t margin_tiles[ROAD_MARGIN_TILE_LIMIT];
static road_pixel_t margin_pixel_pool[ROAD_MARGIN_PIXEL_POOL_PIXELS];
static unsigned int margin_pixel_used;
static unsigned int margin_tile_count;
static road_pixel_t hill_temporary[16384];
static rr_u8 hill_scratch[32768];
static road_pixel_t scenery_pixels[32][4096];
static road_pixel_t large_scenery_pixels[3][24576];
static road_pixel_t huge_scenery_pixels[32768];
static road_sprite_t scenery_sprites[32];
static rr_u8 scenery_scratch[65536];
static road_pixel_t road_texture_pixels[3][5656];
static road_sprite_t road_textures[27];
static road_pixel_t edge_fill_pixels[2][256u * 32u];
static road_sprite_t edge_fill[2];
static road_pixel_t hud_pixels[300u * 58u];
static road_pixel_t hud_speed_needle_pixels[48u * 6u];
static road_pixel_t hud_health_needle_pixels[10u * 2u];
static road_sprite_t hud_speed_needle;
static road_sprite_t hud_health_needle;
static rr_u8 hud_scratch[12000u];
static road_sprite_t hud;
static road_pixel_t hud_health_pixels[32][32u * 8u];
static road_sprite_t hud_health_frames[32];
static road_pixel_t hud_portrait_pixels[43u * 14u];
static road_sprite_t hud_portrait;
static const road_scenery_placement_t distant_placement = {65535u, 0u, 0u, 8u, 0u};
static const char *preview_image_path;
static unsigned int anchored_course_mask;
static unsigned int preview_course_index;
static unsigned int preview_sample_index = UINT_MAX;
static const road_pixel_t test_car_pixels[16] = {
    0x07e0u, 0x07e0u, 0x07e0u, 0x07e0u,
    0x07e0u, 0x07e0u, 0x07e0u, 0x07e0u,
    0x07e0u, 0x07e0u, 0x07e0u, 0x07e0u,
    0x07e0u, 0x07e0u, 0x07e0u, 0x07e0u
};
static const road_sprite_t test_car = {4u, 4u, test_car_pixels};
static const road_pixel_t test_car_red_pixels[16] = {
    0xf800u, 0xf800u, 0xf800u, 0xf800u,
    0xf800u, 0xf800u, 0xf800u, 0xf800u,
    0xf800u, 0xf800u, 0xf800u, 0xf800u,
    0xf800u, 0xf800u, 0xf800u, 0xf800u
};
static const road_sprite_t test_car_frames[3] = {
    {4u, 4u, test_car_pixels},
    {4u, 4u, test_car_red_pixels},
    {4u, 4u, test_car_pixels}
};
static const road_car_frame_map_t test_car_map = {
    2u, 0x100000u, {2u, 4u, 1u},
    {{1u, 1u}, {2u, 2u}, {0u, 0u}}
};
static const road_car_animation_t test_car_animation = {
    test_car_frames, 3u, &test_car_map, 0
};
static const road_car_frame_anchor_t test_car_anchors[3] = {
    {0, 0, 0u}, {0, 0, 1u}, {0, 0, 0u}
};
static const road_car_animation_t test_car_animation_anchored = {
    test_car_frames, 3u, &test_car_map, test_car_anchors
};

static unsigned int screen_hash(void)
{
    unsigned int i, hash = 2166136261u;
    for (i = 0u; i < ROAD_PIXELS; ++i)
        hash = (hash ^ screen[i]) * 16777619u;
    return hash;
}

/* Literal MIPS worker/background pipeline, independent of the renderer's
 * quotient implementation. Test inputs stay inside signed 32-bit phase. */
static int upstream_background_scroll(int heading_8_8)
{
    int phase = heading_8_8 * 256;
    int reduced;
    if (phase < 0) phase += 3;
    phase >>= 2;
    reduced = phase;
    if (reduced < 0) reduced += 3;
    return (phase + (reduced >> 2)) >> 16;
}

static void save_preview_image(void)
{
    FILE *output;
    unsigned int i;
    if (!preview_image_path) return;
    output = fopen(preview_image_path, "wb");
    assert(output);
    assert(fprintf(output, "P6\n%u %u\n255\n", ROAD_WIDTH,
                   ROAD_HEIGHT) > 0);
    for (i = 0u; i < ROAD_PIXELS; ++i) {
        road_pixel_t color = screen[i];
        unsigned char rgb[3];
        rgb[0] = (unsigned char)(((color >> 11u) & 31u) * 255u / 31u);
        rgb[1] = (unsigned char)(((color >> 5u) & 63u) * 255u / 63u);
        rgb[2] = (unsigned char)((color & 31u) * 255u / 31u);
        assert(fwrite(rgb, 1u, sizeof(rgb), output) == sizeof(rgb));
    }
    assert(fclose(output) == 0);
}

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *f = (FILE *)user;
    return fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, f) == size;
}

static unsigned int load_terrain_tiles(const char *root,
                                       unsigned int profile_count,
                                       unsigned int sample_count,
                                       unsigned int *roadside_count)
{
    char path[512];
    rr_rsrc_file_t families_catalog;
    FILE *file;
    long size;
    unsigned int count;
    assert(snprintf(path, sizeof(path), "%s/Families.RSRC", root) > 0);
    file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&families_catalog, read_at, file,
                                     (rr_u32)size));
    count = road_terrain_load_hill_tiles(&families_catalog,
        hill_profiles, profile_count, hill_tiles, hill_mips,
        hill_narrow_mips, &hill_tile_pixels[0][0],
        &hill_mip_pixels[0][0], &hill_narrow_pixels[0][0],
        hill_family_keys, hill_selector_keys, hill_band_keys,
        0u, ROAD_HILL_TILE_LIMIT, hill_temporary, 16384u,
        hill_scratch, sizeof(hill_scratch));
    hill_cache_count = count;
    if (count) {
        assert(hill_tiles[0].width == ROAD_HILL_TILE_WIDTH);
        assert(hill_tiles[0].height == ROAD_HILL_TILE_HEIGHT);
        assert(hill_tiles[0].pixels);
        assert(hill_mips[0].height == 8u && hill_mips[0].pixels);
        assert(hill_mips[3].height == 1u && hill_mips[3].pixels);
        assert(hill_narrow_mips[0].width == 16u &&
               hill_narrow_mips[0].height == 8u &&
               hill_narrow_mips[0].pixels);
        assert(hill_narrow_mips[11].width == 64u &&
               hill_narrow_mips[11].height == 1u &&
               hill_narrow_mips[11].pixels);
    }
    *roadside_count = road_terrain_load_roadside_tiles(
        &families_catalog, surface_samples, terrain_mode, sample_count,
        roadside_tiles, roadside_tile_heights,
        roadside_repeat_indices,
        roadside_repeat_mips,
        &roadside_tile_pixels[0][0],
        roadside_family_keys, roadside_selector_keys,
        roadside_child_keys, 0u, ROAD_ROADSIDE_TILE_LIMIT,
        hill_temporary, 16384u,
        hill_scratch, sizeof(hill_scratch));
    roadside_cache_count = *roadside_count;
    margin_pixel_used = 0u;
    margin_tile_count = road_terrain_load_margin_tiles(
        &families_catalog, left_edge_resource, right_edge_resource,
        left_edge_family, right_edge_family, sample_count,
        margin_tiles, margin_pixel_pool, &margin_pixel_used,
        ROAD_MARGIN_PIXEL_POOL_PIXELS, 0u, ROAD_MARGIN_TILE_LIMIT,
        hill_temporary, 16384u, hill_scratch, sizeof(hill_scratch));
    assert(margin_tile_count < ROAD_MARGIN_TILE_LIMIT);
    assert(margin_pixel_used <= ROAD_MARGIN_PIXEL_POOL_PIXELS);
    assert(fclose(file) == 0);
    road_game_set_hill_tiles(hill_tiles, hill_mips,
        hill_narrow_mips, count);
    road_game_set_roadside_tiles(roadside_tiles,
        roadside_tile_heights, roadside_repeat_indices,
        roadside_repeat_mips,
        *roadside_count);
    road_game_set_margin_tiles(margin_tiles, margin_tile_count);
    return count;
}

static void load_fork_terrain_tiles(const char *root,
                                   unsigned int sample_count)
{
    char path[512];
    rr_rsrc_file_t catalog;
    FILE *file;
    long size;
    assert(snprintf(path, sizeof(path), "%s/Families.RSRC", root) > 0);
    file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file,
                                     (rr_u32)size));
    hill_cache_count = road_terrain_load_hill_tiles(&catalog,
        fork_hill_profiles, fork_hill_profile_count, hill_tiles,
        hill_mips, hill_narrow_mips, &hill_tile_pixels[0][0],
        &hill_mip_pixels[0][0], &hill_narrow_pixels[0][0],
        hill_family_keys, hill_selector_keys, hill_band_keys,
        hill_cache_count, ROAD_HILL_TILE_LIMIT,
        hill_temporary, 16384u, hill_scratch, sizeof(hill_scratch));
    assert(hill_cache_count <= ROAD_HILL_TILE_LIMIT);
    {
        unsigned int p, side, band;
        for (p = 0u; p < fork_hill_profile_count; ++p)
            for (side = 0u; side < 2u; ++side)
                for (band = 0u; band < 3u; ++band)
                    if (fork_hill_profiles[p].family_id[side] &&
                        fork_hill_profiles[p].tile_index[side][band]
                            >= hill_cache_count) {
                        /* Highway's FAM 66 root 1 has only three
                         * entries; selector 67 maps to absent entry 3. */
                        assert(p == 26u && side == 0u &&
                            fork_hill_profiles[p].family_id[side] == 66u &&
                            fork_hill_profiles[p].selector[side] == 67u);
                    }
    }
    road_game_set_hill_tiles(hill_tiles, hill_mips,
        hill_narrow_mips, hill_cache_count);
    roadside_cache_count = road_terrain_load_roadside_tiles(
        &catalog, fork_surface_samples, fork_terrain_mode,
        sample_count, roadside_tiles, roadside_tile_heights,
        roadside_repeat_indices, roadside_repeat_mips,
        &roadside_tile_pixels[0][0], roadside_family_keys,
        roadside_selector_keys, roadside_child_keys,
        roadside_cache_count, ROAD_ROADSIDE_TILE_LIMIT,
        hill_temporary, 16384u, hill_scratch,
        sizeof(hill_scratch));
    road_game_set_roadside_tiles(roadside_tiles,
        roadside_tile_heights, roadside_repeat_indices,
        roadside_repeat_mips, roadside_cache_count);
    margin_tile_count = road_terrain_load_margin_tiles(&catalog,
        fork_left_edge_resource, fork_right_edge_resource,
        fork_left_edge_family, fork_right_edge_family, sample_count,
        margin_tiles, margin_pixel_pool, &margin_pixel_used,
        ROAD_MARGIN_PIXEL_POOL_PIXELS, margin_tile_count,
        ROAD_MARGIN_TILE_LIMIT, hill_temporary, 16384u,
        hill_scratch, sizeof(hill_scratch));
    assert(margin_tile_count < ROAD_MARGIN_TILE_LIMIT);
    assert(margin_pixel_used <= ROAD_MARGIN_PIXEL_POOL_PIXELS);
    printf("  margin CEL keys after fork: %u, RGB565 pixels %u\n",
        margin_tile_count, margin_pixel_used);
    assert(fclose(file) == 0);
    road_game_set_margin_tiles(margin_tiles, margin_tile_count);
}

static void load_scenery(const char *root, unsigned int family_count)
{
    char path[512];
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    FILE *file;
    long size;
    unsigned int i;
    assert(snprintf(path, sizeof(path), "%s/Families.RSRC", root) > 0);
    file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file,
                                     (rr_u32)size));
    for (i = 0u; i < family_count; ++i) {
        rr_u32 id = families[i];
        rr_u32 capacity = 4096u;
        int prefer_last = id == 171u;
        image.pixels = scenery_pixels[i];
        if (id == 69u || id == 164u || id == 185u) {
            unsigned int slot = id == 69u ? 0u : id == 164u ? 1u : 2u;
            image.pixels = large_scenery_pixels[slot];
            capacity = 24576u;
        } else if (id == 168u || id == 242u) {
            image.pixels = huge_scenery_pixels;
            capacity = 32768u;
        }
        if (!rr_family_load_frame(&catalog, id, prefer_last,
            &image, capacity, scenery_scratch,
            sizeof(scenery_scratch)) &&
            !rr_family_load_frame(&catalog, id, !prefer_last,
            &image, capacity, scenery_scratch,
            sizeof(scenery_scratch))) {
            image.width = image.height = 1u;
            image.pixels[0] = RR_CEL_TRANSPARENT;
        }
        scenery_sprites[i].width = image.width;
        scenery_sprites[i].height = image.height;
        scenery_sprites[i].pixels = image.pixels;
    }
    assert(fclose(file) == 0);
    road_game_set_scenery(scenery_sprites, family_count);
}

static void load_road_textures(const char *root)
{
    char path[512];
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    rr_rsrc_record_t gauge;
    rr_u8 gauge_record[4096];
    FILE *file;
    long size;
    unsigned int variant, style;
    assert(snprintf(path, sizeof(path), "%s/rashOpt.rsrc", root) > 0);
    file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    for (variant = 0u; variant < 3u; ++variant) {
        unsigned int used = 0u;
        for (style = 0u; style < 9u; ++style) {
            unsigned int slot = variant * 9u + style;
            image.pixels = &road_texture_pixels[variant][used];
            assert(rr_cel_load_road_surface(&catalog, slot + 1u,
                                             &image, 5656u - used));
            road_textures[slot].width = image.width;
            road_textures[slot].height = image.height;
            road_textures[slot].pixels = image.pixels;
            used += image.width * image.height;
        }
        assert(used == 5656u);
    }
    for (variant = 0u; variant < 2u; ++variant) {
        image.pixels = edge_fill_pixels[variant];
        assert(rr_cel_load_road_surface(&catalog, 73u + variant,
            &image, 256u * 32u));
        assert(image.width == 256u && image.height == 32u);
        edge_fill[variant].width = image.width;
        edge_fill[variant].height = image.height;
        edge_fill[variant].pixels = image.pixels;
    }
    image.pixels = hud_pixels;
    assert(rr_cel_load_packed_image(&catalog, 55u, &image,
                                     300u * 58u,
                                     hud_scratch, sizeof(hud_scratch)));
    hud.width = image.width;
    hud.height = image.height;
    hud.pixels = image.pixels;
    image.pixels = hud_speed_needle_pixels;
    assert(rr_cel_load_uncoded6(&catalog, 57u, &image, 48u * 6u));
    hud_speed_needle.width = image.width;
    hud_speed_needle.height = image.height;
    hud_speed_needle.pixels = image.pixels;
    image.pixels = hud_health_needle_pixels;
    assert(rr_cel_load_uncoded1(&catalog, 58u, &image, 10u * 2u));
    hud_health_needle.width = image.width;
    hud_health_needle.height = image.height;
    hud_health_needle.pixels = image.pixels;
    assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                        1u, &gauge) && gauge.size <= sizeof(gauge_record));
    assert(rr_rsrc_read(&catalog, &gauge, 0u,
                        gauge_record, gauge.size));
    for (style = 0u; style < 32u; ++style) {
        image.pixels = hud_health_pixels[style];
        assert(rr_animation_decode_rect_frame(gauge_record, gauge.size,
            style, &image, 32u * 8u));
        hud_health_frames[style].width = image.width;
        hud_health_frames[style].height = image.height;
        hud_health_frames[style].pixels = image.pixels;
    }
    image.pixels = hud_portrait_pixels;
    assert(rr_cel_load_packed_image(&catalog, 59u, &image,
                                     43u * 14u,
                                     hud_scratch, sizeof(hud_scratch)));
    hud_portrait.width = image.width;
    hud_portrait.height = image.height;
    hud_portrait.pixels = image.pixels;
    assert(fclose(file) == 0);
    road_game_set_surface_textures(road_textures, 27u);
    road_game_set_edge_fill(&edge_fill[0], &edge_fill[1]);
    road_game_set_hud(&hud);
    road_game_set_hud_needles(&hud_speed_needle, &hud_health_needle);
    road_game_set_alternate_hud(hud_health_frames, 32u, &hud_portrait);
}

static void check_asymmetric_road_projection(void)
{
    road_game_t game;
    unsigned int i;
    unsigned int row = 100u * ROAD_WIDTH;
    for (i = 0u; i < 512u; ++i) {
        test_left_width[i] = 256u;
        test_right_width[i] = 768u;
        test_left_margin[i] = 128u;
        test_right_margin[i] = 32u;
    }
    road_game_set_course(straight, 512u);
    road_game_set_elevation(straight, 512u);
    road_game_set_widths(test_left_width, test_right_width, 512u);
    road_game_set_cross_section(test_left_margin, test_right_margin,
        test_left_depth, test_right_depth, test_terrain_mode, 512u);
    road_game_set_backdrop(0, 0, 0u, 0u);
    road_game_set_sky(0);
    road_game_set_scenery_placements(&distant_placement, 1u);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_bikes(0, 0u);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    road_game_render(&game, screen);
    assert(screen[row + 110u] == screen[row]);
    assert(screen[row + 140u] != screen[row]);
    assert(screen[row + 190u] != screen[row]);
    assert(screen[row + 210u] != screen[row]);
    road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    puts("Asymmetric RLAN width and RSLD shoulder projection passed.");
}

static void check_near_surface_quarters(void)
{
    static const road_pixel_t rows[4] = {
        0xf800u, 0x07e0u, 0x001fu, 0xffffu
    };
    road_sprite_t textures[27];
    road_game_t game;
    unsigned int i;
    for (i = 0u; i < 27u; ++i) {
        textures[i].width = 1u;
        textures[i].height = 4u;
        textures[i].pixels = rows;
    }
    for (i = 0u; i < 512u; ++i) {
        test_left_width[i] = 512u;
        test_right_width[i] = 512u;
    }
    road_game_set_course(straight, 512u);
    road_game_set_elevation(straight, 512u);
    road_game_set_widths(test_left_width, test_right_width, 512u);
    road_game_set_surface_textures(textures, 27u);
    road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    road_game_render(&game, screen);
    assert(screen[145u * ROAD_WIDTH + 50u] == rows[1]);
    assert(screen[150u * ROAD_WIDTH + 50u] == rows[2]);
    assert(screen[160u * ROAD_WIDTH + 50u] == rows[3]);
    road_game_set_surface_textures(0, 0u);
    puts("Near road four-band source rows passed.");
}

static void check_fractional_node_horizon(void)
{
    road_game_t game;
    unsigned int i;
    for (i = 0u; i < 512u; ++i) {
        crest_elevation[i] = i >= 13u ? 25 : 0;
        test_left_width[i] = 512u;
        test_right_width[i] = 512u;
        test_terrain_mode[i] = 4u;
    }
    road_game_set_course(straight, 512u);
    road_game_set_elevation(crest_elevation, 512u);
    road_game_set_widths(test_left_width, test_right_width, 512u);
    road_game_set_surface_textures(0, 0u);
    road_game_set_cross_section(test_left_margin, test_right_margin,
        test_left_depth, test_right_depth, test_terrain_mode, 512u);
    road_game_set_backdrop(backdrop, palette, 360u, 94u);
    for (i = 0u; i < 80u * 145u; ++i)
        sky_pixels[i] = 0x1234u;
    sky_sprite.width = 80u;
    sky_sprite.height = 145u;
    sky_sprite.pixels = sky_pixels;
    road_game_set_sky(&sky_sprite);
    road_game_init(&game);
    road_game_seek(&game, 128u);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    road_game_render(&game, screen);
    /* At half a sample, the next 3DO node starts 128 units ahead.
     * Sampling at +256 instead clips the sky one scanline too soon. */
    assert(screen[79u * ROAD_WIDTH] == 0x1234u);
    assert(screen[80u * ROAD_WIDTH] == 0u);
    road_game_set_elevation(straight, 512u);
    road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    road_game_set_sky(0);
    road_game_set_backdrop(0, 0, 0u, 0u);
    puts("Fractional track-node horizon passed.");
}

static void check_textured_crest_occlusion(void)
{
    static const road_pixel_t red_pixels[4] = {
        0xf800u, 0xf800u, 0xf800u, 0xf800u
    };
    road_sprite_t textures[27];
    road_game_t game;
    unsigned int i;
    for (i = 0u; i < 512u; ++i) {
        crest_elevation[i] = i < 8u ? 40 : i < 16u ? -40 : 0;
        crest_curvature[i] = i >= 8u && i < 16u ? 50 : 0;
        test_left_width[i] = 256u;
        test_right_width[i] = 256u;
        test_terrain_mode[i] = 0u;
    }
    for (i = 0u; i < 27u; ++i) {
        textures[i].width = 1u;
        textures[i].height = 4u;
        textures[i].pixels = red_pixels;
    }
    road_game_set_course(crest_curvature, 512u);
    road_game_set_elevation(crest_elevation, 512u);
    road_game_set_widths(test_left_width, test_right_width, 512u);
    road_game_set_surface_textures(textures, 27u);
    road_game_set_cross_section(test_left_margin, test_right_margin,
        test_left_depth, test_right_depth, test_terrain_mode, 512u);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    road_game_render(&game, screen);
    assert(screen[97u * ROAD_WIDTH + 310u] != red_pixels[0]);
    assert(screen[140u * ROAD_WIDTH + 100u] == red_pixels[0]);
    road_game_set_surface_textures(0, 0u);
    road_game_set_elevation(straight, 512u);
    road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    puts("Textured-road crest occlusion passed.");
}

static void check_original_hud_numeric_readouts(void)
{
    static road_pixel_t blank_hud_pixels[300u * 58u];
    static const road_pixel_t transparent = 0xf81fu;
    static const road_sprite_t blank_hud = {
        300u, 58u, blank_hud_pixels
    };
    static const road_sprite_t empty_needle = {
        1u, 1u, &transparent
    };
    road_game_t game;
    unsigned int i;
    for (i = 0u; i < 300u * 58u; ++i)
        blank_hud_pixels[i] = 0xffffu;
    road_game_set_hud(&blank_hud);
    road_game_set_hud_needles(&empty_needle, &empty_needle);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    road_game_render(&game, screen);
    /* Three zero-padded progress digits and a one-place rank. */
    assert(screen[213u * ROAD_WIDTH + 106u] != 0xffffu);
    assert(screen[213u * ROAD_WIDTH + 120u] != 0xffffu);
    assert(screen[213u * ROAD_WIDTH + 208u] == 0xffffu);
    game.distance = 33u * 256u;
    game.opponents[0].distance = game.distance + 1u;
    road_game_render(&game, screen);
    assert(screen[213u * ROAD_WIDTH + 120u] == 0xffffu);
    assert(screen[213u * ROAD_WIDTH + 208u] != 0xffffu);
    road_game_set_hud(0);
    road_game_set_hud_needles(0, 0);
    puts("Original HUD progress and race-position readouts passed.");
}

static unsigned int load_and_render(const char *root, unsigned int index)
{
    char path[512];
    char name[16];
    rr_rsrc_file_t catalog;
    rr_course_buffer_t course = {0};
    rr_u32 width, height;
    road_game_t game;
    FILE *file;
    long size;
    unsigned int i, hash, terrain_count, roadside_count;
    assert(index < RR_COURSE_COUNT);
    assert(strchr(rr_course_paths[index], ':') == NULL);
    assert(strchr(rr_car_paths[index], ':') == NULL);
    assert(strlen(rr_course_names[index]) < sizeof(name));
    strcpy(name, rr_course_names[index]);
    for (i = 1u; name[i]; ++i)
        if (name[i] >= 'A' && name[i] <= 'Z')
            name[i] = (char)(name[i] + ('a' - 'A'));
    assert(snprintf(path, sizeof(path), "%s/%s/%sopt.rsrc", root,
                    name, name) > 0);
    file = fopen(path, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    course.curvature = curvature;
    course.elevation = elevation;
    course.left_width = left_width;
    course.right_width = right_width;
    course.left_margin = left_margin;
    course.right_margin = right_margin;
    course.left_depth = left_depth;
    course.right_depth = right_depth;
    course.left_edge_resource = left_edge_resource;
    course.right_edge_resource = right_edge_resource;
    course.left_edge_family = left_edge_family;
    course.right_edge_family = right_edge_family;
    course.terrain_mode = terrain_mode;
    course.surface_flags = surface_flags;
    course.surface_samples = surface_samples;
    course.hill_profiles = hill_profiles;
    course.hill_samples = hill_samples;
    course.hill_profile_capacity = 512u;
    course.edge_profiles[0] = edge_profiles[0];
    course.edge_profiles[1] = edge_profiles[1];
    course.edge_outer_samples[0] = edge_outer_samples[0];
    course.edge_outer_samples[1] = edge_outer_samples[1];
    course.edge_profile_capacity[0] = 256u;
    course.edge_profile_capacity[1] = 256u;
    course.objects = objects;
    course.object_capacity = 8000u;
    course.hazards = hazards;
    course.hazard_capacity = 1000u;
    course.effects = effects;
    course.effect_capacity = 1000u;
    course.crossing_zones = 0;
    course.crossing_capacity = 0u;
    course.difficulty_level = 0u;
    course.family_ids = families;
    course.family_capacity = 32u;
    course.hazard_family_ids = hazard_families;
    course.hazard_frame_ids = hazard_frames;
    course.hazard_family_capacity = 64u;
    course.capacity = 10000u;
    assert(rr_course_load_primary(&catalog, &course));
    assert(course.hill_profile_count > 0u);
    if (index == 1u) {
        assert(terrain_mode[600u] == 1u);
        assert(course.edge_profile_count[0] > 0u);
        assert(course.edge_profile_count[1] > 0u);
        assert(surface_samples[600u].family_id[0] == 15u);
        assert(surface_samples[600u].family_id[1] == 15u);
    }
    {
        unsigned int varying = 0u, p;
        for (p = 0u; p < course.hill_profile_count; ++p) {
            unsigned int s;
            for (s = hill_profiles[p].start_sample + 1u;
                 s < hill_profiles[p].end_sample; ++s)
                if (memcmp(&hill_samples[s], &hill_samples[s - 1u],
                           sizeof(hill_samples[s])) != 0)
                    ++varying;
        }
        printf("  RHIL changing samples: %u\n", varying);
        assert(varying > 0u);
    }
    for (i = 0u; i < course.hill_profile_count; ++i) {
        assert(hill_profiles[i].start_sample < hill_profiles[i].end_sample);
        assert(hill_profiles[i].end_sample <= course.sample_count);
    }
    assert(rr_cel_load_backdrop(&catalog, 2u, backdrop, sizeof(backdrop),
                                palette, &width, &height));
    {
        rr_cel_image_t sky;
        sky.pixels = sky_pixels;
        assert(rr_cel_load_road_surface(&catalog, 1u, &sky,
                                        80u * 145u));
        assert(sky.width == 20u || sky.width == 80u);
        assert(sky.height >= 125u && sky.height <= 145u);
        sky_sprite.width = sky.width;
        sky_sprite.height = sky.height;
        sky_sprite.pixels = sky_pixels;
        road_game_set_sky(&sky_sprite);
    }
    road_game_set_course(curvature, course.sample_count);
    road_game_set_elevation(elevation, course.sample_count);
    road_game_set_widths(left_width, right_width, course.sample_count);
    road_game_set_cross_section(left_margin, right_margin,
        left_depth, right_depth, terrain_mode, course.sample_count);
    road_game_set_hill_profiles(hill_profiles, course.hill_profile_count);
    road_game_set_hill_samples(hill_samples, course.sample_count);
    road_game_set_edge_profiles(edge_profiles[0],
        course.edge_profile_count[0], edge_profiles[1],
        course.edge_profile_count[1]);
    road_game_set_edge_outer_samples(edge_outer_samples[0],
        edge_outer_samples[1], course.sample_count);
    road_game_set_surface_samples(surface_samples, course.sample_count);
    road_game_set_margin_resources(0u, left_edge_resource,
        right_edge_resource, left_edge_family, right_edge_family,
        surface_flags, terrain_mode,
        course.sample_count);
    terrain_count = load_terrain_tiles(root, course.hill_profile_count,
        course.sample_count, &roadside_count);
    if (index == 3u) {
        unsigned int key;
        for (key = 0u; key < margin_tile_count; ++key)
            if (margin_tiles[key].family_id == 54u &&
                margin_tiles[key].selector == 0u) break;
        assert(key < margin_tile_count);
        assert(margin_tiles[key].primary_mips[0].width == 8u);
        assert(margin_tiles[key].primary_mips[0].height == 4u);
        assert(margin_tiles[key].primary_mips[0].pixels ==
               margin_tiles[key].primary_mips[2].pixels);
        assert(margin_tiles[key].primary_mips[3].width == 16u);
        assert(margin_tiles[key].primary_mips[3].height == 8u);
        assert(margin_tiles[key].primary_mips[4].width == 32u);
        assert(margin_tiles[key].primary_mips[4].height == 16u);
        assert(margin_tiles[key].primary_mips[5].width == 64u);
        assert(margin_tiles[key].primary_mips[5].height == 32u);
    }
    {
        unsigned int covered = 0u, total = 0u;
        unsigned int p, side, band;
        for (p = 0u; p < course.hill_profile_count; ++p)
            for (side = 0u; side < 2u; ++side)
                for (band = 0u; band < 3u; ++band) {
                    ++total;
                    if (hill_profiles[p].tile_index[side][band] <
                        terrain_count) ++covered;
                }
        printf("  RHIL terrain: %u tiles, %u/%u band placements\n",
               terrain_count, covered, total);
        assert(terrain_count > 0u && covered == total);
    }
    {
        unsigned int selected = 0u, covered = 0u;
        unsigned int edge_selected = 0u, edge_covered = 0u;
        unsigned int edge_unique = 0u;
        unsigned int repeat_decoded = 0u;
        unsigned char edge_seen[ROAD_ROADSIDE_TILE_LIMIT] = {0};
        unsigned int first = UINT_MAX;
        for (i = 0u; i < course.sample_count; ++i)
            if (terrain_mode[i] == 0u) {
                unsigned int side;
                for (side = 0u; side < 2u; ++side)
                    if (surface_samples[i].selector[side] != 255u) {
                        if (first == UINT_MAX) first = i;
                        ++selected;
                        if (surface_samples[i].tile_index[side] <
                            roadside_count) ++covered;
                    }
            } else if (terrain_mode[i] == 1u) {
                unsigned int side;
                for (side = 0u; side < 2u; ++side)
                    if (surface_samples[i].selector[side] != 255u) {
                        ++edge_selected;
                        if (surface_samples[i].tile_index[side] <
                            roadside_count) {
                            unsigned int tile = surface_samples[i].
                                tile_index[side];
                            ++edge_covered;
                            if (!edge_seen[tile]) {
                                edge_seen[tile] = 1u;
                                ++edge_unique;
                                assert(roadside_repeat_indices[tile] <
                                    roadside_count);
                                if (roadside_repeat_indices[tile] != tile) {
                                    const road_sprite_t *repeat =
                                        &roadside_tiles[
                                            roadside_repeat_indices[tile]];
                                    assert(repeat->width == 128u &&
                                           repeat->height == 8u &&
                                           repeat->pixels);
                                    {
                                        unsigned int level;
                                        for (level = 0u; level <
                                            ROAD_ROADSIDE_REPEAT_LEVELS;
                                            ++level) {
                                            const road_sprite_t *mip =
                                                &roadside_repeat_mips[tile *
                                                ROAD_ROADSIDE_REPEAT_LEVELS +
                                                level];
                                            assert(mip->width == 128u &&
                                                   mip->height ==
                                                       (1u << level) &&
                                                   mip->pixels);
                                        }
                                    }
                                    ++repeat_decoded;
                                }
                            }
                        }
                    }
            }
        printf("  RBLD terrain: %u tiles, %u/%u side samples, first %u\n",
               roadside_count, covered, selected, first);
        printf("  RMTN/RDWD terrain: %u tiles, %u/%u side samples\n",
               edge_unique, edge_covered, edge_selected);
        if (index == 1u) assert(repeat_decoded == edge_unique);
        if (selected) assert(covered == selected);
        if (edge_selected) assert(edge_covered == edge_selected);
    }
    road_game_set_backdrop(backdrop, palette, width, height);
    load_scenery(root, course.family_count);
    road_game_set_scenery_placements(objects, course.object_count);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_bikes(g_original_bikes, ROAD_BIKE_COUNT);
    road_game_init(&game);
    assert(game.distance == 0u && game.speed == 0u && !game.finished);
    road_game_seek(&game, 256u);
    assert(game.road_scroll_heading_8_8 ==
           (int)curvature[0] * 256);
    road_game_seek(&game, 0u);
    assert(game.road_scroll_heading_8_8 == 0);
    for (i = 0u; i < 40u; ++i)
        road_game_step(&game, ROAD_ACCEL, 33u);
    assert(game.distance > 0u);
    road_game_render(&game, screen);
    if (index == 0u) {
        unsigned int matched = 0u, y, x;
        unsigned int backdrop_matched = 0u;
        unsigned int backdrop_top = sky_sprite.height - height;
        int scroll = upstream_background_scroll(
            game.road_scroll_heading_8_8);
        int offset = scroll % (int)width;
        if (offset < 0) offset += (int)width;
        assert(screen[0] == sky_pixels[0]);
        for (y = backdrop_top; y < 90u; ++y)
            for (x = 0u; x < 300u; ++x) {
                road_pixel_t color = palette[backdrop[
                    (y - backdrop_top) * width +
                    ((unsigned int)offset + x) % width]];
                if (color && screen[y * ROAD_WIDTH + x] == color)
                    ++backdrop_matched;
            }
        assert(backdrop_matched > 100u);
        for (y = 0u; y < 62u; ++y)
            for (x = 0u; x < 37u; ++x) {
                road_pixel_t source = hud.pixels[
                    (y * hud.height / 62u) * hud.width +
                    x * hud.width / ROAD_WIDTH];
                if (source != RR_CEL_TRANSPARENT) {
                    assert(screen[(y + 178u) * ROAD_WIDTH + x] == source);
                    ++matched;
                }
            }
        assert(matched > 100u);
    }
    hash = screen_hash();
    if (index == 0u) {
        unsigned int position = game.distance;
        unsigned int matched = 0u, distinct = 0u, y, x;
        unsigned int backdrop_top = sky_sprite.height - height;
        int offset;
        road_game_seek(&game, 600u * 256u);
        assert(game.road_scroll_heading_8_8 == -70 * 256);
        road_game_render(&game, screen);
        offset = upstream_background_scroll(
            game.road_scroll_heading_8_8) % (int)width;
        assert(upstream_background_scroll(
            game.road_scroll_heading_8_8) == -22);
        if (offset < 0) offset += (int)width;
        for (y = backdrop_top; y < 90u; ++y)
            for (x = 0u; x < 300u; ++x) {
                road_pixel_t color = palette[backdrop[
                    (y - backdrop_top) * width +
                    ((unsigned int)offset + x) % width]];
                road_pixel_t prior = palette[backdrop[
                    (y - backdrop_top) * width +
                    ((unsigned int)(offset + 1) + x) % width]];
                if (color && screen[y * ROAD_WIDTH + x] == color) {
                    ++matched;
                    if (color != prior) ++distinct;
                }
            }
        assert(matched > 100u);
        assert(distinct > 20u);
        road_game_seek(&game, position);
        road_game_render(&game, screen);
        assert(screen_hash() == hash);
    }
    if (index == 1u) {
        unsigned int with_edges, without_edges;
        unsigned int repeat_visible = UINT_MAX;
        unsigned int varying_edge_visible = UINT_MAX;
        road_sprite_t saved_repeat_mips[ROAD_ROADSIDE_TILE_LIMIT *
            ROAD_ROADSIDE_REPEAT_LEVELS];
        unsigned int variation_count = 0u;
        assert(edge_outer_samples[0][0] == 806 &&
               edge_outer_samples[1][0] == 456);
        assert(edge_profiles[0][1].start_sample == 65u &&
               edge_profiles[0][1].inner_offset == 0 &&
               edge_profiles[0][1].height == 1305);
        for (i = 1u; i < 50u; ++i) {
            if (edge_outer_samples[0][i] !=
                edge_outer_samples[0][i - 1u]) ++variation_count;
            assert(edge_outer_samples[0][i] >= 296 &&
                   edge_outer_samples[0][i] <= 1316);
            assert(edge_outer_samples[1][i] >= 156 &&
                   edge_outer_samples[1][i] <= 756);
        }
        assert(variation_count > 10u);
        road_game_seek(&game, 600u * 256u);
        road_game_render(&game, screen);
        with_edges = screen_hash();
        road_game_set_edge_profiles(0, 0u, 0, 0u);
        road_game_render(&game, screen);
        without_edges = screen_hash();
        assert(with_edges != without_edges);
        road_game_set_edge_profiles(edge_profiles[0],
            course.edge_profile_count[0], edge_profiles[1],
            course.edge_profile_count[1]);
        for (i = 0u; i < 650u; i += 25u) {
            unsigned int with_variation, without_variation;
            road_game_seek(&game, i * 256u);
            road_game_render(&game, screen);
            with_variation = screen_hash();
            road_game_set_edge_outer_samples(0, 0, 0u);
            road_game_render(&game, screen);
            without_variation = screen_hash();
            road_game_set_edge_outer_samples(edge_outer_samples[0],
                edge_outer_samples[1], course.sample_count);
            if (with_variation != without_variation) {
                varying_edge_visible = i;
                break;
            }
        }
        printf("  RMTN outer variation visible at sample %u\n",
            varying_edge_visible);
        assert(varying_edge_visible != UINT_MAX);
        memcpy(saved_repeat_mips, roadside_repeat_mips,
            sizeof(saved_repeat_mips));
        for (i = 0u; i < course.sample_count; i += 50u) {
            unsigned int with_repeat, without_repeat, tile;
            road_game_seek(&game, i * 256u);
            road_game_render(&game, screen);
            with_repeat = screen_hash();
            for (tile = 0u; tile < roadside_count; ++tile)
                if (roadside_repeat_indices[tile] != tile) {
                    unsigned int level;
                    for (level = 0u; level <
                        ROAD_ROADSIDE_REPEAT_LEVELS; ++level)
                        roadside_repeat_mips[tile *
                            ROAD_ROADSIDE_REPEAT_LEVELS + level] =
                            roadside_tiles[tile];
                }
            road_game_render(&game, screen);
            without_repeat = screen_hash();
            memcpy(roadside_repeat_mips, saved_repeat_mips,
                sizeof(saved_repeat_mips));
            if (with_repeat != without_repeat) {
                repeat_visible = i;
                break;
            }
        }
        printf("  RMTN repeat visible at sample %u\n", repeat_visible);
        assert(repeat_visible != UINT_MAX);
        road_game_seek(&game, 0u);
    }
    if (preview_image_path && index == preview_course_index &&
        preview_sample_index != UINT_MAX) {
        road_game_seek(&game, preview_sample_index * 256u);
        road_game_render(&game, screen);
        save_preview_image();
    }
    if (index == 3u) {
        unsigned int position = game.distance, textured;
        unsigned int margin_textured, margin_missing;
        unsigned int adjusted = 0u;
        for (i = 10u; i < course.sample_count; ++i)
            if (terrain_mode[i] == 0u &&
                (surface_flags[i] & 0x10u) &&
                !(surface_flags[i] & 0x08u) &&
                (left_depth[i] || right_depth[i])) {
                adjusted = i;
                break;
            }
        assert(adjusted > 10u);
        printf("  Napa adjusted RSLD margin sample %u\n", adjusted);
        road_game_seek(&game, 3025u * 256u);
        road_game_render(&game, screen);
        margin_textured = screen_hash();
        road_game_set_margin_tiles(0, 0u);
        road_game_render(&game, screen);
        margin_missing = screen_hash();
        road_game_set_margin_tiles(margin_tiles, margin_tile_count);
        assert(margin_textured != margin_missing);
        road_game_seek(&game, (adjusted - 10u) * 256u);
        road_game_render(&game, screen);
        margin_textured = screen_hash();
        road_game_set_margin_tiles(0, 0u);
        road_game_render(&game, screen);
        margin_missing = screen_hash();
        road_game_set_margin_tiles(margin_tiles, margin_tile_count);
        assert(margin_textured != margin_missing);
        road_game_seek(&game, 600u * 256u);
        road_game_render(&game, screen);
        textured = screen_hash();
        road_game_set_hill_tiles(hill_tiles, hill_mips, 0,
            terrain_count);
        road_game_render(&game, screen);
        assert(screen_hash() != textured);
        road_game_set_hill_tiles(hill_tiles, 0, 0, terrain_count);
        road_game_render(&game, screen);
        assert(screen_hash() != textured);
        road_game_set_hill_tiles(hill_tiles, hill_mips,
            hill_narrow_mips, terrain_count);
        road_game_seek(&game, position);
    }
    if (index == 2u && roadside_count) {
        road_game_set_roadside_tiles(0, 0, 0, 0, 0u);
        road_game_render(&game, screen);
        assert(screen_hash() != hash);
        road_game_set_roadside_tiles(roadside_tiles,
            roadside_tile_heights, roadside_repeat_indices,
            roadside_repeat_mips,
            roadside_count);
    }
    if (index == 0u) {
        road_game_set_hill_samples(0, 0u);
        road_game_render(&game, screen);
        assert(screen_hash() != hash);
        road_game_set_hill_samples(hill_samples, course.sample_count);
        road_game_set_hill_tiles(0, 0, 0, 0u);
        road_game_render(&game, screen);
        assert(screen_hash() != hash);
        road_game_set_hill_tiles(hill_tiles, hill_mips,
            hill_narrow_mips, terrain_count);
        road_game_set_hill_profiles(0, 0u);
        road_game_render(&game, screen);
        assert(screen_hash() != hash);
        road_game_set_hill_profiles(hill_profiles,
                                     course.hill_profile_count);
        road_game_set_hill_samples(hill_samples, course.sample_count);
    }
    assert(course.branch_count > 0u);
    {
        unsigned int fork_distance = course.branch_samples[0] * 256u;
        unsigned int before, alternate, preview_base, preview_visible;
        unsigned int fork_count;
        road_game_seek(&game, fork_distance);
        road_game_render(&game, screen);
        before = screen_hash();
        course.left_depth = fork_left_depth;
        course.right_depth = fork_right_depth;
        assert(rr_course_load_selected(&catalog, &course, 1u));
        fork_hill_profile_count = course.hill_profile_count;
        memcpy(fork_hill_profiles, hill_profiles,
            fork_hill_profile_count * sizeof(hill_profiles[0]));
        memcpy(fork_hill_samples, hill_samples,
            course.sample_count * sizeof(hill_samples[0]));
        road_game_set_edge_profiles(edge_profiles[0],
            course.edge_profile_count[0], edge_profiles[1],
            course.edge_profile_count[1]);
        road_game_set_edge_outer_samples(edge_outer_samples[0],
            edge_outer_samples[1], course.sample_count);
        load_scenery(root, course.family_count);
        (void)load_terrain_tiles(root, course.hill_profile_count,
            course.sample_count, &roadside_count);
        for (i = 0u; i < course.sample_count; ++i)
            if (terrain_mode[i] == 0u) {
                unsigned int side;
                for (side = 0u; side < 2u; ++side)
                    if (surface_samples[i].selector[side] != 255u)
                        assert(surface_samples[i].tile_index[side] <
                               roadside_count);
            }
        road_game_set_hill_profiles(hill_profiles,
                                     course.hill_profile_count);
        road_game_set_hill_samples(hill_samples, course.sample_count);
        road_game_set_surface_samples(surface_samples,
            course.sample_count);
        fork_count = course.sample_count;
        memcpy(fork_curvature, curvature, fork_count);
        memcpy(fork_elevation, elevation, fork_count);
        memcpy(fork_left_width, left_width,
               fork_count * sizeof(fork_left_width[0]));
        memcpy(fork_right_width, right_width,
               fork_count * sizeof(fork_right_width[0]));
        memcpy(fork_left_margin, left_margin, fork_count);
        memcpy(fork_right_margin, right_margin, fork_count);
        memcpy(fork_terrain_mode, terrain_mode, fork_count);
        memcpy(fork_surface_flags, surface_flags, fork_count);
        memcpy(fork_surface_samples, surface_samples,
               fork_count * sizeof(fork_surface_samples[0]));
        memcpy(fork_left_edge_resource, left_edge_resource, fork_count);
        memcpy(fork_right_edge_resource, right_edge_resource, fork_count);
        memcpy(fork_left_edge_family, left_edge_family,
               fork_count * sizeof(fork_left_edge_family[0]));
        memcpy(fork_right_edge_family, right_edge_family,
               fork_count * sizeof(fork_right_edge_family[0]));
        course.left_depth = left_depth;
        course.right_depth = right_depth;
        road_game_set_course(curvature, course.sample_count);
        road_game_set_elevation(elevation, course.sample_count);
        road_game_set_widths(left_width, right_width, course.sample_count);
        road_game_set_cross_section(left_margin, right_margin,
            left_depth, right_depth, terrain_mode, course.sample_count);
        road_game_seek(&game, fork_distance);
        assert(game.distance == fork_distance);
        road_game_render(&game, screen);
        alternate = screen_hash();
        assert(rr_course_load_primary(&catalog, &course));
        road_game_set_edge_profiles(edge_profiles[0],
            course.edge_profile_count[0], edge_profiles[1],
            course.edge_profile_count[1]);
        road_game_set_edge_outer_samples(edge_outer_samples[0],
            edge_outer_samples[1], course.sample_count);
        load_scenery(root, course.family_count);
        (void)load_terrain_tiles(root, course.hill_profile_count,
            course.sample_count, &roadside_count);
        road_game_set_margin_resources(0u, left_edge_resource,
            right_edge_resource, left_edge_family, right_edge_family,
            surface_flags, terrain_mode,
            course.sample_count);
        load_fork_terrain_tiles(root, fork_count);
        road_game_set_fork_hill_profiles(fork_hill_profiles,
            fork_hill_profile_count);
        road_game_set_fork_hill_samples(fork_hill_samples, fork_count);
        road_game_set_fork_surface_samples(fork_surface_samples,
            fork_count);
        road_game_set_margin_resources(1u, fork_left_edge_resource,
            fork_right_edge_resource, fork_left_edge_family,
            fork_right_edge_family, fork_surface_flags,
            fork_terrain_mode, fork_count);
        if (index == 3u) {
            unsigned int at, rbld = 0u, tiled = 0u;
            for (at = fork_distance / 256u;
                 at < fork_distance / 256u + 33u; ++at) {
                rbld += fork_terrain_mode[at] == 0u;
                tiled += fork_surface_samples[at].tile_index[0] <
                    roadside_cache_count ||
                    fork_surface_samples[at].tile_index[1] <
                    roadside_cache_count;
            }
            printf("  Napa fork RBLD samples %u, tiled %u, cache %u\n",
                rbld, tiled, roadside_cache_count);
        }
        road_game_set_hill_profiles(hill_profiles,
                                     course.hill_profile_count);
        road_game_set_surface_samples(surface_samples,
            course.sample_count);
        road_game_set_course(curvature, course.sample_count);
        road_game_set_elevation(elevation, course.sample_count);
        road_game_set_widths(left_width, right_width, course.sample_count);
        road_game_set_cross_section(left_margin, right_margin,
            left_depth, right_depth, terrain_mode, course.sample_count);
        road_game_seek(&game, fork_distance);
        road_game_render(&game, screen);
        assert(screen_hash() == before);
        road_game_seek(&game, fork_distance -
            (index == 3u ? 2u : 10u) * 256u);
        {
            unsigned int sample = fork_distance / 256u;
            unsigned int different = 0u;
            unsigned int different_margins = 0u;
            unsigned int end = sample + 33u;
            if (end > fork_count) end = fork_count;
            if (end > course.sample_count) end = course.sample_count;
            for (; sample < end; ++sample) {
                different += fork_elevation[sample] != elevation[sample];
                different_margins +=
                    fork_left_margin[sample] != left_margin[sample] ||
                    fork_right_margin[sample] != right_margin[sample];
            }
            printf("  fork 33-node differences: elevation %u, RSLD margins %u\n",
                   different, different_margins);
        }
        road_game_render(&game, screen);
        preview_base = screen_hash();
        road_game_set_fork_preview(fork_curvature, fork_elevation,
            fork_left_width, fork_right_width, fork_count,
            fork_distance / 256u,
            course.branch_main_channels[0]);
        road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
            fork_count);
        road_game_set_fork_depths(fork_left_depth, fork_right_depth,
            fork_count);
        road_game_render(&game, screen);
        preview_visible = screen_hash();
        assert(preview_visible != preview_base);
        if (index == 3u) {
            unsigned int without_margin, without_fork_hill;
            road_game_set_fork_hill_profiles(0, 0u);
            road_game_render(&game, screen);
            without_fork_hill = screen_hash();
            assert(without_fork_hill != preview_visible);
            road_game_set_fork_hill_profiles(fork_hill_profiles,
                fork_hill_profile_count);
            road_game_render(&game, screen);
            assert(screen_hash() == preview_visible);
            road_game_set_margin_tiles(0, 0u);
            road_game_render(&game, screen);
            without_margin = screen_hash();
            assert(preview_visible == without_margin);
            assert(surface_flags[fork_distance / 256u] == 0u);
            assert(fork_surface_flags[fork_distance / 256u] == 0u);
            road_game_set_margin_tiles(margin_tiles, margin_tile_count);
            road_game_render(&game, screen);
            assert(screen_hash() == preview_visible);
            road_game_set_fork_preview(fork_curvature, elevation,
                fork_left_width, fork_right_width, fork_count,
                fork_distance / 256u, course.branch_main_channels[0]);
            road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
                fork_count);
            road_game_set_fork_depths(fork_left_depth, fork_right_depth,
                fork_count);
            road_game_render(&game, screen);
            assert(screen_hash() != preview_visible);
            road_game_set_fork_preview(fork_curvature, fork_elevation,
                fork_left_width, fork_right_width, fork_count,
                fork_distance / 256u, course.branch_main_channels[0]);
            road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
                fork_count);
            road_game_set_fork_depths(fork_left_depth, fork_right_depth,
                fork_count);
            road_game_render(&game, screen);
            assert(screen_hash() == preview_visible);
            road_game_set_fork_slopes(left_margin, right_margin,
                course.sample_count);
            road_game_render(&game, screen);
            assert(screen_hash() != preview_visible);
            road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
                fork_count);
            road_game_set_fork_depths(fork_left_depth, fork_right_depth,
                fork_count);
            road_game_render(&game, screen);
            assert(screen_hash() == preview_visible);
        }
        {
            unsigned int first = fork_distance / 256u;
            signed char saved = fork_elevation[first];
            fork_elevation[first] = saved == 100 ? -100 : 100;
            road_game_render(&game, screen);
            assert(screen_hash() != preview_visible);
            fork_elevation[first] = saved;
            road_game_render(&game, screen);
            assert(screen_hash() == preview_visible);
        }
        if (index == preview_course_index &&
            preview_sample_index == UINT_MAX) save_preview_image();
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        if (index == 3u) {
            unsigned int after_fork, without_fork;
            rr_u32 chosen_span, other_span;
            assert(rr_course_graph_load(&catalog, 0u, &junction_graph));
            assert(rr_course_graph_branch_span(&junction_graph, 0u, 0u,
                                               &chosen_span, &other_span));
            assert(rr_course_reconcile_fork_elevation(
                elevation, course.sample_count, fork_elevation, fork_count,
                fork_distance / 256u, chosen_span, other_span, 1));
            road_game_seek(&game, fork_distance + 1u * 256u);
            road_game_set_fork_preview(fork_curvature, fork_elevation,
                fork_left_width, fork_right_width, fork_count,
                fork_distance / 256u, course.branch_main_channels[0]);
            road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
                fork_count);
            road_game_set_fork_depths(fork_left_depth, fork_right_depth,
                fork_count);
            road_game_set_fork_span(fork_distance / 256u +
                                    RR_COURSE_JUNCTION_SAMPLES);
            road_game_render(&game, screen);
            after_fork = screen_hash();
            {
                unsigned int pixel, pale = 0u, bridge = 0u;
                for (pixel = 90u * ROAD_WIDTH;
                     pixel < 180u * ROAD_WIDTH; ++pixel) {
                    pale += screen[pixel] == 0xa534u;
                    bridge += screen[pixel] == 0x3188u;
                }
                /* Both original RGB555 center colors must survive the
                 * near and far fork quads in this source-data scene. */
                assert(pale > 0u && bridge > 0u);
            }
            if (preview_image_path && index == preview_course_index &&
                preview_sample_index == fork_distance / 256u + 1u)
                save_preview_image();
            road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
            road_game_render(&game, screen);
            without_fork = screen_hash();
            assert(after_fork != without_fork);
            printf("  Napa post-fork dual span: %08x / %08x\n",
                   after_fork, without_fork);
            road_game_set_fork_preview(fork_curvature, fork_elevation,
                fork_left_width, fork_right_width, fork_count,
                fork_distance / 256u, course.branch_main_channels[0]);
            road_game_set_fork_slopes(fork_left_margin, fork_right_margin,
                fork_count);
            road_game_set_fork_depths(fork_left_depth, fork_right_depth,
                fork_count);
            road_game_set_fork_reverse(fork_distance / 256u + chosen_span,
                                       fork_distance / 256u + other_span);
            road_game_seek(&game, fork_distance +
                (chosen_span - 70u) * 256u);
            road_game_render(&game, screen);
            if (preview_image_path && index == preview_course_index &&
                preview_sample_index == fork_distance / 256u +
                    chosen_span - 70u)
                save_preview_image();
            {
                unsigned int pixel, bridge = 0u;
                for (pixel = 90u * ROAD_WIDTH;
                     pixel < 180u * ROAD_WIDTH; ++pixel)
                    bridge += screen[pixel] == 0x3188u;
                /* This already-clear reverse span truncates overlapping
                 * inner profiles and shows the original RHIL center child
                 * instead of a solid bridge or an outer extension. */
                assert(bridge == 0u);
                assert(screen[90u * ROAD_WIDTH + 270u] == 0x29c4u);
            }
            road_game_seek(&game, fork_distance +
                (chosen_span - 10u) * 256u);
            road_game_render(&game, screen);
            after_fork = screen_hash();
            if (preview_image_path && index == preview_course_index &&
                preview_sample_index == fork_distance / 256u +
                    chosen_span - 10u)
                save_preview_image();
            road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
            road_game_render(&game, screen);
            without_fork = screen_hash();
            assert(after_fork != without_fork);
            printf("  Napa reverse join: %08x / %08x\n",
                   after_fork, without_fork);
        }
        road_game_seek(&game, index == 4u ? 0u : fork_distance);
        for (i = 1u; i < ROAD_TRAFFIC_COUNT; ++i)
            game.traffic[i].distance = 0u;
        for (i = 0u; i < ROAD_CROSSING_COUNT; ++i)
            game.crossing[i].distance = 0u;
        road_game_set_cars(&test_car, 1u);
        /* Original track_object_mode_update chooses the middle CANS
         * state between 0x500 and 0xA00 world units. */
        game.traffic[0].distance = game.distance + 1400u;
        game.traffic[0].lane = game.lane;
        game.traffic[0].collision_mode = 0u;
        road_game_render(&game, screen);
        assert(screen_hash() != before);
        {
            unsigned int static_hash = screen_hash();
            unsigned int mapped_hash;
            road_game_set_car_animations(&test_car_animation, 1u);
            road_game_render(&game, screen);
            mapped_hash = screen_hash();
            assert(mapped_hash != static_hash);
            road_game_set_car_animations(&test_car_animation_anchored, 1u);
            road_game_render(&game, screen);
            if (screen_hash() != mapped_hash)
                anchored_course_mask |= 1u << index;
            road_game_set_car_animations(&test_car_animation, 1u);
            game.elapsed_ms += 1000u;
            road_game_render(&game, screen);
            assert(screen_hash() == mapped_hash);
        }
        game.speed = 100u;
        game.traffic[0].distance = game.distance + 100u;
        game.traffic[0].speed = 0u;
        road_game_step(&game, 0u, 1u);
        assert(game.speed < 100u);
        road_game_set_cars(0, 0u);
        printf("  fork %u: main %08x, alternate %08x, preview %08x\n",
               fork_distance / 256u, before, alternate, preview_visible);
    }
    fclose(file);
    printf("%s: %u samples, frame hash %08x\n",
           rr_course_names[index], course.sample_count, hash);
    return hash;
}

int main(int argc, char **argv)
{
    static unsigned int reciprocal[0x1100];
    unsigned int index = 0u, first, i;
    check_asymmetric_road_projection();
    check_near_surface_quarters();
    check_fractional_node_horizon();
    check_textured_crest_occlusion();
    check_original_hud_numeric_readouts();
    assert(road_car_select_frame(12u, 0, 30) == 0u);
    assert(road_car_select_frame(12u, -50, 20) == 4u);
    assert(road_car_select_frame(12u, 50, 10) == 8u);
    assert(road_car_select_frame(12u, 100, 30) == 9u);
    assert(road_car_select_frame(3u, 50, 10) == 2u);
    assert(road_fork_channel(0, 512u, 256u, 128u) == 1u);
    assert(road_fork_channel(-40, 512u, 256u, 128u) == 0u);
    assert(argc >= 2 && argc <= 5);
    if (argc >= 3) preview_image_path = argv[2];
    if (argc >= 4) {
        preview_course_index = (unsigned int)atoi(argv[3]);
        assert(preview_course_index < RR_COURSE_COUNT);
    }
    if (argc == 5)
        preview_sample_index = (unsigned int)atoi(argv[4]);
    {
        char path[1024];
        unsigned char bytes[4];
        FILE *table;
        assert(snprintf(path, sizeof(path), "%s/HalfOneOver.table",
                        argv[1]) < (int)sizeof(path));
        table = fopen(path, "rb");
        assert(table);
        for (i = 0u; i < 0x1100u; ++i) {
            assert(fread(bytes, 1u, 4u, table) == 4u);
            reciprocal[i] = ((unsigned int)bytes[0] << 24u) |
                ((unsigned int)bytes[1] << 16u) |
                ((unsigned int)bytes[2] << 8u) | bytes[3];
        }
        assert(fgetc(table) == EOF);
        fclose(table);
        road_game_set_reciprocal_table(reciprocal, 0x1100u);
        assert(road_car_projected_height(&test_car_animation,
                   &test_car_frames[0], 512u) ==
               (int)((4u * reciprocal[256u] * 5u + 0x8000u) >> 16u));
        assert(road_car_projected_height(&test_car_animation,
                   &test_car_frames[1], 1400u) ==
               (int)((4u * reciprocal[700u] * 10u + 0x8000u) >> 16u));
    }
    load_road_textures(argv[1]);
    assert(rr_course_move(0u, -1) == RR_COURSE_COUNT - 1u);
    assert(rr_course_move(RR_COURSE_COUNT - 1u, 1) == 0u);
    first = load_and_render(argv[1], index);
    if (!preview_image_path) {
        road_game_set_reciprocal_table(0, 0u);
        assert(load_and_render(argv[1], index) != first);
        road_game_set_reciprocal_table(reciprocal, 0x1100u);
        assert(load_and_render(argv[1], index) == first);
        puts("Original depth table changes road projection: PASS");
    }
    for (i = 1u; i < RR_COURSE_COUNT; ++i) {
        index = rr_course_move(index, 1);
        assert(index == i);
        (void)load_and_render(argv[1], index);
    }
    index = rr_course_move(index, 1);
    assert(index == 0u);
    assert(load_and_render(argv[1], index) == first);
    assert(anchored_course_mask == (1u << RR_COURSE_COUNT) - 1u);
    puts("Unified course switching passed.");
    return 0;
}

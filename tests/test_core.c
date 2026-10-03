#include "road_core.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef ROAD_TEST_ORIGINAL
#include "local-data/road_course_data.h"
#include "local-data/road_backdrop_data.h"
#include "local-data/road_scenery_data.h"
#include "local-data/road_object_data.h"
#include "local-data/road_static_data.h"
#include "local-data/road_bike_data.h"
#endif

static road_pixel_t landscape[ROAD_PIXELS];
static road_pixel_t portrait[ROAD_PIXELS];

int main(int argc, char **argv)
{
    road_game_t game;
    unsigned int start_distance;
    unsigned int count = 0u;
    int i;
    road_game_init(&game);
#ifdef ROAD_TEST_ORIGINAL
#if ROAD_COURSE_SAMPLE_COUNT > 0
    if (ROAD_COURSE_SEGMENT_COUNT <= 1 ||
        ROAD_COURSE_SAMPLE_COUNT <= 900) return 8;
    road_game_set_course(g_original_course_curvature,
                         ROAD_COURSE_SAMPLE_COUNT);
    road_game_set_elevation(g_original_course_elevation,
                            ROAD_COURSE_SAMPLE_COUNT);
    road_game_set_backdrop(g_original_backdrop_indices,
                           g_original_backdrop_palette,
                           ROAD_BACKDROP_WIDTH, ROAD_BACKDROP_HEIGHT);
#if ROAD_SCENERY_COUNT > 0
    road_game_set_scenery(g_original_scenery, ROAD_SCENERY_COUNT);
#if ROAD_SCENERY_COUNT >= 6
    {
        const road_sprite_t *marker = &g_original_scenery[5];
        unsigned int opaque = 0u;
        unsigned int pixel;
        for (pixel = 0u; pixel < marker->width * marker->height; ++pixel)
            if (marker->pixels[pixel] != 0xf81fu) ++opaque;
        if (marker->width != 19u || marker->height != 35u || opaque < 100u)
            return 14;
    }
#endif
#endif
#if ROAD_OBJECT_COUNT > 0
    for (i = 0; i < ROAD_OBJECT_COUNT; ++i) {
        const road_scenery_placement_t *placement =
            &g_original_object_placements[i];
        if (placement->sample >= ROAD_COURSE_SAMPLE_COUNT ||
            placement->variant >= ROAD_SCENERY_COUNT ||
            placement->side > 1u || placement->scale == 0u ||
            (i && placement->sample < g_original_object_placements[i - 1].sample))
            return 12;
    }
    road_game_set_scenery_placements(g_original_object_placements,
                                     ROAD_OBJECT_COUNT);
#endif
#if ROAD_STATIC_COUNT > 0
    for (i = 0; i < ROAD_STATIC_COUNT; ++i) {
        const road_static_placement_t *placement =
            &g_original_static_placements[i];
        if (placement->sample >= ROAD_COURSE_SAMPLE_COUNT ||
            placement->variant >= ROAD_STATIC_SPRITE_COUNT ||
            (i && placement->sample < g_original_static_placements[i - 1].sample))
            return 16;
    }
    road_game_set_static_scenery(g_original_static_sprites,
                                ROAD_STATIC_SPRITE_COUNT,
                                g_original_static_placements,
                                ROAD_STATIC_COUNT);
#endif
#if ROAD_BIKE_COUNT > 0
    road_game_set_bikes(g_original_bikes, ROAD_BIKE_COUNT);
#if ROAD_BIKE_COUNT > ROAD_BIKE_OPPONENT_INDEX
    {
        unsigned int changed = 0u;
        game.attack_ms = ROAD_ATTACK_DURATION_MS;
        road_game_render(&game, landscape);
        game.attack_ms = ROAD_ATTACK_DURATION_MS / 2u;
        road_game_render(&game, portrait);
        for (i = 0; i < ROAD_PIXELS; ++i)
            if (landscape[i] != portrait[i]) ++changed;
        if (changed < 100u) return 15;
        game.attack_ms = 0u;
    }
#endif
#if ROAD_BIKE_COUNT >= ROAD_BIKE_FRAME_COUNT
    {
        unsigned int changed = 0u;
        road_game_apply_collision_damage(&game, 8u, 1600u,
                                          ROAD_DAMAGE_STANDARD);
        road_game_render(&game, landscape);
        for (i = 0; i < 16; ++i)
            road_game_step(&game, 0u, 100u);
        road_game_render(&game, portrait);
        for (i = 0; i < ROAD_PIXELS; ++i)
            if (landscape[i] != portrait[i]) ++changed;
        if (changed < 100u) return 17;
        road_game_init(&game);
    }
#endif
#endif
    game.distance = 100u * 256u;
#else
    return 8;
#endif
#endif
    for (i = 0; i < 45; ++i)
        road_game_step(&game, ROAD_ACCEL | (i > 25 ? ROAD_RIGHT : 0u), 33u);
    if (!game.distance || !game.speed || game.lane <= 0) return 1;
    start_distance = game.distance;
    road_game_step(&game, ROAD_BRAKE, 33u);
    if (game.distance <= start_distance) return 2;
    road_game_render(&game, landscape);
    road_rotate_counterclockwise(landscape, portrait);
    if (portrait[0] != landscape[ROAD_WIDTH - 1] ||
        portrait[(ROAD_WIDTH - 1) * ROAD_HEIGHT + ROAD_HEIGHT - 1] !=
            landscape[(ROAD_HEIGHT - 1) * ROAD_WIDTH]) return 3;
    for (i = 0; i < ROAD_PIXELS; ++i)
        if (landscape[i] != landscape[0]) ++count;
    if (count < ROAD_PIXELS / 2) return 4;
    if (argc > 1) {
        FILE *file = fopen(argv[1], "wb");
        if (!file) return 5;
        fputs("P6\n320 240\n255\n", file);
        for (i = 0; i < ROAD_PIXELS; ++i) {
            road_pixel_t pixel = landscape[i];
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(((pixel >> 11) & 31) * 255 / 31);
            rgb[1] = (unsigned char)(((pixel >> 5) & 63) * 255 / 63);
            rgb[2] = (unsigned char)((pixel & 31) * 255 / 31);
            if (fwrite(rgb, 1, 3, file) != 3) return 6;
        }
        if (fclose(file)) return 7;
    }
#ifdef ROAD_TEST_ORIGINAL
    {
        unsigned int changed = 0u;
#if ROAD_OBJECT_COUNT > 0
        game.distance = 100u * 256u;
        road_game_render(&game, landscape);
        road_game_set_scenery_placements(NULL, 0u);
        road_game_render(&game, portrait);
        for (i = 0; i < ROAD_PIXELS; ++i)
            if (landscape[i] != portrait[i]) ++changed;
        if (changed < 100u) return 13;
        road_game_set_scenery_placements(g_original_object_placements,
                                         ROAD_OBJECT_COUNT);
        changed = 0u;
#endif
#if ROAD_STATIC_COUNT > 0
        game.distance = 149u * 256u;
        road_game_render(&game, landscape);
        road_game_set_static_scenery(NULL, 0u, NULL, 0u);
        road_game_render(&game, portrait);
        for (i = 0; i < ROAD_PIXELS; ++i)
            if (landscape[i] != portrait[i]) ++changed;
        if (changed < 100u) return 17;
        road_game_set_static_scenery(g_original_static_sprites,
                                    ROAD_STATIC_SPRITE_COUNT,
                                    g_original_static_placements,
                                    ROAD_STATIC_COUNT);
        changed = 0u;
#endif
        game.distance = 300u * 256u;
        road_game_render(&game, landscape);
        road_game_set_elevation(NULL, 0u);
        road_game_render(&game, portrait);
        for (i = 0; i < ROAD_PIXELS; ++i)
            if (landscape[i] != portrait[i]) ++changed;
        if (changed < 100u) return 11;
    }
    road_game_set_elevation(g_original_course_elevation,
                            ROAD_COURSE_SAMPLE_COUNT);
    game.distance = ROAD_COURSE_SAMPLE_COUNT * 256u - 1u;
    game.speed = 180u;
    road_game_step(&game, ROAD_ACCEL, 33u);
    if (!game.finished || game.speed != 0u ||
        game.distance != ROAD_COURSE_SAMPLE_COUNT * 256u) return 9;
#ifdef ROAD_USE_UPSTREAM_PATH
    {
        int expected = 0;
        for (i = 0; i < ROAD_COURSE_SAMPLE_COUNT; ++i)
            expected += g_original_course_elevation[i];
        if (game.track_elevation != expected) return 18;
        road_game_seek(&game, 0u);
        if (game.track_elevation != 0 || game.distance != 0u) return 19;
        road_game_seek(&game, ROAD_COURSE_SAMPLE_COUNT * 256u);
    }
#endif
    road_game_render(&game, landscape);
    if (landscape[107 * ROAD_WIDTH + 124] == landscape[97 * ROAD_WIDTH + 108])
        return 10;
#endif
    printf("distance=%u speed=%u lane=%d pixels=%u\n",
        game.distance, game.speed, game.lane, count);
    return 0;
}

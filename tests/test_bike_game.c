#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bike_spec_runtime.h"
#include "road_core.h"

static road_pixel_t before_frame[ROAD_PIXELS];
static road_pixel_t after_frame[ROAD_PIXELS];

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, file) == size;
}

int main(int argc, char **argv)
{
    static unsigned int reciprocal[0x1100];
    rr_rsrc_file_t catalog;
    rr_bike_spec_t specs[15];
    road_game_t game;
    road_pixel_t pixel = 0xffffu;
    road_pixel_t effect_blue = 0x001fu, effect_red = 0xf800u;
    road_sprite_t effect_frames[ROAD_EFFECT_SPRITE_FRAMES] = {
        [ROAD_EFFECT_STANDING_FRAME] = {1u, 1u, &effect_blue},
        [ROAD_EFFECT_MOTION_FIRST] = {1u, 1u, &effect_blue},
        [ROAD_EFFECT_MOTION_FIRST + 1u] = {1u, 1u, &effect_red},
        [ROAD_EFFECT_FALL_FIRST] = {1u, 1u, &effect_red},
        [ROAD_EFFECT_FALL_FIRST + 1u] = {1u, 1u, &effect_blue}
    };
    unsigned char effect_ticks[ROAD_EFFECT_SPRITE_FRAMES] = {0};
    road_sprite_t sprite = {1u, 1u, &pixel};
    road_static_placement_t hazard = {1u, 0, 0u, 0u, 0u};
    road_effect_placement_t effect = {20u, 0, 10u, 0u, 0u, 0u, 255u};
    unsigned char moving[1] = {1u};
    FILE *file;
    long size;
    unsigned int i;
    assert(argc == 3);
    {
        unsigned char word[4];
        FILE *table = fopen(argv[2], "rb");
        assert(table);
        for (i = 0u; i < 0x1100u; ++i) {
            assert(fread(word, 1u, 4u, table) == 4u);
            reciprocal[i] = ((unsigned int)word[0] << 24u) |
                ((unsigned int)word[1] << 16u) |
                ((unsigned int)word[2] << 8u) | word[3];
        }
        assert(fgetc(table) == EOF);
        fclose(table);
        assert(reciprocal[256u] == 0x8000u &&
               reciprocal[462u] == 0x2ffdu);
        road_game_set_reciprocal_table(reciprocal, 0x1100u);
    }
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    for (i = 0u; i < 15u; ++i)
        assert(rr_bike_spec_load(&catalog, i + 1u, &specs[i]));
    assert(specs[0].forward_speed_factor == 38u);
    assert(specs[0].gear[0].acceleration == 256u);
    assert(specs[0].gear[0].upshift == 3000u);
    assert(specs[0].gear[0].pitch_per_forward_velocity_8_8 == 5000);
    assert(specs[0].gear[5].pitch_per_forward_velocity_8_8 == 512);
    assert(specs[0].base_engine_pitch == 0);
    assert(specs[0].braking_factor == -60);
    assert(specs[7].forward_speed_factor == 48u);
    assert(specs[7].steering_speed_scale == 256u);
    assert(specs[0].maximum_bike_health == 20000u);
    assert(specs[0].collision_profile_id == 0u);
    assert(specs[0].minimum_acceleration == -800);
    assert(specs[0].contact_threshold == 1024u);
    assert(specs[0].bounce_scale == 160u);
    assert(specs[0].impact_strength == 160u);
    assert(specs[0].slide_grip_scale_8_8 == 320u);
    assert(specs[0].surface_heading_scale_8_8 == 196u);
    assert(!rr_bike_spec_load(&catalog, 16u, &specs[0]));
    fclose(file);

    {
        road_game_t pitch_game;
        road_game_init(&pitch_game);
        road_game_set_bike_spec(&pitch_game, &specs[0]);
        assert(pitch_game.engine_pitch == specs[0].base_engine_pitch);
        road_game_step(&pitch_game, ROAD_ACCEL, 33u);
        assert(pitch_game.engine_pitch == 0x2000);
        assert(pitch_game.engine_gauge_pitch == -0x1000);
        puts("SPEC engine pitch and original gauge ramp: PASS");
    }
    {
        static const signed char short_course[10] = {0};
        road_game_t finish_game;
        road_game_set_course(short_course, 10u);
        road_game_set_finish_sample(3u);
        road_game_init(&finish_game);
        road_game_seek(&finish_game, 3u * 256u);
        road_game_step(&finish_game, 0u, 0u);
        assert(finish_game.finished && finish_game.distance == 3u * 256u);
        road_game_set_course(0, 0u);
        puts("Level-gated race finish: PASS");
    }
    {
        road_game_t fight;
        unsigned int before;
        road_game_init(&fight);
        fight.opponents[0].distance = fight.distance + 560u;
        fight.opponents[0].lane = 35;
        before = fight.opponents[0].distance;
        road_game_step(&fight, ROAD_ATTACK | ROAD_KICK, 16u);
        assert(fight.hits == 1u && fight.attack_style == 1u);
        assert(fight.attack_side == 1u);
        assert(fight.opponents[0].lane >= 47);
        assert(fight.opponents[0].hit_reaction_ms > 0u);
        assert(fight.opponents[0].distance > before);
        before = fight.opponents[0].distance;
        road_game_step(&fight, 0u, 16u);
        assert(fight.opponents[0].distance > before);
        assert(fight.opponents[0].hit_reaction_ms < 600u);
        road_game_init(&fight);
        fight.opponents[0].distance = fight.distance + 560u;
        fight.opponents[0].lane = -28;
        road_game_step(&fight, ROAD_ATTACK, 16u);
        assert(fight.hits == 1u && fight.attack_style == 0u);
        assert(fight.attack_side == 0u);
        assert(fight.opponents[0].hit_reaction_ms > 0u);
        puts("Kick, whip, hit reaction, and opponent motion: PASS");
    }
    {
        static const signed char flat[256] = {0};
        static signed char fork_elevation[256];
        static signed char curved_fork[256];
        static unsigned short main_width[256], fork_width[256];
        static const road_pixel_t strip_colors[3] = {
            0xf800u, 0x07e0u, 0x001fu
        };
        static road_sprite_t strips[27];
        road_game_t fork_game;
        unsigned int x, y;
        for (i = 0u; i < 256u; ++i) {
            fork_elevation[i] = 0;
            curved_fork[i] = i == 1u ? 12 : 0;
            main_width[i] = 512u;
            fork_width[i] = 768u;
        }
        for (i = 0u; i < 27u; ++i) {
            strips[i].width = 1u;
            strips[i].height = 1u;
            strips[i].pixels = &strip_colors[i / 9u];
        }
        road_game_set_course(flat, 256u);
        road_game_set_elevation(flat, 256u);
        road_game_set_widths(main_width, main_width, 256u);
        road_game_set_surface_textures(strips, 27u);
        road_game_init(&fork_game);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        for (y = 125u; y < 170u; ++y)
            for (x = 110u; x < 190u; ++x)
                assert(before_frame[y * ROAD_WIDTH + x] ==
                       after_frame[y * ROAD_WIDTH + x]);
        road_game_set_fork_span(40u);
        road_game_seek(&fork_game, 10u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(curved_fork, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_span(40u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        road_game_seek(&fork_game, 40u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) == 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_span(40u);
        road_game_set_fork_span(0u);
        road_game_seek(&fork_game, 100u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) == 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_reverse(200u, 204u);
        road_game_seek(&fork_game, 85u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) == 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_reverse(200u, 204u);
        road_game_seek(&fork_game, 110u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_reverse(200u, 204u);
        road_game_seek(&fork_game, 190u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 0u);
        road_game_set_fork_reverse(200u, 204u);
        road_game_seek(&fork_game, 200u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) == 0);
        road_game_set_fork_preview(flat, fork_elevation,
            fork_width, fork_width, 256u, 1u, 1u);
        road_game_set_fork_reverse(200u, 204u);
        road_game_seek(&fork_game, 190u * 256u);
        road_game_render(&fork_game, before_frame);
        road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
        road_game_render(&fork_game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        road_game_set_course(0, 0u);
        road_game_set_elevation(0, 0u);
        road_game_set_widths(0, 0, 0u);
        road_game_set_surface_textures(0, 0u);
        puts("Selected fork overlap and post-branch span: PASS");
    }

    road_game_init(&game);
    assert(game.opponents[ROAD_OPPONENT_COUNT - 1u].distance >
           game.opponents[0].distance);
    game.distance = 1000u;
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i) {
        game.opponents[i].distance = 900u;
        game.opponents[i].lane = -100;
    }
    game.opponents[1].distance = 1100u;
    assert(road_game_finish_place(&game) == 1u);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 1100u;
    assert(road_game_finish_place(&game) == ROAD_OPPONENT_COUNT);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        if (i != 1u) game.opponents[i].distance = 900u;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].distance < game.distance);
    assert(road_game_finish_place(&game) == 1u);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 100000000u;
    game.opponents[0].distance = 100u;
    game.opponents[0].lane = game.lane;
    game.opponents[0].route_visible = 0u;
    game.opponents[0].graph_curve_speed = 20u;
    road_game_step(&game, ROAD_ATTACK, 33u);
    assert(game.hits == 0u && game.rider_hits == 0u);
    assert(game.opponents[0].distance > 100u);
    assert(game.opponents[0].speed ==
           game.opponents[0].base_speed - 1u);
    game.graph_rank = 7u;
    game.graph_rank_enabled = 1u;
    assert(road_game_finish_place(&game) == 7u);
    road_game_init(&game);
    road_game_apply_collision_damage(&game, 4u, 500u,
                                      ROAD_DAMAGE_STANDARD);
    assert(game.rider_health == 2244u &&
           game.rider_recovery_ceiling == 2484u &&
           game.bike_health == 19500u);
    road_game_apply_collision_damage(&game, 8u, 80u,
                                      ROAD_DAMAGE_GLANCING);
    assert(game.rider_health == 2180u && game.bike_health == 19490u);
    road_game_apply_collision_damage(&game, 20u, 0u,
                                      ROAD_DAMAGE_SEVERE);
    assert(game.rider_health == 0u && game.recovery_ms == 1500u);
    for (i = 0u; i < 15u; ++i) road_game_step(&game, ROAD_ACCEL, 100u);
    assert(game.recovery_ms == 0u && game.rider_health ==
           game.rider_recovery_ceiling && game.speed == 0u);
    road_game_init(&game);
    game.speed = 90u;
    road_game_apply_collision_damage(&game, 7u, 1600u,
                                      ROAD_DAMAGE_GLANCING);
    assert(game.recovery_ms == 0u && game.crash_elapsed_ms == 0u);
    assert(game.rider_health == game.rider_max_health &&
           game.bike_health == game.bike_max_health - 200u);
    road_game_apply_collision_damage(&game, 8u, 1600u,
                                      ROAD_DAMAGE_GLANCING);
    assert(game.recovery_ms == 0u && game.crash_elapsed_ms == 0u);
    road_game_init(&game);
    game.speed = 90u;
    road_game_apply_collision_damage(&game, 8u, 1600u,
                                      ROAD_DAMAGE_STANDARD);
    assert(game.recovery_ms == 3600u && game.crash_elapsed_ms == 1u);
    road_game_set_cars(&sprite, 1u);
    game.opponents[0].distance = game.distance + 480u;
    game.opponents[0].lane = 0;
    game.traffic[0].distance = game.distance + 480u;
    game.traffic[0].lane = 0;
    game.traffic[0].speed = 40u;
    game.traffic[0].travel_direction = 1;
    road_game_set_effects(&game, &effect, 1u);
    game.effects[0].active = 1u;
    game.effects[0].distance = game.distance + 480u;
    game.effects[0].lane = 0;
    game.effects[0].age_ms = 0u;
    game.effects[0].state_ms = 0u;
    game.effects[0].behavior = ROAD_EFFECT_WAITING;
    { unsigned int health_before = game.bike_health;
    road_game_step(&game, ROAD_ACCEL | ROAD_LEFT, 100u);
    assert(game.bike_health == health_before);
    }
    assert(game.speed == 0u && game.crash_elapsed_ms == 101u &&
           game.recovery_ms == 3500u);
    assert(game.opponents[0].distance > 480u &&
           game.traffic[0].distance > 480u &&
           game.effects[0].age_ms == 100u);
    assert(game.distance == 0u && game.lane == 0);
    for (i = 0u; i < 35u; ++i)
        road_game_step(&game, ROAD_ACCEL, 100u);
    assert(game.recovery_ms == 0u && game.crash_elapsed_ms == 0u);
    road_game_set_effects(&game, 0, 0u);
    road_game_set_cars(0, 0u);

    {
        static const signed char flat[32] = {0};
        static const road_pixel_t colors[3] = {
            0x1234u, 0x2345u, 0x3456u
        };
        static const unsigned int source_heights[3] = {5u, 10u, 20u};
        static const unsigned int expected_pixels[7] = {
            3u, 7u, 8u, 8u, 15u, 15u, 19u
        };
        road_pixel_t scale_pixels[3][20];
        road_sprite_t frames[3];
        road_sprite_t baseline = {1u, 10u, scale_pixels[1]};
        road_static_placement_t placement = {8u, 0, 0u, 0u, 0u};
        unsigned int distances[7] = {
            0u, 1252u, 1253u, 1280u, 1576u, 1577u, 1648u
        };
        unsigned int expected_buckets[7] = {
            0u, 0u, 1u, 1u, 1u, 2u, 2u
        };
        unsigned int bucket, case_index;
        for (bucket = 0u; bucket < 3u; ++bucket) {
            for (i = 0u; i < source_heights[bucket]; ++i)
                scale_pixels[bucket][i] = colors[bucket];
            frames[bucket].width = 1u;
            frames[bucket].height = source_heights[bucket];
            frames[bucket].pixels = scale_pixels[bucket];
        }
        road_game_set_course(flat, 32u);
        road_game_set_elevation(flat, 32u);
        road_game_set_static_scenery(&baseline, 1u, &placement, 1u);
        road_game_set_static_scale_frames(frames, 1u);
        for (case_index = 0u; case_index < 7u; ++case_index) {
            unsigned int relative = 2048u - distances[case_index];
            unsigned int selected = expected_buckets[case_index];
            unsigned int pixel_index, found = 0u;
            road_game_init(&game);
            road_game_seek(&game, distances[case_index]);
            placement.lateral = (short)(55u * 111u /
                (128000u / relative));
            for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
                game.opponents[i].distance = 100000000u;
            for (i = 0u; i < ROAD_TRAFFIC_COUNT; ++i)
                game.traffic[i].distance = 100000000u;
            road_game_render(&game, before_frame);
            for (pixel_index = 0u; pixel_index < ROAD_PIXELS;
                 ++pixel_index) {
                if (before_frame[pixel_index] == colors[selected])
                    ++found;
                if (before_frame[pixel_index] ==
                    colors[(selected + 1u) % 3u] ||
                    before_frame[pixel_index] ==
                    colors[(selected + 2u) % 3u])
                    assert(0 && "wrong static scale frame selected");
            }
            assert(found == expected_pixels[case_index]);
            if (case_index == 3u) {
                road_static_frame_anchor_t anchors[3] = {{0}};
                unsigned int before_at = 0u, after_at = 0u;
                for (pixel_index = 0u; pixel_index < ROAD_PIXELS;
                     ++pixel_index)
                    if (before_frame[pixel_index] == colors[1]) {
                        before_at = pixel_index;
                        break;
                    }
                anchors[1].x = 2;
                anchors[1].y = 5;
                anchors[1].valid = 1u;
                road_game_set_static_frame_anchors(anchors, 1u);
                road_game_render(&game, after_frame);
                for (pixel_index = 0u; pixel_index < ROAD_PIXELS;
                     ++pixel_index)
                    if (after_frame[pixel_index] == colors[1]) {
                        after_at = pixel_index;
                        break;
                    }
                assert(after_at == before_at + 4u * ROAD_WIDTH - 1u);
                road_game_set_static_frame_anchors(0, 0u);
            }
        }
        road_game_set_static_scenery(0, 0u, 0, 0u);
        road_game_set_course(0, 0u);
        road_game_set_elevation(0, 0u);
        puts("Three original static scale frame distances: PASS");
    }

    road_game_set_course(0, 0u);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_init(&game);
    road_game_set_bike_spec(&game, &specs[0]);
    assert(game.bike_physics_ready && game.accel_limit == 9728);
    assert(game.accel_rise_step == 972 && game.brake_target == -15360);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 100000000u;
    road_game_step(&game, ROAD_ACCEL, 33u);
    assert(game.throttle_control == 1944 && game.velocity_raw == 0u);
    for (i = 1u; i < 300u; ++i)
        road_game_step(&game, ROAD_ACCEL, 33u);
    assert(game.speed > 60u && game.gear >= 2u);
    for (i = 0u; i < 20u; ++i)
        road_game_step(&game, ROAD_ACCEL | ROAD_RIGHT, 33u);
    assert(game.steering_angle_raw > 0 && game.lane > 0);
    for (i = 0u; i < 50u; ++i)
        road_game_step(&game, ROAD_ACCEL | ROAD_LEFT, 33u);
    assert(game.steering_angle_raw < 0);
    {
        unsigned int accelerated = game.speed;
        for (i = 0u; i < 90u; ++i)
            road_game_step(&game, 0u, 33u);
        assert(game.speed < accelerated);
    }
    road_game_init(&game);
    road_game_set_bike_spec(&game, &specs[0]);
    game.speed = 100u;
    for (i = 0u; i < 60u; ++i)
        road_game_step(&game, ROAD_BRAKE, 33u);
    assert(game.speed < 100u);

    {
        signed char flat_curve[8] = {0};
        signed char crest[8] = {-127, 0, 0, 0, 0, 0, 0, 0};
        road_game_set_course(flat_curve, 8u);
        road_game_set_elevation(crest, 8u);
        road_game_set_widths(0, 0, 0u);
        road_game_set_static_scenery(0, 0u, 0, 0u);
        road_game_init(&game);
        road_game_set_bike_spec(&game, &specs[0]);
        road_game_seek(&game, 200u);
        game.speed = 200u;
        road_game_step(&game, 0u, 33u);
        assert(game.vertical_height_8_8 > 0);
        assert(game.surface_contact_scale_8_8 == 0u);
        road_game_render(&game, before_frame);
        game.vertical_height_8_8 = 0;
        road_game_render(&game, after_frame);
        assert(memcmp(before_frame, after_frame,
                      sizeof(before_frame)) != 0);
        for (i = 0u; i < 30u; ++i)
            road_game_step(&game, 0u, 33u);
        assert(game.vertical_height_8_8 == 0);
        assert(game.surface_contact_scale_8_8 != 0u);
        road_game_set_course(0, 0u);
        road_game_set_elevation(0, 0u);
    }
    {
        road_game_t on_road, off_road;
        road_game_init(&on_road);
        road_game_set_bike_spec(&on_road, &specs[0]);
        on_road.speed = 190u;
        on_road.steering_angle_raw = 11000;
        off_road = on_road;
        off_road.lane = 120;
        road_game_step(&on_road, ROAD_ACCEL | ROAD_RIGHT, 33u);
        road_game_step(&off_road, ROAD_ACCEL | ROAD_RIGHT, 33u);
        assert(on_road.slip_amount == 0u);
        assert(off_road.slip_amount > 0u);
        on_road.slip_amount = 80u;
        off_road.slip_amount = 80u;
        assert(road_game_road_skid_active(&on_road));
        assert(!road_game_road_skid_active(&off_road));
        on_road.surface_contact_scale_8_8 = 0u;
        assert(!road_game_road_skid_active(&on_road));
    }
    {
        unsigned int frames;
        road_game_set_course(0, 0u);
        road_game_set_widths(0, 0, 0u);
        road_game_set_static_scenery(0, 0u, 0, 0u);
        road_game_init(&game);
        road_game_set_bike_spec(&game, &specs[0]);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        game.lane = 120;
        game.speed = 90u;
        road_game_apply_collision_damage(&game, 8u, 1600u,
                                         ROAD_DAMAGE_STANDARD);
        assert(game.recovery_ms == 3600u && game.speed == 0u);
        for (frames = 0u; frames < 120u; ++frames)
            road_game_step(&game, ROAD_ACCEL | ROAD_LEFT, 33u);
        assert(game.recovery_ms == 0u);
        for (frames = 0u; frames < 180u; ++frames)
            road_game_step(&game, ROAD_ACCEL |
                (game.lane > 0 ? ROAD_LEFT : 0u), 33u);
        assert(game.speed > 20u && game.lane > -110 && game.lane < 110);
    }
    {
        road_game_t narrow, crossing;
        road_game_set_cars(&sprite, 1u);
        road_game_init(&narrow);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            narrow.opponents[i].distance = 100000000u;
        for (i = 1u; i < ROAD_TRAFFIC_COUNT; ++i)
            narrow.traffic[i].distance = 0u;
        narrow.traffic[0].distance = 480u;
        narrow.traffic[0].lane = 35;
        narrow.traffic[0].speed = 0u;
        narrow.traffic[0].collision_mode = 0u;
        crossing = narrow;
        crossing.traffic[0].collision_mode = 1u;
        crossing.traffic[0].target_lane = 125;
        road_game_step(&narrow, 0u, 33u);
        road_game_step(&crossing, 0u, 33u);
        assert(narrow.player_stun_ms == 0u);
        assert(crossing.player_stun_ms > 0u);
        road_game_set_cars(0, 0u);
    }
    {
        road_crossing_zone_t zone = {20u, 24u, 2u, 2, 70, 0u, 0u};
        road_game_set_crossing_zones(&zone, 1u);
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        road_game_seek(&game, 2200u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_cursor == 1u);
        assert(game.crossing_parents[0].active);
        assert(game.crossing_parents[0].child_mask == 0x0du);
        assert(game.crossing_parents[0].phase[0] == 0u);
        assert(game.crossing_parents[0].phase[2] == 0u);
        assert(game.crossing_parents[0].phase[3] == 2u);
        assert(game.crossing_parents[0].next_update_tick == 31u);
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].phase[3] == 2u);
        assert(game.crossing_parents[0].timer[3] == 30u);
        game.crossing_parents[0].phase[3] = 2u;
        game.crossing_parents[0].timer[3] = 779u;
        game.elapsed_ms = 500u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing[0].distance > game.distance);
        assert(game.crossing[0].collision_mode == 3u);
        assert(game.crossing[0].lane < 125);
        road_game_set_crossing_zones(0, 0u);
        road_game_set_cars(0, 0u);
    }
    {
        unsigned short left_width[32], right_width[32];
        road_crossing_zone_t zone = {20u, 24u, 1u, 4, 70, 0u, 0u};
        for (i = 0u; i < 32u; ++i)
            left_width[i] = right_width[i] = 512u;
        right_width[20] = 255u;
        left_width[23] = 255u;
        road_game_set_widths(left_width, right_width, 32u);
        road_game_set_crossing_zones(&zone, 1u);
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        road_game_seek(&game, 2200u);
        game.speed = 0u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].child_mask == 0x0au);
        right_width[20] = 256u;
        left_width[23] = 256u;
        road_game_init(&game);
        road_game_seek(&game, 2200u);
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].child_mask == 0x0fu);
        road_game_set_widths(0, 0, 0u);
        road_game_set_crossing_zones(0, 0u);
        road_game_set_cars(0, 0u);
    }
    {
        road_crossing_zone_t zone = {20u, 24u, 1u, 4, 70, 0u, 0u};
        road_game_set_crossing_zones(&zone, 1u);
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        road_game_seek(&game, 2200u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].child_mask == 0x0fu);
        game.crossing_parents[0].phase[1] = 2u;
        game.crossing_parents[0].timer[1] = 779u;
        game.crossing_parents[0].phase[3] = 2u;
        game.crossing_parents[0].timer[3] = 779u;
        game.elapsed_ms = 500u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing[0].collision_mode == 1u);
        assert(game.crossing[1].collision_mode == 3u);
        assert(game.crossing[0].animation_choice !=
               game.crossing[1].animation_choice);
        road_game_set_crossing_zones(0, 0u);
        road_game_set_cars(0, 0u);
    }
    {
        road_crossing_zone_t zone = {20u, 24u, 1u, 4, 70, 0u, 1u};
        road_game_set_crossing_zones(&zone, 1u);
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        road_game_seek(&game, 2200u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].active);
        assert(game.crossing_parents[0].child_mask == 0x05u);
        assert(game.crossing_parents[0].phase[0] == 2u);
        assert(game.crossing_parents[0].timer[0] < 240u);
        assert(game.crossing_parents[0].phase[1] == 0u);
        assert(game.crossing_parents[0].timer[1] == 780u);
        game.crossing_parents[0].phase[0] = 0u;
        game.crossing_parents[0].timer[0] = 75u;
        game.elapsed_ms = 500u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].phase[0] == 2u);
        for (i = 0u; i < ROAD_CROSSING_COUNT; ++i)
            assert(game.crossing[i].distance == 0u);
        road_game_set_crossing_zones(0, 0u);
        road_game_set_cars(0, 0u);
    }
    {
        road_crossing_zone_t zone = {100u, 110u, 1u, 4, 70, 0u, 0u};
        road_game_set_crossing_zones(&zone, 1u);
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        road_game_seek(&game, 110u * 256u - 0x5000u - 1u);
        game.speed = 0u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_cursor == 0u);
        assert(!game.crossing_parents[0].active);
        road_game_seek(&game, 110u * 256u - 0x5000u);
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].active);
        assert(game.crossing_parents[0].phase[1] == 2u);
        assert(game.crossing_parents[0].phase[0] == 2u);
        assert(game.crossing_parents[0].timer[0] == 810u);
        road_game_seek(&game, 110u * 256u - 0x4c00u);
        game.elapsed_ms = 517u;
        road_game_step(&game, 0u, 33u);
        assert(game.crossing_parents[0].phase[0] == 0u);
        assert(game.crossing_parents[0].timer[0] == 60u);
        road_game_seek(&game, 110u * 256u + 0x3001u);
        road_game_step(&game, 0u, 33u);
        assert(!game.crossing_parents[0].active);
        road_game_set_crossing_zones(0, 0u);
        road_game_set_cars(0, 0u);
    }
    {
        road_game_set_cars(&sprite, 1u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        for (i = 1u; i < ROAD_TRAFFIC_COUNT; ++i)
            game.traffic[i].distance = 0u;
        game.traffic[0].distance = 480u;
        game.traffic[0].lane = 0;
        game.traffic[0].speed = 0u;
        game.vertical_height_8_8 = 90 * 256;
        road_game_step(&game, 0u, 33u);
        assert(game.player_stun_ms == 0u);
        road_game_set_bike_spec(&game, &specs[0]);
        game.vertical_velocity_8_8 = -30 * 256;
        road_game_step(&game, 0u, 33u);
        assert(game.player_stun_ms > 0u);
        assert(game.vertical_height_8_8 >= 70 * 256);
        road_game_set_cars(0, 0u);
    }
    {
        road_game_init(&game);
        road_game_set_bike_spec(&game, &specs[0]);
        game.vertical_height_8_8 = 30 * 256;
        game.vertical_velocity_8_8 = -10000;
        road_game_step(&game, 0u, 33u);
        assert(game.vertical_height_8_8 == 0);
        assert(game.bounce_events == 1u);
        assert(game.vertical_velocity_8_8 > 0);
        assert(game.vertical_surface_acceleration_8_8 ==
               -(int)(specs[0].impact_strength * 2u));
        road_game_step(&game, 0u, 33u);
        assert(game.vertical_height_8_8 > 0);
        assert(game.vertical_surface_acceleration_8_8 ==
               -(int)specs[0].impact_strength);
    }
    {
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        road_game_step(&game, ROAD_ATTACK, 33u);
        assert(game.hits == 0u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            game.opponents[i].distance = 100000000u;
        game.opponents[0].distance = 480u;
        game.opponents[0].lane = game.lane;
        road_game_step(&game, ROAD_ATTACK, 33u);
        assert(game.hits == 1u);
    }
    {
        unsigned char margin[8] = {16u, 16u, 16u, 16u,
                                   16u, 16u, 16u, 16u};
        unsigned char depth[8] = {10u, 10u, 10u, 10u,
                                  10u, 10u, 10u, 10u};
        unsigned char mode[8] = {0u};
        road_game_set_elevation(0, 0u);
        road_game_set_cross_section(margin, margin, depth, depth,
                                     mode, 8u);
        road_game_init(&game);
        road_game_set_bike_spec(&game, &specs[0]);
        game.lateral_impulse = 300;
        road_game_step(&game, 0u, 33u);
        assert(game.lane > 111);
        assert(game.vertical_velocity_8_8 == 1280);
        road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    }

    hazard.sample = 3u;
    road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    road_game_init(&game);
    road_game_set_bike_tuning(&game, specs[7].forward_speed_factor,
                              specs[7].tuning_base,
                              specs[7].steering_speed_scale);
    assert(game.top_speed == 220u && game.steer_percent == 188u);
    road_game_seek(&game, 250u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u && game.speed < 100u);
    assert(game.sound_event_mask == ((1u << 4u) | (1u << 7u)));
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u);
    assert(game.sound_event_mask == 0u);
    road_game_init(&game);
    road_game_seek(&game, 250u);
    game.speed = 60u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u);
    assert(game.sound_event_mask == ((1u << 3u) | (1u << 6u)));
    road_game_init(&game);
    game.lane = 60;
    road_game_seek(&game, 250u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 0u);
    assert(game.sound_event_mask == 0u);
    road_game_init(&game);
    game.lane = 21;
    road_game_seek(&game, 250u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u);
    road_game_init(&game);
    game.lane = 29;
    road_game_seek(&game, 250u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 0u);
    {
        road_static_collision_t collision = {0};
        collision.count = 1u;
        collision.box[0].left = 10;
        collision.box[0].top = -20;
        collision.box[0].right = 20;
        collision.box[0].bottom = 0;
        road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
        road_game_set_static_collisions(&collision, 1u);
        road_game_init(&game);
        game.lane = 30;
        road_game_seek(&game, 250u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.hazard_hits == 1u);
        road_game_init(&game);
        game.lane = -30;
        road_game_seek(&game, 250u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.hazard_hits == 0u);
        hazard.mirrored = 1u;
        road_game_init(&game);
        game.lane = -30;
        road_game_seek(&game, 250u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.hazard_hits == 1u);
        hazard.mirrored = 0u;
        collision.box[0].top = -100;
        collision.box[0].bottom = -90;
        road_game_init(&game);
        game.lane = 30;
        road_game_seek(&game, 250u);
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.hazard_hits == 0u);
        road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    }
    hazard.sample = 100u;
    hazard.lateral = 0;
    road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    road_game_set_hazard_motion(0, 0u);
    road_game_init(&game);
    road_game_seek(&game, 25120u);
    game.speed = 30u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u && game.distance < 25600u);
    hazard.lateral = 40;
    road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    road_game_set_hazard_motion(moving, 1u);
    road_game_init(&game);
    road_game_seek(&game, 25110u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 1u);
    road_game_init(&game);
    road_game_seek(&game, 25110u);
    game.elapsed_ms = 1600u;
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 0u);
    road_game_init(&game);
    road_game_seek(&game, 24500u);
    road_game_render(&game, before_frame);
    game.elapsed_ms = 1600u;
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    effect.sprite_index = 0u;
    effect.travel_mode = 4u;
    road_game_set_effect_sprites(effect_frames, 1u);
    road_game_init(&game);
    road_game_set_effects(&game, &effect, 1u);
    road_game_step(&game, 0u, 33u);
    road_game_render(&game, before_frame);
    game.effects[0].state_ms += 140u;
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    effect_ticks[ROAD_EFFECT_MOTION_FIRST] = 3u;
    effect_ticks[ROAD_EFFECT_MOTION_FIRST + 1u] = 6u;
    road_game_set_effect_frame_ticks(effect_ticks, 1u);
    game.effects[0].state_ms = 0u;
    road_game_render(&game, before_frame);
    game.effects[0].state_ms = 60u;
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    game.effects[0].animation = ROAD_EFFECT_ANIM_STANDING;
    road_game_render(&game, before_frame);
    game.effects[0].animation = ROAD_EFFECT_ANIM_FALL;
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    game.effects[0].state_ms = 0u;
    road_game_render(&game, before_frame);
    game.effects[0].state_ms = 70u;
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    road_game_set_effect_sprites(0, 0u);
    effect.sprite_index = 255u;
    effect.travel_mode = 10u;
    pixel = 0xf81fu;
    road_game_init(&game);
    road_game_seek(&game, 25590u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.hazard_hits == 0u);
    road_game_set_static_scenery(0, 0u, 0, 0u);

    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_APPROACH);
    assert(game.opponents[0].target_lane == -31);
    assert(game.opponents[0].lane < 0);
    road_game_init(&game);
    game.opponents[0].distance = 500u;
    game.opponents[0].lane = -100;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_PASS_RIGHT);
    road_game_init(&game);
    game.speed = 100u;
    game.opponents[0].distance = 480u;
    game.opponents[0].lane = 0;
    road_game_step(&game, 0u, 1u);
    assert(game.opponents[0].behavior == ROAD_AI_ATTACK);
    assert(game.rider_hits == 1u && game.player_stun_ms > 0u);
    road_game_init(&game);
    road_game_set_cars(&sprite, 1u);
    {
        unsigned int first_choice = game.traffic[0].animation_choice;
        unsigned int second_choice = game.traffic[1].animation_choice;
        assert(first_choice != second_choice);
        for (i = 0u; i < ROAD_TRAFFIC_COUNT; ++i)
            game.traffic[i].distance = 0u;
        game.traffic_spawn_timer = 0;
        game.traffic_reverse_timer = 0;
        game.speed = 100u;
        road_game_step(&game, 0u, 33u);
        assert(game.traffic[0].distance > game.distance);
        assert(game.traffic[1].distance > game.distance);
        assert(game.traffic[0].animation_choice != first_choice);
        assert(game.traffic[1].animation_choice != second_choice);
        first_choice = game.traffic[0].animation_choice;
        second_choice = game.traffic[1].animation_choice;
        road_game_step(&game, 0u, 33u);
        assert(game.traffic[0].animation_choice == first_choice);
        assert(game.traffic[1].animation_choice == second_choice);
    }
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    game.traffic[0].distance = 1500u;
    game.traffic[0].lane = 0;
    game.traffic[0].speed = 0u;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_AVOID_TRAFFIC);
    assert(game.opponents[0].target_lane < -30);
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    game.traffic[0].distance = 1500u;
    game.traffic[0].lane = 0;
    game.traffic[0].speed = 80u;
    game.traffic[0].travel_direction = 1;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior != ROAD_AI_AVOID_TRAFFIC);
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    game.traffic[0].distance = 2500u;
    game.traffic[0].lane = 0;
    game.traffic[0].speed = 40u;
    game.traffic[0].travel_direction = -1;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_AVOID_TRAFFIC);
    hazard.sample = 6u;
    hazard.lateral = 0;
    pixel = 0xffffu;
    road_game_set_hazard_motion(0, 0u);
    road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_AVOID_TRAFFIC);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    game.effects[0].active = 1u;
    game.effects[0].distance = 1500u;
    game.effects[0].lane = 0;
    game.effects[0].behavior = ROAD_EFFECT_WAITING;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_AVOID_TRAFFIC);
    road_game_init(&game);
    game.opponents[0].distance = 1000u;
    game.opponents[0].lane = 0;
    game.opponents[0].speed = 80u;
    game.opponents[0].base_speed = 80u;
    game.opponents[1].distance = 1500u;
    game.opponents[1].lane = 0;
    game.opponents[1].speed = 20u;
    road_game_step(&game, 0u, 33u);
    assert(game.opponents[0].behavior == ROAD_AI_AVOID_TRAFFIC);
    road_game_set_cars(0, 0u);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_cars(&sprite, 1u);
    road_game_init(&game);
    game.speed = 200u;
    game.traffic[0].distance = 650u;
    game.traffic[0].lane = 0;
    game.traffic[0].speed = 0u;
    road_game_step(&game, 0u, 100u);
    assert(game.bike_health < game.bike_max_health);
    {
        unsigned int stationary_health = game.bike_health;
        road_game_init(&game);
        game.speed = 200u;
        game.traffic[0].distance = 650u;
        game.traffic[0].lane = 0;
        game.traffic[0].speed = 20u;
        game.traffic[0].travel_direction = -1;
        road_game_step(&game, 0u, 100u);
        assert(game.bike_health < stationary_health);
    }
    road_game_init(&game);
    game.speed = 200u;
    game.traffic[0].distance = 650u;
    game.traffic[0].lane = 40;
    game.traffic[0].speed = 0u;
    road_game_step(&game, 0u, 100u);
    assert(game.bike_health == game.bike_max_health);
    road_game_init(&game);
    game.speed = 200u;
    game.traffic[0].distance = 500u;
    game.traffic[0].lane = 22;
    game.traffic[0].speed = 0u;
    road_game_step(&game, ROAD_RIGHT, 100u);
    assert(game.lateral_impulse < 0);
    assert(game.speed > 100u && game.bike_health < game.bike_max_health);
    hazard.sample = 2u;
    hazard.lateral = 30;
    road_game_set_static_scenery(&sprite, 1u, &hazard, 1u);
    road_game_init(&game);
    game.speed = 200u;
    road_game_step(&game, ROAD_RIGHT, 100u);
    assert(game.hazard_hits == 1u && game.lateral_impulse < 0);
    assert(game.speed > 100u);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_cars(0, 0u);
    {
        signed char curve[64];
        unsigned short width[64];
        int curved_lane, straight_lane;
        unsigned int narrow_speed, wide_speed;
        for (i = 0u; i < 64u; ++i) {
            curve[i] = 60;
            width[i] = 512u;
        }
        road_game_set_course(curve, 64u);
        road_game_set_widths(width, width, 64u);
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            if (i != 1u) game.opponents[i].distance = 100000000u;
        game.opponents[1].distance = 4600u;
        game.opponents[1].lane = 0;
        road_game_step(&game, 0u, 33u);
        curved_lane = game.opponents[1].target_lane;
        assert(game.opponents[1].speed <
               game.opponents[1].base_speed);
        for (i = 0u; i < 64u; ++i) curve[i] = 0;
        road_game_init(&game);
        for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
            if (i != 1u) game.opponents[i].distance = 100000000u;
        game.opponents[1].distance = 4600u;
        game.opponents[1].lane = 0;
        road_game_step(&game, 0u, 33u);
        straight_lane = game.opponents[1].target_lane;
        assert(curved_lane > straight_lane);
        for (i = 0u; i < 64u; ++i) width[i] = 256u;
        road_game_init(&game);
        game.speed = 100u;
        game.lane = 80;
        road_game_step(&game, 0u, 33u);
        narrow_speed = game.speed;
        for (i = 0u; i < 64u; ++i) width[i] = 512u;
        road_game_init(&game);
        game.speed = 100u;
        game.lane = 80;
        road_game_step(&game, 0u, 33u);
        wide_speed = game.speed;
        assert(narrow_speed < wide_speed);
        road_game_set_course(0, 0u);
        road_game_set_widths(0, 0, 0u);
    }
    road_game_init(&game);
    road_game_render(&game, before_frame);
    road_game_set_effects(&game, &effect, 1u);
    road_game_step(&game, 0u, 33u);
    assert(game.effects[0].active && game.effect_cursor == 1u);
    road_game_render(&game, after_frame);
    assert(memcmp(before_frame, after_frame, sizeof(before_frame)) != 0);
    road_game_init(&game);
    road_game_seek(&game, 20u * 256u - 580u);
    road_game_set_effects(&game, &effect, 1u);
    game.speed = 100u;
    road_game_step(&game, 0u, 33u);
    assert(game.effect_hits == 1u && game.player_stun_ms > 0u);
    assert(game.effects[0].active && game.speed < 100u);
    assert(game.effects[0].behavior == ROAD_EFFECT_CONTACT_LOCKED);
    assert(game.effects[0].animation == ROAD_EFFECT_ANIM_FALL);
    game.speed = 0u;
    game.velocity_raw = 0u;
    game.lane = 80;
    for (i = 0u; i < 5u; ++i)
        road_game_step(&game, 0u, 100u);
    assert(game.effects[0].active &&
           game.effects[0].behavior == ROAD_EFFECT_RECOVERING &&
           game.effects[0].animation == ROAD_EFFECT_ANIM_SHAKE);
    for (i = 0u; i < 3u; ++i)
        road_game_step(&game, 0u, 100u);
    assert(game.effects[0].active &&
           game.effects[0].behavior == ROAD_EFFECT_SHAKE_RECOVERY);
    for (i = 0u; i < 3u; ++i)
        road_game_step(&game, 0u, 100u);
    assert(game.effects[0].active &&
           game.effects[0].behavior == ROAD_EFFECT_WAITING);
    effect.sample = 20u;
    effect.lateral = 96;
    effect.travel_mode = 4u;
    road_game_init(&game);
    road_game_seek(&game, 20u * 256u - 480u);
    road_game_set_effects(&game, &effect, 1u);
    game.speed = 100u;
    road_game_step(&game, ROAD_RIGHT, 100u);
    assert(game.effect_hits == 1u && game.lateral_impulse < 0);
    assert(game.speed > 70u);
    road_game_set_effects(&game, 0, 0u);
    road_game_init(&game);
    road_game_seek(&game, 5200u);
    game.speed = 100u;
    road_game_step(&game, 0u, 100u);
    assert(game.player_stun_ms == 0u &&
           game.bike_health == game.bike_max_health);
    effect.sample = 100u;
    effect.lateral = -640;
    effect.travel_mode = 7u;
    road_game_init(&game);
    road_game_seek(&game, 100u * 256u - 5000u);
    road_game_set_effects(&game, &effect, 1u);
    for (i = 0u; i < 4u; ++i)
        road_game_step(&game, 0u, 100u);
    assert(game.effects[0].active && game.effects[0].lane > -80);
    road_game_set_effects(&game, 0, 0u);
    puts("Original SPEC selection and adapted hazard/AI behavior: PASS");
    return 0;
}

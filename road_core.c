#include "road_core.h"
#ifndef ROAD_RUNTIME_TRACE
#define ROAD_RUNTIME_TRACE(stage, a, b, c) ((void)0)
#endif
#ifndef ROAD_RUNTIME_AUDIO_PUMP
#define ROAD_RUNTIME_AUDIO_PUMP() ((void)0)
#endif
#ifdef ROAD_USE_UPSTREAM_PATH
#include "local-data/upstream-src/track_traversal_runtime.h"
void advance_road_path_traversal(RoadPathTraversalState *path_state,
                                 int delta);
static unsigned int g_path_words[(16u + 10000u * 2u + 3u) / 4u];
static RoadPathTraversalState g_path_cursor;
static int g_path_ready;
#endif

#define HORIZON 90
#define ROAD_VIEW_WIDTH ROAD_WIDTH
#define ROAD_VIEW_BOTTOM ROAD_HEIGHT
#define ROAD_CAMERA_HEIGHT 150
#define ROAD_VISIBLE_DEPTH (33u * 256u)
#define ROAD_PROJECTION_HEIGHT (ROAD_CAMERA_HEIGHT * 256u)
#define ROAD_PROJECTION_WIDTH (250u * 512u)
#define ROAD_PLAYER_DRAW_Y 170
#define ROAD_PLAYER_CONTACT_DEPTH \
    (ROAD_PROJECTION_HEIGHT / (ROAD_PLAYER_DRAW_Y - HORIZON))
#define ROAD_NEAR_RIDER_HEIGHT 64
#define ROAD_NEAR_PEDESTRIAN_HEIGHT 56
/* Thirty-three visible nodes plus one interpolated endpoint. */
#define COURSE_LOOKAHEAD (ROAD_VISIBLE_DEPTH / 256u + 2u)
static const unsigned int *g_reciprocal_table;
static unsigned int g_reciprocal_count;
static unsigned short g_depth_for_dy[ROAD_VIEW_BOTTOM - HORIZON + 1];

static unsigned int road_reciprocal(unsigned int depth)
{
    unsigned int index = depth >> 1u;
    if (!g_reciprocal_table || !g_reciprocal_count)
        return depth ? (256u << 16u) / depth : 0u;
    if (index >= g_reciprocal_count) index = g_reciprocal_count - 1u;
    return g_reciprocal_table[index];
}

static int road_projection_dy(unsigned int depth)
{
    if (!depth) return ROAD_VIEW_BOTTOM;
    if (g_reciprocal_table && g_reciprocal_count)
        return (int)(road_reciprocal(depth) *
                     ROAD_CAMERA_HEIGHT >> 16u);
    return (int)(ROAD_PROJECTION_HEIGHT / depth);
}

static int road_projection_unit(unsigned int depth)
{
    if (!depth) return ROAD_VIEW_WIDTH;
    if (g_reciprocal_table && g_reciprocal_count)
        return (int)(road_reciprocal(depth) * 500u >> 16u);
    return (int)(ROAD_PROJECTION_WIDTH / depth);
}

/* The player sprite's wheels sit at y=170. Give other riders and walkers a
 * matching perspective scale at that road row instead of using separate
 * ad-hoc slopes (previously a nearby walker was about twice a racer). */
static int road_dynamic_actor_height(unsigned int depth, int near_height)
{
    int dy = road_projection_dy(depth);
    int height = (dy * near_height +
        (ROAD_PLAYER_DRAW_Y - HORIZON) / 2) /
        (ROAD_PLAYER_DRAW_Y - HORIZON);
    if (height < 8) return 8;
    if (height > 80) return 80;
    return height;
}

static int road_project_world_delta(int value, unsigned int depth)
{
    if (!depth) return 0;
    if (g_reciprocal_table && g_reciprocal_count)
        return value * (int)road_reciprocal(depth) / 65536;
    return value * 256 / (int)depth;
}

static unsigned int road_depth_for_dy(unsigned int dy)
{
    if (!dy) return ROAD_VISIBLE_DEPTH;
    if (g_reciprocal_table && g_reciprocal_count) {
        if (dy > ROAD_VIEW_BOTTOM - HORIZON)
            dy = ROAD_VIEW_BOTTOM - HORIZON;
        return g_depth_for_dy[dy];
    }
    return ROAD_PROJECTION_HEIGHT / dy;
}

static const signed char *g_course_curvature;
static unsigned int g_course_count;
static unsigned int g_finish_sample;
static unsigned int g_course_generation;
static int g_course_projection[COURSE_LOOKAHEAD];
static const signed char *g_course_elevation;
static unsigned int g_elevation_count;
static const unsigned short *g_course_left_width;
static const unsigned short *g_course_right_width;
static unsigned int g_width_count;
static const signed char *g_fork_curvature;
static const signed char *g_fork_elevation;
static const unsigned short *g_fork_left_width;
static const unsigned short *g_fork_right_width;
static const unsigned char *g_fork_left_margin;
static const unsigned char *g_fork_right_margin;
static unsigned int g_fork_slope_count;
static unsigned int g_fork_count;
static unsigned int g_fork_sample;
static unsigned int g_fork_visible_start;
static unsigned int g_fork_end_sample;
static unsigned int g_fork_main_channel;
static int g_fork_active_span;
static int g_fork_reverse_mode;
static int g_fork_sample_shift;
static int g_fork_reverse_center_offset;
static unsigned int g_fork_relative_sample;
static int g_fork_relative_heading;
static int g_fork_relative_x;
static int g_fork_relative_y;
static int g_fork_forward_left_step[81];
static unsigned int g_fork_forward_step_count;
static unsigned int g_fork_forward_clearance_index;
static int g_fork_reverse_left_step[81];
static unsigned int g_fork_reverse_step_count;
static unsigned int g_fork_reverse_step_start;
static unsigned int g_fork_reverse_clearance_offset;
static int g_fork_projection[COURSE_LOOKAHEAD];
static int g_fork_elevation_projection[COURSE_LOOKAHEAD];
static const road_sprite_t *g_surface_textures;
static unsigned int g_surface_texture_count;
static const road_sprite_t *g_hud;
static const road_sprite_t *g_hud_speed_needle;
static const road_sprite_t *g_hud_health_needle;
static const road_sprite_t *g_hud_health_frames;
static unsigned int g_hud_health_frame_count;
static const road_sprite_t *g_hud_portrait;
static const road_hill_profile_t *g_hill_profiles;
static unsigned int g_hill_profile_count;
static const road_hill_sample_t *g_hill_samples;
static unsigned int g_hill_sample_count;
static const road_hill_profile_t *g_fork_hill_profiles;
static unsigned int g_fork_hill_profile_count;
static const road_hill_sample_t *g_fork_hill_samples;
static unsigned int g_fork_hill_sample_count;
static const road_edge_profile_t *g_edge_profiles[2];
static unsigned int g_edge_profile_count[2];
static const short *g_edge_outer_samples[2];
static unsigned int g_edge_outer_sample_count;
static const road_sprite_t *g_edge_fill_primary;
static const road_sprite_t *g_edge_fill_horizon;
static const road_sprite_t *g_hill_tiles;
static const road_sprite_t *g_hill_mips;
static const road_sprite_t *g_hill_narrow_mips;
static unsigned int g_hill_tile_count;
static const road_surface_sample_t *g_surface_samples;
static unsigned int g_surface_sample_count;
static const road_surface_sample_t *g_fork_surface_samples;
static unsigned int g_fork_surface_sample_count;
static const unsigned char *g_margin_selector[2][2];
static const unsigned short *g_margin_family[2][2];
static const unsigned char *g_margin_surface_flags[2];
static const unsigned char *g_margin_terrain_mode[2];
static unsigned int g_margin_sample_count[2];
static unsigned int g_margin_repeat_threshold;
static const road_margin_tile_t *g_margin_tiles;
static unsigned int g_margin_tile_count;
static const road_sprite_t *g_roadside_tiles;
static const unsigned short *g_roadside_tile_heights;
static const unsigned char *g_roadside_repeat_indices;
static const road_sprite_t *g_roadside_repeat_mips;
static unsigned int g_roadside_tile_count;
static const unsigned char *g_course_left_margin;
static const unsigned char *g_course_right_margin;
static const unsigned char *g_course_left_depth;
static const unsigned char *g_course_right_depth;
static const unsigned char *g_fork_left_depth;
static const unsigned char *g_fork_right_depth;
static unsigned int g_fork_depth_count;
static const unsigned char *g_course_terrain_mode;
static unsigned char g_road_surface_visible[2][32];
static int g_projected_sort_depth[32];
static unsigned int g_road_visibility_first_sample;
static int g_projected_horizon_bottom;
static unsigned int g_cross_section_count;
static int g_elevation_projection[COURSE_LOOKAHEAD];
static const unsigned char *g_backdrop_indices;
static const road_sprite_t *g_sky_layer;
static const road_pixel_t *g_backdrop_palette;
static unsigned int g_backdrop_width;
static unsigned int g_backdrop_height;
static const road_sprite_t *g_scenery;
static unsigned int g_scenery_count;
static const road_scenery_placement_t *g_scenery_placements;
static unsigned int g_scenery_placement_count;
static const road_sprite_t *g_static_sprites;
static const road_sprite_t *g_static_scale_frames;
static unsigned int g_static_scale_variant_count;
static const road_static_frame_anchor_t *g_static_frame_anchors;
static unsigned int g_static_anchor_variant_count;
static unsigned int g_static_sprite_count;
static const road_static_placement_t *g_static_placements;
static unsigned int g_static_placement_count;
static const road_static_collision_t *g_static_collisions;
static unsigned int g_static_collision_count;
static const unsigned char *g_hazard_moving;
static unsigned int g_hazard_moving_count;
static const road_effect_placement_t *g_effect_placements;
static unsigned int g_effect_count;
static const road_crossing_zone_t *g_crossing_zones;
static unsigned int g_crossing_count;
static const road_sprite_t *g_effect_sprites;
static unsigned int g_effect_sprite_count;
static const unsigned char *g_effect_frame_ticks;
static unsigned int g_effect_tick_style_count;
static const road_sprite_t *g_bikes;
static unsigned int g_bike_count;
static const road_sprite_t *g_cars;
static unsigned int g_car_count;
static const road_car_animation_t *g_car_animations;
static unsigned int g_car_animation_count;

static int clamp_int(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void road_lane_bounds(unsigned int distance, int *left, int *right)
{
    unsigned int sample;
    *left = -111;
    *right = 111;
    if (!g_width_count || !g_course_left_width ||
        !g_course_right_width) return;
    sample = distance / 256u;
    if (sample >= g_width_count) sample = g_width_count - 1u;
    *left = -clamp_int((int)g_course_left_width[sample] * 111 / 512,
                      20, 128);
    *right = clamp_int((int)g_course_right_width[sample] * 111 / 512,
                       20, 128);
}

int road_game_road_skid_active(const road_game_t *game)
{
    int left, right;
    if (!game || game->slip_amount <= 50u || !game->velocity_raw ||
        !game->surface_contact_scale_8_8 || game->recovery_ms ||
        game->finished || game->player_stun_ms)
        return 0;
    road_lane_bounds(game->distance, &left, &right);
    return game->lane >= left && game->lane <= right;
}

static void road_shoulder_bounds(unsigned int distance, int *left, int *right)
{
    unsigned int sample = distance / 256u;
    road_lane_bounds(distance, left, right);
    if (!g_cross_section_count || sample >= g_cross_section_count ||
        !g_course_left_margin || !g_course_right_margin) {
        *left -= 30;
        *right += 30;
        return;
    }
    *left -= (int)g_course_left_margin[sample] * 111 / 512;
    *right += (int)g_course_right_margin[sample] * 111 / 512;
}

static int road_cross_section_height_8_8(unsigned int distance, int lane)
{
    unsigned int sample = distance / 256u;
    int left, right;
    if (!g_cross_section_count || sample >= g_cross_section_count ||
        !g_course_terrain_mode || g_course_terrain_mode[sample] != 0u ||
        !g_course_left_depth || !g_course_right_depth) return 0;
    road_lane_bounds(distance, &left, &right);
    if (lane < left) return (int)g_course_left_depth[sample] * 256;
    if (lane > right) return (int)g_course_right_depth[sample] * 256;
    return 0;
}

static int road_surface_height_8_8(const road_game_t *game)
{
    unsigned int sample = game->distance / 256u;
    int grade = sample < g_elevation_count ?
        (int)g_course_elevation[sample] : 0;
    return game->track_elevation * 256 +
        grade * (int)(game->distance & 255u);
}

static int road_surface_height_at_8_8(const road_game_t *game,
                                      unsigned int distance)
{
    unsigned int cursor = game->distance;
    int height = road_surface_height_8_8(game);
    if (!g_course_elevation || !g_elevation_count) return height;
    while (cursor < distance) {
        unsigned int sample = cursor / 256u;
        unsigned int end = (sample + 1u) * 256u;
        if (end > distance) end = distance;
        if (sample < g_elevation_count)
            height += (int)g_course_elevation[sample] *
                (int)(end - cursor);
        cursor = end;
    }
    while (cursor > distance) {
        unsigned int sample = (cursor - 1u) / 256u;
        unsigned int start = sample * 256u;
        if (start < distance) start = distance;
        if (sample < g_elevation_count)
            height -= (int)g_course_elevation[sample] *
                (int)(cursor - start);
        cursor = start;
    }
    return height;
}

static int road_surface_height_at_lane_8_8(const road_game_t *game,
                                            unsigned int distance, int lane)
{
    return road_surface_height_at_8_8(game, distance) +
        road_cross_section_height_8_8(distance, lane);
}

/* The source half-widths are wider than the adapted on-screen sprites when
 * converted through this renderer's lane projection. Use visible body widths
 * for contacts, so a side hit requires the sprites to overlap. */
#define ROAD_PLAYER_HALF_LANE 6
#define ROAD_OPPONENT_HALF_LANE 6
#define ROAD_CAR_HALF_LANE 12
#define ROAD_CROSSING_CAR_HALF_LANE 32
#define ROAD_EFFECT_HALF_LANE 4
#define ROAD_STATIC_HALF_LANE 16
#define ROAD_PLAYER_HALF_LENGTH 64
#define ROAD_OPPONENT_HALF_LENGTH 64
#define ROAD_CAR_HALF_LENGTH 64
#define ROAD_EFFECT_HALF_LENGTH 16
#define ROAD_STATIC_HALF_LENGTH 64
#define ROAD_PLAYER_COLLISION_MASS 256u
#define ROAD_OPPONENT_COLLISION_MASS 256u
#define ROAD_CAR_COLLISION_MASS 1800u
#define ROAD_EFFECT_COLLISION_MASS 100u
#define ROAD_PLAYER_HALF_HEIGHT_8_8 8960
#define ROAD_CAR_HALF_HEIGHT_8_8 8960
#define ROAD_OPPONENT_HALF_HEIGHT_8_8 8960
#define ROAD_EFFECT_HALF_HEIGHT_8_8 7680
#define ROAD_STATIC_HALF_HEIGHT_8_8 4096

static int road_static_half_height(const road_static_placement_t *placement)
{
    /* emit_track_hazards_for_segment: [-0x1000,+0x1000] laterally,
     * [-0x2000,0] vertically. Visibility group 2 extends to 0x3fff. */
    return placement->visibility_group == 2u ? 8192 :
        ROAD_STATIC_HALF_HEIGHT_8_8;
}

static int road_car_half_lane(const road_rider_t *car)
{
    /* track_object_mode_templates: modes 0/2 use 0x4000, 1/3 use
     * 0xA000.  Both are scaled with the renderer's 1/640 lane unit. */
    return (car->collision_mode & 1u) ?
        ROAD_CROSSING_CAR_HALF_LANE : ROAD_CAR_HALF_LANE;
}

static unsigned int road_traffic_random(road_game_t *game)
{
    game->traffic_seed = game->traffic_seed * 1664525u + 1013904223u;
    return game->traffic_seed;
}

static int road_contact_axis(int before, int after, int half,
                             int *enter, int *leave)
{
    int first, second, delta;
    if ((before > half && after > half) ||
        (before < -half && after < -half)) return 0;
    before = clamp_int(before, -30000, 30000);
    after = clamp_int(after, -30000, 30000);
    delta = after - before;
    if (!delta) return before >= -half && before <= half;
    first = (-half - before) * 1024 / delta;
    second = (half - before) * 1024 / delta;
    if (first > second) {
        int swap = first;
        first = second;
        second = swap;
    }
    if (second < 0 || first > 1024) return 0;
    if (first > *enter) *enter = first;
    if (second < *leave) *leave = second;
    return *enter <= *leave;
}

static int road_swept_box_contact_detail(int before_long, int after_long,
                                          int before_lane, int after_lane,
                                          int half_length, int half_lane,
                                          int *side_contact)
{
    int long_enter = 0, long_leave = 1024;
    int lane_enter = 0, lane_leave = 1024;
    if (!road_contact_axis(before_long, after_long, half_length,
                           &long_enter, &long_leave) ||
        !road_contact_axis(before_lane, after_lane, half_lane,
                           &lane_enter, &lane_leave) ||
        long_enter > lane_leave || lane_enter > long_leave)
        return 0;
    if (side_contact) *side_contact = lane_enter > long_enter;
    return 1;
}

static int road_swept_contact_3d(const road_game_t *game,
                                 unsigned int player_before_distance,
                                 int player_before_lane,
                                 int player_before_height_8_8,
                                 unsigned int other_before_distance,
                                 unsigned int other_after_distance,
                                 int other_before_lane, int other_after_lane,
                                 int half_length, int half_lane,
                                 int other_center_height_8_8,
                                 int other_half_height_8_8,
                                 int *side_contact, int *vertical_contact)
{
    int long_enter = 0, long_leave = 1024;
    int lane_enter = 0, lane_leave = 1024;
    int height_enter = 0, height_leave = 1024;
    int before_height, after_height;
    if (!road_swept_box_contact_detail(
            (int)other_before_distance - (int)player_before_distance -
                ROAD_PLAYER_CONTACT_DEPTH,
            (int)other_after_distance - (int)game->distance -
                ROAD_PLAYER_CONTACT_DEPTH,
            other_before_lane - player_before_lane,
            other_after_lane - game->lane,
            half_length, half_lane, 0)) return 0;
    before_height = road_surface_height_at_lane_8_8(game,
        player_before_distance + ROAD_PLAYER_CONTACT_DEPTH,
        player_before_lane) +
        player_before_height_8_8 +
        ROAD_PLAYER_HALF_HEIGHT_8_8 -
        road_surface_height_at_lane_8_8(game, other_before_distance,
                                        other_before_lane) -
        other_center_height_8_8;
    after_height = road_surface_height_at_lane_8_8(game,
        game->distance + ROAD_PLAYER_CONTACT_DEPTH,
        game->lane) +
        game->vertical_height_8_8 + ROAD_PLAYER_HALF_HEIGHT_8_8 -
        road_surface_height_at_lane_8_8(game, other_after_distance,
                                        other_after_lane) -
        other_center_height_8_8;
    if (!road_contact_axis((int)other_before_distance -
                           (int)player_before_distance -
                           ROAD_PLAYER_CONTACT_DEPTH,
                           (int)other_after_distance -
                           (int)game->distance -
                           ROAD_PLAYER_CONTACT_DEPTH,
                           half_length, &long_enter, &long_leave) ||
        !road_contact_axis(other_before_lane - player_before_lane,
                           other_after_lane - game->lane,
                           half_lane, &lane_enter, &lane_leave) ||
        !road_contact_axis(before_height, after_height,
                           ROAD_PLAYER_HALF_HEIGHT_8_8 +
                           other_half_height_8_8,
                           &height_enter, &height_leave)) return 0;
    if (long_enter > lane_leave || lane_enter > long_leave ||
        long_enter > height_leave || height_enter > long_leave ||
        lane_enter > height_leave || height_enter > lane_leave)
        return 0;
    if (side_contact) *side_contact = lane_enter > long_enter &&
        lane_enter >= height_enter;
    if (vertical_contact) *vertical_contact =
        height_enter > long_enter && height_enter > lane_enter ?
        (before_height >= 0 ? 1 : -1) : 0;
    return 1;
}

static unsigned int road_elastic_contact_speed(unsigned int player_speed,
                                                int other_speed,
                                                unsigned int other_mass)
{
    /* 3DO compute_elastic_collision_velocity_delta, followed by the
     * half-impulse applied by integrate_track_object_motion. */
    int weighted = (int)player_speed *
        ((int)ROAD_PLAYER_COLLISION_MASS - (int)other_mass) +
        2 * other_speed * (int)other_mass;
    int elastic = weighted /
        (int)(ROAD_PLAYER_COLLISION_MASS + other_mass);
    int applied = (int)player_speed +
        (elastic - (int)player_speed) / 2;
    return applied < 0 ? 0u : (unsigned int)applied;
}

static void road_apply_lateral_contact(road_game_t *game,
                                        int old_lane, int other_lane,
                                        int old_other_lane,
                                        unsigned int other_mass,
                                        int combined_half_width,
                                        unsigned int dt_ms)
{
    int separation = other_lane - game->lane;
    int overlap = combined_half_width -
        (separation < 0 ? -separation : separation);
    int player_velocity, other_velocity, elastic, correction;
    if (!dt_ms) return;
    player_velocity = (game->lane - old_lane) * 60 / (int)dt_ms;
    other_velocity = (other_lane - old_other_lane) * 60 / (int)dt_ms;
    elastic = (player_velocity *
        ((int)ROAD_PLAYER_COLLISION_MASS - (int)other_mass) +
        2 * other_velocity * (int)other_mass) /
        (int)(ROAD_PLAYER_COLLISION_MASS + other_mass);
    game->lateral_impulse = clamp_int(game->lateral_impulse +
        (elastic - player_velocity) / 2, -24, 24);
    if (overlap > 0) {
        correction = overlap * (int)other_mass /
            (int)(ROAD_PLAYER_COLLISION_MASS + other_mass) + 1;
        game->lane = clamp_int(game->lane +
            (separation >= 0 ? -correction : correction), -128, 128);
    }
}

static void road_apply_vertical_contact(road_game_t *game,
                                         unsigned int other_distance,
                                         int other_lane,
                                         int other_top_height_8_8,
                                         unsigned int other_mass,
                                         int contact_direction)
{
    int velocity = game->vertical_velocity_8_8;
    int elastic = (velocity *
        ((int)ROAD_PLAYER_COLLISION_MASS - (int)other_mass)) /
        (int)(ROAD_PLAYER_COLLISION_MASS + other_mass);
    int surface = road_surface_height_at_lane_8_8(game, other_distance,
        other_lane) + other_top_height_8_8 -
        road_surface_height_at_lane_8_8(game, game->distance, game->lane);
    if (contact_direction < 0) {
        game->vertical_height_8_8 = 0;
        game->vertical_velocity_8_8 = 0;
        game->surface_contact_scale_8_8 = 256u;
        game->speed = game->speed * 2u / 5u;
        return;
    }
    game->vertical_height_8_8 = clamp_int(surface + 3, 0, 32768);
    game->vertical_velocity_8_8 = clamp_int(
        velocity + (elastic - velocity) / 2, 0, 32768);
    game->surface_contact_scale_8_8 = 0u;
}

static road_pixel_t rgb(unsigned int r, unsigned int g, unsigned int b)
{
    return (road_pixel_t)(((r & 248u) << 8) | ((g & 252u) << 3) | (b >> 3));
}

static void span(road_pixel_t *pixels, int y, int x0, int x1, road_pixel_t color)
{
    road_pixel_t *row;
    int x;
    if (y < 0 || y >= ROAD_HEIGHT) return;
    x0 = clamp_int(x0, 0, ROAD_WIDTH);
    x1 = clamp_int(x1, 0, ROAD_WIDTH);
    row = pixels + y * ROAD_WIDTH;
    for (x = x0; x < x1; ++x) row[x] = color;
}

static void box(road_pixel_t *pixels, int x, int y, int w, int h,
                road_pixel_t color)
{
    int yy;
    for (yy = y; yy < y + h; ++yy) span(pixels, yy, x, x + w, color);
}

static int track_offset(unsigned int distance)
{
    static const int points[9] = {0, 0, 72, 112, 48, -48, -112, -64, 0};
    unsigned int phase = distance % 64000u;
    unsigned int segment = phase / 8000u;
    unsigned int fraction = phase % 8000u;
    return points[segment] +
        ((points[segment + 1u] - points[segment]) * (int)fraction) / 8000;
}

void road_game_set_course(const signed char *curvature, unsigned int count)
{
    g_course_curvature = curvature;
    g_course_count = curvature && count ? count : 0u;
    g_finish_sample = g_course_count;
    ++g_course_generation;
#ifdef ROAD_USE_UPSTREAM_PATH
    g_path_ready = 0;
#endif
}

void road_game_set_finish_sample(unsigned int sample)
{
    if (g_course_count && sample && sample <= g_course_count)
        g_finish_sample = sample;
}

void road_game_set_elevation(const signed char *steps, unsigned int count)
{
    g_course_elevation = steps;
    g_elevation_count = steps && count ? count : 0u;
#ifdef ROAD_USE_UPSTREAM_PATH
    if (g_course_count && g_course_count == g_elevation_count &&
        g_course_count <= 10000u) {
        unsigned char *path = (unsigned char *)g_path_words;
        unsigned int i;
        for (i = 0u; i < g_course_count; ++i) {
            path[16u + i * 2u] = (unsigned char)g_course_curvature[i];
            path[17u + i * 2u] = (unsigned char)steps[i];
        }
        g_path_cursor.resource = (const RoadPathResource *)path;
        g_path_cursor.elevation = 0;
        g_path_cursor.track_position = 0;
        g_path_ready = 1;
    }
#endif
}

void road_game_set_widths(const unsigned short *left,
                          const unsigned short *right, unsigned int count)
{
    g_course_left_width = left;
    g_course_right_width = right;
    g_width_count = left && right ? count : 0u;
}

static int fork_profile_clearance_width(unsigned int sample,
                                        unsigned int mapped)
{
    int clearance = 0x100;
    if (sample < g_cross_section_count &&
        mapped < g_fork_slope_count) {
        clearance += g_fork_main_channel ?
            (int)g_fork_right_margin[mapped] +
                (int)g_course_left_margin[sample] :
            (int)g_course_right_margin[sample] +
                (int)g_fork_left_margin[mapped];
    }
    return clearance;
}

/* The source scans 0x51 transition samples and records only the first
 * clearance hit. Its junction_progress then advances independently of
 * later curvature changes. Preserve that latch for both travel directions. */
static void recalculate_fork_clearance(void)
{
    unsigned int i;
    g_fork_forward_clearance_index = g_fork_forward_step_count ?
        g_fork_forward_step_count - 1u : 0u;
    for (i = 0u; i < g_fork_forward_step_count; ++i) {
        unsigned int sample = g_fork_sample + i;
        if (g_fork_forward_left_step[i] >=
            fork_profile_clearance_width(sample, sample)) {
            g_fork_forward_clearance_index = i;
            break;
        }
    }
    g_fork_reverse_clearance_offset = g_fork_reverse_step_count ?
        g_fork_reverse_step_count - 1u : 0u;
    for (i = 0u; i < g_fork_reverse_step_count; ++i) {
        unsigned int sample = g_fork_reverse_step_start +
            g_fork_reverse_step_count - 1u - i;
        int mapped = (int)sample + g_fork_sample_shift;
        if (mapped >= 0 && (unsigned int)mapped < g_fork_count &&
            g_fork_reverse_left_step[
                g_fork_reverse_step_count - 1u - i] >=
            fork_profile_clearance_width(sample,
                (unsigned int)mapped)) {
            g_fork_reverse_clearance_offset = i;
            break;
        }
    }
}

void road_game_set_fork_preview(const signed char *curvature,
                                const signed char *elevation,
                                const unsigned short *left,
                                const unsigned short *right,
                                unsigned int count,
                                unsigned int fork_sample,
                                unsigned int main_channel)
{
    g_fork_curvature = curvature;
    g_fork_elevation = elevation;
    g_fork_left_width = left;
    g_fork_right_width = right;
    g_fork_count = curvature && elevation && left && right &&
        count > fork_sample ?
        count : 0u;
    g_fork_sample = g_fork_count ? fork_sample : 0u;
    g_fork_visible_start = g_fork_sample;
    g_fork_end_sample = g_fork_count;
    g_fork_main_channel = main_channel & 1u;
    g_fork_active_span = 0;
    g_fork_reverse_mode = 0;
    g_fork_sample_shift = 0;
    g_fork_reverse_center_offset = 0;
    g_fork_relative_sample = g_fork_sample;
    g_fork_relative_heading = 0;
    g_fork_relative_x = 0;
    g_fork_relative_y = 0;
    g_fork_left_margin = 0;
    g_fork_right_margin = 0;
    g_fork_left_depth = 0;
    g_fork_right_depth = 0;
    g_fork_depth_count = 0u;
    g_fork_slope_count = 0u;
    g_fork_forward_step_count = 0u;
    g_fork_reverse_step_count = 0u;
    if (g_fork_count && g_course_curvature &&
        g_fork_sample < g_course_count) {
        unsigned int i, count = g_fork_count - g_fork_sample;
        const signed char *first = g_fork_main_channel ?
            g_fork_curvature : g_course_curvature;
        const signed char *second = g_fork_main_channel ?
            g_course_curvature : g_fork_curvature;
        int left_step = 0;
        int right_step = (int)second[g_fork_sample] -
            (int)first[g_fork_sample];
        if (count > g_course_count - g_fork_sample)
            count = g_course_count - g_fork_sample;
        if (count > 81u) count = 81u;
        g_fork_forward_left_step[0] = 0;
        for (i = 1u; i < count; ++i) {
            left_step += right_step;
            right_step += (int)second[g_fork_sample + i] -
                (int)first[g_fork_sample + i];
            g_fork_forward_left_step[i] = left_step;
        }
        g_fork_forward_step_count = count;
    }
    recalculate_fork_clearance();
}

void road_game_set_fork_span(unsigned int end_sample)
{
    g_fork_reverse_step_count = 0u;
    g_fork_active_span = g_fork_count && g_course_count &&
        g_elevation_count && end_sample > g_fork_sample &&
        end_sample <= g_fork_count && end_sample <= g_course_count &&
        end_sample <= g_elevation_count;
    if (g_fork_active_span) {
        g_fork_visible_start = g_fork_sample;
        g_fork_end_sample = end_sample;
        g_fork_reverse_mode = 0;
        g_fork_sample_shift = 0;
        g_fork_reverse_center_offset = 0;
    }
}

void road_game_set_fork_reverse(unsigned int selected_join,
                                unsigned int other_join)
{
    unsigned int selected_last, other_last;
    int new_left, new_right;
    g_fork_active_span = 0;
    g_fork_reverse_step_count = 0u;
    if (!g_fork_count || !g_width_count || !g_elevation_count ||
        selected_join < g_fork_sample + 81u ||
        other_join < g_fork_sample + 81u ||
        selected_join >= g_width_count ||
        selected_join >= g_course_count ||
        selected_join >= g_elevation_count ||
        other_join >= g_fork_count) return;
    selected_last = selected_join - 1u;
    other_last = other_join - 1u;
    new_left = (int)g_course_left_width[selected_join];
    new_right = (int)g_course_right_width[selected_join];
    if (g_fork_main_channel == 0u)
        g_fork_reverse_center_offset =
            new_left - (int)g_course_left_width[selected_last] +
            new_right - (int)g_fork_right_width[other_last];
    else
        g_fork_reverse_center_offset =
            (int)g_course_right_width[selected_last] - new_right +
            (int)g_fork_left_width[other_last] - new_left;
    g_fork_visible_start = selected_join - 81u;
    g_fork_end_sample = selected_join;
    g_fork_sample_shift = (int)other_join - (int)selected_join;
    g_fork_reverse_mode = 1;
    g_fork_active_span = 1;
    {
        unsigned int offset;
        int left_step, right_step;
        const signed char *first = g_fork_main_channel ?
            g_fork_curvature : g_course_curvature;
        const signed char *second = g_fork_main_channel ?
            g_course_curvature : g_fork_curvature;
        int diff = (int)second[g_fork_main_channel ?
            selected_last : other_last] -
            (int)first[g_fork_main_channel ?
            other_last : selected_last];
        left_step = diff;
        right_step = -diff;
        g_fork_reverse_step_start = selected_join - 81u;
        g_fork_reverse_left_step[80u] = left_step;
        for (offset = 1u; offset < 81u; ++offset) {
            unsigned int selected_at = selected_last - offset;
            unsigned int other_at = other_last - offset;
            diff = (int)second[g_fork_main_channel ?
                selected_at : other_at] -
                (int)first[g_fork_main_channel ?
                other_at : selected_at];
            right_step -= diff;
            left_step -= right_step;
            g_fork_reverse_left_step[80u - offset] = left_step;
        }
        g_fork_reverse_step_count = 81u;
    }
    recalculate_fork_clearance();
}

void road_game_set_fork_slopes(const unsigned char *left_margin,
                               const unsigned char *right_margin,
                               unsigned int count)
{
    g_fork_left_margin = left_margin;
    g_fork_right_margin = right_margin;
    g_fork_slope_count = g_fork_count && left_margin && right_margin ?
        count < g_fork_count ? count : g_fork_count : 0u;
    recalculate_fork_clearance();
}

void road_game_set_fork_depths(const unsigned char *left_depth,
                               const unsigned char *right_depth,
                               unsigned int count)
{
    g_fork_left_depth = left_depth;
    g_fork_right_depth = right_depth;
    g_fork_depth_count = g_fork_count && left_depth && right_depth ?
        count < g_fork_count ? count : g_fork_count : 0u;
}

unsigned int road_fork_channel(int lane, unsigned int old_left_width,
                               unsigned int first_left_width,
                               unsigned int first_right_width)
{
    /* The 3DO selects the second lane when the racer passes the first
     * lane's right road edge. Convert that edge to the 9588 lane scale. */
    int first_right = ((int)first_left_width +
        (int)first_right_width - (int)old_left_width) * 111 / 512;
    return lane >= first_right ? 1u : 0u;
}

void road_game_set_surface_textures(const road_sprite_t *textures,
                                    unsigned int count)
{
    g_surface_textures = textures;
    g_surface_texture_count = textures && count >= 27u ? count : 0u;
}

void road_game_set_hud(const road_sprite_t *hud)
{
    g_hud = hud && hud->pixels && hud->width == 300u &&
        hud->height == 58u ? hud : 0;
}

void road_game_set_hud_needles(const road_sprite_t *speed,
                               const road_sprite_t *health)
{
    g_hud_speed_needle = speed && speed->pixels &&
        speed->width == 48u && speed->height == 6u ? speed : 0;
    g_hud_health_needle = health && health->pixels &&
        health->width == 10u && health->height == 2u ? health : 0;
}

void road_game_set_alternate_hud(const road_sprite_t *health_frames,
                                 unsigned int frame_count,
                                 const road_sprite_t *portrait)
{
    g_hud_health_frames = health_frames && frame_count == 32u ?
        health_frames : 0;
    g_hud_health_frame_count = g_hud_health_frames ? frame_count : 0u;
    g_hud_portrait = portrait && portrait->pixels ? portrait : 0;
}

void road_game_set_hill_profiles(const road_hill_profile_t *profiles,
                                 unsigned int count)
{
    g_hill_profiles = profiles;
    g_hill_profile_count = profiles ? count : 0u;
}

void road_game_set_hill_samples(const road_hill_sample_t *samples,
                                unsigned int count)
{
    g_hill_samples = samples;
    g_hill_sample_count = samples ? count : 0u;
}

void road_game_set_fork_hill_profiles(const road_hill_profile_t *profiles,
                                      unsigned int count)
{
    g_fork_hill_profiles = profiles;
    g_fork_hill_profile_count = profiles ? count : 0u;
}

void road_game_set_fork_hill_samples(const road_hill_sample_t *samples,
                                     unsigned int count)
{
    g_fork_hill_samples = samples;
    g_fork_hill_sample_count = samples ? count : 0u;
}

void road_game_set_edge_profiles(const road_edge_profile_t *left,
                                 unsigned int left_count,
                                 const road_edge_profile_t *right,
                                 unsigned int right_count)
{
    g_edge_profiles[0] = left;
    g_edge_profiles[1] = right;
    g_edge_profile_count[0] = left ? left_count : 0u;
    g_edge_profile_count[1] = right ? right_count : 0u;
}

void road_game_set_edge_outer_samples(const short *left,
                                      const short *right,
                                      unsigned int count)
{
    g_edge_outer_samples[0] = left;
    g_edge_outer_samples[1] = right;
    g_edge_outer_sample_count = left && right ? count : 0u;
}

void road_game_set_edge_fill(const road_sprite_t *primary,
                             const road_sprite_t *horizon)
{
    g_edge_fill_primary = primary && primary->pixels ? primary : 0;
    g_edge_fill_horizon = horizon && horizon->pixels ? horizon : 0;
}

void road_game_set_hill_tiles(const road_sprite_t *tiles,
                              const road_sprite_t *mip_tiles,
                              const road_sprite_t *narrow_tiles,
                              unsigned int count)
{
    g_hill_tiles = tiles;
    g_hill_mips = tiles ? mip_tiles : 0;
    g_hill_narrow_mips = tiles ? narrow_tiles : 0;
    g_hill_tile_count = tiles ? count : 0u;
}

void road_game_set_surface_samples(const road_surface_sample_t *samples,
                                    unsigned int count)
{
    g_surface_samples = samples;
    g_surface_sample_count = samples ? count : 0u;
}

void road_game_set_fork_surface_samples(
    const road_surface_sample_t *samples, unsigned int count)
{
    g_fork_surface_samples = samples;
    g_fork_surface_sample_count = samples ? count : 0u;
}

void road_game_set_margin_resources(unsigned int channel,
    const unsigned char *left_selector,
    const unsigned char *right_selector,
    const unsigned short *left_family,
    const unsigned short *right_family,
    const unsigned char *surface_flags,
    const unsigned char *terrain_mode,
    unsigned int count)
{
    unsigned int sample;
    if (channel > 1u) return;
    g_margin_selector[channel][0] = left_selector;
    g_margin_selector[channel][1] = right_selector;
    g_margin_family[channel][0] = left_family;
    g_margin_family[channel][1] = right_family;
    g_margin_surface_flags[channel] = surface_flags;
    g_margin_terrain_mode[channel] = terrain_mode;
    g_margin_sample_count[channel] = left_selector && right_selector &&
        left_family && right_family && surface_flags && terrain_mode ?
        count : 0u;
    if (channel == 0u) {
        /* project_road_render_side latches the first textured lane:
           ordinary lanes set the threshold to 1, while a depth-adjusted
           lane sets it to its owner node's track index. */
        g_margin_repeat_threshold = 0u;
        for (sample = 0u; sample < g_margin_sample_count[0]; ++sample)
            if (terrain_mode[sample] == 0u) {
                if (surface_flags[sample] == 0u) {
                    g_margin_repeat_threshold = 1u;
                    break;
                }
                if (surface_flags[sample] & 0x10u) {
                    g_margin_repeat_threshold = sample;
                    break;
                }
            }
    }
}

void road_game_set_margin_tiles(const road_margin_tile_t *tiles,
                                unsigned int count)
{
    g_margin_tiles = tiles;
    g_margin_tile_count = tiles ? count : 0u;
}

void road_game_set_roadside_tiles(const road_sprite_t *tiles,
                                  const unsigned short *heights,
                                  const unsigned char *repeat_indices,
                                  const road_sprite_t *repeat_mips,
                                  unsigned int count)
{
    g_roadside_tiles = tiles && heights && repeat_indices &&
        repeat_mips ? tiles : 0;
    g_roadside_tile_heights = g_roadside_tiles ? heights : 0;
    g_roadside_repeat_indices = g_roadside_tiles ? repeat_indices : 0;
    g_roadside_repeat_mips = g_roadside_tiles ? repeat_mips : 0;
    g_roadside_tile_count = g_roadside_tiles ? count : 0u;
}

void road_game_set_cross_section(const unsigned char *left_margin,
                                 const unsigned char *right_margin,
                                 const unsigned char *left_depth,
                                 const unsigned char *right_depth,
                                 const unsigned char *terrain_mode,
                                 unsigned int count)
{
    g_course_left_margin = left_margin;
    g_course_right_margin = right_margin;
    g_course_left_depth = left_depth;
    g_course_right_depth = right_depth;
    g_course_terrain_mode = terrain_mode;
    g_cross_section_count = left_margin && right_margin && left_depth &&
        right_depth && terrain_mode ? count : 0u;
}

void road_game_set_backdrop(const unsigned char *indices,
                            const road_pixel_t *palette,
                            unsigned int width, unsigned int height)
{
    g_backdrop_indices = indices;
    g_backdrop_palette = palette;
    g_backdrop_width = indices && palette && width >= ROAD_WIDTH && height ?
        width : 0u;
    g_backdrop_height = g_backdrop_width ? height : 0u;
}

void road_game_set_sky(const road_sprite_t *sky)
{
    g_sky_layer = sky && sky->pixels && sky->width && sky->height ?
        sky : 0;
}

void road_game_set_scenery(const road_sprite_t *sprites, unsigned int count)
{
    g_scenery = sprites;
    g_scenery_count = sprites ? count : 0u;
}

void road_game_set_scenery_placements(const road_scenery_placement_t *placements,
                                      unsigned int count)
{
    g_scenery_placements = placements;
    g_scenery_placement_count = placements ? count : 0u;
}

void road_game_set_static_scenery(const road_sprite_t *sprites,
                                 unsigned int sprite_count,
                                 const road_static_placement_t *placements,
                                 unsigned int placement_count)
{
    g_static_sprites = sprites;
    g_static_scale_frames = 0;
    g_static_scale_variant_count = 0u;
    g_static_frame_anchors = 0;
    g_static_anchor_variant_count = 0u;
    g_static_sprite_count = sprites ? sprite_count : 0u;
    g_static_placements = placements;
    g_static_placement_count = placements && g_static_sprite_count ?
        placement_count : 0u;
    g_static_collisions = 0;
    g_static_collision_count = 0u;
    g_hazard_moving = 0;
    g_hazard_moving_count = 0u;
}

void road_game_set_static_scale_frames(const road_sprite_t *frames,
                                       unsigned int variant_count)
{
    g_static_scale_frames = frames;
    g_static_scale_variant_count = frames ? variant_count : 0u;
}

void road_game_set_static_frame_anchors(
    const road_static_frame_anchor_t *anchors,
    unsigned int variant_count)
{
    g_static_frame_anchors = anchors;
    g_static_anchor_variant_count = anchors ? variant_count : 0u;
}

void road_game_set_reciprocal_table(const unsigned int *table,
                                    unsigned int count)
{
    unsigned int dy;
    g_reciprocal_table = table;
    g_reciprocal_count = table ? count : 0u;
    if (!g_reciprocal_count) return;
    for (dy = 1u; dy <= ROAD_VIEW_BOTTOM - HORIZON; ++dy) {
        unsigned int low = 1u, high = ROAD_VISIBLE_DEPTH;
        while (low < high) {
            unsigned int middle = low + (high - low + 1u) / 2u;
            if ((unsigned int)road_projection_dy(middle) >= dy)
                low = middle;
            else
                high = middle - 1u;
        }
        g_depth_for_dy[dy] = (unsigned short)low;
    }
}

void road_game_set_static_collisions(const road_static_collision_t *collisions,
                                    unsigned int count)
{
    g_static_collisions = collisions;
    g_static_collision_count = collisions && g_static_sprite_count ?
        count : 0u;
}

void road_game_set_hazard_motion(const unsigned char *moving,
                                 unsigned int count)
{
    g_hazard_moving = moving;
    g_hazard_moving_count = moving ? count : 0u;
}

void road_game_set_effects(road_game_t *game,
                           const road_effect_placement_t *placements,
                           unsigned int count)
{
    unsigned int i;
    g_effect_placements = placements;
    g_effect_count = placements ? count : 0u;
    if (!game) return;
    game->effect_cursor = 0u;
    for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i)
        game->effects[i].active = 0u;
    while (game->effect_cursor < g_effect_count &&
           (unsigned int)g_effect_placements[game->effect_cursor].sample *
               256u + 500u < game->distance)
        game->effect_cursor++;
}

void road_game_set_crossing_zones(const road_crossing_zone_t *zones,
                                  unsigned int count)
{
    g_crossing_zones = zones;
    g_crossing_count = zones ? count : 0u;
}

void road_game_set_effect_sprites(const road_sprite_t *state_frames,
                                  unsigned int actor_style_count)
{
    g_effect_sprites = state_frames;
    g_effect_sprite_count = state_frames ? actor_style_count : 0u;
    g_effect_frame_ticks = 0;
    g_effect_tick_style_count = 0u;
}

void road_game_set_effect_frame_ticks(const unsigned char *ticks,
                                      unsigned int actor_style_count)
{
    g_effect_frame_ticks = ticks;
    g_effect_tick_style_count = ticks ? actor_style_count : 0u;
}

static int hazard_lateral(const road_static_placement_t *placement,
                          unsigned int elapsed_ms)
{
    unsigned int phase;
    int triangle;
    if (!g_hazard_moving ||
        placement->variant >= g_hazard_moving_count ||
        !g_hazard_moving[placement->variant]) return placement->lateral;
    phase = (elapsed_ms / 80u + (unsigned int)placement->sample * 7u) & 63u;
    triangle = phase < 32u ? (int)phase : 64 - (int)phase;
    return placement->lateral + (triangle - 16) * 2;
}

void road_game_set_bikes(const road_sprite_t *sprites, unsigned int count)
{
    g_bikes = sprites;
    g_bike_count = sprites ? count : 0u;
}

void road_game_set_cars(const road_sprite_t *sprites, unsigned int count)
{
    g_cars = sprites;
    g_car_count = sprites ? count : 0u;
    g_car_animations = 0;
    g_car_animation_count = 0u;
}

void road_game_set_car_animations(const road_car_animation_t *animations,
                                  unsigned int count)
{
    g_car_animations = animations;
    g_car_animation_count = animations ? count : 0u;
    g_cars = 0;
    g_car_count = 0u;
}

unsigned int road_car_select_frame(unsigned int frame_count,
                                   int lateral, int projected_height)
{
    unsigned int lod, view = 0u;
    if (frame_count == 0u) return 0u;
    lod = projected_height < 13 ? 2u :
          projected_height < 27 ? 1u : 0u;
    if (frame_count == 3u) return lod;
    if (frame_count == 12u) {
        if (lateral < -90 || lateral > 90) view = 3u;
        else if (lateral < -40) view = 1u;
        else if (lateral > 40) view = 2u;
        return view * 3u + lod;
    }
    return 0u;
}

unsigned int road_car_select_mapped_frame(const road_car_animation_t *animation,
                                          unsigned int distance)
{
    unsigned int row, column, index;
    const road_car_frame_map_t *map;
    if (!animation || !animation->frame_count) return 0u;
    map = animation->frame_map;
    if (!map || !map->column_count || map->column_count > 16u)
        return road_car_select_frame(animation->frame_count, 0,
                                     distance > 0xa00u ? 10 :
                                     distance > 0x500u ? 20 : 30);
    /* create_track_object selects Cars/Car1/Car2 at 0x500/0xA00 world
     * units, independent of screen height. The CANS groups are stored in
     * Car1, Car2, Cars order. */
    row = distance > 0xa00u ? 1u : distance > 0x500u ? 0u : 2u;
    /* TrackObject's CANS bank reads previous_orientation.steering_heading.
     * Unlike rider steering, track-object steering only updates movement_heading,
     * so the bank stays zero; the direction-specific ANIM supplies the view. */
    column = map->column_count / 2u;
    index = map->frames[row][column];
    return index < animation->frame_count ? index : 0u;
}

unsigned int road_car_select_animation(const road_car_animation_t *animations,
                                        unsigned int count,
                                        unsigned int collision_mode,
                                        unsigned int choice)
{
    unsigned int i, eligible = 0u, target;
    unsigned int limit = count > 1u ? count - 1u : count;
    unsigned int mask = collision_mode & 1u ? 0x40000u :
                        collision_mode == 2u ? 0x80000u : 0x100000u;
    if (!animations || !count) return count;
    /* create_track_object scans all but the last ANIM and keeps resources
     * without direction flags or with a matching mode flag. */
    for (i = 0u; i < limit; ++i) {
        unsigned int flags = animations[i].frame_map ?
            animations[i].frame_map->direction_flags : 0u;
        if (!flags || (flags & mask)) ++eligible;
    }
    if (!eligible) return count;
    target = choice % eligible;
    for (i = 0u; i < limit; ++i) {
        unsigned int flags = animations[i].frame_map ?
            animations[i].frame_map->direction_flags : 0u;
        if (!flags || (flags & mask)) {
            if (!target) return i;
            --target;
        }
    }
    return count;
}

int road_car_projected_height(const road_car_animation_t *animation,
                               const road_sprite_t *sprite,
                               unsigned int distance)
{
    unsigned int row = distance > 0xa00u ? 1u :
                       distance > 0x500u ? 0u : 2u;
    unsigned int multiplier = animation && animation->frame_map ?
        animation->frame_map->scale_multiplier[row] : 1u;
    unsigned long long scaled;
    if (!sprite || !sprite->height || !distance) return 1;
    if (!multiplier) multiplier = 1u;
    /* render_road_projected_cel(base_scale=0x500):
     * step = reciprocal[depth/2] * (0x500>>6) >> 2 = reciprocal * 5.
     * The CANS double/quadruple flags multiply that fixed-point step. */
    scaled = (unsigned long long)sprite->height *
             road_reciprocal(distance) * 5u * multiplier;
    scaled = (scaled + 0x8000u) >> 16u;
    if (scaled < 1u) return 1;
    if (scaled > 4096u) return 4096;
    return (int)scaled;
}

static int road_fork_in_view(const road_game_t *game)
{
    unsigned int sample = game->distance / 256u;
    return g_fork_count && (sample < g_fork_sample ||
        (g_fork_active_span &&
         (sample >= g_fork_visible_start ||
          (g_fork_reverse_mode &&
           sample + ROAD_VISIBLE_DEPTH / 256u >= g_fork_visible_start)) &&
         sample < g_fork_end_sample));
}

static int road_fork_sample_index(unsigned int sample,
                                  unsigned int *mapped)
{
    int index = (int)sample + g_fork_sample_shift;
    if (index < 0 || (unsigned int)index >= g_fork_count) return 0;
    *mapped = (unsigned int)index;
    return 1;
}

static void road_fork_relative_position(const road_game_t *game,
                                        int *x, int *heading, int *y)
{
    unsigned int sample = game->distance / 256u;
    unsigned int fraction = game->distance & 255u;
    if (!g_fork_active_span || sample >= g_fork_end_sample ||
        (sample < g_fork_visible_start &&
         (!g_fork_reverse_mode ||
          sample + ROAD_VISIBLE_DEPTH / 256u < g_fork_visible_start))) {
        *x = *heading = *y = 0;
        return;
    }
    if (g_fork_reverse_mode) {
        unsigned int at = g_fork_end_sample;
        *x = *heading = *y = 0;
        while (at > sample) {
            unsigned int mapped;
            --at;
            if (!road_fork_sample_index(at, &mapped)) break;
            *x -= *heading;
            *heading -= (int)g_fork_curvature[mapped] -
                        (int)g_course_curvature[at];
            *y -= (int)g_fork_elevation[mapped] -
                  (int)g_course_elevation[at];
        }
        if (fraction) {
            unsigned int mapped;
            if (road_fork_sample_index(sample, &mapped)) {
                int next_heading = *heading +
                    (int)g_fork_curvature[mapped] -
                    (int)g_course_curvature[sample];
                *x += next_heading * (int)fraction / 256;
                *heading += (next_heading - *heading) *
                            (int)fraction / 256;
                *y += ((int)g_fork_elevation[mapped] -
                       (int)g_course_elevation[sample]) *
                      (int)fraction / 256;
            }
        }
        return;
    }
    if (g_fork_relative_sample < g_fork_sample ||
        g_fork_relative_sample > sample) {
        g_fork_relative_sample = g_fork_sample;
        g_fork_relative_heading = 0;
        g_fork_relative_x = 0;
        g_fork_relative_y = 0;
    }
    while (g_fork_relative_sample < sample) {
        unsigned int at = g_fork_relative_sample++;
        g_fork_relative_heading +=
            (int)g_fork_curvature[at] - (int)g_course_curvature[at];
        g_fork_relative_x += g_fork_relative_heading;
        g_fork_relative_y +=
            (int)g_fork_elevation[at] - (int)g_course_elevation[at];
    }
    *heading = g_fork_relative_heading;
    *x = g_fork_relative_x;
    *y = g_fork_relative_y;
    if (fraction) {
        int next_heading = *heading +
            (int)g_fork_curvature[sample] -
            (int)g_course_curvature[sample];
        *x += next_heading * (int)fraction / 256;
        *heading += (next_heading - *heading) * (int)fraction / 256;
        *y += ((int)g_fork_elevation[sample] -
               (int)g_course_elevation[sample]) *
              (int)fraction / 256;
    }
}

static void project_course(const road_game_t *game)
{
    unsigned int i;
    unsigned int sample = game->distance / 256u;
    int slope = 0;
    int offset = 0;
    int elevation = 0;
    int fork_slope = 0;
    int fork_offset = 0;
    int fork_elevation = 0;
    road_fork_relative_position(game, &fork_offset, &fork_slope,
                                &fork_elevation);
    g_course_projection[0] = 0;
    g_fork_projection[0] = fork_offset;
    g_fork_elevation_projection[0] = fork_elevation;
    g_elevation_projection[0] = 0;
    for (i = 1u; i < COURSE_LOOKAHEAD; ++i) {
        unsigned int fork_sample;
        if (sample < g_course_count)
            slope += g_course_curvature[sample];
        offset += slope;
        g_course_projection[i] = offset;
        if (road_fork_sample_index(sample, &fork_sample))
            fork_slope += g_fork_curvature[fork_sample];
        fork_offset += fork_slope;
        g_fork_projection[i] = fork_offset;
        if (road_fork_sample_index(sample, &fork_sample))
            fork_elevation += g_fork_elevation[fork_sample];
        g_fork_elevation_projection[i] = fork_elevation;
        if (sample < g_elevation_count)
            elevation += g_course_elevation[sample];
        g_elevation_projection[i] = elevation;
        sample++;
    }
}

static int road_project_y_from_elevation(const road_game_t *game,
                                          unsigned int world, int base_y,
                                          const int *projection,
                                          unsigned int count)
{
    unsigned int delta;
    unsigned int index;
    unsigned int fraction;
    int elevation;
    int dy = base_y - HORIZON;
    if (!count || world <= game->distance || dy <= 0)
        return base_y;
    delta = world - game->distance;
    index = delta / 256u;
    fraction = delta & 255u;
    if (index >= COURSE_LOOKAHEAD - 1u)
        index = COURSE_LOOKAHEAD - 2u;
    elevation = projection[index] +
        (projection[index + 1u] - projection[index]) *
        (int)fraction / 256;
    return base_y - road_project_world_delta(elevation, delta);
}

static int road_project_y(const road_game_t *game, unsigned int world,
                          int base_y)
{
    return road_project_y_from_elevation(game, world, base_y,
        g_elevation_projection, g_elevation_count);
}

static int road_fork_project_y(const road_game_t *game,
                               unsigned int world, int base_y)
{
    return road_project_y_from_elevation(game, world, base_y,
        g_fork_elevation_projection, g_fork_count);
}

static int road_center(const road_game_t *game, unsigned int world, int dy)
{
    int bend;
    unsigned int depth = world > game->distance ?
        world - game->distance : 1u;
    int unit = road_projection_unit(depth);
    (void)dy;
    if (g_course_count) {
        unsigned int delta = world - game->distance;
        unsigned int index = delta / 256u;
        unsigned int fraction = delta & 255u;
        if (index >= COURSE_LOOKAHEAD - 1u)
            index = COURSE_LOOKAHEAD - 2u;
        bend = g_course_projection[index] +
            (g_course_projection[index + 1u] - g_course_projection[index]) *
            (int)fraction / 256;
        return ROAD_VIEW_WIDTH / 2 +
            road_project_world_delta(bend, depth) -
            game->lane * unit / 111;
    }
    bend = track_offset(world) - track_offset(game->distance);
    return ROAD_VIEW_WIDTH / 2 +
        road_project_world_delta(bend, depth) -
        game->lane * unit / 111;
}

static int road_fork_center(const road_game_t *game,
                            unsigned int world, int dy)
{
    unsigned int delta = world - game->distance;
    unsigned int index = delta / 256u;
    unsigned int fraction = delta & 255u;
    int bend;
    int unit = road_projection_unit(delta ? delta : 1u);
    (void)dy;
    if (index >= COURSE_LOOKAHEAD - 1u)
        index = COURSE_LOOKAHEAD - 2u;
    bend = g_fork_projection[index] +
        (g_fork_projection[index + 1u] - g_fork_projection[index]) *
        (int)fraction / 256;
    return ROAD_VIEW_WIDTH / 2 +
        road_project_world_delta(bend, delta ? delta : 1u) -
        game->lane * unit / 111 +
        (g_fork_reverse_mode ?
         g_fork_reverse_center_offset * unit / 512 : 0);
}

/* In textured-road mode each 3DO side horizon is its projected road center.
 * Scan near to far as update_visibility does: clamp sort_depth against the
 * nearer node, then reject each side facing behind its own next node. Profile
 * and edge modes need their own surface-profile horizon, so leave those
 * visible until that part of the original projection is ported. */
static void project_road_surface_visibility(const road_game_t *game)
{
    int side_horizon[2][33], sort_depth[33];
    int previous = ROAD_VIEW_BOTTOM;
    unsigned int node;
    g_road_visibility_first_sample = game->distance / 256u + 1u;
    g_projected_horizon_bottom = ROAD_VIEW_BOTTOM;
    for (node = 0u; node < 33u; ++node) {
        unsigned int sample = g_road_visibility_first_sample + node;
        unsigned int world = sample * 256u;
        unsigned int depth = world - game->distance;
        int dy = road_projection_dy(depth);
        int main_y = road_project_y(game, world, HORIZON + dy);
        int alternate_y = main_y;
        int dual = road_fork_in_view(game) &&
            sample >= g_fork_visible_start &&
            sample < g_fork_end_sample;
        if (dual)
            alternate_y = road_fork_project_y(game, world,
                HORIZON + dy);
        sort_depth[node] = main_y > alternate_y ? main_y : alternate_y;
        if (sort_depth[node] < g_projected_horizon_bottom)
            g_projected_horizon_bottom = sort_depth[node];
        side_horizon[0][node] = main_y;
        side_horizon[1][node] = dual ? alternate_y : main_y;
    }
    for (node = 0u; node < 32u; ++node) {
        unsigned int sample = g_road_visibility_first_sample + node;
        int textured = g_course_terrain_mode &&
            sample + 1u < g_cross_section_count &&
            g_course_terrain_mode[sample] == 0u &&
            g_course_terrain_mode[sample + 1u] == 0u;
        int value = node ? sort_depth[node] : ROAD_VIEW_BOTTOM;
        if (!textured) {
            g_road_surface_visible[0][node] = 1u;
            g_road_surface_visible[1][node] = 1u;
            g_projected_sort_depth[node] = ROAD_VIEW_BOTTOM;
            previous = ROAD_VIEW_BOTTOM;
            continue;
        }
        if (previous <= value) value = previous;
        g_projected_sort_depth[node] = value;
        g_road_surface_visible[0][node] =
            value >= side_horizon[0][node + 1u] &&
            side_horizon[0][node] >= side_horizon[0][node + 1u];
        g_road_surface_visible[1][node] =
            value >= side_horizon[1][node + 1u] &&
            side_horizon[1][node] >= side_horizon[1][node + 1u];
        previous = value;
    }
}

static int road_surface_sample_visible(unsigned int sample,
                                       unsigned int channel)
{
    unsigned int index;
    if (sample < g_road_visibility_first_sample) return 1;
    index = sample - g_road_visibility_first_sample;
    return index >= 32u || g_road_surface_visible[channel][index] != 0u;
}

/* The 3DO roadside CEL renderer rejects an object when its projected top
 * lies below the owning node's clamped sort depth. These placements are
 * attached to whole track samples, so use the corresponding node. */
static int road_roadside_object_visible(unsigned int sample, int top)
{
    unsigned int index;
    if (sample < g_road_visibility_first_sample) return 1;
    index = sample - g_road_visibility_first_sample;
    if (index >= 32u) return 1;
    if (g_projected_sort_depth[index] > ROAD_VIEW_BOTTOM)
        return top <= ROAD_VIEW_BOTTOM;
    return top < g_projected_sort_depth[index];
}

/* render_road_projected_cel uses its clip pointer as a RoadRenderNode:
 * the field at offset 0x10 is sort_depth. Car ANIMs and track-effect
 * children use mode 1, which rejects frames whose top is behind it.
 * The primary racer CEL uses mode 0 and bypasses this test. */
static int road_projected_secondary_visible(unsigned int world, int top)
{
    unsigned int sample = (world + 255u) / 256u;
    unsigned int index;
    if (sample < g_road_visibility_first_sample) return 1;
    index = sample - g_road_visibility_first_sample;
    return index >= 32u || top < g_projected_sort_depth[index];
}

void road_game_init(road_game_t *game)
{
    unsigned int i;
    game->distance = 0u;
    game->track_elevation = 0;
    game->road_scroll_heading_8_8 = 0;
    game->road_scroll_generation = g_course_generation;
    game->speed = 0u;
    game->elapsed_ms = 0u;
    game->hits = 0u;
    game->bounce_events = 0u;
    game->hazard_hits = 0u;
    game->sound_event_mask = 0u;
    game->last_static_contact_index = ~0u;
    game->rider_hits = 0u;
    game->rider_max_health = 2500u;
    game->rider_health = 2500u;
    game->rider_recovery_ceiling = 2500u;
    game->bike_max_health = 20000u;
    game->bike_health = 20000u;
    game->recovery_ms = 0u;
    game->crash_elapsed_ms = 0u;
    game->effect_hits = 0u;
    game->effect_cursor = 0u;
    game->crossing_cursor = 0u;
    game->player_stun_ms = 0u;
    game->top_speed = 180u;
    game->accel_percent = 100u;
    game->steer_percent = 100u;
    game->bike_physics_ready = 0u;
    game->velocity_raw = 0u;
    game->vertical_height_8_8 = 0;
    game->vertical_velocity_8_8 = 0;
    game->vertical_surface_acceleration_8_8 = -74;
    game->surface_contact_scale_8_8 = 256u;
    game->contact_threshold = 256u;
    game->minimum_vertical_acceleration = -2560;
    game->bounce_scale = 24u;
    game->impact_strength = 74u;
    game->impact_scale = 4u;
    game->slide_grip_scale_8_8 = 256u;
    game->slide_activation_threshold = 64u;
    game->slip_amount = 0u;
    game->throttle_control = 0;
    game->gear = 0u;
    game->engine_pitch = 0;
    game->engine_gauge_pitch = 0;
    game->previous_engine_forward_velocity_target = 0u;
    game->steering_angle_raw = 0;
    game->traffic_spawn_timer = 3500;
    game->traffic_reverse_timer = 2500;
    game->traffic_seed = 0x5a17u;
    game->finished = 0u;
    game->graph_rank = 0u;
    game->graph_rank_enabled = 0u;
    game->input = 0u;
    game->attack_ms = 0u;
    game->attack_style = 0u;
    game->attack_side = 1u;
    game->last_frame_ms = 0u;
    game->last_render_ms = 0u;
    game->last_present_ms = 0u;
    game->lane = 0;
    game->lean = 0;
    game->lateral_impulse = 0;
    for (i = 0; i < ROAD_OPPONENT_COUNT; ++i) {
        game->opponents[i].distance = 1600u + i * 1100u;
        game->opponents[i].lane = (int)(i % 5u) * 40 - 80;
        game->opponents[i].speed = 52u + (i % 7u) * 8u;
        game->opponents[i].base_speed = game->opponents[i].speed;
        game->opponents[i].graph_curve_speed =
            game->opponents[i].base_speed;
        game->opponents[i].stun_ms = 0u;
        game->opponents[i].hit_reaction_ms = 0u;
        game->opponents[i].target_lane = game->opponents[i].lane;
        game->opponents[i].attack_cooldown_ms = 0u;
        game->opponents[i].behavior = ROAD_AI_CRUISE;
        game->opponents[i].behavior_ms = 0u;
        game->opponents[i].travel_direction = 1;
        game->opponents[i].collision_mode = 0u;
        game->opponents[i].route_visible = 1u;
    }
    for (i = 0u; i < ROAD_TRAFFIC_COUNT; ++i) {
        game->traffic[i].distance = 7500u + i * 7300u;
        game->traffic[i].lane = (i & 1u) ? 65 : -65;
        game->traffic[i].speed = 20u + (i % 3u) * 8u;
        game->traffic[i].base_speed = game->traffic[i].speed;
        game->traffic[i].stun_ms = 0u;
        game->traffic[i].target_lane = game->traffic[i].lane;
        game->traffic[i].attack_cooldown_ms = 0u;
        game->traffic[i].behavior = ROAD_AI_CRUISE;
        game->traffic[i].behavior_ms = 0u;
        game->traffic[i].travel_direction = (i & 1u) ? 1 : -1;
        game->traffic[i].collision_mode =
            game->traffic[i].travel_direction < 0 ? 2u : 0u;
        game->traffic[i].animation_choice = road_traffic_random(game);
        game->traffic[i].lane_fraction = 0u;
    }
    for (i = 0u; i < ROAD_CROSSING_COUNT; ++i) {
        game->crossing[i].distance = 0u;
        game->crossing[i].animation_choice = 0u;
    }
    for (i = 0u; i < ROAD_CROSSING_PARENT_COUNT; ++i)
        game->crossing_parents[i].active = 0u;
    for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i)
        game->effects[i].active = 0u;
}

unsigned int road_game_finish_place(const road_game_t *game)
{
    unsigned int place = 0u;
    unsigned int i;
    if (game->graph_rank_enabled) return game->graph_rank;
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        if (game->opponents[i].distance > game->distance) ++place;
    return place;
}

void road_game_set_bike_tuning(road_game_t *game, unsigned int speed_factor,
                               unsigned int tuning_base,
                               unsigned int steering_scale)
{
    if (!game || speed_factor < 20u || speed_factor > 80u ||
        tuning_base < 64u || tuning_base > 512u ||
        steering_scale < 16u || steering_scale > 512u) return;
    /* The SPEC fields are authentic; this is a 9588-scale driving adapter. */
    game->top_speed = 28u + speed_factor * 4u;
    game->accel_percent = 75u + tuning_base / 8u;
    game->steer_percent = 60u + steering_scale / 2u;
}

void road_game_set_bike_spec(road_game_t *game, const rr_bike_spec_t *spec)
{
    unsigned int i, rise_divisor, fall_divisor;
    unsigned int steering_factor, steering_divisor;
    if (!game || !spec) return;
    road_game_set_bike_tuning(game, spec->forward_speed_factor,
                              spec->tuning_base,
                              spec->steering_speed_scale);
    game->bike_max_health = spec->maximum_bike_health;
    game->bike_health = spec->maximum_bike_health;
    game->engine_pitch = spec->base_engine_pitch;
    game->engine_gauge_pitch = 0;
    game->previous_engine_forward_velocity_target = 0u;
    game->contact_threshold = spec->contact_threshold;
    game->minimum_vertical_acceleration = spec->minimum_acceleration;
    game->bounce_scale = spec->bounce_scale;
    game->impact_strength = spec->impact_strength;
    game->vertical_surface_acceleration_8_8 =
        -(int)spec->impact_strength;
    game->impact_scale = spec->impact_scale;
    game->slide_grip_scale_8_8 = spec->slide_grip_scale_8_8;
    game->slide_activation_threshold = spec->slide_activation_threshold;
    if (!spec->forward_acceleration_divisor ||
        !spec->forward_deceleration_divisor ||
        !spec->braking_rise_divisor || !spec->braking_fall_divisor ||
        spec->braking_factor >= 0) return;
    /* For the original baseline rider rating of 75, the SPEC tuning
     * numerator and denominator cancel in these two control limits. */
    game->accel_limit = (int)(spec->forward_speed_factor * 256u);
    game->brake_target = spec->braking_factor * 256;
    rise_divisor = spec->forward_acceleration_divisor;
    fall_divisor = spec->forward_deceleration_divisor;
    if (rise_divisor > 15u) rise_divisor = 15u;
    if (fall_divisor > 15u) fall_divisor = 15u;
    game->accel_rise_step = game->accel_limit / (int)rise_divisor;
    game->accel_fall_step = game->accel_limit / (int)fall_divisor;
    rise_divisor = spec->braking_rise_divisor;
    fall_divisor = spec->braking_fall_divisor;
    if (rise_divisor > 15u) rise_divisor = 15u;
    if (fall_divisor > 15u) fall_divisor = 15u;
    game->brake_rise_step = -game->brake_target / (int)rise_divisor;
    game->brake_fall_step = -game->brake_target / (int)fall_divisor;
    for (i = 0u; i < 6u; ++i) {
        game->gear_pitch_scale_8_8[i] =
            spec->gear[i].pitch_per_forward_velocity_8_8;
        game->gear_acceleration[i] = spec->gear[i].acceleration;
        game->gear_upshift[i] = spec->gear[i].upshift;
        game->gear_downshift[i] = spec->gear[i].downshift;
    }
    steering_factor = spec->tuning_base + 75u;
    steering_divisor = spec->tuning_base + 75u / 4u;
    game->steering_rise_step = 11776 /
        (int)(spec->primary_steering_response * steering_factor /
              steering_divisor);
    game->steering_fall_step = 11776 /
        (int)(spec->secondary_steering_response * steering_factor /
              steering_divisor);
    game->steering_velocity_scale = spec->steering_velocity_scale;
    game->steering_left_scale = spec->left_steering_rate_scale_8_8;
    game->steering_right_scale = spec->right_steering_rate_scale_8_8;
    if (game->steering_rise_step < 1) game->steering_rise_step = 1;
    if (game->steering_fall_step < 1) game->steering_fall_step = 1;
    game->bike_physics_ready = 1u;
}

/* 3DO reset_rider_to_normal_mode queues impact events 2/3 or 3/4
 * together with recovery events 5/6 or 6/7. The adapted forward speed
 * uses 64 raw velocity units per game speed unit. */
static void road_queue_impact_audio(road_game_t *game,
                                    unsigned int speed)
{
    unsigned int variant = (game->elapsed_ms / 17u) & 1u;
    if (speed * 64u > 6000u) {
        game->sound_event_mask |= (1u << (3u + variant)) |
                                  (1u << (6u + variant));
    } else if (speed * 64u > 2000u) {
        game->sound_event_mask |= (1u << (2u + variant)) |
                                  (1u << (5u + variant));
    }
}

void road_game_apply_collision_damage(road_game_t *game,
                                       unsigned int rider_damage,
                                       unsigned int bike_damage,
                                       unsigned int kind)
{
    unsigned int floor_health, rider_loss, ceiling_loss;
    unsigned int eject;
    if (!game) return;
    if (kind == ROAD_DAMAGE_NONE) return;
    if (game->recovery_ms && game->crash_elapsed_ms) return;
    if (rider_damage > 100000u) rider_damage = 100000u;
    if (bike_damage > 100000u) bike_damage = 100000u;
    eject = !game->recovery_ms && kind != ROAD_DAMAGE_GLANCING &&
        rider_damage >= 5u && bike_damage >= 700u;
    if (kind == ROAD_DAMAGE_SEVERE) {
        rider_damage *= 2u;
        bike_damage *= 2u;
    } else if (kind == ROAD_DAMAGE_GLANCING) {
        rider_damage /= 8u;
        bike_damage /= 8u;
    }
    if (rider_damage > 100000u) rider_damage = 100000u;
    if (bike_damage > 100000u) bike_damage = 100000u;
    /* A grazing impact is attenuated before deciding whether it ejects the
     * rider. The old order compared its full, pre-attenuation values and
     * could launch the rider despite almost no actual health loss. */
    if (kind == ROAD_DAMAGE_GLANCING)
        eject = !game->recovery_ms && rider_damage >= 5u &&
            bike_damage >= 1200u;
    rider_loss = rider_damage * 64u;
    ceiling_loss = rider_damage * 4u;
    floor_health = game->rider_max_health / 2u;
    game->rider_health = rider_loss >= game->rider_health ?
        0u : game->rider_health - rider_loss;
    game->rider_recovery_ceiling =
        ceiling_loss >= game->rider_recovery_ceiling - floor_health ?
        floor_health : game->rider_recovery_ceiling - ceiling_loss;
    game->bike_health = bike_damage >= game->bike_health ?
        0u : game->bike_health - bike_damage;
    if ((!game->rider_health || !game->bike_health) &&
        game->speed >= 20u)
        eject = 1u;
    if ((eject || !game->rider_health || !game->bike_health) &&
        !game->recovery_ms) {
        game->recovery_ms = eject ? 3600u : 1500u;
        game->crash_elapsed_ms = eject ? 1u : 0u;
        game->speed = 0u;
        game->velocity_raw = 0u;
    }
}

static void road_step_original_steering(road_game_t *game,
                                        int steer, unsigned int dt_ms)
{
    unsigned int ticks = (dt_ms * 60u + 500u) / 1000u;
    int target = 0, current = game->steering_angle_raw;
    int step, scale, delta, left_bound, right_bound;
    unsigned int surface_grip, budget, required;
    if (ticks == 0u && dt_ms) ticks = 1u;
    if (steer && game->velocity_raw) {
        target = (int)(game->velocity_raw * 181u /
                       game->steering_velocity_scale);
        if (target > 11776) target = 11776;
        target *= steer;
    }
    step = target &&
           (current < 0 ? -current : current) <
           (target < 0 ? -target : target) ?
           game->steering_rise_step : game->steering_fall_step;
    step *= (int)ticks;
    scale = steer < 0 ? (int)game->steering_left_scale :
            steer > 0 ? (int)game->steering_right_scale : 256;
    step = step * scale / 256;
    if (step < 1) step = 1;
    if (current < target) {
        current += step;
        if (current > target) current = target;
    } else if (current > target) {
        current -= step;
        if (current < target) current = target;
    }
    game->steering_angle_raw = current;
    road_lane_bounds(game->distance, &left_bound, &right_bound);
    surface_grip = game->lane >= left_bound &&
        game->lane <= right_bound ? 256u :
        100u;
    if (surface_grip == 100u) {
        int shoulder_left, shoulder_right;
        road_shoulder_bounds(game->distance, &shoulder_left,
                             &shoulder_right);
        if (game->lane >= shoulder_left && game->lane <= shoulder_right)
            surface_grip = 128u;
    }
    /* The player template has base/active slip budgets of 300/256. */
    budget = (game->slip_amount ? 256u : 300u) *
        game->slide_grip_scale_8_8 / 256u;
    budget = budget * surface_grip / 256u;
    budget = budget * game->surface_contact_scale_8_8 / 256u;
    required = (game->velocity_raw / 2048u) *
        (unsigned int)((current < 0 ? -current : current) / 256);
    if (game->throttle_control > 0)
        required += (unsigned int)game->throttle_control *
            game->gear_acceleration[game->gear] / 65536u;
    game->slip_amount = required > budget ?
        (required - budget > 255u ? 255u : required - budget) : 0u;
    if (game->slip_amount > 40u && game->velocity_raw) {
        unsigned int loss = (surface_grip == 256u ? 6u : 1u) * ticks;
        game->velocity_raw = game->velocity_raw > loss ?
            game->velocity_raw - loss : 0u;
        game->speed = game->velocity_raw / 64u;
    }
    /* Keep the low-speed fraction until the final division.  The old
     * current/256 truncation made a held direction key produce no lateral
     * movement at the speeds reached immediately after race start. */
    delta = (int)game->steering_velocity_scale * current *
            (int)dt_ms / (15000 * 256);
    if (!game->surface_contact_scale_8_8) delta = 0;
    else if (required > budget && required)
        delta = delta * (int)budget / (int)required;
    game->lane = clamp_int(game->lane + delta, -128, 128);
    game->lean = clamp_int(current / 3000, -3, 3);
}

static void road_step_original_forward(road_game_t *game,
                                       unsigned int input,
                                       unsigned int dt_ms, int steer)
{
    unsigned int ticks = (dt_ms * 60u + 500u) / 1000u;
    int target = (input & ROAD_BRAKE) ? game->brake_target :
                 (input & ROAD_ACCEL) ? game->accel_limit : 0;
    int step, drive, drag, speed;
    if (ticks == 0u && dt_ms) ticks = 1u;
    if (game->speed != game->velocity_raw / 64u)
        game->velocity_raw = game->speed * 64u;
    if (game->throttle_control < target) {
        step = game->throttle_control < 0 ? game->brake_fall_step :
               game->accel_rise_step;
        game->throttle_control += step * (int)ticks;
        if (game->throttle_control > target) game->throttle_control = target;
    } else if (game->throttle_control > target) {
        step = game->throttle_control > 0 ? game->accel_fall_step :
               game->brake_rise_step;
        game->throttle_control -= step * (int)ticks;
        if (game->throttle_control < target) game->throttle_control = target;
    }
    drive = game->throttle_control > 0 ?
        game->throttle_control * (int)game->gear_acceleration[game->gear] /
            65536 : game->throttle_control / 256;
    /* The road-contact drag constants and 8-unit base drag follow the
     * upstream forward-velocity routine and player creation template. */
    drag = 8 + (int)(game->velocity_raw * 130u / 65536u);
    int left_bound, right_bound;
    road_lane_bounds(game->distance, &left_bound, &right_bound);
    if (game->lane < left_bound || game->lane > right_bound)
        drag += (int)(game->velocity_raw * 20u / 16384u);
    drag += (steer < 0 ? -steer : steer) *
            (int)game->steer_percent / 16;
    speed = (int)game->velocity_raw + (int)ticks * (drive - drag);
    if (speed < 0) speed = 0;
    if (speed > (int)(game->top_speed * 64u))
        speed = (int)(game->top_speed * 64u);
    game->velocity_raw = (unsigned int)speed;
    if (game->gear < 5u && game->velocity_raw >
        game->gear_upshift[game->gear]) game->gear++;
    else if (game->gear > 0u && (int)game->velocity_raw <
             game->gear_downshift[game->gear]) game->gear--;
    game->speed = game->velocity_raw / 64u;
}

static void road_step_engine_pitch(road_game_t *game)
{
    int target, delta;
    unsigned int velocity = game->velocity_raw;
    unsigned int gear = game->gear < 6u ? game->gear : 5u;
    if (!game->bike_physics_ready) return;
    /* update_rider_engine_pitch: the selected SPEC gear supplies the
     * velocity-to-pitch scale; airborne and recovery have separate ramps. */
    if (game->previous_engine_forward_velocity_target != velocity ||
        velocity == 0u) {
        if (game->vertical_height_8_8 > 0x64 && velocity) {
            target = game->engine_pitch + 0x1000;
            if (target > 0xffff) target = 0xffff;
        } else {
            game->previous_engine_forward_velocity_target = velocity;
            target = (game->gear_pitch_scale_8_8[gear] *
                      (int)velocity) / 256 + 0x2000;
            if ((game->recovery_ms || game->player_stun_ms) &&
                game->engine_pitch > 0x2000)
                target = game->engine_pitch - 0x1000;
            else {
                delta = target - game->engine_pitch;
                if (delta > 0x2000) target = game->engine_pitch + 0x2000;
                if (delta < -0x2000) target = game->engine_pitch - 0x2000;
            }
        }
        game->engine_pitch = target;
    }
    /* draw_player_engine_pitch_needle smooths the gauge separately. */
    target = game->engine_pitch - 0x3000;
    delta = target - game->engine_gauge_pitch;
    if (delta > 0x5000)
        game->engine_gauge_pitch += 0x5000;
    else if (delta < -0x5000)
        game->engine_gauge_pitch -= 0x5000;
    else
        game->engine_gauge_pitch = target;
    if (game->engine_gauge_pitch < -0x1000)
        game->engine_gauge_pitch = -0x1000;
}

/* The 3DO surface solver works in 8.8 height units and carries vertical
 * velocity between road samples.  Keep the same contact/bounce parameters
 * from SPEC while adapting its 60 Hz timestep to the host loop. */
static void road_step_vertical(road_game_t *game, int road_delta_8_8,
                               unsigned int dt_ms)
{
    int ticks = (int)((dt_ms * 60u + 500u) / 1000u);
    int next_height, next_velocity, threshold;
    if (!dt_ms) return;
    if (ticks < 1) ticks = 1;
    next_height = game->vertical_height_8_8 +
        game->vertical_velocity_8_8 * ticks - road_delta_8_8;
    next_velocity = game->vertical_velocity_8_8 +
        game->vertical_surface_acceleration_8_8 * ticks;
    threshold = (int)game->contact_threshold * (ticks * 4 - 3);
    if (next_height <= threshold) {
        if (game->vertical_height_8_8 > threshold &&
            next_velocity < game->minimum_vertical_acceleration) {
            game->bounce_events++;
            game->vertical_velocity_8_8 =
                -next_velocity * (int)game->bounce_scale / 256;
            game->vertical_surface_acceleration_8_8 =
                -(int)(game->impact_strength * 2u);
            game->surface_contact_scale_8_8 = 256u;
        } else {
            if (game->vertical_height_8_8 > threshold)
                game->slip_amount = 1u;
            game->vertical_velocity_8_8 = road_delta_8_8 / ticks;
            game->surface_contact_scale_8_8 = 256u;
        }
        game->vertical_height_8_8 = 0;
    } else {
        int limit = (int)(game->impact_strength * game->impact_scale);
        game->vertical_height_8_8 = clamp_int(next_height, 0, 32768);
        game->vertical_velocity_8_8 = clamp_int(next_velocity,
            -32768, limit);
        game->vertical_surface_acceleration_8_8 =
            -(int)game->impact_strength;
        game->surface_contact_scale_8_8 = 0u;
    }
}

void road_game_seek(road_game_t *game, unsigned int distance)
{
    unsigned int previous_sample = game->distance / 256u;
    unsigned int next_sample;
    unsigned int scroll_cursor = game->distance;
    if (g_course_count && distance > g_course_count * 256u)
        distance = g_course_count * 256u;
    next_sample = distance / 256u;
    if (game->road_scroll_generation != g_course_generation) {
        game->road_scroll_heading_8_8 = 0;
        game->road_scroll_generation = g_course_generation;
        scroll_cursor = 0u;
    }
    if (g_course_curvature) {
        while (scroll_cursor < distance) {
            unsigned int sample = scroll_cursor / 256u;
            unsigned int end = (sample + 1u) * 256u;
            if (end > distance) end = distance;
            if (sample < g_course_count)
                game->road_scroll_heading_8_8 +=
                    (int)g_course_curvature[sample] *
                    (int)(end - scroll_cursor);
            scroll_cursor = end;
        }
        while (scroll_cursor > distance) {
            unsigned int sample = (scroll_cursor - 1u) / 256u;
            unsigned int start = sample * 256u;
            if (start < distance) start = distance;
            if (sample < g_course_count)
                game->road_scroll_heading_8_8 -=
                    (int)g_course_curvature[sample] *
                    (int)(scroll_cursor - start);
            scroll_cursor = start;
        }
    }
#ifdef ROAD_USE_UPSTREAM_PATH
    if (g_path_ready) {
        while ((unsigned int)g_path_cursor.track_position != distance) {
            unsigned int current = (unsigned int)g_path_cursor.track_position;
            unsigned int step;
            if (distance > current) {
                step = 256u - (current & 255u);
                if (step > distance - current) step = distance - current;
                advance_road_path_traversal(&g_path_cursor, (int)step);
            } else {
                step = (current & 255u) ? (current & 255u) : 256u;
                if (step > current - distance) step = current - distance;
                advance_road_path_traversal(&g_path_cursor, -(int)step);
            }
        }
        game->track_elevation = g_path_cursor.elevation;
    } else
#endif
    if (g_course_elevation && g_elevation_count) {
        while (previous_sample < next_sample) {
            if (previous_sample < g_elevation_count)
                game->track_elevation +=
                    (int)g_course_elevation[previous_sample];
            previous_sample++;
        }
        while (previous_sample > next_sample) {
            previous_sample--;
            if (previous_sample < g_elevation_count)
                game->track_elevation -=
                    (int)g_course_elevation[previous_sample];
        }
    }
    game->distance = distance;
}

static void road_spawn_traffic(road_game_t *game, int direction)
{
    unsigned int i;
    for (i = 0u; i < ROAD_TRAFFIC_COUNT; ++i) {
        road_rider_t *car = &game->traffic[i];
        if (car->distance || car->travel_direction != direction) continue;
        road_traffic_random(game);
        car->distance = game->distance + 18000u +
                        ((game->traffic_seed >> 16u) & 8191u);
        car->lane = (game->traffic_seed & 1u) ? 65 : -65;
        car->speed = 20u + ((game->traffic_seed >> 8u) & 31u);
        car->collision_mode = direction < 0 ? 2u : 0u;
        car->animation_choice = road_traffic_random(game);
        return;
    }
}

static void road_emit_crossing_car(road_game_t *game,
                                    const road_crossing_zone_t *zone,
                                    unsigned int child)
{
    road_rider_t *car = 0;
    unsigned int i, mode, world;
    int spread, offset, scale;
    for (i = 0u; i < ROAD_CROSSING_COUNT; ++i)
        if (!game->crossing[i].distance) {
            car = &game->crossing[i];
            break;
        }
    if (!car) return;
    mode = zone->mode == 1u ? child : zone->mode == 2u ? 3u : 1u;
    spread = zone->spread;
    if (zone->mode == 1u) spread /= 2;
    if (spread < 0) spread = -spread;
    road_traffic_random(game);
    offset = spread ?
        (int)((game->traffic_seed >> 16u) % (unsigned int)spread) : 0;
    if (zone->mode == 1u && mode == 3u) offset += spread;
    world = ((unsigned int)zone->start_sample +
        (unsigned int)offset + 1u) * 256u;
    if (world + 500u < game->distance ||
        world > game->distance + 12000u) return;
    car->distance = world;
    car->lane = mode == 1u ? -125 : 125;
    car->target_lane = -car->lane;
    scale = zone->simulation_scale;
    if (scale < 0) scale = -scale;
    car->speed = (unsigned int)clamp_int(50 + scale / 8, 40, 85);
    car->base_speed = car->speed;
    car->travel_direction = 0;
    car->collision_mode = mode;
    car->animation_choice = road_traffic_random(game);
    car->lane_fraction = 0u;
}

static void road_step_crossing_zones(road_game_t *game)
{
    unsigned int i, child;
    unsigned int window_left = game->distance > 0x2f00u ?
        game->distance - 0x2f00u : 0u;
    unsigned int window_right = game->distance + 0x4e00u;
    unsigned int frame_tick = (game->elapsed_ms / 1000u) * 60u +
        ((game->elapsed_ms % 1000u) * 60u) / 1000u;
    /* The 3DO parent can have four phase clocks. Normal-zone odd children
     * create cars when phase 2 wraps to 0; short zones omit both odd ones. */
    while (game->crossing_cursor < g_crossing_count) {
        const road_crossing_zone_t *zone =
            &g_crossing_zones[game->crossing_cursor];
        road_crossing_parent_t *parent = 0;
        /* Forward section traversal spawns the parent on the RSEC terminal
         * cell, when that cell enters the road runtime's front window. */
        if ((unsigned int)zone->end_sample * 256u >
            game->distance + 0x5000u) break;
        if ((unsigned int)zone->end_sample * 256u + 0x3000u <
            game->distance) {
            game->crossing_cursor++;
            continue;
        }
        for (i = 0u; i < ROAD_CROSSING_PARENT_COUNT; ++i)
            if (!game->crossing_parents[i].active) {
                parent = &game->crossing_parents[i];
                break;
            }
        if (!parent) break;
        parent->active = 1u;
        parent->zone_index = (unsigned short)game->crossing_cursor++;
        parent->child_mask = 0x0fu;
        if (zone->short_timing)
            parent->child_mask &= (unsigned char)~0x0au;
        else if (zone->mode == 2u)
            parent->child_mask &= (unsigned char)~0x02u;
        else if (zone->mode == 3u)
            parent->child_mask &= (unsigned char)~0x08u;
        if (g_width_count && g_course_right_width &&
            zone->start_sample < g_width_count &&
            g_course_right_width[zone->start_sample] < 256u)
            parent->child_mask &= (unsigned char)~0x01u;
        if (g_width_count && g_course_left_width && zone->end_sample &&
            (unsigned int)zone->end_sample - 1u < g_width_count &&
            g_course_left_width[zone->end_sample - 1u] < 256u)
            parent->child_mask &= (unsigned char)~0x04u;
        parent->last_update_tick = frame_tick - 30u;
        parent->next_update_tick = frame_tick;
        for (child = 0u; child < 4u; ++child) {
            parent->phase[child] = (child & 1u) ? 0u : 2u;
            parent->timer[child] = 780u;
        }
    }
    for (i = 0u; i < ROAD_CROSSING_PARENT_COUNT; ++i) {
        road_crossing_parent_t *parent = &game->crossing_parents[i];
        const road_crossing_zone_t *zone;
        unsigned int ticks;
        if (!parent->active) continue;
        zone = &g_crossing_zones[parent->zone_index];
        if ((unsigned int)zone->end_sample * 256u + 0x3000u <
            game->distance) {
            parent->active = 0u;
            continue;
        }
        /* The original parent descriptor updates every 0x1e frame ticks.
         * Its first update sees 30 ticks because creation primes last_tick. */
        if (frame_tick < parent->next_update_tick) continue;
        ticks = frame_tick - parent->last_update_tick;
        parent->last_update_tick = frame_tick;
        parent->next_update_tick = frame_tick + 30u;
        for (child = 0u; child < 4u; ++child) {
            unsigned int phase, timer, duration, child_world;
            if (!(parent->child_mask & (1u << child))) continue;
            phase = parent->phase[child];
            timer = parent->timer[child] + ticks;
            /* The end-side pair uses the preceding cell at fraction 0x50;
             * the start-side pair uses the first cell at fraction 0xb0. */
            child_world = (child == 0u || child == 3u) ?
                ((unsigned int)(zone->end_sample ?
                    zone->end_sample - 1u : 0u) * 256u + 0x50u) :
                ((unsigned int)zone->start_sample * 256u + 0xb0u);
            if (child_world < window_left || child_world > window_right) {
                parent->timer[child] = timer;
                continue;
            }
            duration = zone->short_timing ?
                (phase == 0u ? 75u : phase == 1u ? 0u : 240u) :
                (phase == 0u ? 600u : phase == 1u ? 180u : 780u);
            while (timer > duration) {
                timer -= duration;
                phase = (phase + 1u) % 3u;
                if ((child & 1u) && phase == 0u &&
                    (frame_tick & 8u))
                    road_emit_crossing_car(game, zone, child);
                duration = zone->short_timing ?
                    (phase == 0u ? 75u : phase == 1u ? 0u : 240u) :
                    (phase == 0u ? 600u : phase == 1u ? 180u : 780u);
            }
            parent->phase[child] = (unsigned char)phase;
            parent->timer[child] = timer;
        }
    }
}

static void road_step_effects(road_game_t *game, unsigned int old_distance,
                              int old_lane, int old_height_8_8,
                              unsigned int dt_ms, int player_recovering)
{
    unsigned int i;
    /* RHZD's high-bit entries create riders in the original engine. Keep
     * its 15-object pool and disc position/mode, with 9588-scale motion. */
    while (game->effect_cursor < g_effect_count) {
        const road_effect_placement_t *placement =
            &g_effect_placements[game->effect_cursor];
        unsigned int world = (unsigned int)placement->sample * 256u;
        road_effect_actor_t *actor = 0;
        if (world > game->distance + 12000u) break;
        game->effect_cursor++;
        if (world + 500u < game->distance) continue;
        for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i)
            if (!game->effects[i].active) {
                actor = &game->effects[i];
                break;
            }
        if (!actor) continue;
        actor->distance = world;
        actor->lane = clamp_int(placement->lateral / 8, -150, 150);
        actor->speed = placement->travel_mode >= 11u ? 65u : 15u;
        actor->age_ms = 0u;
        actor->state_ms = 0u;
        actor->contact_delay_ms = 0u;
        actor->travel_mode = placement->travel_mode;
        actor->sprite_index = placement->sprite_index;
        actor->behavior = placement->travel_mode == 10u ?
            ROAD_EFFECT_FLAG :
            placement->travel_mode == 4u ? ROAD_EFFECT_RIDING :
            ROAD_EFFECT_WAITING;
        actor->animation = actor->behavior == ROAD_EFFECT_FLAG ?
            ROAD_EFFECT_ANIM_FLAG :
            actor->behavior == ROAD_EFFECT_RIDING ?
            ROAD_EFFECT_ANIM_MOTION : ROAD_EFFECT_ANIM_STANDING;
        actor->active = 1u;
    }
    for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i) {
        road_effect_actor_t *actor = &game->effects[i];
        unsigned int before;
        int relative_after;
        int actor_lane_before, side_contact = 0, vertical_contact = 0;
        if (!actor->active) continue;
        before = actor->distance;
        actor_lane_before = actor->lane;
        actor->age_ms += dt_ms;
        actor->state_ms += dt_ms;
        if (actor->behavior == ROAD_EFFECT_CONTACT_LOCKED) {
            actor->speed = 0u;
            if (actor->contact_delay_ms > dt_ms)
                actor->contact_delay_ms -= dt_ms;
            else {
                actor->contact_delay_ms = 0u;
                actor->behavior = ROAD_EFFECT_RECOVERING;
                actor->animation = ROAD_EFFECT_ANIM_SHAKE;
                actor->state_ms = 0u;
            }
        } else if (actor->behavior == ROAD_EFFECT_RECOVERING) {
            if (actor->state_ms >= 300u) {
                actor->behavior = ROAD_EFFECT_SHAKE_RECOVERY;
                actor->animation = ROAD_EFFECT_ANIM_STANDING;
                actor->state_ms = 0u;
            }
        } else if (actor->behavior == ROAD_EFFECT_SHAKE_RECOVERY) {
            if (actor->state_ms >= 250u) {
                actor->behavior = ROAD_EFFECT_WAITING;
                actor->animation = ROAD_EFFECT_ANIM_STANDING;
                actor->state_ms = 0u;
            }
        } else if (actor->behavior == ROAD_EFFECT_WAITING &&
                   actor->travel_mode >= 5u &&
                   actor->travel_mode <= 7u &&
                   actor->state_ms >= 250u) {
            actor->behavior = ROAD_EFFECT_RIDING;
            actor->animation = ROAD_EFFECT_ANIM_MOTION;
            actor->state_ms = 0u;
        }
        if (actor->behavior == ROAD_EFFECT_RIDING) {
            actor->distance += actor->speed * dt_ms / 60u;
            if (actor->travel_mode == 5u)
                actor->lane -= (int)(dt_ms / 45u + 1u);
            else if (actor->travel_mode == 6u)
                actor->lane += actor->lane < 0 ? 1 :
                               actor->lane > 0 ? -1 : 0;
            else if (actor->travel_mode == 7u)
                actor->lane += (int)(dt_ms / 45u + 1u);
            if ((actor->travel_mode == 5u && actor->lane <= -110) ||
                (actor->travel_mode == 6u && actor->lane == 0) ||
                (actor->travel_mode == 7u && actor->lane >= 110)) {
                actor->behavior = ROAD_EFFECT_WAITING;
                actor->animation = ROAD_EFFECT_ANIM_STANDING;
                actor->state_ms = 0u;
            }
        }
        relative_after = (int)actor->distance - (int)game->distance;
        if (!player_recovering &&
            road_swept_contact_3d(game, old_distance, old_lane,
            old_height_8_8, before, actor->distance,
            actor_lane_before, actor->lane,
            ROAD_PLAYER_HALF_LENGTH + ROAD_EFFECT_HALF_LENGTH,
            ROAD_PLAYER_HALF_LANE + ROAD_EFFECT_HALF_LANE,
            ROAD_EFFECT_HALF_HEIGHT_8_8,
            ROAD_EFFECT_HALF_HEIGHT_8_8,
            &side_contact, &vertical_contact) &&
            !game->player_stun_ms &&
            actor->behavior != ROAD_EFFECT_CONTACT_LOCKED &&
            actor->behavior != ROAD_EFFECT_RECOVERING &&
            actor->behavior != ROAD_EFFECT_SHAKE_RECOVERY) {
            unsigned int impact_speed;
            if (vertical_contact) {
                int fall = game->vertical_velocity_8_8;
                if (fall < 0) fall = -fall;
                impact_speed = (unsigned int)fall / 64u;
                road_apply_vertical_contact(game, actor->distance,
                    actor->lane,
                    ROAD_EFFECT_HALF_HEIGHT_8_8 * 2,
                    ROAD_EFFECT_COLLISION_MASS, vertical_contact);
            } else if (side_contact) {
                int relative_lane_move = (actor->lane - actor_lane_before) -
                    (game->lane - old_lane);
                if (relative_lane_move < 0)
                    relative_lane_move = -relative_lane_move;
                impact_speed = dt_ms ?
                    (unsigned int)(relative_lane_move * 60 / (int)dt_ms) :
                    0u;
                road_apply_lateral_contact(game, old_lane, actor->lane,
                    actor_lane_before, ROAD_EFFECT_COLLISION_MASS,
                    ROAD_PLAYER_HALF_LANE + ROAD_EFFECT_HALF_LANE,
                    dt_ms);
            } else {
                impact_speed = game->speed;
                game->speed = road_elastic_contact_speed(game->speed,
                    actor->speed, ROAD_EFFECT_COLLISION_MASS);
            }
            game->player_stun_ms = side_contact || vertical_contact ?
                240u : 420u;
            game->effect_hits++;
            road_queue_impact_audio(game, impact_speed);
            road_game_apply_collision_damage(game,
                2u + impact_speed / 30u, impact_speed * 8u,
                side_contact || vertical_contact ? ROAD_DAMAGE_GLANCING :
                               ROAD_DAMAGE_STANDARD);
            actor->behavior = ROAD_EFFECT_CONTACT_LOCKED;
            actor->animation = ROAD_EFFECT_ANIM_FALL;
            actor->state_ms = 0u;
            actor->contact_delay_ms = 25u * 1000u / 60u;
        } else if (relative_after < -500 || actor->age_ms > 16000u ||
                   actor->lane < -170 || actor->lane > 170) {
            actor->active = 0u;
        }
    }
}

static unsigned int road_ai_curve_speed(const road_rider_t *opponent)
{
    unsigned int sample, i, curve = 0u, reduction;
    if (!g_course_curvature || !g_course_count)
        return opponent->base_speed;
    sample = (opponent->distance + 0x800u) / 256u;
    for (i = 0u; i < 4u; ++i) {
        int value;
        unsigned int at = sample + i;
        if (at >= g_course_count) at = g_course_count - 1u;
        value = g_course_curvature[at];
        curve += (unsigned int)(value < 0 ? -value : value);
    }
    curve /= 4u;
    if (curve <= 12u) return opponent->base_speed;
    reduction = (curve - 12u) * opponent->base_speed / 128u;
    if (reduction > opponent->base_speed / 2u)
        reduction = opponent->base_speed / 2u;
    return opponent->base_speed - reduction;
}

static int road_ai_course_lane(const road_rider_t *opponent,
                                unsigned int index)
{
    unsigned int sample, i, width, lane_span;
    int bend = 0, side;
    if (!g_course_curvature || !g_course_count ||
        !g_width_count || !g_course_right_width)
        return (int)index * 52 - 52;
    sample = (opponent->distance + 0x3c0u) / 256u;
    if (sample >= g_course_count) sample = g_course_count - 1u;
    width = g_course_right_width[sample < g_width_count ?
                                  sample : g_width_count - 1u];
    lane_span = width * 70u / 512u;
    if (lane_span < 40u) lane_span = 40u;
    if (lane_span > 100u) lane_span = 100u;
    for (i = 0u; i < 8u; ++i) {
        unsigned int at = sample + i;
        if (at >= g_course_count) at = g_course_count - 1u;
        bend += g_course_curvature[at];
    }
    bend /= 8;
    side = index == 1u ? 1 : index == 2u ? -1 : 0;
    return clamp_int(side * (int)(index == 1u ?
        lane_span / 2u : lane_span) + bend / 3, -110, 110);
}

typedef struct road_ai_obstacle {
    unsigned int distance;
    unsigned int intercept_ms;
    unsigned int half_lane;
    int lane;
} road_ai_obstacle_t;

static void road_ai_consider_obstacle(const road_rider_t *opponent,
                                      road_ai_obstacle_t *target,
                                      unsigned int distance, int lane,
                                      int speed, unsigned int half_lane)
{
    unsigned int separation, intercept_ms;
    int closing_speed, lateral;
    if (!distance || distance <= opponent->distance) return;
    separation = distance - opponent->distance;
    if (separation > 2400u) return;
    lateral = lane - opponent->lane;
    if (lateral < -(int)(half_lane + ROAD_OPPONENT_HALF_LANE + 20u) ||
        lateral > (int)(half_lane + ROAD_OPPONENT_HALF_LANE + 20u))
        return;
    closing_speed = (int)opponent->speed - speed;
    if (closing_speed <= 0) return;
    intercept_ms = separation * 60u / (unsigned int)closing_speed;
    if (intercept_ms > 2500u ||
        intercept_ms >= target->intercept_ms) return;
    target->distance = distance;
    target->intercept_ms = intercept_ms;
    target->half_lane = half_lane;
    target->lane = lane;
}

static int road_ai_collision_avoidance(const road_game_t *game,
                                        const road_rider_t *opponent,
                                        int *target_lane,
                                        unsigned int *speed_target)
{
    unsigned int i;
    road_ai_obstacle_t target = {0u, 2501u, 0u, 0};
    int left_bound, right_bound, candidate;
    if (g_cars || g_car_animations) {
        for (i = 0u; i < ROAD_TRAFFIC_COUNT; ++i) {
            const road_rider_t *car = &game->traffic[i];
            road_ai_consider_obstacle(opponent, &target, car->distance,
                car->lane, car->travel_direction < 0 ?
                -(int)car->speed : (int)car->speed,
                road_car_half_lane(car));
        }
        for (i = 0u; i < ROAD_CROSSING_COUNT; ++i) {
            const road_rider_t *car = &game->crossing[i];
            if (car->distance)
                road_ai_consider_obstacle(opponent, &target, car->distance,
                    car->lane, 0, road_car_half_lane(car));
        }
    }
    if (g_static_placements && g_static_sprites) {
        unsigned int low = 0u, high = g_static_placement_count;
        while (low < high) {
            unsigned int middle = low + (high - low) / 2u;
            if ((unsigned int)g_static_placements[middle].sample * 256u <=
                opponent->distance) low = middle + 1u;
            else high = middle;
        }
        for (i = low; i < g_static_placement_count; ++i) {
            const road_static_placement_t *hazard = &g_static_placements[i];
            unsigned int world = (unsigned int)hazard->sample * 256u;
            int obstacle_lane = hazard_lateral(hazard, game->elapsed_ms);
            int obstacle_half_lane = ROAD_STATIC_HALF_LANE;
            if (world - opponent->distance > 2400u) break;
            if (hazard->variant >= g_static_sprite_count ||
                !g_static_sprites[hazard->variant].pixels ||
                (g_static_sprites[hazard->variant].width == 1u &&
                 g_static_sprites[hazard->variant].height == 1u &&
                 g_static_sprites[hazard->variant].pixels[0] == 0xf81fu))
                continue;
            if (g_static_collisions &&
                hazard->variant < g_static_collision_count &&
                g_static_collisions[hazard->variant].count) {
                const road_static_collision_t *collision =
                    &g_static_collisions[hazard->variant];
                int left = 256, right = -256;
                unsigned int box_index;
                for (box_index = 0u; box_index < collision->count &&
                     box_index < 2u; ++box_index) {
                    const road_static_collision_box_t *box =
                        &collision->box[box_index];
                    int box_left = hazard->mirrored ?
                        -box->right : box->left;
                    int box_right = hazard->mirrored ?
                        -box->left : box->right;
                    if (box_left < left) left = box_left;
                    if (box_right > right) right = box_right;
                }
                if (left < right) {
                    obstacle_lane += left + right;
                    obstacle_half_lane = right - left;
                }
            }
            road_ai_consider_obstacle(opponent, &target, world,
                obstacle_lane, 0, obstacle_half_lane);
        }
    }
    for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i) {
        const road_effect_actor_t *effect = &game->effects[i];
        if (!effect->active) continue;
        road_ai_consider_obstacle(opponent, &target, effect->distance,
            effect->lane, effect->behavior == ROAD_EFFECT_RIDING ?
            (int)effect->speed : 0, ROAD_EFFECT_HALF_LANE);
    }
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i) {
        const road_rider_t *peer = &game->opponents[i];
        if (peer == opponent || !peer->route_visible) continue;
        road_ai_consider_obstacle(opponent, &target, peer->distance,
            peer->lane, peer->stun_ms ? 18 : (int)peer->speed,
            ROAD_OPPONENT_HALF_LANE);
    }
    if (game->distance > opponent->distance + 400u)
        road_ai_consider_obstacle(opponent, &target, game->distance,
            game->lane, (int)game->speed, ROAD_PLAYER_HALF_LANE);
    if (!target.distance) return 0;
    road_lane_bounds(target.distance, &left_bound, &right_bound);
    candidate = target.lane +
        (opponent->lane <= target.lane ? -1 : 1) *
        (int)(ROAD_OPPONENT_HALF_LANE + target.half_lane + 32u);
    if (candidate < left_bound + 8 || candidate > right_bound - 8)
        candidate = target.lane +
            (opponent->lane <= target.lane ? 1 : -1) *
            (int)(ROAD_OPPONENT_HALF_LANE + target.half_lane + 32u);
    *target_lane = candidate < left_bound + 8 ||
        candidate > right_bound - 8 ? opponent->lane : candidate;
    *speed_target = target.intercept_ms < 617u ?
        opponent->base_speed / 2u :
        target.intercept_ms < 1250u ?
        opponent->base_speed * 3u / 4u :
        opponent->base_speed;
    return 1;
}

void road_game_step(road_game_t *game, unsigned int input, unsigned int dt_ms)
{
    unsigned int i;
    unsigned int old_input = game->input;
    unsigned int old_distance = game->distance;
    int old_lane = game->lane;
    int old_height_8_8 = game->vertical_height_8_8;
    int old_road_height = road_surface_height_8_8(game) +
        road_cross_section_height_8_8(game->distance, old_lane);
    int steer = 0;
    int recovering = game->recovery_ms != 0u;
    if (dt_ms > 100u) dt_ms = 100u;
    game->elapsed_ms += dt_ms;
    game->sound_event_mask = 0u;
    if (game->finished) {
        game->input = input;
        return;
    }
    if (recovering) {
        if (game->crash_elapsed_ms)
            game->crash_elapsed_ms += dt_ms;
        game->recovery_ms = game->recovery_ms > dt_ms ?
            game->recovery_ms - dt_ms : 0u;
        game->speed = 0u;
        game->velocity_raw = 0u;
        game->lateral_impulse = 0;
        game->throttle_control = 0;
        if (!game->recovery_ms) {
            game->crash_elapsed_ms = 0u;
            if (!game->rider_health)
                game->rider_health = game->rider_recovery_ceiling;
            if (!game->bike_health)
                game->bike_health = game->bike_max_health / 2u;
            game->player_stun_ms = 0u;
        }
        goto update_world;
    }
    if (input & ROAD_LEFT) steer--;
    if (input & ROAD_RIGHT) steer++;
    if (game->bike_physics_ready) {
        road_step_original_forward(game, input, dt_ms, steer);
    } else if (input & ROAD_ACCEL) {
        game->speed += (1u + dt_ms / 12u) * game->accel_percent / 100u;
        if (game->speed > game->top_speed) game->speed = game->top_speed;
    } else if (game->speed > 0u) {
        unsigned int drag = 1u + dt_ms / 25u;
        game->speed = game->speed > drag ? game->speed - drag : 0u;
    }
    if (!game->bike_physics_ready && (input & ROAD_BRAKE)) {
        unsigned int brake = 1u + dt_ms / 4u;
        game->speed = game->speed > brake ? game->speed - brake : 0u;
    }
    if (game->player_stun_ms > dt_ms) game->player_stun_ms -= dt_ms;
    else game->player_stun_ms = 0u;
    if (game->player_stun_ms && game->speed > game->top_speed / 2u)
        game->speed = game->top_speed / 2u;
    if (game->bike_physics_ready)
        road_step_original_steering(game, steer, dt_ms);
    else {
        game->lean = steer;
        game->lane = clamp_int(game->lane + steer *
            (int)((game->speed + 35u) * dt_ms * game->steer_percent /
                  65000u), -128, 128);
    }
    if (game->lateral_impulse && dt_ms) {
        game->lane = clamp_int(game->lane +
            game->lateral_impulse * (int)dt_ms / 60, -128, 128);
        game->lateral_impulse = game->lateral_impulse * 3 / 4;
    }
    {
        int left_bound, right_bound;
        road_lane_bounds(game->distance, &left_bound, &right_bound);
        /* SPEC-driven forward physics already applies shoulder drag in
         * raw velocity units. An extra whole-speed penalty here erased
         * every gain when remounting outside the road edge. */
        if (!game->bike_physics_ready &&
            (game->lane < left_bound || game->lane > right_bound) &&
            game->speed > 2u) game->speed -= 2u;
    }
    road_game_seek(game, game->distance + game->speed * dt_ms / 60u);
    if (game->bike_physics_ready)
        road_step_vertical(game,
            road_surface_height_8_8(game) +
            road_cross_section_height_8_8(game->distance, game->lane) -
            old_road_height, dt_ms);
    if (g_static_placements && g_static_sprites &&
        game->distance > old_distance && !game->player_stun_ms) {
        unsigned int low = 0u, high = g_static_placement_count;
        unsigned int first_world = old_distance +
            ROAD_PLAYER_CONTACT_DEPTH >
            ROAD_PLAYER_HALF_LENGTH + ROAD_STATIC_HALF_LENGTH ?
            old_distance + ROAD_PLAYER_CONTACT_DEPTH -
            ROAD_PLAYER_HALF_LENGTH -
            ROAD_STATIC_HALF_LENGTH : 0u;
        while (low < high) {
            unsigned int middle = low + (high - low) / 2u;
            if ((unsigned int)g_static_placements[middle].sample * 256u <
                first_world) low = middle + 1u;
            else high = middle;
        }
        for (i = low; i < g_static_placement_count; ++i) {
            const road_static_placement_t *hazard = &g_static_placements[i];
            unsigned int world = (unsigned int)hazard->sample * 256u;
            int side_contact = 0, vertical_contact = 0;
            int static_half_height = road_static_half_height(hazard);
            int old_hazard_lane = hazard_lateral(hazard,
                game->elapsed_ms - dt_ms);
            int hazard_lane = hazard_lateral(hazard, game->elapsed_ms);
            int contact_old_lane = old_hazard_lane;
            int contact_lane = hazard_lane;
            int contact_half_lane = ROAD_STATIC_HALF_LANE;
            int contact_top_height = static_half_height * 2;
            int hit = 0;
            if (world > game->distance + ROAD_PLAYER_CONTACT_DEPTH +
                ROAD_PLAYER_HALF_LENGTH +
                ROAD_STATIC_HALF_LENGTH) break;
            if (hazard->variant >= g_static_sprite_count ||
                i == game->last_static_contact_index ||
                !g_static_sprites[hazard->variant].pixels ||
                (g_static_sprites[hazard->variant].width == 1u &&
                 g_static_sprites[hazard->variant].height == 1u &&
                 g_static_sprites[hazard->variant].pixels[0] == 0xf81fu))
                continue;
            if (g_static_collisions &&
                hazard->variant < g_static_collision_count &&
                g_static_collisions[hazard->variant].count) {
                const road_static_collision_t *collision =
                    &g_static_collisions[hazard->variant];
                unsigned int box_index;
                for (box_index = 0u; box_index < collision->count &&
                     box_index < 2u; ++box_index) {
                    const road_static_collision_box_t *box =
                        &collision->box[box_index];
                    int left = hazard->mirrored ? -box->right : box->left;
                    int right = hazard->mirrored ? -box->left : box->right;
                    int bottom = -(int)box->bottom * 512;
                    int top = -(int)box->top * 512;
                    int center_height, half_height;
                    if (hazard->visibility_group == 1u)
                        top = bottom + 0x2000;
                    else if (hazard->visibility_group == 2u)
                        top = bottom + 0x3fff;
                    center_height = (top + bottom) / 2;
                    half_height = (top - bottom) / 2;
                    if (half_height <= 0 || left >= right) continue;
                    contact_old_lane = old_hazard_lane + left + right;
                    contact_lane = hazard_lane + left + right;
                    contact_half_lane = right - left;
                    if (road_swept_contact_3d(game, old_distance, old_lane,
                        old_height_8_8, world, world,
                        contact_old_lane, contact_lane,
                        ROAD_PLAYER_HALF_LENGTH + ROAD_STATIC_HALF_LENGTH,
                        ROAD_PLAYER_HALF_LANE + contact_half_lane,
                        center_height, half_height,
                        &side_contact, &vertical_contact)) {
                        contact_top_height = top;
                        hit = 1;
                        break;
                    }
                }
            } else {
                hit = road_swept_contact_3d(game, old_distance, old_lane,
                    old_height_8_8, world, world,
                    old_hazard_lane, hazard_lane,
                    ROAD_PLAYER_HALF_LENGTH + ROAD_STATIC_HALF_LENGTH,
                    ROAD_PLAYER_HALF_LANE + ROAD_STATIC_HALF_LANE,
                    static_half_height, static_half_height,
                    &side_contact, &vertical_contact);
            }
            if (!hit) continue;
            {
                unsigned int impact_speed;
                if (vertical_contact) {
                    int fall = game->vertical_velocity_8_8;
                    if (fall < 0) fall = -fall;
                    impact_speed = (unsigned int)fall / 64u;
                    road_apply_vertical_contact(game, world, contact_lane,
                        contact_top_height,
                        ROAD_CAR_COLLISION_MASS, vertical_contact);
                } else if (side_contact) {
                    int lane_move = (contact_lane - contact_old_lane) -
                        (game->lane - old_lane);
                    if (lane_move < 0) lane_move = -lane_move;
                    impact_speed = dt_ms ?
                        (unsigned int)(lane_move * 60 / (int)dt_ms) : 0u;
                    road_apply_lateral_contact(game, old_lane,
                        contact_lane, contact_old_lane,
                        ROAD_CAR_COLLISION_MASS,
                        ROAD_PLAYER_HALF_LANE + contact_half_lane,
                        dt_ms);
                } else {
                    impact_speed = game->speed;
                    game->speed = game->speed * 2u / 5u;
                }
                road_queue_impact_audio(game, impact_speed);
                road_game_apply_collision_damage(game,
                    2u + impact_speed / 15u, impact_speed * 18u,
                    side_contact || vertical_contact ?
                        ROAD_DAMAGE_GLANCING :
                                   ROAD_DAMAGE_STANDARD);
            }
            game->player_stun_ms = side_contact || vertical_contact ?
                240u : 420u;
            game->hazard_hits++;
            game->last_static_contact_index = i;
            break;
        }
    }
    if (g_finish_sample && game->distance >= g_finish_sample * 256u) {
        game->distance = g_finish_sample * 256u;
        game->speed = 0u;
        game->finished = 1u;
        game->input = input;
        return;
    }
update_world:
    if (game->attack_ms > dt_ms) game->attack_ms -= dt_ms;
    else game->attack_ms = 0u;
    if (!recovering && (input & ROAD_ATTACK) && !(old_input & ROAD_ATTACK) &&
        game->attack_ms == 0u) {
        road_rider_t *target = 0;
        int target_lane_delta = 0;
        int best_metric = 0x7fffffff;
        unsigned int kick = (input & ROAD_KICK) != 0u;
        game->attack_ms = ROAD_ATTACK_DURATION_MS;
        game->attack_style = kick;
        game->attack_side = game->lean < 0 ? 0u : 1u;
        for (i = 0; i < ROAD_OPPONENT_COUNT; ++i) {
            road_rider_t *opponent = &game->opponents[i];
            int relative = (int)opponent->distance -
                (int)game->distance - ROAD_PLAYER_CONTACT_DEPTH;
            int lane_delta = opponent->lane - game->lane;
            int lane_abs = lane_delta < 0 ? -lane_delta : lane_delta;
            int metric = lane_abs * 8 +
                (relative < 0 ? -relative : relative);
            if (opponent->route_visible &&
                relative > (kick ? -200 : -150) &&
                relative < (kick ? 420 : 400) &&
                lane_abs < (kick ? 80 : 48) &&
                metric < best_metric) {
                target = opponent;
                target_lane_delta = lane_delta;
                best_metric = metric;
            }
        }
        if (target) {
            game->attack_side = target_lane_delta < 0 ? 0u : 1u;
            target->stun_ms = kick ? 1000u : 750u;
            target->hit_reaction_ms = 600u;
            if (kick)
                target->lane = clamp_int(target->lane +
                    (target_lane_delta < 0 ? -12 : 12), -110, 110);
            game->hits++;
        }
    }
    for (i = 0; i < ROAD_OPPONENT_COUNT; ++i) {
        road_rider_t *opponent = &game->opponents[i];
        unsigned int opponent_before = opponent->distance;
        int opponent_lane_before = opponent->lane;
        int side_contact = 0, vertical_contact = 0;
        int nearby = opponent->route_visible &&
                     opponent->distance + 30000u > game->distance &&
                     opponent->distance < game->distance + 30000u;
        unsigned int curve_speed = nearby ?
            road_ai_curve_speed(opponent) :
            opponent->route_visible ? opponent->base_speed :
            opponent->graph_curve_speed;
        unsigned int avoid_speed = curve_speed;
        int avoid_lane = opponent->lane;
        int avoiding = nearby && road_ai_collision_avoidance(game,
            opponent, &avoid_lane, &avoid_speed);
        if (avoiding && avoid_speed < curve_speed)
            curve_speed = avoid_speed;
        if (opponent->speed > curve_speed) opponent->speed--;
        else if (opponent->speed < curve_speed) opponent->speed++;
        if (opponent->stun_ms > dt_ms) opponent->stun_ms -= dt_ms;
        else opponent->stun_ms = 0u;
        if (opponent->hit_reaction_ms > dt_ms)
            opponent->hit_reaction_ms -= dt_ms;
        else opponent->hit_reaction_ms = 0u;
        opponent->distance += (opponent->stun_ms ? 18u : opponent->speed)
            * dt_ms / 60u;
        if (opponent->attack_cooldown_ms > dt_ms)
            opponent->attack_cooldown_ms -= dt_ms;
        else opponent->attack_cooldown_ms = 0u;
        if (!opponent->route_visible) continue;
        if (!opponent->stun_ms) {
            int relative = (int)opponent->distance -
                (int)game->distance - ROAD_PLAYER_CONTACT_DEPTH;
            int lane_delta = opponent->lane - game->lane;
            unsigned int behavior;
            if (relative > -100 && relative < 220 &&
                     lane_delta > -34 && lane_delta < 34 &&
                     !opponent->attack_cooldown_ms &&
                     !game->player_stun_ms && !recovering &&
                     game->vertical_height_8_8 <
                         2 * ROAD_OPPONENT_HALF_HEIGHT_8_8)
                behavior = ROAD_AI_ATTACK;
            else if (avoiding)
                behavior = ROAD_AI_AVOID_TRAFFIC;
            else if (relative < -400 || relative > 3000)
                behavior = ROAD_AI_CRUISE;
            else if (relative > 220 && lane_delta > -70 &&
                     lane_delta < 70)
                behavior = ROAD_AI_APPROACH;
            else
                behavior = lane_delta < 0 ? ROAD_AI_PASS_RIGHT :
                           ROAD_AI_PASS_LEFT;
            if (behavior != opponent->behavior) {
                opponent->behavior = behavior;
                opponent->behavior_ms = 0u;
            } else opponent->behavior_ms += dt_ms;
            if (behavior == ROAD_AI_CRUISE)
                opponent->target_lane = road_ai_course_lane(opponent, i);
            else if (behavior == ROAD_AI_APPROACH)
                opponent->target_lane = game->lane +
                    ((i & 1u) ? 31 : -31);
            else if (behavior == ROAD_AI_PASS_RIGHT)
                opponent->target_lane = game->lane + 58;
            else if (behavior == ROAD_AI_PASS_LEFT)
                opponent->target_lane = game->lane - 58;
            else if (behavior == ROAD_AI_AVOID_TRAFFIC)
                opponent->target_lane = avoid_lane;
            else opponent->target_lane = game->lane +
                (lane_delta < 0 ? -16 : 16);
            opponent->target_lane = clamp_int(opponent->target_lane,
                                               -100, 100);
            if (opponent->lane < opponent->target_lane)
                opponent->lane += 1 + (int)(dt_ms / 24u);
            else if (opponent->lane > opponent->target_lane)
                opponent->lane -= 1 + (int)(dt_ms / 24u);
            opponent->lane = clamp_int(opponent->lane, -110, 110);
            if (behavior == ROAD_AI_ATTACK && !recovering) {
                game->speed = game->speed * 3u / 4u;
                game->player_stun_ms = 240u;
                game->rider_hits++;
                road_game_apply_collision_damage(game,
                    3u + opponent->speed / 20u, 0u,
                    ROAD_DAMAGE_STANDARD);
                opponent->attack_cooldown_ms = 1200u;
            }
        }
        if (!recovering && !opponent->stun_ms &&
            !game->player_stun_ms &&
            road_swept_contact_3d(game, old_distance, old_lane,
                old_height_8_8, opponent_before, opponent->distance,
                opponent_lane_before, opponent->lane,
                ROAD_PLAYER_HALF_LENGTH + ROAD_OPPONENT_HALF_LENGTH,
                ROAD_PLAYER_HALF_LANE + ROAD_OPPONENT_HALF_LANE,
                ROAD_OPPONENT_HALF_HEIGHT_8_8,
                ROAD_OPPONENT_HALF_HEIGHT_8_8,
                &side_contact, &vertical_contact)) {
            unsigned int relative_speed = game->speed > opponent->speed ?
                game->speed - opponent->speed :
                opponent->speed - game->speed;
            if (vertical_contact) {
                int fall = game->vertical_velocity_8_8;
                if (fall < 0) fall = -fall;
                if ((unsigned int)fall / 64u > relative_speed)
                    relative_speed = (unsigned int)fall / 64u;
                road_apply_vertical_contact(game, opponent->distance,
                    opponent->lane,
                    ROAD_OPPONENT_HALF_HEIGHT_8_8 * 2,
                    ROAD_OPPONENT_COLLISION_MASS, vertical_contact);
            } else if (side_contact) {
                int lane_move = (opponent->lane - opponent_lane_before) -
                    (game->lane - old_lane);
                unsigned int lateral_speed;
                if (lane_move < 0) lane_move = -lane_move;
                lateral_speed = dt_ms ?
                    (unsigned int)(lane_move * 60 / (int)dt_ms) : 0u;
                if (lateral_speed > relative_speed)
                    relative_speed = lateral_speed;
                road_apply_lateral_contact(game, old_lane,
                    opponent->lane, opponent_lane_before,
                    ROAD_OPPONENT_COLLISION_MASS,
                    ROAD_PLAYER_HALF_LANE + ROAD_OPPONENT_HALF_LANE,
                    dt_ms);
            } else
                game->speed = road_elastic_contact_speed(game->speed,
                    (int)opponent->speed, ROAD_OPPONENT_COLLISION_MASS);
            game->player_stun_ms = 180u;
            road_queue_impact_audio(game, relative_speed);
            road_game_apply_collision_damage(game,
                1u + relative_speed / 35u, relative_speed * 7u,
                ROAD_DAMAGE_GLANCING);
        }
    }
    if (g_car_count || g_car_animation_count) {
        int traveled = (int)(game->distance - old_distance);
        road_step_crossing_zones(game);
        game->traffic_spawn_timer -= traveled;
        game->traffic_reverse_timer -= traveled;
        if (game->traffic_spawn_timer <= 0) {
            road_spawn_traffic(game, 1);
            game->traffic_spawn_timer += 3500;
            if (game->traffic_spawn_timer < 0)
                game->traffic_spawn_timer = 0;
        }
        if (game->traffic_reverse_timer <= 0) {
            road_spawn_traffic(game, -1);
            game->traffic_reverse_timer += 2500;
            if (game->traffic_reverse_timer < 0)
                game->traffic_reverse_timer = 0;
        }
    for (i = 0u; i < ROAD_TRAFFIC_COUNT + ROAD_CROSSING_COUNT; ++i) {
        road_rider_t *car = i < ROAD_TRAFFIC_COUNT ?
            &game->traffic[i] : &game->crossing[i - ROAD_TRAFFIC_COUNT];
        unsigned int car_before = car->distance;
        unsigned int move = car->speed * dt_ms / 60u;
        int car_lane_before = car->lane;
        int side_contact = 0, vertical_contact = 0;
        if (!car->distance) continue;
        if (car->collision_mode & 1u) {
            unsigned int lateral = car->speed * dt_ms +
                car->lane_fraction;
            int lane_move = (int)(lateral / 1000u);
            car->lane_fraction = lateral % 1000u;
            if (car->collision_mode == 3u) lane_move = -lane_move;
            car->lane += lane_move;
            if ((car->collision_mode == 1u &&
                 car->lane >= car->target_lane) ||
                (car->collision_mode == 3u &&
                 car->lane <= car->target_lane)) {
                car->distance = 0u;
                continue;
            }
        } else if (car->travel_direction < 0)
            car->distance = car->distance > move ?
                            car->distance - move : 0u;
        else car->distance += move;
        if (car->distance && car->distance + 500u < game->distance) {
            car->distance = 0u;
            continue;
        }
        if (!recovering && !game->player_stun_ms &&
            road_swept_contact_3d(game, old_distance, old_lane,
                old_height_8_8, car_before, car->distance,
                car_lane_before, car->lane,
                ROAD_PLAYER_HALF_LENGTH + ROAD_CAR_HALF_LENGTH,
                ROAD_PLAYER_HALF_LANE + road_car_half_lane(car),
                ROAD_CAR_HALF_HEIGHT_8_8,
                ROAD_CAR_HALF_HEIGHT_8_8,
                &side_contact, &vertical_contact)) {
            int car_speed = (car->collision_mode & 1u) ? 0 :
                car->travel_direction < 0 ?
                -(int)car->speed : (int)car->speed;
            unsigned int impact_speed = car_speed < 0 ?
                game->speed + car->speed :
                game->speed > car->speed ? game->speed - car->speed :
                car->speed - game->speed;
            if (vertical_contact) {
                road_apply_vertical_contact(game, car->distance, car->lane,
                    ROAD_CAR_HALF_HEIGHT_8_8 * 2,
                    ROAD_CAR_COLLISION_MASS, vertical_contact);
                game->player_stun_ms = 180u;
                road_game_apply_collision_damage(game,
                    1u + impact_speed / 24u, impact_speed * 6u,
                    ROAD_DAMAGE_GLANCING);
            } else if (side_contact) {
                int lane_delta = (car->lane - car_lane_before) -
                    (game->lane - old_lane);
                unsigned int lateral_speed;
                if (lane_delta < 0) lane_delta = -lane_delta;
                lateral_speed = dt_ms ?
                    (unsigned int)(lane_delta * 60 / (int)dt_ms) : 0u;
                if (lateral_speed > impact_speed)
                    impact_speed = lateral_speed;
                road_apply_lateral_contact(game, old_lane, car->lane,
                    car_lane_before, ROAD_CAR_COLLISION_MASS,
                    ROAD_PLAYER_HALF_LANE + road_car_half_lane(car),
                    dt_ms);
                game->player_stun_ms = 180u;
                road_game_apply_collision_damage(game,
                    1u + lateral_speed / 4u, lateral_speed * 12u,
                    ROAD_DAMAGE_GLANCING);
            } else {
                game->speed = road_elastic_contact_speed(game->speed,
                    car_speed, ROAD_CAR_COLLISION_MASS);
                game->player_stun_ms = 300u;
                road_game_apply_collision_damage(game,
                    3u + impact_speed / 24u, impact_speed * 20u,
                    ROAD_DAMAGE_STANDARD);
            }
            road_queue_impact_audio(game, impact_speed);
        }
    }
    }
    if (g_effect_count) road_step_effects(game, old_distance, old_lane,
                                         old_height_8_8, dt_ms, recovering);
    if (game->bike_physics_ready &&
        game->speed != game->velocity_raw / 64u)
        game->velocity_raw = game->speed * 64u;
    road_step_engine_pitch(game);
    game->input = input;
}

static void draw_sprite_scaled(road_pixel_t *pixels,
                               const road_sprite_t *sprite,
                               int x, int y, int height)
{
    if (sprite && sprite->pixels && sprite->width && sprite->height) {
        int width = height * (int)sprite->width / (int)sprite->height;
        int left = x - width / 2;
        int top = y - height;
        int yy;
        if (width < 1) width = 1;
        if (height < 1) height = 1;
        if (left >= ROAD_WIDTH || left + width <= 0 ||
            top >= ROAD_HEIGHT || y <= 0) return;
        for (yy = clamp_int(top, 0, ROAD_HEIGHT);
             yy < clamp_int(y, 0, ROAD_HEIGHT); ++yy) {
            unsigned int source_y =
                (unsigned int)((yy - top) * (int)sprite->height / height);
            unsigned int source_x =
                (unsigned int)((clamp_int(left, 0, ROAD_WIDTH) - left) *
                               (int)sprite->width / width);
            unsigned int fraction =
                (unsigned int)((clamp_int(left, 0, ROAD_WIDTH) - left) *
                               (int)sprite->width % width);
            int xx;
            for (xx = clamp_int(left, 0, ROAD_WIDTH);
                 xx < clamp_int(left + width, 0, ROAD_WIDTH); ++xx) {
                road_pixel_t color = sprite->pixels[
                    source_y * sprite->width + source_x];
                if (color != 0xf81fu)
                    pixels[yy * ROAD_WIDTH + xx] = color;
                fraction += sprite->width;
                if (fraction >= (unsigned int)width) {
                    source_x += fraction / (unsigned int)width;
                    fraction %= (unsigned int)width;
                }
            }
        }
    }
}

static void draw_hud_needle(road_pixel_t *pixels, int cx, int cy,
                            unsigned int value, unsigned int max_value)
{
    static const signed char tip_x[9] =
        {-34, -30, -24, -13, 0, 13, 24, 30, 34};
    static const signed char tip_y[9] =
        {0, -13, -24, -30, -34, -30, -24, -13, 0};
    unsigned int scaled = value >= max_value ? 8u * 256u :
        value * (8u * 256u) / max_value;
    unsigned int slot = scaled >> 8u;
    unsigned int fraction = scaled & 255u;
    int tx, ty, dx, dy, steps, i;
    if (slot >= 8u) {
        tx = cx + tip_x[8];
        ty = cy + tip_y[8];
    } else {
        tx = cx + tip_x[slot] +
            ((int)(tip_x[slot + 1u] - tip_x[slot]) * (int)fraction >> 8);
        ty = cy + tip_y[slot] +
            ((int)(tip_y[slot + 1u] - tip_y[slot]) * (int)fraction >> 8);
    }
    dx = tx - cx;
    dy = ty - cy;
    steps = dx < 0 ? -dx : dx;
    if ((dy < 0 ? -dy : dy) > steps)
        steps = dy < 0 ? -dy : dy;
    if (!steps) return;
    for (i = 0; i <= steps; ++i) {
        int x = cx + dx * i / steps;
        int y = cy + dy * i / steps;
        if (x >= 0 && x < ROAD_WIDTH && y >= 0 && y < ROAD_HEIGHT)
            pixels[y * ROAD_WIDTH + x] = rgb(196, 38, 27);
    }
    box(pixels, cx - 2, cy - 2, 4, 4, rgb(35, 35, 35));
}

static int hud_sine_1024(int angle)
{
    static const unsigned short quarter[17] = {
        0, 100, 200, 297, 392, 483, 569, 650, 724,
        788, 851, 903, 946, 980, 1004, 1019, 1024
    };
    unsigned int a = (unsigned int)angle & 255u;
    unsigned int q = a & 63u;
    unsigned int slot, fraction;
    int result;
    if (a & 64u) q = 64u - q;
    slot = q >> 2u;
    fraction = q & 3u;
    result = (int)quarter[slot];
    if (slot < 16u)
        result += ((int)quarter[slot + 1u] - result) * (int)fraction / 4;
    return a & 128u ? -result : result;
}

static void draw_rotated_hud_sprite(road_pixel_t *pixels,
                                    const road_sprite_t *sprite,
                                    int anchor_x, int anchor_y,
                                    int pivot_x, int pivot_y, int angle)
{
    int sine = hud_sine_1024(angle);
    int cosine = hud_sine_1024(angle + 64);
    unsigned int y;
    if (!sprite) return;
    for (y = 0u; y < sprite->height; ++y) {
        unsigned int x;
        for (x = 0u; x < sprite->width; ++x) {
            road_pixel_t color = sprite->pixels[y * sprite->width + x];
            int dx, dy, xx, yy;
            if (color == 0xf81fu) continue;
            dx = (int)x - pivot_x;
            dy = (int)y - pivot_y;
            xx = anchor_x + (dx * cosine - dy * sine + 512) / 1024;
            yy = anchor_y + (dx * sine + dy * cosine + 512) / 1024;
            xx = xx * ROAD_WIDTH / 300;
            yy = 178 + (yy - 172) * 62 / 58;
            if (xx >= 0 && xx < ROAD_WIDTH && yy >= 0 && yy < ROAD_HEIGHT)
                pixels[yy * ROAD_WIDTH + xx] = color;
        }
    }
}

static void draw_hud_digit(road_pixel_t *pixels, int x, int y,
                           unsigned int digit, road_pixel_t color)
{
    static const unsigned char bits[10][7] = {
        {31,17,17,17,17,17,31}, {4,12,4,4,4,4,14},
        {31,1,1,31,16,16,31}, {31,1,1,15,1,1,31},
        {17,17,17,31,1,1,1}, {31,16,16,31,1,1,31},
        {31,16,16,31,17,17,31}, {31,1,2,4,4,4,4},
        {31,17,17,31,17,17,31}, {31,17,17,31,1,1,31}
    };
    unsigned int row;
    for (row = 0u; row < 7u; ++row) {
        unsigned int column;
        for (column = 0u; column < 5u; ++column)
            if (bits[digit][row] & (16u >> column))
                pixels[(y + (int)row) * ROAD_WIDTH +
                       x + (int)column] = color;
    }
}

static void draw_hud_three_digits(road_pixel_t *pixels, int x, int y,
                                  unsigned int value, int zero_pad,
                                  road_pixel_t color)
{
    unsigned int divisor = 100u;
    int started = zero_pad;
    if (value > 999u) value = 999u;
    while (divisor) {
        unsigned int digit = value / divisor % 10u;
        if (digit || started || divisor == 1u) {
            draw_hud_digit(pixels, x, y, digit, color);
            started = 1;
        }
        x += 7;
        divisor /= 10u;
    }
}

static void draw_original_hud(road_pixel_t *pixels, const road_game_t *game)
{
    unsigned int y;
    /* The 300x58 source panel fills the 320x62 bottom strip. */
    for (y = 0u; y < 62u; ++y) {
        unsigned int x;
        unsigned int sy = y * g_hud->height / 62u;
        for (x = 0u; x < ROAD_WIDTH; ++x) {
            unsigned int sx = x * g_hud->width / ROAD_WIDTH;
            road_pixel_t color = g_hud->pixels[sy * g_hud->width + sx];
            if (color != 0xf81fu)
                pixels[(y + 178u) * ROAD_WIDTH + x] = color;
        }
    }
    /* The 3DO HUD places the zero-padded course unit readout at (100,205)
     * and the right-aligned-in-field race position at (182,205).  The
     * adapted race starts at distance zero rather than the original
     * position origin of 50 units. */
    draw_hud_three_digits(pixels, 100 * ROAD_WIDTH / 300,
        178 + (205 - 172) * 62 / 58,
        game->distance / (33u * 256u), 1, rgb(16, 16, 16));
    draw_hud_three_digits(pixels, 182 * ROAD_WIDTH / 300,
        178 + (205 - 172) * 62 / 58,
        road_game_finish_place(game) + 1u, 0, rgb(16, 16, 16));
    if (g_hud_speed_needle) {
        int speed = (int)game->speed;
        int angle;
        if (speed < 50) speed = 50;
        if (speed > 180) speed = 180;
        angle = (speed * 128 - 0x2300) / 90;
        draw_rotated_hud_sprite(pixels, g_hud_speed_needle,
                                110, 220, 33, 3, angle);
        if (game->bike_physics_ready) {
            int angle = game->engine_gauge_pitch >> 9;
            if (angle > 250) angle = 250;
            draw_rotated_hud_sprite(pixels, g_hud_speed_needle,
                                    192, 220, 33, 3, angle);
        } else
            draw_rotated_hud_sprite(pixels, g_hud_speed_needle,
                                    192, 220, 33, 3,
                                    (int)(game->speed * 5u /
                                    (game->gear ? game->gear : 1u)));
    } else {
        draw_hud_needle(pixels, 110 * ROAD_WIDTH / 300,
            178 + (220 - 172) * 62 / 58, game->speed, 160u);
        draw_hud_needle(pixels, 192 * ROAD_WIDTH / 300,
            178 + (220 - 172) * 62 / 58, game->speed / 16u, 12u);
    }
    if (g_hud_health_needle) {
        draw_rotated_hud_sprite(pixels, g_hud_health_needle,
                                66, 200, 8, 1,
                                (int)(game->rider_health * 128u /
                                game->rider_max_health));
        draw_rotated_hud_sprite(pixels, g_hud_health_needle,
                                152, 204, 17, 1,
                                (int)(game->bike_health * 64u /
                                game->bike_max_health + 30u));
    }
}

static void draw_native_hud_sprite(road_pixel_t *pixels,
                                   const road_sprite_t *sprite,
                                   int left, int top)
{
    unsigned int y;
    if (!sprite || !sprite->pixels) return;
    for (y = 0u; y < sprite->height; ++y) {
        unsigned int x;
        int yy = top + (int)y;
        if (yy < 0 || yy >= ROAD_HEIGHT) continue;
        for (x = 0u; x < sprite->width; ++x) {
            int xx = left + (int)x;
            road_pixel_t color = sprite->pixels[y * sprite->width + x];
            if (xx >= 0 && xx < ROAD_WIDTH && color != 0xf81fu)
                pixels[yy * ROAD_WIDTH + xx] = color;
        }
    }
}

static void draw_number(road_pixel_t *pixels, int x, int y,
                        unsigned int value, road_pixel_t color);

static void draw_alternate_hud(road_pixel_t *pixels,
                               const road_game_t *game)
{
    unsigned int rider_frame = game->rider_health * 31u /
        game->rider_max_health;
    unsigned int bike_frame = game->bike_health * 31u /
        game->bike_max_health;
    if (rider_frame >= g_hud_health_frame_count)
        rider_frame = g_hud_health_frame_count - 1u;
    if (bike_frame >= g_hud_health_frame_count)
        bike_frame = g_hud_health_frame_count - 1u;
    draw_native_hud_sprite(pixels, g_hud_portrait, 20, 193);
    draw_native_hud_sprite(pixels,
        &g_hud_health_frames[rider_frame], 20, 210);
    draw_native_hud_sprite(pixels,
        &g_hud_health_frames[bike_frame], 136, 210);
    draw_number(pixels, 94, 210, game->speed, rgb(255, 255, 255));
    if (g_finish_sample)
        draw_number(pixels, 195, 210,
            game->distance * 100u / (g_finish_sample * 256u),
            rgb(255, 255, 255));
}

static void draw_scenery(road_pixel_t *pixels, int x, int y, int size,
                         unsigned int variant)
{
    if (g_scenery_count) {
        draw_sprite_scaled(pixels,
            &g_scenery[variant % g_scenery_count], x, y, size * 2);
        return;
    }
    if (size < 2) return;
    box(pixels, x - size / 8, y - size, size / 4 + 1, size,
        rgb(87, 58, 31));
    box(pixels, x - size / 2, y - size * 2, size, size,
        rgb(23, 85, 43));
    box(pixels, x - size / 3, y - size * 2 - size / 3,
        size * 2 / 3, size / 2, rgb(29, 116, 49));
}

static void draw_original_scenery_placements(const road_game_t *game,
                                             road_pixel_t *pixels,
                                             unsigned int only_sample)
{
    unsigned int left = 0u;
    unsigned int right = g_scenery_placement_count;
    unsigned int sample_limit = only_sample;
    while (left < right) {
        unsigned int middle = left + (right - left) / 2u;
        if (g_scenery_placements[middle].sample <= sample_limit)
            left = middle + 1u;
        else
            right = middle;
    }
    while (left > 0u) {
        const road_scenery_placement_t *placement =
            &g_scenery_placements[--left];
        unsigned int world = (unsigned int)placement->sample * 256u;
        unsigned int relative;
        int yy, dy, edge, center, size, x, projected;
        if (placement->sample != only_sample || world <= game->distance)
            break;
        relative = world - game->distance;
        if (relative > ROAD_VISIBLE_DEPTH) continue;
        yy = HORIZON + road_projection_dy(relative);
        dy = yy - HORIZON;
        if (dy <= 2 || dy > ROAD_VIEW_BOTTOM - HORIZON + 24) continue;
        edge = road_projection_unit(relative);
        if (g_width_count) {
            unsigned int sample = world / 256u;
            if (sample >= g_width_count) sample = g_width_count - 1u;
            edge = edge * (int)(placement->side ?
                g_course_left_width[sample] :
                g_course_right_width[sample]) / 512;
        }
        center = road_center(game, world, dy);
        size = (2 + dy / 3) * (int)placement->scale / 8;
        if (size < 1) size = 1;
        x = edge + size + (int)placement->lateral * dy / 4096;
        x = placement->side ? center - x : center + x;
        projected = road_project_y(game, world, yy);
        if (projected > -size && projected < ROAD_HEIGHT + size &&
            road_roadside_object_visible(placement->sample,
                projected - size * 2))
            draw_scenery(pixels, x, projected, size, placement->variant);
    }
}

static void draw_static_scenery(const road_game_t *game,
                               road_pixel_t *pixels,
                               unsigned int only_sample)
{
    unsigned int left = 0u;
    unsigned int right = g_static_placement_count;
    unsigned int sample_limit = only_sample;
    while (left < right) {
        unsigned int middle = left + (right - left) / 2u;
        if (g_static_placements[middle].sample <= sample_limit)
            left = middle + 1u;
        else
            right = middle;
    }
    while (left > 0u) {
        const road_static_placement_t *placement =
            &g_static_placements[--left];
        const road_sprite_t *sprite;
        unsigned int world = (unsigned int)placement->sample * 256u;
        unsigned int relative;
        unsigned int scale_bucket = 1u;
        int scaled_frame = 0;
        int yy, dy, x, projected, height;
        if (placement->sample != only_sample || world <= game->distance)
            break;
        if (placement->variant >= g_static_sprite_count) continue;
        relative = world - game->distance;
        if (relative > ROAD_VISIBLE_DEPTH) continue;
        yy = HORIZON + road_projection_dy(relative);
        dy = yy - HORIZON;
        if (dy <= 2 || dy > ROAD_VIEW_BOTTOM - HORIZON + 24) continue;
        sprite = &g_static_sprites[placement->variant];
        height = (int)sprite->height * dy / 140;
        if (height < 1) height = 1;
        if (g_reciprocal_table && g_reciprocal_count &&
            g_static_scale_frames &&
            placement->variant < g_static_scale_variant_count) {
            unsigned int half_depth = (relative + 128u) >> 1u;
            unsigned int width_scale, bucket;
            if (half_depth >= g_reciprocal_count)
                half_depth = g_reciprocal_count - 1u;
            width_scale = g_reciprocal_table[half_depth] * 8u;
            bucket = width_scale < 0x18000u ? 0u :
                width_scale >= 0x30000u ? 2u : 1u;
            const road_sprite_t *frame =
                &g_static_scale_frames[placement->variant * 3u + bucket];
            if (frame->pixels && frame->width && frame->height) {
                unsigned int pixel_scale;
                sprite = frame;
                scale_bucket = bucket;
                scaled_frame = 1;
                pixel_scale = width_scale >> bucket;
                height = (int)((sprite->height * pixel_scale +
                                0x8000u) >> 16u);
                if (height < 1) height = 1;
            }
        }
        x = road_center(game, world, dy) +
            hazard_lateral(placement, game->elapsed_ms) *
            road_projection_unit(relative) / 111;
        projected = road_project_y(game, world, yy);
        if (scaled_frame && g_static_frame_anchors &&
            placement->variant < g_static_anchor_variant_count) {
            const road_static_frame_anchor_t *anchor =
                &g_static_frame_anchors[
                    placement->variant * 3u + scale_bucket];
            if (anchor->valid) {
                int width = height * (int)sprite->width /
                    (int)sprite->height;
                x += width / 2 - (int)anchor->x * height /
                    (int)sprite->height;
                projected += height - (int)anchor->y * height /
                    (int)sprite->height;
            }
        }
        if (projected > -height && projected < ROAD_HEIGHT + height &&
            road_roadside_object_visible(placement->sample,
                projected - height))
            draw_sprite_scaled(pixels, sprite, x, projected, height);
    }
}

static void draw_bike(road_pixel_t *pixels, int x, int y, int size,
                      road_pixel_t suit, int lean, unsigned int attack_ms,
                      unsigned int variant)
{
    if (g_bike_count > ROAD_BIKE_OPPONENT_INDEX) {
        unsigned int sprite_index = variant == 4u ?
            ROAD_BIKE_OPPONENT_INDEX :
            attack_ms ? ROAD_BIKE_ATTACK_FIRST +
                (ROAD_ATTACK_DURATION_MS - attack_ms) *
                ROAD_BIKE_ATTACK_COUNT / ROAD_ATTACK_DURATION_MS :
            lean < 0 ? 1u : lean > 0 ? 2u : 0u;
        if (variant != 4u && !attack_ms &&
            g_bike_count >= ROAD_BIKE_FRAME_COUNT) {
            if (lean < 0)
                sprite_index = lean <= -3 ? 1u :
                    lean == -2 ? 27u : 26u;
            else if (lean > 0)
                sprite_index = lean >= 3 ? 2u :
                    lean == 2 ? 22u : 23u;
        }
        if (sprite_index >= ROAD_BIKE_OPPONENT_INDEX && variant != 4u)
            if (sprite_index < ROAD_BIKE_TURN_FIRST ||
                sprite_index >= ROAD_BIKE_FALLEN_INDEX)
                sprite_index = ROAD_BIKE_OPPONENT_INDEX - 1u;
        draw_sprite_scaled(pixels, &g_bikes[sprite_index], x, y,
            variant == 4u ? size :
            size * (int)g_bikes[sprite_index].height /
                (int)g_bikes[0].height);
        return;
    }
    int wheel = size / 5;
    if (size < 6) size = 6;
    if (wheel < 2) wheel = 2;
    box(pixels, x - size / 2, y - wheel, wheel * 2, wheel * 2,
        rgb(15, 17, 19));
    box(pixels, x + size / 2 - wheel * 2, y - wheel,
        wheel * 2, wheel * 2, rgb(15, 17, 19));
    box(pixels, x - size / 2 + wheel, y - size / 3,
        size - wheel * 2, size / 4 + 2, rgb(52, 54, 57));
    box(pixels, x - size / 4 + lean * 2, y - size,
        size / 2, size * 2 / 3, suit);
    box(pixels, x - size / 5 + lean * 2, y - size - size / 3,
        size * 2 / 5, size / 3, rgb(243, 199, 137));
    if (attack_ms)
        box(pixels, x + size / 4, y - size * 3 / 4,
            size / 2, 3, rgb(243, 199, 137));
}

static void draw_player_crash(road_pixel_t *pixels,
                              const road_game_t *game)
{
    unsigned int t = game->crash_elapsed_ms;
    unsigned int rider = ROAD_BIKE_STAND_INDEX;
    int bike_x = ROAD_VIEW_WIDTH / 2 + game->lean * 4;
    int rider_x = bike_x + 62;
    int rider_y = 170;
    if (g_bike_count < ROAD_BIKE_FRAME_COUNT) {
        draw_bike(pixels, bike_x, 170, 52, rgb(56,105,224), 0, 0u, 0u);
        return;
    }
    draw_sprite_scaled(pixels, &g_bikes[ROAD_BIKE_FALLEN_INDEX],
                       bike_x, 171, 32);
    if (t < 500u) {
        rider = ROAD_BIKE_TUMBLE_FIRST + t * 7u / 500u;
        rider_x = bike_x + 15 + (int)(t * 47u / 500u);
        rider_y -= (int)(t < 250u ? t : 500u - t) / 5;
    } else if (t < 1050u) {
        rider = ROAD_BIKE_GROUND_FIRST +
            (t - 500u) * 5u / 550u;
    } else if (t < 1350u) {
        rider = ROAD_BIKE_STAND_INDEX;
    } else if (t < 2750u) {
        unsigned int walk = t - 1350u;
        rider = ROAD_BIKE_RUN_FIRST + (walk / 110u) % 6u;
        rider_x = bike_x + 62 - (int)(walk * 47u / 1400u);
    } else {
        rider = ROAD_BIKE_MOUNT_FIRST +
            (t - 2750u) * 5u / 850u;
        rider_x = bike_x + 15;
    }
    if (rider >= ROAD_BIKE_FRAME_COUNT)
        rider = ROAD_BIKE_FRAME_COUNT - 1u;
    draw_sprite_scaled(pixels, &g_bikes[rider], rider_x, rider_y,
        (int)g_bikes[rider].height * 60 / 64);
}

static void draw_digit(road_pixel_t *pixels, int x, int y, unsigned int digit,
                       road_pixel_t color)
{
    static const unsigned char bits[10][5] = {
        {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7},
        {5,5,7,1,1}, {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1},
        {7,5,7,5,7}, {7,5,7,1,7}
    };
    int yy;
    for (yy = 0; yy < 5; ++yy) {
        int xx;
        for (xx = 0; xx < 3; ++xx)
            if (bits[digit][yy] & (4 >> xx))
                box(pixels, x + xx * 2, y + yy * 2, 2, 2, color);
    }
}

static void draw_number(road_pixel_t *pixels, int x, int y,
                        unsigned int value, road_pixel_t color)
{
    unsigned int divisor = 100u;
    int started = 0;
    while (divisor) {
        unsigned int digit = value / divisor % 10u;
        if (digit || started || divisor == 1u) {
            draw_digit(pixels, x, y, digit, color);
            x += 8;
            started = 1;
        }
        divisor /= 10u;
    }
}

static void draw_finish(road_pixel_t *pixels)
{
    static const unsigned char glyphs[5][7] = {
        {31,16,16,30,16,16,16}, /* F */
        {31,4,4,4,4,4,31},       /* I */
        {17,25,21,19,17,17,17}, /* N */
        {15,16,16,14,1,1,30},   /* S */
        {17,17,17,31,17,17,17}  /* H */
    };
    static const unsigned char word[6] = {0,1,2,1,3,4};
    unsigned int letter;
    box(pixels, 108, 97, 104, 34, rgb(20, 28, 36));
    for (letter = 0u; letter < 6u; ++letter) {
        int yy;
        for (yy = 0; yy < 7; ++yy) {
            int xx;
            unsigned int bits = glyphs[word[letter]][yy];
            for (xx = 0; xx < 5; ++xx)
                if (bits & (16u >> xx))
                    box(pixels, 124 + (int)letter * 12 + xx * 2,
                        107 + yy * 2, 2, 2, rgb(248, 230, 170));
        }
    }
}

static unsigned int road_texture_style_for_span(int height, int width)
{
    if (height < 0) height = -height;
    if (width < 0) width = -width;
    if (height <= 1) return 8u;
    if (height <= 4) return width >= 13 ? 5u : 7u;
    if (height < 8) return width >= 13 ? 4u : 6u;
    if (height < 12) return width >= 25 ? 2u : 3u;
    if (height >= 30) return 0u;
    return 1u;
}

static unsigned int road_texture_style(int dy, int unit)
{
    unsigned int depth = road_depth_for_dy((unsigned int)dy);
    int height = dy - road_projection_dy(depth + 256u);
    /* The row renderer estimates the source quad until the exact strip
       vertices are submitted in the second road pass. */
    return road_texture_style_for_span(height, unit / 2);
}

static unsigned int road_texture_variant(unsigned int left_width,
                                          unsigned int right_width,
                                          unsigned int strip)
{
    unsigned int rounded_left = (left_width + 255u) >> 8u;
    if (strip == 0u) return 0u;
    if (strip == rounded_left) return 2u;
    if ((left_width & 255u) && strip <= 1u) return 0u;
    if ((right_width & 255u) == 0u) return 1u;
    return strip < rounded_left + (right_width >> 8u) ? 1u : 0u;
}

static void draw_road_texture_row(road_pixel_t *pixels, int y,
                                   int left, int right, int dy, int unit,
                                   unsigned int world,
                                   unsigned int left_width,
                                   unsigned int right_width)
{
    unsigned int style = road_texture_style(dy, unit);
    unsigned int total = left_width + right_width;
    unsigned int position_fixed, position_step;
    int x;
    if (right <= left || !total) return;
    x = clamp_int(left, 0, ROAD_WIDTH);
    position_step = (total << 8u) / (unsigned int)(right - left);
    position_fixed = (unsigned int)(x - left) * position_step;
    for (;
         x < right && x < ROAD_WIDTH; ++x) {
        unsigned int position = position_fixed >> 8u;
        position_fixed += position_step;
        unsigned int variant = road_texture_variant(left_width, right_width,
                                                    position >> 8u);
        const road_sprite_t *texture = &g_surface_textures[variant * 9u + style];
        unsigned int source_x, source_y;
        if (!texture->pixels || !texture->width || !texture->height)
            continue;
        source_x = (position & 255u) * texture->width / 256u;
        source_y = (255u - (world & 255u)) * texture->height / 256u;
        pixels[y * ROAD_WIDTH + x] =
            texture->pixels[source_y * texture->width + source_x];
    }
}

static const road_hill_profile_t *hill_profile_at(unsigned int sample,
                                                 unsigned int channel)
{
    const road_hill_profile_t *profiles = channel ?
        g_fork_hill_profiles : g_hill_profiles;
    unsigned int count = channel ?
        g_fork_hill_profile_count : g_hill_profile_count;
    unsigned int low = 0u, high = count;
    if (!profiles) return 0;
    while (low < high) {
        unsigned int middle = low + (high - low) / 2u;
        if (profiles[middle].end_sample <= sample)
            low = middle + 1u;
        else
            high = middle;
    }
    if (low < count && profiles[low].start_sample <= sample)
        return &profiles[low];
    return 0;
}

static int hill_profile_shade(int x, int y, int previous_x,
                              int previous_y)
{
    int dx = (x - previous_x) * 32;
    int dy = (y - previous_y) * 16;
    int distance, fraction, value;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    distance = dx + dy - (dx < dy ? dx : dy) / 2;
    if (!distance) return 31;
    fraction = (dy << 16) / distance;
    value = ((65536 - fraction) * 32) >> 16;
    return clamp_int(value, 0, 31);
}

typedef struct road_hill_section {
    int x[2][5];
    int y[2][5];
    int shade[2][3];
    unsigned char tile[2][3];
    unsigned char extension[2];
    unsigned char profile_count[2];
    unsigned char paired_profile;
    int valid;
} road_hill_section_t;

typedef struct road_quad_vertex {
    int x;
    int y;
} road_quad_vertex_t;

/* The 3DO profile renderer rejects quads whose winding faces away from the
 * viewer. Keep the same two-cross-product test before software rasterizing. */
static int hill_quad_faces_camera(const road_quad_vertex_t *quad,
                                  unsigned int side)
{
    int cross0 = (quad[2].y - quad[3].y) *
                 (quad[0].x - quad[3].x);
    int cross1 = (quad[0].y - quad[3].y) *
                 (quad[2].x - quad[3].x);
    int cross2 = (quad[0].y - quad[1].y) *
                 (quad[2].x - quad[1].x);
    int cross3 = (quad[2].y - quad[1].y) *
                 (quad[0].x - quad[1].x);
    if (side)
        return cross0 > cross1 || cross2 > cross3;
    return cross0 < cross1 || cross2 < cross3;
}

/* Rasterize the same far/near, inner/outer corner order submitted to the
   3DO mapped-CEL path. UVs remain affine in this RGB565 implementation. */
static void draw_textured_surface_quad_rows(road_pixel_t *pixels,
                                            const road_quad_vertex_t *quad,
                                            const road_sprite_t *tile,
                                            int shade, road_pixel_t fallback,
                                            unsigned int source_first,
                                            unsigned int source_last)
{
    int min_y = quad[0].y, max_y = min_y;
    int min_x = quad[0].x, max_x = min_x;
    int tex_width = tile && tile->pixels && tile->width && tile->height ?
        (int)tile->width : 32;
    int tex_height = tile && tile->pixels && tile->width && tile->height ?
        (int)tile->height : 16;
    int u[4] = {0, tex_width * 256, tex_width * 256, 0};
    int v[4] = {(int)(source_first * (unsigned int)tex_height * 64u),
                (int)(source_first * (unsigned int)tex_height * 64u),
                (int)(source_last * (unsigned int)tex_height * 64u),
                (int)(source_last * (unsigned int)tex_height * 64u)};
    unsigned int brightness = 64u + (unsigned int)shade * 192u / 31u;
    road_pixel_t fallback_shaded = (road_pixel_t)(
        (((((fallback >> 11u) & 31u) * brightness) >> 8u) << 11u) |
        (((((fallback >> 5u) & 63u) * brightness) >> 8u) << 5u) |
        (((fallback & 31u) * brightness) >> 8u));
    int i, y;
    for (i = 1; i < 4; ++i) {
        if (quad[i].y < min_y) min_y = quad[i].y;
        if (quad[i].y > max_y) max_y = quad[i].y;
        if (quad[i].x < min_x) min_x = quad[i].x;
        if (quad[i].x > max_x) max_x = quad[i].x;
    }
    if (max_y <= 0 || min_y >= ROAD_HEIGHT ||
        max_x <= 0 || min_x >= ROAD_WIDTH) return;
    if (min_y < 0) min_y = 0;
    if (max_y > ROAD_HEIGHT) max_y = ROAD_HEIGHT;
    for (y = min_y; y < max_y; ++y) {
        int left_x = 0x7fffffff, right_x = (-0x7fffffff - 1);
        int left_u = 0, left_v = 0, right_u = 0, right_v = 0;
        int hits = 0;
        int x, first, last, span_x, step_u, step_v;
        int u_acc, v_acc;
        for (i = 0; i < 4; ++i) {
            int j = (i + 1) & 3;
            int y0 = quad[i].y, y1 = quad[j].y;
            int delta_y = y1 - y0;
            int x_at, u_at, v_at;
            if (!delta_y || y < (y0 < y1 ? y0 : y1) ||
                y >= (y0 > y1 ? y0 : y1)) continue;
            x_at = quad[i].x * 256 +
                (quad[j].x - quad[i].x) * 256 * (y - y0) / delta_y;
            u_at = u[i] + (u[j] - u[i]) * (y - y0) / delta_y;
            v_at = v[i] + (v[j] - v[i]) * (y - y0) / delta_y;
            if (x_at < left_x) {
                left_x = x_at;
                left_u = u_at;
                left_v = v_at;
            }
            if (x_at > right_x) {
                right_x = x_at;
                right_u = u_at;
                right_v = v_at;
            }
            ++hits;
        }
        if (hits < 2 || right_x <= left_x) continue;
        first = (left_x + 255) / 256;
        last = (right_x + 255) / 256;
        if (first < 0) first = 0;
        if (last > ROAD_WIDTH) last = ROAD_WIDTH;
        if (first >= last) continue;
        span_x = right_x - left_x;
        step_u = ((right_u - left_u) * 32768 / span_x) * 2;
        step_v = ((right_v - left_v) * 32768 / span_x) * 2;
        u_acc = left_u * 256 + (int)(
            ((long long)(first * 256 - left_x) * step_u) / 256);
        v_acc = left_v * 256 + (int)(
            ((long long)(first * 256 - left_x) * step_v) / 256);
        for (x = first; x < last; ++x) {
            road_pixel_t color = fallback_shaded;
            if (tile && tile->pixels && tile->width && tile->height) {
                int sx = clamp_int(u_acc >> 16, 0, tex_width - 1);
                int sy = clamp_int(v_acc >> 16, 0, tex_height - 1);
                color = tile->pixels[sy * tex_width + sx];
                if (color == 0xf81fu) {
                    u_acc += step_u;
                    v_acc += step_v;
                    continue;
                }
                color = (road_pixel_t)(
                    (((((color >> 11u) & 31u) * brightness) >> 8u)
                        << 11u) |
                    (((((color >> 5u) & 63u) * brightness) >> 8u)
                        << 5u) |
                    (((color & 31u) * brightness) >> 8u));
            }
            pixels[y * ROAD_WIDTH + x] = color;
            u_acc += step_u;
            v_acc += step_v;
        }
    }
}

static void draw_textured_surface_quad(road_pixel_t *pixels,
                                       const road_quad_vertex_t *quad,
                                       const road_sprite_t *tile,
                                       int shade, road_pixel_t fallback)
{
    draw_textured_surface_quad_rows(pixels, quad, tile, shade,
                                    fallback, 0u, 4u);
}

/* reconcile_road_lane_transition_profiles grows the two inner shoulders
 * from the junction to the first-clearance node. Before they fit between
 * the roads, each is capped at half of left_step and the scale is at least
 * one half. The profile heights on those inner sides are flat until the
 * transition amount reaches zero. */
static int fork_transition_inner_margin(unsigned int sample,
    unsigned int mapped, unsigned int channel, unsigned int side,
    int margin, int *flatten)
{
    unsigned int phase, limit, physical_left;
    int left_margin, right_margin, left_step, scale;
    *flatten = 0;
    if (!g_fork_active_span || sample >= g_cross_section_count ||
        mapped >= g_fork_slope_count || !g_course_left_margin ||
        !g_course_right_margin || !g_fork_left_margin ||
        !g_fork_right_margin) return margin;
    physical_left = g_fork_main_channel ? 1u : 0u;
    if (!((channel == physical_left && side == 1u) ||
          (channel != physical_left && side == 0u))) return margin;
    if (g_fork_reverse_mode) {
        if (sample < g_fork_reverse_step_start ||
            sample - g_fork_reverse_step_start >=
                g_fork_reverse_step_count) return margin;
        phase = g_fork_reverse_step_start +
            g_fork_reverse_step_count - 1u - sample;
        limit = g_fork_reverse_clearance_offset;
        left_step = g_fork_reverse_left_step[
            sample - g_fork_reverse_step_start];
    } else {
        if (sample < g_fork_sample ||
            sample - g_fork_sample >=
                g_fork_forward_step_count) return margin;
        phase = sample - g_fork_sample;
        limit = g_fork_forward_clearance_index;
        left_step = g_fork_forward_left_step[phase];
    }
    if (!limit || phase >= limit) return margin;
    *flatten = 1;
    left_margin = g_fork_main_channel ?
        (int)g_fork_right_margin[mapped] :
        (int)g_course_right_margin[sample];
    right_margin = g_fork_main_channel ?
        (int)g_course_left_margin[sample] :
        (int)g_fork_left_margin[mapped];
    scale = (int)(phase * 65536u / limit);
    if (left_margin + right_margin > left_step) {
        margin = left_step / 2;
        if (margin < 0) margin = 0;
        if (scale < 32768) scale = 32768;
    }
    return margin * scale >> 16;
}

static void hill_screen_section_raw(const road_game_t *game,
                                unsigned int world,
                                unsigned int channel,
                                road_hill_section_t *section)
{
    unsigned int sample = world / 256u;
    unsigned int mapped = sample;
    const road_hill_profile_t *profile;
    const road_hill_sample_t *shape;
    unsigned int shape_count = channel ?
        g_fork_hill_sample_count : g_hill_sample_count;
    const road_hill_sample_t *shapes = channel ?
        g_fork_hill_samples : g_hill_samples;
    unsigned int width_sample = sample, side, band;
    int dy, unit, projected, center, left, right;
    int left_width = 512, right_width = 512;
    int left_margin = 2, right_margin = 2;
    int flatten_inner[2] = {0, 0};
    section->valid = 0;
    section->paired_profile = 0u;
    if (channel) {
        if (!road_fork_in_view(game) ||
            sample < g_fork_visible_start ||
            sample >= g_fork_end_sample ||
            !road_fork_sample_index(sample, &mapped)) return;
        width_sample = mapped;
    }
    profile = hill_profile_at(mapped, channel);
    shape = shapes && mapped < shape_count ? &shapes[mapped] : 0;
    if (!profile || world <= game->distance) return;
    dy = road_projection_dy(world - game->distance);
    if (dy < 1) dy = 1;
    unit = road_projection_unit(world - game->distance);
    projected = channel ? road_fork_project_y(game, world, HORIZON + dy) :
        road_project_y(game, world, HORIZON + dy);
    center = channel ? road_fork_center(game, world, dy) :
        road_center(game, world, dy);
    if (road_fork_in_view(game) && !g_fork_reverse_mode &&
        sample >= g_fork_visible_start &&
        sample < g_fork_end_sample && g_fork_sample < g_width_count) {
        unsigned int before = g_fork_sample ? g_fork_sample - 1u : 0u;
        if (channel) {
            if (g_fork_main_channel == 0u)
                center += unit * ((int)g_course_right_width[before] -
                    (int)g_fork_right_width[g_fork_sample]) / 512;
            else
                center -= unit * ((int)g_course_left_width[before] -
                    (int)g_fork_left_width[g_fork_sample]) / 512;
        } else if (g_fork_main_channel == 0u)
            center -= unit * ((int)g_course_left_width[before] -
                (int)g_course_left_width[g_fork_sample]) / 512;
        else
            center += unit * ((int)g_course_right_width[before] -
                (int)g_course_right_width[g_fork_sample]) / 512;
    }
    if (channel && g_fork_count) {
        left_width = g_fork_left_width[width_sample];
        right_width = g_fork_right_width[width_sample];
    } else if (g_width_count) {
        if (width_sample >= g_width_count)
            width_sample = g_width_count - 1u;
        left_width = g_course_left_width[width_sample];
        right_width = g_course_right_width[width_sample];
    }
    left = center - unit * left_width / 512;
    right = center + unit * right_width / 512;
    if (channel && mapped < g_fork_slope_count) {
        left_margin = unit * fork_transition_inner_margin(sample,
            mapped, channel, 0u, (int)g_fork_left_margin[mapped],
            &flatten_inner[0]) / 512;
        right_margin = unit * fork_transition_inner_margin(sample,
            mapped, channel, 1u, (int)g_fork_right_margin[mapped],
            &flatten_inner[1]) / 512;
    } else if (!channel && sample < g_cross_section_count) {
        left_margin = unit * fork_transition_inner_margin(sample,
            mapped, channel, 0u, (int)g_course_left_margin[sample],
            &flatten_inner[0]) / 512;
        right_margin = unit * fork_transition_inner_margin(sample,
            mapped, channel, 1u, (int)g_course_right_margin[sample],
            &flatten_inner[1]) / 512;
    }
    if (channel ? mapped < g_fork_slope_count :
                  sample < g_cross_section_count) {
        if (left_margin < 1 && !flatten_inner[0]) left_margin = 1;
        if (right_margin < 1 && !flatten_inner[1]) right_margin = 1;
    }
    for (side = 0u; side < 2u; ++side) {
        int edge = side ? right + right_margin : left - left_margin;
        int previous_x = 0, previous_y = 0;
        int far_y = 0;
        section->x[side][0] = edge;
        section->y[side][0] = projected;
        for (band = 0u; band < 3u; ++band) {
            int px = shape ? shape->x[side][band] :
                profile->x[side][band];
            int py = shape ? shape->y[side][band] :
                profile->y[side][band];
            if (flatten_inner[side]) py = 0;
            section->x[side][band + 1u] = edge +
                (side ? 1 : -1) * unit * px * 32 / 500;
            section->y[side][band + 1u] = projected -
                unit * py * 16 / 500;
            section->shade[side][band] = hill_profile_shade(px, py,
                previous_x, previous_y);
            section->tile[side][band] = profile->tile_index[side][band];
            if (band == 1u) far_y = py;
            previous_x = px;
            previous_y = py;
        }
        section->extension[side] = (unsigned char)(side ?
            section->x[side][3] < ROAD_VIEW_WIDTH :
            section->x[side][3] > 0);
        section->x[side][4] = 2 * section->x[side][3] -
            section->x[side][2];
        if (side && section->x[side][4] < ROAD_VIEW_WIDTH)
            section->x[side][4] = ROAD_VIEW_WIDTH;
        if (!side && section->x[side][4] > 0)
            section->x[side][4] = 0;
        section->y[side][4] = projected - unit * far_y * 16 / 500;
        section->profile_count[side] = 4u;
    }
    section->valid = 1;
}

/* The source reconciles facing RHIL profiles in track space before their
 * points are projected. At one depth the software projection is affine, so
 * the same point search and fixed-point interpolation can be applied to the
 * paired screen sections. The resulting counts also decide which inner
 * bands exist and whether there is space for the center CEL. */
static void reconcile_fork_hill_sections(road_hill_section_t *main,
    road_hill_section_t *fork, unsigned int sample)
{
    road_hill_section_t *left = g_fork_main_channel ? fork : main;
    road_hill_section_t *right = g_fork_main_channel ? main : fork;
    road_quad_vertex_t left_points[4], right_points[4];
    unsigned int i, left_index = 1u, right_index = 1u;
    unsigned int attempts = 1u;
    int found = 0, ix, iy, dx;
    for (i = 0u; i < 4u; ++i) {
        left_points[i].x = left->x[1][i];
        left_points[i].y = left->y[1][i];
        right_points[i].x = right->x[0][i];
        right_points[i].y = right->y[0][i];
    }
    do {
        if (left_points[left_index].x <
                right_points[right_index].x && left_index < 3u)
            ++left_index;
        found = left_points[left_index].x >=
            right_points[right_index].x;
        if (!found && right_index < 3u) ++right_index;
        ++attempts;
    } while (attempts < 4u && !found);
    if (!found) found = left_points[left_index].x >=
        right_points[right_index].x;
    if (!found) return;
    ix = (left_points[left_index - 1u].x +
        right_points[right_index - 1u].x) / 2;
    dx = left_points[left_index].x -
        left_points[left_index - 1u].x;
    if (dx) {
        int slope = (left_points[left_index].y -
            left_points[left_index - 1u].y) * 65536 / dx;
        iy = left_points[left_index - 1u].y +
            (int)((long long)slope *
                (ix - left_points[left_index - 1u].x) / 65536);
    } else {
        iy = (left_points[left_index].y +
            right_points[right_index].y) / 2;
    }
    if (!g_fork_reverse_mode) {
        if (sample >= g_fork_sample &&
            sample - g_fork_sample == g_fork_forward_clearance_index)
            iy = left->y[1][0];
    } else if (sample >= g_fork_reverse_step_start &&
        sample - g_fork_reverse_step_start <
            g_fork_reverse_step_count &&
        g_fork_reverse_step_start +
            g_fork_reverse_step_count - 1u - sample ==
            g_fork_reverse_clearance_offset) {
        iy = left->y[1][0];
    }
    left->profile_count[1] = (unsigned char)left_index;
    right->profile_count[0] = (unsigned char)right_index;
    for (i = left_index; i < 4u; ++i) {
        left->x[1][i] = ix;
        left->y[1][i] = iy;
    }
    for (i = right_index; i < 4u; ++i) {
        right->x[0][i] = ix;
        right->y[0][i] = iy;
    }
}

static void hill_screen_section(const road_game_t *game,
    unsigned int world, unsigned int channel,
    road_hill_section_t *section)
{
    road_hill_section_t other;
    unsigned int sample = world / 256u, mapped;
    hill_screen_section_raw(game, world, channel, section);
    if (!section->valid || !road_fork_in_view(game) ||
        !g_course_terrain_mode || !g_margin_terrain_mode[1] ||
        sample >= g_cross_section_count ||
        !road_fork_sample_index(sample, &mapped) ||
        mapped >= g_margin_sample_count[1] ||
        g_course_terrain_mode[sample] != 4u ||
        g_margin_terrain_mode[1][mapped] != 4u) return;
    hill_screen_section_raw(game, world, 1u - channel, &other);
    if (!other.valid) return;
    section->paired_profile = 1u;
    if (channel)
        reconcile_fork_hill_sections(&other, section, sample);
    else
        reconcile_fork_hill_sections(section, &other, sample);
}

static const road_sprite_t *select_hill_tile_for_quad(unsigned int index,
    const road_quad_vertex_t *quad)
{
    const road_sprite_t *tile = index < g_hill_tile_count ?
        &g_hill_tiles[index] : 0;
    int dx0, dy0, dx1, dy1, wx, wy;
    unsigned int span0, span1, span, width_span, level;
    const road_sprite_t *mip;
    if (!tile || !g_hill_mips) return tile;
    dx0 = quad[3].x - quad[0].x;
    dy0 = quad[3].y - quad[0].y;
    dx1 = quad[2].x - quad[1].x;
    dy1 = quad[2].y - quad[1].y;
    wx = quad[1].x - quad[0].x;
    wy = quad[1].y - quad[0].y;
    if (dx0 < 0) dx0 = -dx0;
    if (dy0 < 0) dy0 = -dy0;
    if (dx1 < 0) dx1 = -dx1;
    if (dy1 < 0) dy1 = -dy1;
    if (wx < 0) wx = -wx;
    if (wy < 0) wy = -wy;
    span0 = (unsigned int)(dx0 + dy0 -
        (dx0 < dy0 ? dx0 : dy0) / 2);
    span1 = (unsigned int)(dx1 + dy1 -
        (dx1 < dy1 ? dx1 : dy1) / 2);
    span = span0 > span1 ? span0 : span1;
    width_span = (unsigned int)(wx + wy -
        (wx < wy ? wx : wy) / 2);
    level = span <= 1u ? 3u : span <= 2u ? 2u :
        span <= 4u ? 1u : 0u;
    if (span > 8u) return tile;
    if (g_hill_narrow_mips && width_span <= 64u) {
        unsigned int width_level = width_span <= 16u ? 0u :
            width_span <= 32u ? 1u : 2u;
        mip = &g_hill_narrow_mips[index * 12u +
            level * 3u + width_level];
    } else {
        mip = &g_hill_mips[index * 4u + level];
    }
    return mip->pixels ? mip : tile;
}

static void draw_hill_surface_quads(const road_game_t *game,
                                    road_pixel_t *pixels,
                                    unsigned int only_sample,
                                    unsigned int channel)
{
    road_hill_section_t sections[2];
    road_hill_section_t *far_section = &sections[0];
    road_hill_section_t *near_section = &sections[1];
    unsigned int far_sample, near_sample, sample;
    if (channel ? !g_fork_hill_profile_count || !g_fork_hill_profiles :
                  !g_hill_profile_count || !g_hill_profiles) return;
    if (only_sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u ||
        only_sample <= (game->distance + 384u) / 256u) return;
    far_sample = only_sample;
    near_sample = only_sample - 1u;
    hill_screen_section(game, far_sample * 256u, channel, far_section);
    for (sample = far_sample; sample > near_sample; --sample) {
        unsigned int side;
        road_hill_section_t *swap;
        hill_screen_section(game, (sample - 1u) * 256u, channel,
            near_section);
        if (far_section->valid && near_section->valid) {
            for (side = 0u; side < 2u; ++side) {
                int band;
                for (band = 3; band >= 0; --band) {
                    road_quad_vertex_t quad[4];
                    unsigned int index;
                    const road_sprite_t *tile;
                    int source_band = band < 3 ? band : 2;
                    if (far_section->paired_profile &&
                        near_section->paired_profile &&
                        ((channel == (g_fork_main_channel ? 1u : 0u) &&
                          side == 1u) ||
                         (channel != (g_fork_main_channel ? 1u : 0u) &&
                          side == 0u))) {
                        unsigned int count = far_section->profile_count[side] >
                            near_section->profile_count[side] ?
                            far_section->profile_count[side] :
                            near_section->profile_count[side];
                        if (count > 3u) count = 3u;
                        if ((unsigned int)band >= count) continue;
                    }
                    if (band == 3 &&
                        !far_section->extension[side] &&
                        !near_section->extension[side]) continue;
                    index = near_section->tile[side][source_band];
                    quad[0].x = far_section->x[side][band];
                    quad[0].y = far_section->y[side][band];
                    quad[1].x = far_section->x[side][band + 1];
                    quad[1].y = far_section->y[side][band + 1];
                    quad[2].x = near_section->x[side][band + 1];
                    quad[2].y = near_section->y[side][band + 1];
                    quad[3].x = near_section->x[side][band];
                    quad[3].y = near_section->y[side][band];
                    if (!hill_quad_faces_camera(quad, side)) continue;
                    tile = select_hill_tile_for_quad(index, quad);
                    draw_textured_surface_quad(pixels, quad, tile,
                        near_section->shade[side][source_band],
                        rgb(43, 119, 49));
                }
            }
        }
        swap = far_section;
        far_section = near_section;
        near_section = swap;
    }
}

static int roadside_screen_edges(const road_game_t *game,
                                  unsigned int world, int *edges,
                                  int *base_y, int *unit_out)
{
    unsigned int sample = world / 256u;
    unsigned int width_sample = sample;
    int dy, unit, center, left_width = 512, right_width = 512;
    int left_margin = 2, right_margin = 2;
    if (world <= game->distance) return 0;
    dy = road_projection_dy(world - game->distance);
    if (dy < 1) dy = 1;
    unit = road_projection_unit(world - game->distance);
    center = road_center(game, world, dy);
    if (road_fork_in_view(game) && !g_fork_reverse_mode &&
        sample >= g_fork_visible_start &&
        sample < g_fork_end_sample &&
        g_fork_sample < g_width_count) {
        unsigned int before = g_fork_sample ? g_fork_sample - 1u : 0u;
        if (g_fork_main_channel == 0u)
            center -= unit * ((int)g_course_left_width[before] -
                (int)g_course_left_width[g_fork_sample]) / 512;
        else
            center += unit * ((int)g_course_right_width[before] -
                (int)g_course_right_width[g_fork_sample]) / 512;
    }
    if (g_width_count) {
        if (width_sample >= g_width_count)
            width_sample = g_width_count - 1u;
        left_width = g_course_left_width[width_sample];
        right_width = g_course_right_width[width_sample];
    }
    if (sample < g_cross_section_count) {
        left_margin = unit * (int)g_course_left_margin[sample] / 512;
        right_margin = unit * (int)g_course_right_margin[sample] / 512;
        if (left_margin < 1) left_margin = 1;
        if (right_margin < 1) right_margin = 1;
    }
    edges[0] = center - unit * left_width / 512 - left_margin;
    edges[1] = center + unit * right_width / 512 + right_margin;
    *base_y = road_project_y(game, world, HORIZON + dy);
    *unit_out = unit;
    return 1;
}

static const road_edge_profile_t *edge_profile_at(unsigned int sample,
                                                  unsigned int side)
{
    unsigned int low = 0u, high = g_edge_profile_count[side];
    const road_edge_profile_t *profiles = g_edge_profiles[side];
    if (!profiles || !high || sample < profiles[0].start_sample)
        return 0;
    while (low + 1u < high) {
        unsigned int middle = low + (high - low) / 2u;
        if (profiles[middle].start_sample <= sample) low = middle;
        else high = middle;
    }
    return &profiles[low];
}

typedef struct road_edge_section {
    int inner_x[2], outer_x[2];
    int inner_y[2], outer_y[2];
    int repeat_step;
    unsigned char tile[2];
    unsigned char raised[2];
    int valid;
} road_edge_section_t;

static void edge_screen_section(const road_game_t *game,
                                unsigned int world,
                                road_edge_section_t *section)
{
    unsigned int sample = world / 256u, side;
    int edges[2], base_y, unit;
    section->valid = 0;
    if (sample >= g_cross_section_count || !g_course_terrain_mode ||
        g_course_terrain_mode[sample] != 1u ||
        !roadside_screen_edges(game, world, edges, &base_y, &unit))
        return;
    for (side = 0u; side < 2u; ++side) {
        const road_edge_profile_t *profile = edge_profile_at(sample, side);
        int direction = side ? 1 : -1;
        int height;
        if (!profile) return;
        height = profile->height > 0 ? profile->height : 0;
        section->inner_x[side] = edges[side] + direction *
            unit * profile->inner_offset / 500;
        section->outer_x[side] = edges[side] + direction *
            unit * (sample < g_edge_outer_sample_count &&
                g_edge_outer_samples[side] ?
                g_edge_outer_samples[side][sample] :
                profile->outer_offset) / 500;
        section->inner_y[side] = base_y;
        section->outer_y[side] = base_y - unit * height / 500;
        section->raised[side] = (unsigned char)(height > 0);
        section->tile[side] = sample < g_surface_sample_count &&
            g_surface_samples ?
            g_surface_samples[sample].tile_index[side] : 255u;
    }
    /* Upstream repeats raised-edge CELs in steps of four projected
     * 250-unit lane cells; unit is the projected width of two cells. */
    section->repeat_step = unit * 2;
    section->valid = 1;
}

static void draw_raised_edge_repeats(road_pixel_t *pixels,
                                     const road_edge_section_t *far,
                                     const road_edge_section_t *near,
                                     unsigned int side,
                                     const road_sprite_t *tile)
{
    road_quad_vertex_t quad[4];
    int direction = side ? 1 : -1;
    int far_x = far->outer_x[side], near_x = near->outer_x[side];
    int far_step = far->repeat_step * direction;
    int near_step = near->repeat_step * direction;
    unsigned int repeat;
    if (!tile || !tile->pixels || far_step == 0 || near_step == 0 ||
        near->outer_y[side] <= far->outer_y[side]) return;
    for (repeat = 0u; repeat < 64u; ++repeat) {
        if (side ? far_x >= ROAD_VIEW_WIDTH && near_x >= ROAD_VIEW_WIDTH :
                   far_x <= 0 && near_x <= 0) break;
        quad[0].x = far_x;
        quad[0].y = far->outer_y[side];
        quad[1].x = far_x + far_step;
        quad[1].y = far->outer_y[side];
        quad[2].x = near_x + near_step;
        quad[2].y = near->outer_y[side];
        quad[3].x = near_x;
        quad[3].y = near->outer_y[side];
        draw_textured_surface_quad(pixels, quad, tile, 31,
            rgb(105, 94, 74));
        far_x += far_step;
        near_x += near_step;
    }
}

/* RMTN uses the same projected inner/outer edge pairs as the 3DO edge mode.
 * Near edge strips come from RRSM/FAM; recessed outer fills use the road
 * particle CEL that the original renderer selects for those bands. */
static void draw_edge_surface_quads(const road_game_t *game,
                                    road_pixel_t *pixels,
                                    unsigned int only_sample)
{
    road_edge_section_t sections[2];
    road_edge_section_t *far = &sections[0], *near = &sections[1];
    unsigned int far_sample, near_sample, sample;
    if (!g_edge_profile_count[0] || !g_edge_profile_count[1]) return;
    if (only_sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u ||
        only_sample <= (game->distance + 384u) / 256u) return;
    far_sample = only_sample;
    near_sample = only_sample - 1u;
    edge_screen_section(game, far_sample * 256u, far);
    for (sample = far_sample; sample > near_sample; --sample) {
        road_edge_section_t *swap;
        unsigned int side;
        edge_screen_section(game, (sample - 1u) * 256u, near);
        if (far->valid && near->valid) {
            for (side = 0u; side < 2u; ++side) {
                road_quad_vertex_t quad[4];
                unsigned int index = near->tile[side];
                const road_sprite_t *strip = index < g_roadside_tile_count ?
                    &g_roadside_tiles[index] : 0;
                const road_sprite_t *fill = far->outer_y[side] <= HORIZON + 8 &&
                    g_edge_fill_horizon ? g_edge_fill_horizon :
                    g_edge_fill_primary;
                int edge = side ? ROAD_VIEW_WIDTH : 0;
                quad[0].x = far->outer_x[side];
                quad[0].y = far->outer_y[side];
                quad[1].x = edge;
                quad[1].y = far->outer_y[side];
                quad[2].x = edge;
                quad[2].y = near->outer_y[side];
                quad[3].x = near->outer_x[side];
                quad[3].y = near->outer_y[side];
                if (!near->raised[side] && fill && fill->pixels)
                    draw_textured_surface_quad(pixels, quad, fill, 31,
                        rgb(53, 89, 102));
                /* The raised-edge CEL's U axis follows the track, while V
                 * crosses from the outer crest to the shoulder.  Recessed
                 * edges use the same axes with the opposite corner order. */
                if (near->raised[side]) {
                    quad[0].x = far->outer_x[side];
                    quad[0].y = far->outer_y[side];
                    quad[1].x = near->outer_x[side];
                    quad[1].y = near->outer_y[side];
                    quad[2].x = near->inner_x[side];
                    quad[2].y = near->inner_y[side];
                    quad[3].x = far->inner_x[side];
                    quad[3].y = far->inner_y[side];
                } else {
                    quad[0].x = far->inner_x[side];
                    quad[0].y = far->inner_y[side];
                    quad[1].x = near->inner_x[side];
                    quad[1].y = near->inner_y[side];
                    quad[2].x = near->outer_x[side];
                    quad[2].y = near->outer_y[side];
                    quad[3].x = far->outer_x[side];
                    quad[3].y = far->outer_y[side];
                }
                if (strip && strip->pixels)
                    draw_textured_surface_quad(pixels, quad, strip, 31,
                        rgb(105, 94, 74));
                if (near->raised[side]) {
                    int vertical_span = near->outer_y[side] -
                        far->outer_y[side];
                    unsigned int level = vertical_span <= 1 ? 0u :
                        vertical_span <= 2 ? 1u :
                        vertical_span <= 4 ? 2u : 3u;
                    draw_raised_edge_repeats(pixels, far, near, side,
                        g_roadside_repeat_mips &&
                        index < g_roadside_tile_count &&
                        g_roadside_repeat_indices[index] <
                            g_roadside_tile_count ?
                            &g_roadside_repeat_mips[index * 4u +
                                level] : strip);
                }
            }
        }
        swap = far;
        far = near;
        near = swap;
    }
}

static void draw_edge_texture_span(road_pixel_t *pixels, int y,
                                   int from, int to,
                                   const road_sprite_t *tile,
                                   unsigned int world)
{
    int x;
    unsigned int row, column;
    if (!tile || !tile->pixels || !tile->width || !tile->height)
        return;
    from = clamp_int(from, 0, ROAD_VIEW_WIDTH);
    to = clamp_int(to, 0, ROAD_VIEW_WIDTH);
    if (from >= to) return;
    row = (world / 128u) % tile->height;
    column = (unsigned int)from % tile->width;
    for (x = from; x < to; ++x) {
        road_pixel_t color = tile->pixels[row * tile->width +
            column];
        if (color != 0xf81fu)
            pixels[y * ROAD_WIDTH + x] = color;
        if (++column == tile->width) column = 0u;
    }
}

/* Integer projection can collapse distant adjacent edge quads onto one
 * scanline.  Fill only those RMTN rows with the same original rock and
 * blue outer CELs, leaving the four-point quads to draw the slope above. */
static void draw_edge_row_fallback(const road_game_t *game,
                                   road_pixel_t *pixels)
{
    int y;
    if (!g_edge_profile_count[0] || !g_edge_profile_count[1] ||
        !g_surface_samples) return;
    for (y = HORIZON + road_projection_dy(ROAD_VISIBLE_DEPTH);
         y < ROAD_VIEW_BOTTOM; ++y) {
        if ((y & 15) == 0) ROAD_RUNTIME_AUDIO_PUMP();
        unsigned int depth = road_depth_for_dy(
            (unsigned int)(y - HORIZON + 1));
        unsigned int world = game->distance + depth;
        unsigned int sample = world / 256u, side;
        int edges[2], base_y, unit;
        if (sample >= g_cross_section_count ||
            sample >= g_surface_sample_count ||
            !g_course_terrain_mode ||
            g_course_terrain_mode[sample] != 1u ||
            !roadside_screen_edges(game, world, edges,
                                   &base_y, &unit)) continue;
        for (side = 0u; side < 2u; ++side) {
            const road_edge_profile_t *profile = edge_profile_at(
                sample, side);
            unsigned int index = g_surface_samples[sample].tile_index[side];
            const road_sprite_t *rock = index < g_roadside_tile_count ?
                &g_roadside_tiles[index] : 0;
            const road_sprite_t *outer;
            int direction = side ? 1 : -1;
            int inner, edge;
            if (!profile) continue;
            inner = edges[side] + direction *
                unit * profile->inner_offset / 500;
            edge = edges[side] + direction *
                unit * (sample < g_edge_outer_sample_count &&
                    g_edge_outer_samples[side] ?
                    g_edge_outer_samples[side][sample] :
                    profile->outer_offset) / 500;
            outer = profile->height > 0 ? rock :
                g_edge_fill_primary;
            if (side) {
                draw_edge_texture_span(pixels, y, inner, edge,
                                       rock, world);
                draw_edge_texture_span(pixels, y, edge,
                                       ROAD_VIEW_WIDTH, outer, world);
            } else {
                draw_edge_texture_span(pixels, y, 0, edge,
                                       outer, world);
                draw_edge_texture_span(pixels, y, edge, inner,
                                       rock, world);
            }
        }
        (void)base_y;
    }
}

static void draw_roadside_surface_quads(const road_game_t *game,
                                        road_pixel_t *pixels,
                                        unsigned int only_sample)
{
    unsigned int far_sample, near_sample, sample;
    if (!g_roadside_tile_count || !g_roadside_tiles ||
        !g_roadside_tile_heights || !g_surface_samples ||
        !g_course_terrain_mode) return;
    if (only_sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u ||
        only_sample <= (game->distance + 384u) / 256u) return;
    far_sample = only_sample;
    near_sample = only_sample - 1u;
    for (sample = far_sample; sample > near_sample; --sample) {
        unsigned int far_index = sample;
        unsigned int near_index = sample - 1u;
        int far_edges[2], near_edges[2], far_y, near_y;
        int far_unit, near_unit;
        unsigned int side;
        if (far_index >= g_surface_sample_count ||
            near_index >= g_surface_sample_count ||
            g_course_terrain_mode[far_index] != 0u ||
            g_course_terrain_mode[near_index] != 0u ||
            !roadside_screen_edges(game, far_index * 256u,
                far_edges, &far_y, &far_unit) ||
            !roadside_screen_edges(game, near_index * 256u,
                near_edges, &near_y, &near_unit)) continue;
        for (side = 0u; side < 2u; ++side) {
            unsigned int index = g_surface_samples[near_index].
                tile_index[side];
            unsigned int far_tile = g_surface_samples[far_index].
                tile_index[side];
            const road_sprite_t *tile;
            road_quad_vertex_t quad[4];
            int far_height, near_height;
            if (index >= g_roadside_tile_count) continue;
            tile = &g_roadside_tiles[index];
            if (!tile->pixels || !tile->width || !tile->height) continue;
            if (far_tile >= g_roadside_tile_count) far_tile = index;
            far_height = far_unit *
                (int)g_roadside_tile_heights[far_tile] / 384;
            near_height = near_unit *
                (int)g_roadside_tile_heights[index] / 384;
            far_height = clamp_int(far_height, 1, ROAD_HEIGHT);
            near_height = clamp_int(near_height, 1, ROAD_HEIGHT);
            quad[0].x = far_edges[side];
            quad[0].y = far_y - far_height;
            quad[1].x = near_edges[side];
            quad[1].y = near_y - near_height;
            quad[2].x = near_edges[side];
            quad[2].y = near_y;
            quad[3].x = far_edges[side];
            quad[3].y = far_y;
            draw_textured_surface_quad(pixels, quad, tile, 31,
                rgb(74, 73, 68));
        }
    }
}

typedef struct road_strip_section {
    int left;
    int center;
    int right;
    int y;
    unsigned int left_width;
    unsigned int right_width;
    unsigned int left_cells;
    unsigned int right_cells;
    unsigned int cell_count;
    int valid;
} road_strip_section_t;

static void road_strip_screen_section(const road_game_t *game,
                                      unsigned int world, unsigned int channel,
                                      road_strip_section_t *section)
{
    unsigned int sample = world / 256u;
    unsigned int width_count = channel ? g_fork_count : g_width_count;
    const unsigned short *left_width = channel ?
        g_fork_left_width : g_course_left_width;
    const unsigned short *right_width = channel ?
        g_fork_right_width : g_course_right_width;
    int dy, unit, center, left_extent, right_extent;
    unsigned int left_cells, right_cells;
    int preview = road_fork_in_view(game) &&
        sample >= g_fork_visible_start &&
        sample < g_fork_end_sample &&
        g_fork_sample < g_width_count;
    section->valid = 0;
    if (world <= game->distance || (channel && !preview)) return;
    dy = road_projection_dy(world - game->distance);
    if (dy < 1) dy = 1;
    unit = road_projection_unit(world - game->distance);
    center = channel ? road_fork_center(game, world, dy) :
        road_center(game, world, dy);
    if (preview && !g_fork_reverse_mode) {
        unsigned int before = g_fork_sample ? g_fork_sample - 1u : 0u;
        if (!channel) {
            if (g_fork_main_channel == 0u)
                center -= unit *
                    ((int)g_course_left_width[before] -
                     (int)g_course_left_width[g_fork_sample]) / 512;
            else
                center += unit *
                    ((int)g_course_right_width[before] -
                     (int)g_course_right_width[g_fork_sample]) / 512;
        } else if (g_fork_main_channel == 0u)
            center += unit *
                ((int)g_course_right_width[before] -
                 (int)g_fork_right_width[g_fork_sample]) / 512;
        else
            center -= unit *
                ((int)g_course_left_width[before] -
                 (int)g_fork_left_width[g_fork_sample]) / 512;
    }
    section->left_width = 512u;
    section->right_width = 512u;
    if (width_count && left_width && right_width) {
        unsigned int width_sample = sample;
        if (channel && !road_fork_sample_index(sample, &width_sample))
            return;
        if (width_sample >= width_count)
            width_sample = width_count - 1u;
        section->left_width = left_width[width_sample];
        section->right_width = right_width[width_sample];
    }
    /* Keep the upstream left/right width-cell count. Until roadside
       occlusion follows the 3DO path, clip the texture mapping to the
       physical road edges so a fractional extra cell cannot paint a wall. */
    left_cells = (section->left_width + 255u) >> 8u;
    right_cells = (section->right_width + 255u) >> 8u;
    section->left_cells = left_cells;
    section->right_cells = right_cells;
    section->cell_count = left_cells + right_cells;
    left_extent = unit * (int)section->left_width / 512;
    right_extent = unit * (int)section->right_width / 512;
    if (left_extent < 2) left_extent = 2;
    if (right_extent < 2) right_extent = 2;
    section->left = center - left_extent;
    section->center = center;
    section->right = center + right_extent;
    section->y = channel ?
        road_fork_project_y(game, world, HORIZON + dy) :
        road_project_y(game, world, HORIZON + dy);
    section->valid = 1;
}

static int road_strip_screen_x(const road_strip_section_t *section,
                               unsigned int strip)
{
    if (strip >= section->cell_count) return section->right;
    if (strip <= section->left_cells) {
        if (!section->left_cells) return section->center;
        return section->left +
            (section->center - section->left) * (int)strip /
            (int)section->left_cells;
    }
    if (!section->right_cells) return section->center;
    return section->center +
        (section->right - section->center) *
        ((int)strip - (int)section->left_cells) /
        (int)section->right_cells;
}

static void draw_road_surface_band(road_pixel_t *pixels,
                                   const road_strip_section_t *far,
                                   const road_strip_section_t *near,
                                   unsigned int source_row,
                                   unsigned int band_count)
{
    unsigned int count, far_count, strip, style;
    int first_far_x, height;
    if (!far->valid || !near->valid) return;
    count = near->cell_count;
    far_count = far->cell_count;
    if (!count || !far_count || count > 32u || far_count > 32u)
        return;
    first_far_x = road_strip_screen_x(far, 1u);
    height = near->y - far->y;
    if (band_count == 4u) {
        height *= 4;
        if (height <= 4 && height >= -4)
            height = height < 0 ? -5 : 5;
    }
    style = road_texture_style_for_span(height,
        first_far_x - far->left);
    for (strip = 0u; strip < count; ++strip) {
        unsigned int variant, tile_index;
        road_quad_vertex_t quad[4];
        variant = road_texture_variant(near->left_width,
            near->right_width, strip);
        tile_index = variant * 9u + style;
        quad[0].x = road_strip_screen_x(far, strip);
        quad[0].y = far->y;
        quad[1].x = road_strip_screen_x(far, strip + 1u);
        quad[1].y = far->y;
        quad[2].x = road_strip_screen_x(near, strip + 1u);
        quad[2].y = near->y;
        quad[3].x = road_strip_screen_x(near, strip);
        quad[3].y = near->y;
        draw_textured_surface_quad_rows(pixels, quad,
            &g_surface_textures[tile_index], 31, rgb(78, 75, 69),
            source_row, source_row + 4u / band_count);
    }
}

static void draw_road_surface_quad_sample(const road_game_t *game,
    road_pixel_t *pixels, unsigned int sample)
{
    road_strip_section_t sections[2][2];
    road_strip_section_t *far[2] = {&sections[0][0], &sections[1][0]};
    road_strip_section_t *near[2] = {&sections[0][1], &sections[1][1]};
    unsigned int far_sample, near_sample, channel, pass;
    if (g_surface_texture_count < 27u || !g_surface_textures) return;
    far_sample = (game->distance + ROAD_VISIBLE_DEPTH) / 256u;
    near_sample = (game->distance + 210u) / 256u;
    if (sample > far_sample || sample <= near_sample) return;
    for (channel = 0u; channel < 2u; ++channel)
        road_strip_screen_section(game, sample * 256u,
            channel, far[channel]);
    /* The 3DO draws the other side first and the rider's selected side
     * last. Channel 0 is this renderer's selected route. */
    for (pass = 0u; pass < 2u; ++pass) {
        unsigned int band_count = (sample - 1u) * 256u <
            game->distance + 0x2d2u ? 4u : 1u;
        unsigned int band;
        channel = 1u - pass;
        for (band = 0u; band < band_count; ++band) {
            unsigned int world = sample * 256u -
                (band + 1u) * (256u / band_count);
            road_strip_section_t *swap;
            if (world < game->distance + 210u) break;
            road_strip_screen_section(game, world,
                channel, near[channel]);
            if (road_surface_sample_visible(sample - 1u, channel))
                draw_road_surface_band(pixels, far[channel],
                    near[channel], band_count == 4u ? band : 0u,
                    band_count);
            swap = far[channel];
            far[channel] = near[channel];
            near[channel] = swap;
        }
    }
}

/* The 3DO's profile-mode dual-lane node submits a solid center quad after
 * both road surfaces. When the neighbor's left_step exceeds 0x18, it instead
 * submits a dark bridge quad and two pale edge quads. Colors here are the
 * source RGB555 values converted to RGB565. The endpoints remain adapted
 * road-edge projections; transition-specific joins need original geometry. */
static int fork_profile_center_has_clearance(unsigned int sample)
{
    if (g_fork_reverse_mode) {
        if (sample < g_fork_reverse_step_start ||
            sample - g_fork_reverse_step_start >=
                g_fork_reverse_step_count) return 0;
        return g_fork_reverse_step_start +
            g_fork_reverse_step_count - 1u - sample >
            g_fork_reverse_clearance_offset;
    }
    return sample >= g_fork_sample &&
        sample - g_fork_sample < g_fork_forward_step_count &&
        sample - g_fork_sample >= g_fork_forward_clearance_index;
}

static void draw_fork_profile_center_band(const road_game_t *game,
    road_pixel_t *pixels, unsigned int far_world,
    unsigned int near_world)
{
    road_hill_section_t main_far, main_near, fork_far, fork_near;
    const road_hill_section_t *left_far, *left_near;
    const road_hill_section_t *right_far, *right_near;
    road_quad_vertex_t quad[4];
    unsigned int tile_index;
    const road_sprite_t *tile;
    hill_screen_section(game, far_world, 0u, &main_far);
    hill_screen_section(game, near_world, 0u, &main_near);
    hill_screen_section(game, far_world, 1u, &fork_far);
    hill_screen_section(game, near_world, 1u, &fork_near);
    if (!main_far.valid || !main_near.valid || !fork_far.valid ||
        !fork_near.valid) return;
    left_far = g_fork_main_channel ? &fork_far : &main_far;
    left_near = g_fork_main_channel ? &fork_near : &main_near;
    right_far = g_fork_main_channel ? &main_far : &fork_far;
    right_near = g_fork_main_channel ? &main_near : &fork_near;
    if (left_far->paired_profile && left_near->paired_profile &&
        left_far->profile_count[1] <= 3u &&
        left_near->profile_count[1] <= 3u) return;
    if (left_far->x[1][3] >= right_far->x[0][3] &&
        left_near->x[1][3] >= right_near->x[0][3]) return;
    quad[0].x = left_far->x[1][3];
    quad[0].y = left_far->y[1][3];
    quad[1].x = right_far->x[0][3];
    quad[1].y = right_far->y[0][3];
    quad[2].x = right_near->x[0][3];
    quad[2].y = right_near->y[0][3];
    quad[3].x = left_near->x[1][3];
    quad[3].y = left_near->y[1][3];
    tile_index = left_near->tile[1][2];
    tile = select_hill_tile_for_quad(tile_index, quad);
    draw_textured_surface_quad(pixels, quad, tile,
        left_near->shade[1][2], rgb(43, 119, 49));
}

static void draw_fork_profile_center_sample(const road_game_t *game,
    road_pixel_t *pixels, unsigned int sample)
{
    road_strip_section_t far_main, near_main, far_fork, near_fork;
    road_quad_vertex_t quad[4];
    unsigned int mapped;
    unsigned int near_world;
    int left_far, right_far, left_near, right_near;
    int y_left_far, y_right_far, y_left_near, y_right_near;
    if (!sample || !road_fork_in_view(game) ||
        sample <= (game->distance + 210u) / 256u ||
        sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u ||
        !g_course_terrain_mode || !g_margin_terrain_mode[1] ||
        !road_fork_sample_index(sample - 1u, &mapped) ||
        sample - 1u >= g_cross_section_count ||
        mapped >= g_margin_sample_count[1] ||
        g_course_terrain_mode[sample - 1u] != 4u ||
        g_margin_terrain_mode[1][mapped] != 4u) return;
    near_world = (sample - 1u) * 256u;
    /* The source clips a center quad whose near node is inside 0xd2 to
     * the 0xd2 projection plane. Reprojecting the same road edges at that
     * depth preserves the continuous band in this software renderer. */
    if (near_world < game->distance + 0xd2u)
        near_world = game->distance + 0xd2u;
    if (fork_profile_center_has_clearance(sample)) {
        draw_fork_profile_center_band(game, pixels, sample * 256u,
            near_world);
        return;
    }
    road_strip_screen_section(game, sample * 256u, 0u, &far_main);
    road_strip_screen_section(game, near_world, 0u,
        &near_main);
    road_strip_screen_section(game, sample * 256u, 1u, &far_fork);
    road_strip_screen_section(game, near_world, 1u,
        &near_fork);
    if (!far_main.valid || !near_main.valid || !far_fork.valid ||
        !near_fork.valid) return;
    if (g_fork_main_channel == 0u) {
        left_far = far_main.right;
        right_far = far_fork.left;
        left_near = near_main.right;
        right_near = near_fork.left;
        y_left_far = far_main.y;
        y_right_far = far_fork.y;
        y_left_near = near_main.y;
        y_right_near = near_fork.y;
    } else {
        left_far = far_fork.right;
        right_far = far_main.left;
        left_near = near_fork.right;
        right_near = near_main.left;
        y_left_far = far_fork.y;
        y_right_far = far_main.y;
        y_left_near = near_fork.y;
        y_right_near = near_main.y;
    }
    if (left_far >= right_far && left_near >= right_near) return;
    quad[0].x = left_far;
    quad[0].y = y_left_far;
    quad[1].x = right_far;
    quad[1].y = y_right_far;
    quad[2].x = right_near;
    quad[2].y = y_right_near;
    quad[3].x = left_near;
    quad[3].y = y_left_near;
    if ((g_fork_reverse_mode ?
        sample >= g_fork_reverse_step_start &&
        sample - g_fork_reverse_step_start <
            g_fork_reverse_step_count &&
        g_fork_reverse_left_step[
            sample - g_fork_reverse_step_start] > 0x18 :
        sample >= g_fork_sample &&
        sample - g_fork_sample < g_fork_forward_step_count &&
        g_fork_forward_left_step[sample - g_fork_sample] > 0x18)) {
        road_quad_vertex_t left_quad[4], right_quad[4], bridge_quad[4];
        int far_width = road_projection_unit(
            sample * 256u - game->distance) * 12 / 500;
        int near_width = road_projection_unit(
            near_world - game->distance) * 12 / 500;
        if (far_width < 1) far_width = 1;
        if (near_width < 1) near_width = 1;
        if (left_far + far_width < right_far - far_width &&
            left_near + near_width < right_near - near_width) {
            left_quad[0] = quad[0];
            left_quad[1].x = left_far + far_width;
            left_quad[1].y = y_left_far;
            left_quad[2].x = left_near + near_width;
            left_quad[2].y = y_left_near;
            left_quad[3] = quad[3];
            right_quad[0].x = right_far - far_width;
            right_quad[0].y = y_right_far;
            right_quad[1] = quad[1];
            right_quad[2] = quad[2];
            right_quad[3].x = right_near - near_width;
            right_quad[3].y = y_right_near;
            bridge_quad[0] = left_quad[1];
            bridge_quad[1] = right_quad[0];
            bridge_quad[2] = right_quad[3];
            bridge_quad[3] = left_quad[2];
            /* Upstream draws the dark bridge before its two pale rims. */
            draw_textured_surface_quad(pixels, bridge_quad,
                0, 31, 0x3188u);
            draw_textured_surface_quad(pixels, left_quad,
                0, 31, 0xa534u);
            draw_textured_surface_quad(pixels, right_quad,
                0, 31, 0xa534u);
            return;
        }
    }
    draw_textured_surface_quad(pixels, quad, 0, 31, 0xa534u);
}

static int road_margin_sample_index(unsigned int world,
                                     unsigned int channel,
                                     unsigned int *index)
{
    unsigned int sample = world / 256u;
    if (channel && !road_fork_sample_index(sample, &sample))
        return 0;
    if (sample >= g_margin_sample_count[channel]) return 0;
    *index = sample;
    return 1;
}

static int road_margin_outer_x(const road_game_t *game,
    unsigned int world, unsigned int channel, unsigned int side,
    const road_strip_section_t *section, int *outer)
{
    unsigned int index;
    const unsigned char *margins = channel ?
        (side ? g_fork_right_margin : g_fork_left_margin) :
        (side ? g_course_right_margin : g_course_left_margin);
    int width, flatten;
    if (!section->valid || !margins ||
        !road_margin_sample_index(world, channel, &index)) return 0;
    width = road_projection_unit(world - game->distance) *
        fork_transition_inner_margin(world / 256u, index, channel,
            side, (int)margins[index], &flatten) / 512;
    if (width <= 0) return 0;
    *outer = side ? section->right + width : section->left - width;
    return 1;
}

static void draw_fork_roadside_surface_quad_sample(
    const road_game_t *game, road_pixel_t *pixels,
    unsigned int sample)
{
    road_strip_section_t far, near;
    unsigned int far_index, near_index, side;
    if (!g_fork_surface_samples || !g_fork_surface_sample_count ||
        !g_margin_terrain_mode[1] || !g_roadside_tiles ||
        !g_roadside_tile_heights || !g_roadside_tile_count ||
        sample <= (game->distance + 384u) / 256u ||
        sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u ||
        !sample ||
        !road_fork_sample_index(sample, &far_index) ||
        !road_fork_sample_index(sample - 1u, &near_index) ||
        far_index >= g_fork_surface_sample_count ||
        near_index >= g_fork_surface_sample_count ||
        g_margin_terrain_mode[1][far_index] != 0u ||
        g_margin_terrain_mode[1][near_index] != 0u ||
        !road_surface_sample_visible(sample - 1u, 1u)) return;
    road_strip_screen_section(game, sample * 256u, 1u, &far);
    road_strip_screen_section(game, (sample - 1u) * 256u,
                              1u, &near);
    if (!far.valid || !near.valid) return;
    for (side = 0u; side < 2u; ++side) {
        unsigned int index = g_fork_surface_samples[near_index].
            tile_index[side];
        unsigned int far_tile = g_fork_surface_samples[far_index].
            tile_index[side];
        const road_sprite_t *tile;
        road_quad_vertex_t quad[4];
        int far_height, near_height, far_outer, near_outer;
        if (index >= g_roadside_tile_count ||
            !road_margin_outer_x(game, sample * 256u, 1u, side,
                &far, &far_outer) ||
            !road_margin_outer_x(game, (sample - 1u) * 256u,
                1u, side, &near, &near_outer)) continue;
        tile = &g_roadside_tiles[index];
        if (!tile->pixels || !tile->width || !tile->height) continue;
        if (far_tile >= g_roadside_tile_count) far_tile = index;
        far_height = road_projection_unit(sample * 256u -
            game->distance) * (int)g_roadside_tile_heights[far_tile] /
            384;
        near_height = road_projection_unit((sample - 1u) * 256u -
            game->distance) * (int)g_roadside_tile_heights[index] /
            384;
        far_height = clamp_int(far_height, 1, ROAD_HEIGHT);
        near_height = clamp_int(near_height, 1, ROAD_HEIGHT);
        quad[0].x = quad[3].x = far_outer;
        quad[1].x = quad[2].x = near_outer;
        quad[0].y = far.y - far_height;
        quad[1].y = near.y - near_height;
        quad[2].y = near.y;
        quad[3].y = far.y;
        draw_textured_surface_quad(pixels, quad, tile, 31,
            rgb(74, 73, 68));
    }
}

static void draw_margin_quad_sample(const road_game_t *game,
    road_pixel_t *pixels, unsigned int sample)
{
    unsigned int channel, side;
    if (!g_margin_tiles || !g_margin_tile_count || !sample ||
        sample <= (game->distance + 210u) / 256u ||
        sample > (game->distance + ROAD_VISIBLE_DEPTH) / 256u)
        return;
    for (channel = 1u; ; --channel) {
        road_strip_section_t far, near;
        unsigned int index;
        road_strip_screen_section(game, sample * 256u, channel, &far);
        road_strip_screen_section(game, (sample - 1u) * 256u,
                                  channel, &near);
        if (far.valid && near.valid &&
            road_surface_sample_visible(sample - 1u, channel) &&
            road_margin_sample_index((sample - 1u) * 256u,
                                     channel, &index) &&
            g_margin_terrain_mode[channel][index] == 0u &&
            (g_margin_surface_flags[channel][index] & 0x18u)) {
            for (side = 0u; side < 2u; ++side) {
                unsigned short family;
                unsigned char selector;
                const road_margin_tile_t *tile = 0;
                unsigned int key, repeat, repeat_count;
                unsigned int level = 0u;
                unsigned int near_world = (sample - 1u) * 256u;
                unsigned int far_world = sample * 256u;
                const unsigned char *margin_data = channel ?
                    (side ? g_fork_right_margin : g_fork_left_margin) :
                    (side ? g_course_right_margin : g_course_left_margin);
                const unsigned char *depth_data = channel ?
                    (side ? g_fork_right_depth : g_fork_left_depth) :
                    (side ? g_course_right_depth : g_course_left_depth);
                unsigned int depth_count = channel ?
                    g_fork_depth_count : g_cross_section_count;
                unsigned int margin, attachment_world;
                int far_outer, near_outer, attachment_outer;
                int far_span, near_span, attachment_span;
                int direction = side ? 1 : -1;
                int height, depth_offset = 0;
                road_strip_section_t attachment;
                road_quad_vertex_t quad[4], fill_quad[4];
                if (!g_margin_family[channel][side] ||
                    !g_margin_selector[channel][side] ||
                    !margin_data ||
                    !road_margin_outer_x(game, sample * 256u,
                        channel, side, &far, &far_outer) ||
                    !road_margin_outer_x(game, (sample - 1u) * 256u,
                        channel, side, &near, &near_outer)) continue;
                family = g_margin_family[channel][side][index];
                selector = g_margin_selector[channel][side][index];
                for (key = 0u; key < g_margin_tile_count; ++key)
                    if (g_margin_tiles[key].family_id == family &&
                        g_margin_tiles[key].selector == selector) {
                        tile = &g_margin_tiles[key];
                        break;
                    }
                if (!tile) continue;
                margin = margin_data[index];
                attachment_world = near_world +
                    ((g_margin_surface_flags[channel][index] & 0x08u) ?
                        256u - margin : margin);
                road_strip_screen_section(game, attachment_world,
                    channel, &attachment);
                if (!attachment.valid ||
                    !road_margin_outer_x(game, attachment_world,
                        channel, side, &attachment,
                        &attachment_outer)) continue;
                far_span = road_project_world_delta(256,
                    far_world - game->distance);
                near_span = road_project_world_delta(256,
                    near_world - game->distance);
                attachment_span = road_project_world_delta(256,
                    attachment_world - game->distance);
                if (far_span < 1) far_span = 1;
                if (near_span < 1) near_span = 1;
                if (attachment_span < 1) attachment_span = 1;
                if (g_margin_surface_flags[channel][index] & 0x08u) {
                    /* REVERSED_MARGIN is one primary CEL between the far
                       shoulder and the 8.8 attachment point. */
                    quad[0].x = far_outer +
                        (side ? 0 : -far_span);
                    quad[1].x = quad[0].x + far_span;
                    quad[2].x = attachment_outer +
                        (side ? attachment_span : 0);
                    quad[3].x = quad[2].x - attachment_span;
                    quad[0].y = quad[1].y = far.y;
                    quad[2].y = quad[3].y = attachment.y;
                    height = attachment.y - far.y;
                    if (height < 0) height = -height;
                    while (level < 7u && (1u << level) <
                        (unsigned int)height) ++level;
                    if (tile->primary_mips[level].pixels)
                        draw_textured_surface_quad(pixels, quad,
                            &tile->primary_mips[level], 31,
                            rgb(78, 75, 69));
                    continue;
                }
                /* DEPTH_ADJUSTED repeats child 2 beside the road; child 3
                   independently fills the vertical lane-edge offset. */
                height = near.y - attachment.y;
                if (height < 0) height = -height;
                while (level < 7u && (1u << level) <
                    (unsigned int)height) ++level;
                if (depth_data && index < depth_count)
                    depth_offset = road_project_world_delta(
                        depth_data[index],
                        near_world - game->distance);
                repeat_count = sample - 1u <=
                    g_margin_repeat_threshold ? 16u : 8u;
                for (repeat = 0u; repeat < repeat_count; ++repeat) {
                    int offset_near = direction * near_span * (int)repeat;
                    int offset_attachment = direction *
                        attachment_span * (int)repeat;
                    quad[0].x = attachment_outer +
                        (side ? 0 : -attachment_span) +
                        offset_attachment;
                    quad[1].x = quad[0].x + attachment_span;
                    quad[2].x = near_outer +
                        (side ? near_span : 0) + offset_near;
                    quad[3].x = quad[2].x - near_span;
                    quad[0].y = quad[1].y = attachment.y;
                    quad[2].y = quad[3].y = near.y;
                    if ((!side && quad[1].x <= 0 && quad[2].x <= 0) ||
                        (side && quad[0].x >= ROAD_WIDTH &&
                         quad[3].x >= ROAD_WIDTH)) break;
                    if (depth_offset > 0 &&
                        tile->fill_mips[level].pixels) {
                        fill_quad[0].x = fill_quad[3].x =
                            near_outer + (side ? 0 : -near_span) +
                            offset_near;
                        fill_quad[1].x = fill_quad[2].x =
                            fill_quad[0].x + near_span;
                        fill_quad[0].y = fill_quad[1].y =
                            near.y - depth_offset;
                        fill_quad[2].y = fill_quad[3].y = near.y;
                        draw_textured_surface_quad(pixels, fill_quad,
                            &tile->fill_mips[level], 31,
                            rgb(78, 75, 69));
                    }
                    if (tile->primary_mips[level].pixels)
                        draw_textured_surface_quad(pixels, quad,
                            &tile->primary_mips[level], 31,
                            rgb(78, 75, 69));
                }
            }
        }
        if (channel == 0u) break;
    }
}

static void draw_roadside_surface_row(road_pixel_t *pixels, int y,
                                      unsigned int sample,
                                      unsigned int world,
                                      int left, int right,
                                      int left_margin, int right_margin,
                                      int unit)
{
    const road_surface_sample_t *surface;
    unsigned int side;
    if (sample >= g_surface_sample_count ||
        !g_course_terrain_mode ||
        g_course_terrain_mode[sample] != 0u) return;
    surface = &g_surface_samples[sample];
    for (side = 0u; side < 2u; ++side) {
        unsigned int index = surface->tile_index[side];
        const road_sprite_t *tile;
        int edge, top, height, thickness, column_y;
        unsigned int column;
        if (index >= g_roadside_tile_count) continue;
        tile = &g_roadside_tiles[index];
        if (!tile->pixels || !tile->width || !tile->height) continue;
        edge = side ? right + right_margin : left - left_margin;
        height = unit * (int)g_roadside_tile_heights[index] / 384;
        if (height < 1) height = 1;
        if (height > ROAD_HEIGHT) height = ROAD_HEIGHT;
        top = y - height;
        thickness = 1 + unit / 48;
        if (thickness > 4) thickness = 4;
        column = (world / 64u) % tile->width;
        for (column_y = top < 0 ? 0 : top;
             column_y <= y; ++column_y) {
            unsigned int row = (unsigned int)
                ((column_y - top) * (int)tile->height / height);
            road_pixel_t color;
            int x;
            if (row >= tile->height) row = tile->height - 1u;
            color = tile->pixels[row * tile->width + column];
            if (color == 0xf81fu) continue;
            for (x = side ? edge : edge - thickness;
                 x < (side ? edge + thickness : edge); ++x)
                if (x >= 0 && x < ROAD_WIDTH)
                    pixels[column_y * ROAD_WIDTH + x] = color;
        }
    }
}

static void draw_road_row_geometry(road_pixel_t *pixels, int y, int dy,
                                   unsigned int world, int center, int unit,
                                   const unsigned short *width_left,
                                   const unsigned short *width_right,
                                   unsigned int width_count,
                                   const unsigned char *margin_left,
                                   const unsigned char *margin_right,
                                   unsigned int margin_count,
                                   int clear_background, int draw_surface)
{
    int left_extent = unit;
    int right_extent = unit;
    int left_margin = 2 + dy / 20;
    int right_margin = left_margin;
    unsigned int sampled_left_width = 512u;
    unsigned int sampled_right_width = 512u;
    int left, right;
    road_pixel_t grass, asphalt, curb;
    unsigned int sample = world / 256u;
    if (width_count) {
        unsigned int width_sample = sample;
        if (width_sample >= width_count)
            width_sample = width_count - 1u;
        sampled_left_width = width_left[width_sample];
        sampled_right_width = width_right[width_sample];
        left_extent = unit * (int)sampled_left_width / 512;
        right_extent = unit * (int)sampled_right_width / 512;
        if (left_extent < 2) left_extent = 2;
        if (right_extent < 2) right_extent = 2;
    }
    if (sample < margin_count) {
        left_margin = unit * (int)margin_left[sample] / 512;
        right_margin = unit * (int)margin_right[sample] / 512;
        if (left_margin < 1) left_margin = 1;
        if (right_margin < 1) right_margin = 1;
    }
    left = center - left_extent;
    right = center + right_extent;
    grass = ((world / 950u) & 1u) ?
        rgb(32, 111, 48) : rgb(39, 123, 50);
    asphalt = ((world / 160u) & 1u) ?
        rgb(76, 79, 82) : rgb(82, 84, 86);
    curb = ((world / 350u) & 1u) ?
        rgb(225, 210, 178) : rgb(196, 52, 43);
    /* RHIL quads follow this pass; the former row fill left a striped wall
     * visible wherever the projected slope did not cover it. */
    if (clear_background) {
        /* The RHIL profile quads define the ground silhouette.  Filling
         * the whole scanline with synthetic grass covers the original
         * background between the distant profile and the road. */
        if (!g_course_terrain_mode || sample >= g_cross_section_count ||
            (g_course_terrain_mode[sample] != 4u &&
             !(g_course_terrain_mode[sample] == 1u &&
               g_edge_profile_count[0] && g_edge_profile_count[1])))
            span(pixels, y, 0, ROAD_WIDTH, grass);
        draw_roadside_surface_row(pixels, y, sample, world, left,
                                  right, left_margin, right_margin, unit);
    }
    if (!draw_surface) return;
    if (g_surface_texture_count >= 27u) {
        span(pixels, y, left - left_margin, right + right_margin,
             rgb(78, 75, 69));
        draw_road_texture_row(pixels, y, left, right, dy, unit, world,
                              sampled_left_width, sampled_right_width);
    } else {
        span(pixels, y, left - left_margin, right + right_margin, curb);
        span(pixels, y, left, right, asphalt);
        if ((world / 520u) & 1u) {
            int stripe = 1 + dy / 50;
            span(pixels, y, center - stripe, center + stripe,
                rgb(226, 208, 154));
        }
    }
}

static void draw_road_row(const road_game_t *game, road_pixel_t *pixels,
                          int y, int dy, unsigned int world,
                          int clear_background)
{
    int main_center = road_center(game, world, dy);
    int preview = road_fork_in_view(game) && !g_fork_reverse_mode &&
        world / 256u >= g_fork_visible_start &&
        world / 256u < g_fork_end_sample &&
        g_fork_sample < g_width_count;
    if (preview) {
        int unit = road_projection_unit(world - game->distance);
        unsigned int before = g_fork_sample ? g_fork_sample - 1u : 0u;
        if (g_fork_main_channel == 0u)
            main_center -= unit *
                ((int)g_course_left_width[before] -
                 (int)g_course_left_width[g_fork_sample]) / 512;
        else
            main_center += unit *
                ((int)g_course_right_width[before] -
                 (int)g_course_right_width[g_fork_sample]) / 512;
    }
    draw_road_row_geometry(pixels, y, dy, world,
        main_center, road_projection_unit(world - game->distance),
        g_course_left_width,
        g_course_right_width, g_width_count, g_course_left_margin,
        g_course_right_margin, g_cross_section_count,
        clear_background, road_surface_sample_visible(world / 256u, 0u));
}

static void draw_fork_row(const road_game_t *game, road_pixel_t *pixels,
                          int y, int dy, unsigned int world)
{
    int unit = road_projection_unit(world - game->distance);
    unsigned int before = g_fork_sample ? g_fork_sample - 1u : 0u;
    int center = road_fork_center(game, world, dy);
    if (!g_fork_reverse_mode) {
        if (g_fork_main_channel == 0u)
            center += unit *
                ((int)g_course_right_width[before] -
                 (int)g_fork_right_width[g_fork_sample]) / 512;
        else
            center -= unit *
                ((int)g_course_left_width[before] -
                 (int)g_fork_left_width[g_fork_sample]) / 512;
    }
    draw_road_row_geometry(pixels, y, dy,
        (unsigned int)((int)world + g_fork_sample_shift * 256), center,
        unit,
        g_fork_left_width, g_fork_right_width, g_fork_count,
        g_fork_left_margin, g_fork_right_margin,
        g_fork_slope_count, 0,
        road_surface_sample_visible(world / 256u, 1u));
}

static void draw_fork_rows(const road_game_t *game, road_pixel_t *pixels)
{
    int y, previous_projected = HORIZON - 1;
    if (!road_fork_in_view(game) || !g_width_count) return;
    for (y = HORIZON; y < ROAD_VIEW_BOTTOM; ++y) {
        if ((y & 15) == 0) ROAD_RUNTIME_AUDIO_PUMP();
        int dy = y - HORIZON + 1;
        unsigned int depth = road_depth_for_dy((unsigned int)dy);
        unsigned int world;
        int projected, from, to, row;
        if (depth > ROAD_VISIBLE_DEPTH) depth = ROAD_VISIBLE_DEPTH;
        world = game->distance + depth;
        if (world / 256u >= g_fork_end_sample) continue;
        if (world / 256u < g_fork_visible_start) break;
        projected = road_fork_project_y(game, world, y);
        if (previous_projected == HORIZON - 1 && y > HORIZON)
            previous_projected = projected - 1;
        from = projected > previous_projected ?
            previous_projected + 1 : projected;
        to = projected > previous_projected ?
            projected : previous_projected;
        for (row = clamp_int(from, 0, ROAD_VIEW_BOTTOM);
             row <= to && row < ROAD_VIEW_BOTTOM; ++row)
            draw_fork_row(game, pixels, row, dy, world);
        previous_projected = projected;
    }
}

static void draw_road_rows(const road_game_t *game,
                           road_pixel_t *pixels, int clear_background)
{
    int y;
    int previous_projected = HORIZON - 1;
    unsigned int drawn_rows = 0u;
    unsigned int last_world = game->distance +
        road_depth_for_dy(ROAD_VIEW_BOTTOM - HORIZON);
    ROAD_RUNTIME_TRACE("ROAD_ROWS_START", game->distance,
        (unsigned int)clear_background, g_surface_texture_count);
    for (y = HORIZON; y < ROAD_VIEW_BOTTOM; ++y) {
        if ((y & 15) == 0) ROAD_RUNTIME_AUDIO_PUMP();
        int dy = y - HORIZON + 1;
        unsigned int depth = road_depth_for_dy((unsigned int)dy);
        unsigned int world;
        if (depth > ROAD_VISIBLE_DEPTH) depth = ROAD_VISIBLE_DEPTH;
        world = game->distance + depth;
        int projected = road_project_y(game, world, y);
        int from = projected > previous_projected ?
            previous_projected + 1 : projected;
        int to = projected > previous_projected ?
            projected : previous_projected;
        int row;
        for (row = clamp_int(from, 0, ROAD_VIEW_BOTTOM);
             row <= to && row < ROAD_VIEW_BOTTOM; ++row) {
            draw_road_row(game, pixels, row, dy, world,
                clear_background);
            ++drawn_rows;
        }
        previous_projected = projected;
        last_world = world;
    }
    ROAD_RUNTIME_TRACE("ROAD_ROWS_SCAN_DONE", game->distance,
        (unsigned int)previous_projected, last_world);
    for (y = clamp_int(previous_projected + 1, 0, ROAD_VIEW_BOTTOM);
         y < ROAD_VIEW_BOTTOM; ++y) {
        if ((y & 15) == 0) ROAD_RUNTIME_AUDIO_PUMP();
        draw_road_row(game, pixels, y, ROAD_VIEW_BOTTOM - HORIZON,
            last_world, clear_background);
        ++drawn_rows;
    }
    ROAD_RUNTIME_TRACE("ROAD_ROWS_DONE", game->distance,
        (unsigned int)clear_background, drawn_rows);
    (void)drawn_rows;
}

static unsigned int road_effect_frame_duration(const road_effect_actor_t *actor,
                                               unsigned int frame,
                                               unsigned int fallback_ms)
{
    if (g_effect_frame_ticks &&
        actor->sprite_index < g_effect_tick_style_count) {
        unsigned int ticks = g_effect_frame_ticks[
            actor->sprite_index * ROAD_EFFECT_SPRITE_FRAMES + frame];
        if (ticks) return (ticks * 1000u + 30u) / 60u;
    }
    return fallback_ms;
}

static unsigned int road_effect_render_frame(const road_effect_actor_t *actor)
{
    unsigned int first, count, fallback_ms, phase, total = 0u, i;
    int loop = 1;
    switch (actor->animation) {
    case ROAD_EFFECT_ANIM_MOTION:
        first = ROAD_EFFECT_MOTION_FIRST;
        count = 6u;
        fallback_ms = 100u;
        phase = actor->state_ms;
        break;
    case ROAD_EFFECT_ANIM_SHAKE:
        first = ROAD_EFFECT_SHAKE_FIRST;
        count = 2u;
        fallback_ms = 120u;
        phase = actor->state_ms;
        loop = 0;
        break;
    case ROAD_EFFECT_ANIM_FALL:
        first = ROAD_EFFECT_FALL_FIRST;
        count = 6u;
        fallback_ms = 70u;
        phase = actor->state_ms;
        loop = 0;
        break;
    case ROAD_EFFECT_ANIM_ATTACK:
        first = ROAD_EFFECT_ATTACK_FIRST;
        count = 2u;
        fallback_ms = 120u;
        phase = actor->state_ms;
        loop = 0;
        break;
    case ROAD_EFFECT_ANIM_FAST:
        first = ROAD_EFFECT_FAST_FIRST;
        count = 6u;
        fallback_ms = 90u;
        phase = actor->state_ms;
        break;
    case ROAD_EFFECT_ANIM_FLAG:
        first = ROAD_EFFECT_FLAG_FIRST;
        count = 6u;
        fallback_ms = 140u;
        phase = actor->state_ms;
        break;
    default:
        return ROAD_EFFECT_STANDING_FRAME;
    }
    for (i = 0u; i < count; ++i)
        total += road_effect_frame_duration(actor, first + i,
                                            fallback_ms);
    if (loop && total) phase %= total;
    for (i = 0u; i < count; ++i) {
        unsigned int duration = road_effect_frame_duration(actor,
            first + i, fallback_ms);
        if (phase < duration) return first + i;
        phase -= duration;
    }
    return first + count - 1u;
}

/* The 3DO renderer starts at the next track-node boundary (256 minus the
 * player's fractional sample position), then clips the background to the
 * minimum sort depth of 33 projected nodes. Recessed EDGE terrain also caps
 * the background at scanline 94 before drawing its horizon bands. */
static int road_background_clip_bottom(const road_game_t *game)
{
    int bottom = g_projected_horizon_bottom;
    if (g_course_terrain_mode && g_edge_profile_count[0] &&
        g_edge_profile_count[1]) {
        unsigned int sample = (game->distance + ROAD_VISIBLE_DEPTH) / 256u;
        if (sample < g_cross_section_count &&
            g_course_terrain_mode[sample] == 1u) {
            const road_edge_profile_t *left = edge_profile_at(sample, 0u);
            const road_edge_profile_t *right = edge_profile_at(sample, 1u);
            if (((left && left->height <= 0) ||
                 (right && right->height <= 0)) && bottom > 94)
                bottom = 94;
        }
    }
    return clamp_int(bottom, 0, ROAD_VIEW_BOTTOM);
}

/* advance_track_simulation_tick accumulates curvature as signed 16.16
 * phase. The render worker then keeps one quarter plus one sixteenth before
 * render_road_background_layers takes the integer pixel offset. Preserve
 * both truncation points and the final arithmetic floor for left bends. */
static int road_background_scroll_pixels(int heading_8_8)
{
    unsigned int raw = (unsigned int)((unsigned long long)
        (long long)heading_8_8 << 8u);
    int phase = raw <= 0x7fffffffu ? (int)raw :
        (int)((long long)raw - 0x100000000ll);
    int quarter = phase / 4;
    int sixteenth = quarter / 4;
    int reduced = quarter + sixteenth;
    if (reduced < 0)
        return (int)(-((-(long long)reduced + 65535ll) / 65536ll));
    return reduced / 65536;
}

typedef struct road_dynamic_draw_entry {
    unsigned int distance;
    unsigned char kind;
    unsigned char index;
} road_dynamic_draw_entry_t;

enum road_dynamic_draw_kind {
    ROAD_DRAW_CAR = 0,
    ROAD_DRAW_OPPONENT = 1,
    ROAD_DRAW_EFFECT = 2
};

/* The 3DO scene walks projected nodes from far to near. Preserve that
 * ordering across all live dynamic pools, instead of painting each pool
 * wholesale over the previous one. Same-depth entries retain pool order. */
static unsigned int road_collect_dynamic_draws(const road_game_t *game,
    road_dynamic_draw_entry_t *entries)
{
    unsigned int count = 0u, i;
    if (g_car_count || g_car_animation_count)
        for (i = 0u; i < ROAD_TRAFFIC_COUNT + ROAD_CROSSING_COUNT; ++i) {
            const road_rider_t *car = i < ROAD_TRAFFIC_COUNT ?
                &game->traffic[i] : &game->crossing[i - ROAD_TRAFFIC_COUNT];
            if (car->distance > game->distance &&
                car->distance - game->distance <= ROAD_VISIBLE_DEPTH) {
                entries[count].distance = car->distance;
                entries[count].kind = ROAD_DRAW_CAR;
                entries[count++].index = (unsigned char)i;
            }
        }
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i) {
        const road_rider_t *opponent = &game->opponents[i];
        if (opponent->route_visible &&
            opponent->distance > game->distance &&
            opponent->distance - game->distance <= ROAD_VISIBLE_DEPTH) {
            entries[count].distance = opponent->distance;
            entries[count].kind = ROAD_DRAW_OPPONENT;
            entries[count++].index = (unsigned char)i;
        }
    }
    for (i = 0u; i < ROAD_EFFECT_POOL_COUNT; ++i) {
        const road_effect_actor_t *actor = &game->effects[i];
        if (actor->active && actor->distance > game->distance &&
            actor->distance - game->distance <= ROAD_VISIBLE_DEPTH) {
            entries[count].distance = actor->distance;
            entries[count].kind = ROAD_DRAW_EFFECT;
            entries[count++].index = (unsigned char)i;
        }
    }
    for (i = 1u; i < count; ++i) {
        road_dynamic_draw_entry_t value = entries[i];
        unsigned int at = i;
        while (at && entries[at - 1u].distance < value.distance) {
            entries[at] = entries[at - 1u];
            --at;
        }
        entries[at] = value;
    }
    return count;
}

void road_game_render(const road_game_t *game, road_pixel_t *pixels)
{
    int y;
    int background_bottom;
    int background_scroll = road_background_scroll_pixels(
        game->road_scroll_heading_8_8);
    unsigned int i;
    road_dynamic_draw_entry_t dynamic_draws[ROAD_TRAFFIC_COUNT +
        ROAD_CROSSING_COUNT + ROAD_OPPONENT_COUNT + ROAD_EFFECT_POOL_COUNT];
    unsigned int dynamic_count, draw_index, scene_sample, surface_sample;
    if (g_course_count) project_course(game);
    project_road_surface_visibility(game);
    background_bottom = road_background_clip_bottom(game);
    ROAD_RUNTIME_TRACE("RENDER_PROJECT", game->distance,
        g_road_visibility_first_sample, (unsigned int)background_bottom);
    for (y = 0; y < ROAD_HEIGHT; ++y)
        span(pixels, y, 0, ROAD_WIDTH, rgb(0, 0, 0));
    for (y = HORIZON; y < ROAD_VIEW_BOTTOM; ++y)
        span(pixels, y, 0, ROAD_VIEW_WIDTH, (road_pixel_t)0x294cu);
    for (y = 0; y < background_bottom; ++y) {
        road_pixel_t sky = rgb(31u + (unsigned int)y / 3u,
            88u + (unsigned int)y / 2u, 150u + (unsigned int)y / 2u);
        span(pixels, y, 0, ROAD_VIEW_WIDTH, sky);
        if (g_sky_layer && (unsigned int)y < g_sky_layer->height) {
            int x;
            int offset = background_scroll % (int)g_sky_layer->width;
            unsigned int sx;
            if (offset < 0) offset += (int)g_sky_layer->width;
            sx = (unsigned int)offset;
            const road_pixel_t *source = g_sky_layer->pixels +
                (unsigned int)y * g_sky_layer->width;
            for (x = 0; x < ROAD_VIEW_WIDTH; ++x) {
                pixels[y * ROAD_WIDTH + x] =
                    source[sx];
                if (++sx == g_sky_layer->width) sx = 0u;
            }
        }
        if (g_backdrop_width) {
            unsigned int top = g_sky_layer &&
                g_sky_layer->height > g_backdrop_height ?
                g_sky_layer->height - g_backdrop_height : 0u;
            if ((unsigned int)y >= top &&
                (unsigned int)y - top < g_backdrop_height) {
                unsigned int source_y = (unsigned int)y - top;
                const unsigned char *source = g_backdrop_indices +
                    source_y * g_backdrop_width;
                int offset = background_scroll % (int)g_backdrop_width;
                unsigned int sx;
                int x;
                if (offset < 0) offset += (int)g_backdrop_width;
                sx = (unsigned int)offset;
                for (x = 0; x < ROAD_VIEW_WIDTH; ++x) {
                    road_pixel_t color = g_backdrop_palette[source[sx]];
                    if (color) pixels[y * ROAD_WIDTH + x] = color;
                    if (++sx == g_backdrop_width) sx = 0u;
                }
            }
        } else if (y > 45) {
            int ridge = 46 + ((y * 3) % 13);
            if (y > ridge) span(pixels, y, 0, ROAD_VIEW_WIDTH,
                                rgb(73, 116, 86));
        }
    }
    ROAD_RUNTIME_TRACE("RENDER_BACKDROP", game->distance,
        g_backdrop_width, g_roadside_tile_count);
    ROAD_RUNTIME_AUDIO_PUMP();
    draw_road_rows(game, pixels, 1);
    ROAD_RUNTIME_TRACE("EDGE_FALLBACK_START", game->distance,
        g_edge_profile_count[0], g_edge_profile_count[1]);
    draw_edge_row_fallback(game, pixels);
    ROAD_RUNTIME_TRACE("RENDER_ROAD_ROWS", game->distance,
        g_road_visibility_first_sample, 0u);
    for (surface_sample = (game->distance + ROAD_VISIBLE_DEPTH) / 256u;
         surface_sample > (game->distance + 384u) / 256u;
         --surface_sample) {
        if ((surface_sample & 3u) == 0u) ROAD_RUNTIME_AUDIO_PUMP();
        draw_hill_surface_quads(game, pixels, surface_sample, 0u);
    }
    if (road_fork_in_view(game))
        for (surface_sample =
                 (game->distance + ROAD_VISIBLE_DEPTH) / 256u;
             surface_sample > (game->distance + 384u) / 256u;
             --surface_sample) {
            if ((surface_sample & 3u) == 0u) ROAD_RUNTIME_AUDIO_PUMP();
            draw_hill_surface_quads(game, pixels, surface_sample, 1u);
        }
    ROAD_RUNTIME_TRACE("RENDER_HILLS", game->distance,
        g_hill_profile_count, g_fork_hill_profile_count);
    if (g_hill_profile_count || g_roadside_tile_count ||
        g_edge_profile_count[0] ||
        (road_fork_in_view(game) &&
         game->distance + ROAD_VISIBLE_DEPTH >=
             g_fork_visible_start * 256u)) {
        /* Keep the selected road in front where both fork surfaces overlap. */
        draw_fork_rows(game, pixels);
        draw_road_rows(game, pixels, 0);
    }
    ROAD_RUNTIME_TRACE("RENDER_FORK", game->distance,
        g_fork_count, g_margin_tile_count);
    if (!g_scenery_placement_count)
    for (i = 0; i < 17u; ++i) {
        unsigned int object_world =
            ((game->distance / 2600u) + i + 1u) * 2600u;
        unsigned int relative = object_world - game->distance;
        if (relative > ROAD_VISIBLE_DEPTH) continue;
        int yy = HORIZON + road_projection_dy(relative);
        int dy = yy - HORIZON;
        int half = road_projection_unit(relative);
        int center = road_center(game, object_world, dy);
        int size = 2 + dy / 3;
        int projected = road_project_y(game, object_world, yy);
        if (yy > HORIZON + 2 && projected > -size &&
            projected < ROAD_HEIGHT + size) {
            draw_scenery(pixels, center - half - size, projected, size,
                object_world / 2600u);
            if ((i & 1u) == 0u)
                draw_scenery(pixels, center + half + size, projected,
                    size * 3 / 4, object_world / 2600u + 1u);
        }
    }
    dynamic_count = road_collect_dynamic_draws(game, dynamic_draws);
    ROAD_RUNTIME_TRACE("RENDER_DYNAMIC", game->distance,
        dynamic_count, g_static_placement_count);
    draw_index = 0u;
    scene_sample = (game->distance + ROAD_VISIBLE_DEPTH + 255u) / 256u;
    while (scene_sample >= g_road_visibility_first_sample) {
        if ((scene_sample & 3u) == 0u) ROAD_RUNTIME_AUDIO_PUMP();
        draw_edge_surface_quads(game, pixels, scene_sample);
        draw_fork_roadside_surface_quad_sample(game, pixels,
            scene_sample);
        draw_roadside_surface_quads(game, pixels, scene_sample);
        draw_margin_quad_sample(game, pixels, scene_sample);
        draw_road_surface_quad_sample(game, pixels, scene_sample);
        draw_fork_profile_center_sample(game, pixels, scene_sample);
        if (scene_sample <= game->distance / 256u + 33u) {
            if (g_scenery_placement_count)
                draw_original_scenery_placements(game, pixels, scene_sample);
            if (g_static_placement_count)
                draw_static_scenery(game, pixels, scene_sample);
        }
        while (draw_index < dynamic_count &&
            (dynamic_draws[draw_index].distance + 255u) / 256u ==
                scene_sample) {
            const road_dynamic_draw_entry_t *entry =
                &dynamic_draws[draw_index++];
            i = entry->index;
            if (entry->kind == ROAD_DRAW_CAR) {
                const road_rider_t *car = i < ROAD_TRAFFIC_COUNT ?
                    &game->traffic[i] : &game->crossing[i - ROAD_TRAFFIC_COUNT];
                if (car->distance > game->distance) {
                    unsigned int relative = car->distance - game->distance;
                    if (relative > ROAD_VISIBLE_DEPTH) continue;
                    int yy = HORIZON + road_projection_dy(relative);
                    int dy = yy - HORIZON;
                    int xx = road_center(game, car->distance, dy) +
                        car->lane * road_projection_unit(relative) / 111;
                    int projected = road_project_y(game, car->distance, yy);
                    int height = 4 + dy * 2 / 5;
                    {
                        const road_sprite_t *sprite;
                        const road_car_frame_anchor_t *anchor = 0;
                        if (g_car_animation_count) {
                            unsigned int animation_index = road_car_select_animation(
                                g_car_animations, g_car_animation_count,
                                car->collision_mode, car->animation_choice);
                            if (animation_index >= g_car_animation_count) continue;
                            const road_car_animation_t *anim =
                                &g_car_animations[animation_index];
                            unsigned int frame;
                            if (!anim->frames || !anim->frame_count) continue;
                            frame = road_car_select_mapped_frame(anim, relative);
                            sprite = &anim->frames[frame];
                            if (anim->anchors) anchor = &anim->anchors[frame];
                            height = road_car_projected_height(anim, sprite, relative);
                        } else {
                            sprite = &g_cars[i % g_car_count];
                        }
                        if (anchor && anchor->valid && sprite->height) {
                            int width = height * (int)sprite->width /
                                (int)sprite->height;
                            xx += width / 2 - (int)anchor->x * height /
                                (int)sprite->height;
                            projected += height - (int)anchor->y * height /
                                (int)sprite->height;
                        }
                        if (projected > -height &&
                            projected < ROAD_HEIGHT + height &&
                            road_projected_secondary_visible(car->distance,
                                projected - height))
                            draw_sprite_scaled(pixels, sprite,
                                               xx, projected, height);
                    }
                }
            } else if (entry->kind == ROAD_DRAW_OPPONENT) {
                const road_rider_t *opponent = &game->opponents[i];
                if (opponent->route_visible &&
                    opponent->distance > game->distance) {
                    unsigned int relative = opponent->distance - game->distance;
                    if (relative > ROAD_VISIBLE_DEPTH) continue;
                    int yy = HORIZON + road_projection_dy(relative);
                    int dy = yy - HORIZON;
                    int xx = road_center(game, opponent->distance, dy) +
                        opponent->lane * road_projection_unit(relative) / 111;
                    int size = road_dynamic_actor_height(relative,
                        ROAD_NEAR_RIDER_HEIGHT);
                    int projected = road_project_y(game, opponent->distance, yy);
                    if (projected > -size && projected < ROAD_HEIGHT + size) {
                        if (opponent->hit_reaction_ms &&
                            g_bike_count >= ROAD_BIKE_FRAME_COUNT) {
                            unsigned int phase =
                                (600u - opponent->hit_reaction_ms) * 5u / 600u;
                            unsigned int frame = phase < 3u ? phase : 4u - phase;
                            const road_sprite_t *sprite = &g_bikes[
                                ROAD_BIKE_OPPONENT_HIT_FIRST + frame];
                            draw_sprite_scaled(pixels, sprite, xx, projected,
                                size * (int)sprite->height /
                                (int)g_bikes[ROAD_BIKE_OPPONENT_INDEX].height);
                        } else draw_bike(pixels, xx, projected, size,
                            opponent->stun_ms ? rgb(209, 133, 57) :
                            rgb(85u + (i * 37u) % 145u,
                                39u + (i * 47u) % 156u,
                                64u + (i * 51u) % 161u),
                            0, 0, i == 2u ? 4u : 0u);
                    }
                }
            } else {
                const road_effect_actor_t *actor = &game->effects[i];
                if (actor->active && actor->distance > game->distance) {
                    unsigned int relative = actor->distance - game->distance;
                    if (relative > ROAD_VISIBLE_DEPTH) continue;
                    int yy = HORIZON + road_projection_dy(relative);
                    int dy = yy - HORIZON;
                    int xx = road_center(game, actor->distance, dy) +
                        actor->lane * road_projection_unit(relative) / 111;
                    int size = road_dynamic_actor_height(relative,
                        actor->travel_mode >= 11u ?
                        ROAD_NEAR_RIDER_HEIGHT :
                        ROAD_NEAR_PEDESTRIAN_HEIGHT);
                    int projected = road_project_y(game, actor->distance, yy);
                    if (projected > -size && projected < ROAD_HEIGHT + size &&
                        road_projected_secondary_visible(actor->distance,
                            projected - size)) {
                        if (actor->sprite_index < g_effect_sprite_count &&
                            actor->travel_mode < 11u) {
                            unsigned int frame = road_effect_render_frame(actor);
                            const road_sprite_t *sprite = &g_effect_sprites[
                                actor->sprite_index * ROAD_EFFECT_SPRITE_FRAMES +
                                frame];
                            draw_sprite_scaled(pixels, sprite, xx, projected,
                                               size);
                        } else
                            draw_bike(pixels, xx, projected, size,
                                actor->travel_mode >= 11u ? rgb(220, 100, 70) :
                                rgb(230, 210, 80), 0, 0, 4u);
                    }
                }
            }
        }
        if (scene_sample == g_road_visibility_first_sample) break;
        --scene_sample;
    }
    ROAD_RUNTIME_TRACE("RENDER_SCENE_DONE", game->distance,
        dynamic_count, draw_index);
    if (game->recovery_ms && game->crash_elapsed_ms)
        draw_player_crash(pixels, game);
    else if (game->attack_ms && game->attack_style &&
             g_bike_count >= ROAD_BIKE_FRAME_COUNT) {
        unsigned int frame = (ROAD_ATTACK_DURATION_MS - game->attack_ms) *
            ROAD_BIKE_KICK_FRAME_COUNT / ROAD_ATTACK_DURATION_MS;
        unsigned int index = (game->attack_side ?
            ROAD_BIKE_KICK_RIGHT_FIRST : ROAD_BIKE_KICK_LEFT_FIRST) + frame;
        const road_sprite_t *sprite;
        if (frame >= ROAD_BIKE_KICK_FRAME_COUNT)
            index--;
        sprite = &g_bikes[index];
        draw_sprite_scaled(pixels, sprite,
            ROAD_VIEW_WIDTH / 2 + game->lean * 4,
            170 - clamp_int(game->vertical_height_8_8 / 256, 0, 32),
            60 * (int)sprite->height / (int)g_bikes[0].height);
    } else
        draw_bike(pixels, ROAD_VIEW_WIDTH / 2 + game->lean * 4,
            170 - clamp_int(game->vertical_height_8_8 / 256, 0, 32), 60,
            rgb(56, 105, 224), game->lean, game->attack_ms, 0u);
    if (g_hud) {
        draw_original_hud(pixels, game);
    } else if (g_hud_health_frames && g_hud_portrait) {
        draw_alternate_hud(pixels, game);
    } else {
    box(pixels, 4, 4, 76, 18, rgb(20, 28, 36));
    draw_number(pixels, 9, 8, game->speed, rgb(248, 230, 170));
    box(pixels, 48, 10, (int)(game->speed / 6u), 6,
        rgb(230, 150, 44));
    box(pixels, 4, 24, 76, 4, rgb(20, 28, 36));
    box(pixels, 4, 24,
        (int)(76u * game->rider_health / game->rider_max_health), 4,
        rgb(224, 79, 66));
    box(pixels, 4, 30, 76, 4, rgb(20, 28, 36));
    box(pixels, 4, 30,
        (int)(76u * game->bike_health / game->bike_max_health), 4,
        rgb(82, 174, 230));
    box(pixels, 251, 4, 65, 18, rgb(20, 28, 36));
    draw_number(pixels, 255, 8, game->last_frame_ms,
        game->last_frame_ms > 50u ? rgb(241, 76, 61) :
        rgb(155, 230, 170));
    box(pixels, 4, 228, (int)game->hits * 8, 4, rgb(240, 184, 52));
    if (g_finish_sample) {
        unsigned int progress = game->distance * 312u /
            (g_finish_sample * 256u);
        box(pixels, 4, 235, 312, 4, rgb(20, 28, 36));
        box(pixels, 4, 235, (int)progress, 4, rgb(240, 184, 52));
    }
    }
    if (game->finished) draw_finish(pixels);
    ROAD_RUNTIME_TRACE("RENDER_DONE", game->distance,
        game->speed, game->finished);
}

void road_rotate_counterclockwise(const road_pixel_t *landscape,
                                  road_pixel_t *portrait)
{
    int y;
    for (y = 0; y < ROAD_HEIGHT; ++y) {
        int x;
        const road_pixel_t *source = landscape + y * ROAD_WIDTH;
        for (x = 0; x < ROAD_WIDTH; ++x)
            portrait[(ROAD_WIDTH - 1 - x) * ROAD_HEIGHT + y] = source[x];
    }
}

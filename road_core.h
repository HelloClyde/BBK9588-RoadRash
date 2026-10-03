#ifndef ROAD_CORE_H
#define ROAD_CORE_H

/* A portable 320x240 software-rendering bring-up for the Road Rash port. */
#define ROAD_WIDTH 320
#define ROAD_HEIGHT 240
#define ROAD_PIXELS (ROAD_WIDTH * ROAD_HEIGHT)

#define ROAD_LEFT   1u
#define ROAD_RIGHT  2u
#define ROAD_ACCEL  4u
#define ROAD_BRAKE  8u
#define ROAD_ATTACK 16u
#define ROAD_KICK 32u
#define ROAD_ATTACK_DURATION_MS 340u
#define ROAD_BIKE_ATTACK_FIRST 3u
#define ROAD_BIKE_ATTACK_COUNT 18u
#define ROAD_BIKE_OPPONENT_INDEX 21u
#define ROAD_BIKE_TURN_FIRST 22u
#define ROAD_BIKE_FALLEN_INDEX 30u
#define ROAD_BIKE_TUMBLE_FIRST 31u
#define ROAD_BIKE_GROUND_FIRST 38u
#define ROAD_BIKE_STAND_INDEX 43u
#define ROAD_BIKE_RUN_FIRST 44u
#define ROAD_BIKE_MOUNT_FIRST 50u
#define ROAD_BIKE_KICK_RIGHT_FIRST 55u
#define ROAD_BIKE_KICK_LEFT_FIRST 61u
#define ROAD_BIKE_KICK_FRAME_COUNT 6u
#define ROAD_BIKE_OPPONENT_HIT_FIRST 67u
#define ROAD_BIKE_OPPONENT_HIT_COUNT 3u
#define ROAD_BIKE_FRAME_COUNT 70u
#define ROAD_TRAFFIC_COUNT 6u
#define ROAD_OPPONENT_COUNT 14u
#define ROAD_CROSSING_COUNT 4u
#define ROAD_CAR_MAX_FRAMES 12u
#define ROAD_AI_CRUISE 1u
#define ROAD_AI_PASS_RIGHT 2u
#define ROAD_AI_ATTACK 3u
#define ROAD_AI_APPROACH 4u
#define ROAD_AI_PASS_LEFT 5u
#define ROAD_AI_AVOID_TRAFFIC 6u
#define ROAD_EFFECT_WAITING 0u
#define ROAD_EFFECT_RIDING 1u
#define ROAD_EFFECT_CONTACT_LOCKED 2u
#define ROAD_EFFECT_RECOVERING 3u
#define ROAD_EFFECT_SHAKE_RECOVERY 4u
#define ROAD_EFFECT_FAST_RECOVERY 5u
#define ROAD_EFFECT_FLAG 7u
#define ROAD_EFFECT_ANIM_MOTION 0u
#define ROAD_EFFECT_ANIM_STANDING 1u
#define ROAD_EFFECT_ANIM_SHAKE 2u
#define ROAD_EFFECT_ANIM_FALL 3u
#define ROAD_EFFECT_ANIM_ATTACK 4u
#define ROAD_EFFECT_ANIM_FAST 5u
#define ROAD_EFFECT_ANIM_FLAG 6u
#define ROAD_DAMAGE_NONE 0u
#define ROAD_DAMAGE_STANDARD 1u
#define ROAD_DAMAGE_SEVERE 2u
#define ROAD_DAMAGE_GLANCING 3u
#include "bike_spec_runtime.h"

typedef unsigned short road_pixel_t;

typedef struct road_sprite {
    unsigned int width;
    unsigned int height;
    const road_pixel_t *pixels;
} road_sprite_t;

/* One RMTN/RDWD edge definition, at an absolute route sample.  The source
 * resource stores its two sides in separate, ordered entry runs. */
typedef struct road_edge_profile {
    unsigned short start_sample;
    short inner_offset;
    short outer_offset;
    short height;
} road_edge_profile_t;

typedef struct road_car_frame_map {
    unsigned int column_count;
    unsigned int direction_flags;
    unsigned char scale_multiplier[3];
    unsigned char frames[3][16];
} road_car_frame_map_t;

typedef struct road_car_frame_anchor {
    short x;
    short y;
    unsigned char valid;
} road_car_frame_anchor_t;

typedef struct road_car_animation {
    const road_sprite_t *frames;
    unsigned int frame_count;
    const road_car_frame_map_t *frame_map;
    const road_car_frame_anchor_t *anchors;
} road_car_animation_t;

typedef struct road_scenery_placement {
    unsigned short sample;
    unsigned char side;
    unsigned char variant;
    unsigned char scale;
    unsigned short lateral;
} road_scenery_placement_t;

typedef struct road_static_placement {
    unsigned short sample;
    short lateral;
    unsigned char variant;
    unsigned char visibility_group;
    unsigned char mirrored;
} road_static_placement_t;

typedef struct road_static_collision_box {
    short left;
    short top;
    short right;
    short bottom;
} road_static_collision_box_t;

typedef struct road_static_collision {
    unsigned char count;
    road_static_collision_box_t box[2];
} road_static_collision_t;

typedef struct road_static_frame_anchor {
    short x;
    short y;
    unsigned char valid;
} road_static_frame_anchor_t;

typedef struct road_effect_placement {
    unsigned short sample;
    short lateral;
    unsigned char travel_mode;
    unsigned char family_inventory;
    unsigned char track_offset;
    unsigned short family_id;
    unsigned char sprite_index;
} road_effect_placement_t;

typedef struct road_crossing_zone {
    unsigned short start_sample;
    unsigned short end_sample;
    unsigned char mode;
    signed char spread;
    signed char simulation_scale;
    unsigned char preserve_child_updates;
    unsigned char short_timing;
} road_crossing_zone_t;

#define ROAD_CROSSING_PARENT_COUNT 10u
typedef struct road_crossing_parent {
    unsigned short zone_index;
    unsigned char active;
    unsigned char child_mask;
    unsigned char phase[4];
    unsigned int timer[4];
    unsigned int last_update_tick;
    unsigned int next_update_tick;
} road_crossing_parent_t;

typedef struct road_effect_actor {
    unsigned int distance;
    int lane;
    unsigned int speed;
    unsigned int age_ms;
    unsigned int state_ms;
    unsigned int contact_delay_ms;
    unsigned char travel_mode;
    unsigned char sprite_index;
    unsigned char behavior;
    unsigned char animation;
    unsigned char active;
} road_effect_actor_t;

#define ROAD_EFFECT_POOL_COUNT 15u
#define ROAD_EFFECT_STANDING_FRAME 0u
#define ROAD_EFFECT_MOTION_FIRST 1u
#define ROAD_EFFECT_SHAKE_FIRST 7u
#define ROAD_EFFECT_FALL_FIRST 9u
#define ROAD_EFFECT_ATTACK_FIRST 15u
#define ROAD_EFFECT_FAST_FIRST 17u
#define ROAD_EFFECT_FLAG_FIRST 23u
#define ROAD_EFFECT_SPRITE_FRAMES 29u

typedef struct road_rider {
    unsigned int distance;
    int lane;
    unsigned int speed;
    unsigned int base_speed;
    unsigned int graph_curve_speed;
    unsigned int stun_ms;
    unsigned int hit_reaction_ms;
    int target_lane;
    unsigned int attack_cooldown_ms;
    unsigned int behavior;
    unsigned int behavior_ms;
    int travel_direction;
    unsigned int collision_mode;
    unsigned int animation_choice;
    unsigned int lane_fraction;
    unsigned char route_visible;
} road_rider_t;

typedef struct road_hill_profile {
    unsigned int start_sample;
    unsigned int end_sample;
    unsigned short family_id[2];
    unsigned char selector[2];
    unsigned char tile_index[2][3];
    unsigned char x[2][3];
    signed char y[2][3];
} road_hill_profile_t;

typedef struct road_hill_sample {
    unsigned char x[2][3];
    signed char y[2][3];
} road_hill_sample_t;

typedef struct road_surface_sample {
    unsigned short family_id[2];
    unsigned char selector[2];
    unsigned char tile_index[2];
} road_surface_sample_t;

typedef struct road_margin_tile {
    unsigned short family_id;
    unsigned char selector;
    road_sprite_t primary_mips[8];
    road_sprite_t fill_mips[8];
} road_margin_tile_t;

typedef struct road_game {
    unsigned int distance;
    int track_elevation;
    int road_scroll_heading_8_8;
    unsigned int road_scroll_generation;
    unsigned int speed;
    unsigned int elapsed_ms;
    unsigned int hits;
    unsigned int bounce_events;
    unsigned int hazard_hits;
    unsigned int sound_event_mask;
    unsigned int last_static_contact_index;
    unsigned int rider_hits;
    unsigned int rider_health;
    unsigned int rider_recovery_ceiling;
    unsigned int rider_max_health;
    unsigned int bike_health;
    unsigned int bike_max_health;
    unsigned int recovery_ms;
    unsigned int crash_elapsed_ms;
    unsigned int player_stun_ms;
    unsigned int top_speed;
    unsigned int accel_percent;
    unsigned int steer_percent;
    unsigned int bike_physics_ready;
    unsigned int velocity_raw;
    int vertical_height_8_8;
    int vertical_velocity_8_8;
    int vertical_surface_acceleration_8_8;
    unsigned int surface_contact_scale_8_8;
    unsigned int contact_threshold;
    int minimum_vertical_acceleration;
    unsigned int bounce_scale;
    unsigned int impact_strength;
    unsigned int impact_scale;
    unsigned int slide_grip_scale_8_8;
    unsigned int slide_activation_threshold;
    unsigned int slip_amount;
    int throttle_control;
    int accel_limit;
    int brake_target;
    int accel_rise_step;
    int accel_fall_step;
    int brake_rise_step;
    int brake_fall_step;
    unsigned int gear;
    int engine_pitch;
    int engine_gauge_pitch;
    unsigned int previous_engine_forward_velocity_target;
    int traffic_spawn_timer;
    int traffic_reverse_timer;
    unsigned int traffic_seed;
    unsigned int effect_cursor;
    unsigned int crossing_cursor;
    unsigned int effect_hits;
    unsigned int gear_acceleration[6];
    int gear_pitch_scale_8_8[6];
    unsigned int gear_upshift[6];
    int gear_downshift[6];
    int steering_angle_raw;
    int steering_rise_step;
    int steering_fall_step;
    unsigned int steering_velocity_scale;
    unsigned int steering_left_scale;
    unsigned int steering_right_scale;
    unsigned int finished;
    unsigned int graph_rank;
    unsigned int graph_rank_enabled;
    unsigned int input;
    unsigned int attack_ms;
    unsigned int attack_style;
    unsigned int attack_side;
    unsigned int last_frame_ms;
    unsigned int last_render_ms;
    unsigned int last_present_ms;
    int lane;
    int lean;
    int lateral_impulse;
    road_rider_t opponents[ROAD_OPPONENT_COUNT];
    road_rider_t traffic[ROAD_TRAFFIC_COUNT];
    road_rider_t crossing[ROAD_CROSSING_COUNT];
    road_crossing_parent_t crossing_parents[ROAD_CROSSING_PARENT_COUNT];
    road_effect_actor_t effects[ROAD_EFFECT_POOL_COUNT];
} road_game_t;

void road_game_init(road_game_t *game);
int road_game_road_skid_active(const road_game_t *game);
unsigned int road_game_finish_place(const road_game_t *game);
void road_game_set_bike_tuning(road_game_t *game, unsigned int speed_factor,
                               unsigned int tuning_base,
                               unsigned int steering_scale);
void road_game_set_bike_spec(road_game_t *game, const rr_bike_spec_t *spec);
void road_game_apply_collision_damage(road_game_t *game,
                                       unsigned int rider_damage,
                                       unsigned int bike_damage,
                                       unsigned int kind);
void road_game_seek(road_game_t *game, unsigned int distance);
void road_game_set_course(const signed char *curvature, unsigned int count);
void road_game_set_finish_sample(unsigned int sample);
void road_game_set_elevation(const signed char *steps, unsigned int count);
void road_game_set_widths(const unsigned short *left,
                          const unsigned short *right, unsigned int count);
void road_game_set_fork_preview(const signed char *curvature,
                                const signed char *elevation,
                                const unsigned short *left,
                                const unsigned short *right,
                                unsigned int count,
                                unsigned int fork_sample,
                                unsigned int main_channel);
/* Keep the two projected routes until their RNOD reverse join.  The caller
 * supplies a sample that is valid in both flattened route buffers. */
void road_game_set_fork_span(unsigned int end_sample);
/* Reopen the final 81-sample dual window before a reverse RNOD join. */
void road_game_set_fork_reverse(unsigned int selected_join,
                                unsigned int other_join);
void road_game_set_fork_slopes(const unsigned char *left_margin,
                               const unsigned char *right_margin,
                               unsigned int count);
void road_game_set_fork_depths(const unsigned char *left_depth,
                               const unsigned char *right_depth,
                               unsigned int count);
unsigned int road_fork_channel(int lane, unsigned int old_left_width,
                               unsigned int first_left_width,
                               unsigned int first_right_width);
void road_game_set_surface_textures(const road_sprite_t *textures,
                                    unsigned int count);
void road_game_set_hud(const road_sprite_t *hud);
void road_game_set_hud_needles(const road_sprite_t *speed,
                               const road_sprite_t *health);
void road_game_set_alternate_hud(const road_sprite_t *health_frames,
                                 unsigned int frame_count,
                                 const road_sprite_t *portrait);
void road_game_set_hill_profiles(const road_hill_profile_t *profiles,
                                 unsigned int count);
void road_game_set_hill_samples(const road_hill_sample_t *samples,
                                unsigned int count);
void road_game_set_fork_hill_profiles(const road_hill_profile_t *profiles,
                                      unsigned int count);
void road_game_set_fork_hill_samples(const road_hill_sample_t *samples,
                                     unsigned int count);
void road_game_set_edge_profiles(const road_edge_profile_t *left,
                                 unsigned int left_count,
                                 const road_edge_profile_t *right,
                                 unsigned int right_count);
void road_game_set_edge_outer_samples(const short *left,
                                      const short *right,
                                      unsigned int count);
void road_game_set_edge_fill(const road_sprite_t *primary,
                             const road_sprite_t *horizon);
/* Four 128-pixel-wide CLGP mip levels ordered 8, 4, 2, 1 rows;
 * narrow levels have widths 16, 32, 64 within each row level. */
void road_game_set_hill_tiles(const road_sprite_t *tiles,
                              const road_sprite_t *mip_tiles,
                              const road_sprite_t *narrow_tiles,
                              unsigned int count);
void road_game_set_surface_samples(const road_surface_sample_t *samples,
                                    unsigned int count);
void road_game_set_fork_surface_samples(
    const road_surface_sample_t *samples, unsigned int count);
void road_game_set_margin_resources(unsigned int channel,
    const unsigned char *left_selector,
    const unsigned char *right_selector,
    const unsigned short *left_family,
    const unsigned short *right_family,
    const unsigned char *surface_flags,
    const unsigned char *terrain_mode,
    unsigned int count);
void road_game_set_margin_tiles(const road_margin_tile_t *tiles,
                                unsigned int count);
void road_game_set_roadside_tiles(const road_sprite_t *tiles,
                                  const unsigned short *heights,
                                  const unsigned char *repeat_indices,
                                  const road_sprite_t *repeat_mips,
                                  unsigned int count);
void road_game_set_cross_section(const unsigned char *left_margin,
                                 const unsigned char *right_margin,
                                 const unsigned char *left_depth,
                                 const unsigned char *right_depth,
                                 const unsigned char *terrain_mode,
                                 unsigned int count);
void road_game_set_backdrop(const unsigned char *indices,
                            const road_pixel_t *palette,
                            unsigned int width, unsigned int height);
void road_game_set_sky(const road_sprite_t *sky);
void road_game_set_scenery(const road_sprite_t *sprites, unsigned int count);
void road_game_set_scenery_placements(const road_scenery_placement_t *placements,
                                      unsigned int count);
void road_game_set_static_scenery(const road_sprite_t *sprites,
                                 unsigned int sprite_count,
                                 const road_static_placement_t *placements,
                                 unsigned int placement_count);
/* Three CANS scale frames per variant, ordered far to near. */
void road_game_set_static_scale_frames(const road_sprite_t *frames,
                                       unsigned int variant_count);
void road_game_set_static_frame_anchors(
    const road_static_frame_anchor_t *anchors,
    unsigned int variant_count);
void road_game_set_reciprocal_table(const unsigned int *table,
                                    unsigned int count);
void road_game_set_static_collisions(const road_static_collision_t *collisions,
                                    unsigned int count);
void road_game_set_hazard_motion(const unsigned char *moving,
                                 unsigned int count);
void road_game_set_effects(road_game_t *game,
                           const road_effect_placement_t *placements,
                           unsigned int count);
void road_game_set_crossing_zones(const road_crossing_zone_t *zones,
                                  unsigned int count);
void road_game_set_effect_sprites(const road_sprite_t *state_frames,
                                  unsigned int actor_style_count);
void road_game_set_effect_frame_ticks(const unsigned char *ticks,
                                      unsigned int actor_style_count);
void road_game_set_bikes(const road_sprite_t *sprites, unsigned int count);
void road_game_set_cars(const road_sprite_t *sprites, unsigned int count);
void road_game_set_car_animations(const road_car_animation_t *animations,
                                  unsigned int count);
unsigned int road_car_select_frame(unsigned int frame_count,
                                   int lateral, int projected_height);
unsigned int road_car_select_mapped_frame(const road_car_animation_t *animation,
                                          unsigned int distance);
unsigned int road_car_select_animation(const road_car_animation_t *animations,
                                        unsigned int count,
                                        unsigned int collision_mode,
                                        unsigned int choice);
int road_car_projected_height(const road_car_animation_t *animation,
                               const road_sprite_t *sprite,
                               unsigned int distance);
void road_game_step(road_game_t *game, unsigned int input, unsigned int dt_ms);
void road_game_render(const road_game_t *game, road_pixel_t *landscape);
void road_rotate_counterclockwise(const road_pixel_t *landscape,
                                  road_pixel_t *portrait);

#endif

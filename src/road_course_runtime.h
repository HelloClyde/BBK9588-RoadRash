#ifndef ROAD_RASH_COURSE_RUNTIME_H
#define ROAD_RASH_COURSE_RUNTIME_H

#include "rsrc_reader.h"
#include "road_core.h"

/* The upstream graph builder aligns RPTH elevation steps on the first/last
 * 0x51 samples of a junction before either branch is traversed. */
#define RR_COURSE_JUNCTION_SAMPLES 81u

typedef struct rr_course_buffer {
    signed char *curvature;
    signed char *elevation;
    unsigned short *left_width;
    unsigned short *right_width;
    rr_u8 *left_margin;
    rr_u8 *right_margin;
    rr_u8 *left_depth;
    rr_u8 *right_depth;
    rr_u8 *left_edge_resource;
    rr_u8 *right_edge_resource;
    unsigned short *left_edge_family;
    unsigned short *right_edge_family;
    rr_u8 *terrain_mode;
    rr_u8 *surface_flags;
    road_surface_sample_t *surface_samples;
    road_hill_sample_t *hill_samples;
    road_hill_profile_t *hill_profiles;
    rr_u32 hill_profile_capacity;
    rr_u32 hill_profile_count;
    road_edge_profile_t *edge_profiles[2];
    rr_u32 edge_profile_capacity[2];
    rr_u32 edge_profile_count[2];
    short *edge_outer_samples[2];
    road_scenery_placement_t *objects;
    rr_u32 object_capacity;
    rr_u32 object_count;
    road_static_placement_t *hazards;
    rr_u32 hazard_capacity;
    rr_u32 hazard_count;
    road_effect_placement_t *effects;
    rr_u32 effect_capacity;
    rr_u32 effect_count;
    road_crossing_zone_t *crossing_zones;
    rr_u32 crossing_capacity;
    rr_u32 crossing_count;
    unsigned int difficulty_level;
    rr_u32 *family_ids;
    rr_u32 family_capacity;
    rr_u32 family_count;
    rr_u32 *hazard_family_ids;
    rr_u8 *hazard_frame_ids;
    rr_u32 hazard_family_capacity;
    rr_u32 hazard_family_count;
    rr_u32 capacity;
    rr_u32 sample_count;
    rr_u32 finish_sample;
    rr_u32 segment_count;
    rr_u32 branch_samples[8];
    rr_u8 branch_main_channels[8];
    rr_u8 branch_alternate_channels[8];
    rr_u32 branch_count;
} rr_course_buffer_t;

/* Follow the game's RNOD primary route and stream each RPTH from the original
 * 3DO file. All segments stay in the original on-disk format until requested. */
int rr_course_load_primary(const rr_rsrc_file_t *file,
                           rr_course_buffer_t *course);

/* At each forward fork, bit 0 selects the second link; successive forks
 * consume successive bits. A zero mask follows the original primary route. */
int rr_course_load_selected(const rr_rsrc_file_t *file,
                            rr_course_buffer_t *course,
                            rr_u32 branch_mask);

int rr_course_reconcile_fork_elevation(
    signed char *selected, rr_u32 selected_count,
    signed char *other, rr_u32 other_count,
    rr_u32 fork_sample, rr_u32 selected_span, rr_u32 other_span,
    int selected_is_source);

#endif

#ifndef ROAD_RASH_COURSE_GRAPH_H
#define ROAD_RASH_COURSE_GRAPH_H

#include "rsrc_reader.h"

#define RR_COURSE_GRAPH_MAX_NODES 128u
#define RR_COURSE_GRAPH_NONE 255u
#define RR_COURSE_GRAPH_MAX_CURVES 10000u

typedef struct rr_course_graph_node {
    rr_u32 resource_offset;
    rr_u32 segment_index;
    rr_u32 sample_count;
    rr_u32 curve_offset;
    rr_u32 finish_gate;
    int finish_distance_samples;
    int fork_edge_lane;
    rr_u8 kind;
    rr_u8 next[2];
    rr_u8 main_channel;
    rr_u8 alternate_channel;
    rr_u8 visit;
} rr_course_graph_node_t;

typedef struct rr_course_graph {
    rr_course_graph_node_t nodes[RR_COURSE_GRAPH_MAX_NODES];
    rr_u32 node_count;
    rr_u32 segment_count;
    rr_u32 root_finish_samples;
    rr_u32 curve_count;
    rr_u8 root;
    signed char curves[RR_COURSE_GRAPH_MAX_CURVES];
} rr_course_graph_t;

typedef struct rr_course_graph_cursor {
    rr_u32 local_8_8;
    rr_u8 node;
    rr_u8 fork_index;
} rr_course_graph_cursor_t;

/* Preserve every reachable RNOD branch, including links outside the
 * player's currently flattened route. finish_gate is local to its segment;
 * finish_distance_samples follows the upstream signed post-finish rule. */
int rr_course_graph_load(const rr_rsrc_file_t *file,
                         unsigned int difficulty_level,
                         rr_course_graph_t *graph);

/* Use the same successive fork bits as rr_course_load_selected. */
rr_u32 rr_course_graph_route_finish(const rr_course_graph_t *graph,
                                     rr_u32 branch_mask);

/* Count RPTH samples on each side of a forward branch up to the first
 * common RNOD join. Returns zero if the branch is absent or nested. */
int rr_course_graph_branch_span(const rr_course_graph_t *graph,
                                rr_u32 branch_mask, unsigned int branch_index,
                                rr_u32 *chosen_span, rr_u32 *other_span);

/* Advance one rider independently through the graph. A cursor remains on its
 * chosen branch until the graph's reverse join; it does not stop at a gate. */
int rr_course_graph_cursor_init(const rr_course_graph_t *graph,
                                rr_course_graph_cursor_t *cursor);
int rr_course_graph_cursor_advance(const rr_course_graph_t *graph,
                                   rr_course_graph_cursor_t *cursor,
                                   rr_u32 delta_8_8, rr_u32 branch_mask);
/* Select a fork at the first lane's right edge, using the original RLAN
 * widths translated to the 9588 lateral scale. */
int rr_course_graph_cursor_advance_lane(const rr_course_graph_t *graph,
                                        rr_course_graph_cursor_t *cursor,
                                        rr_u32 delta_8_8, int lane);
int rr_course_graph_cursor_advance_main(const rr_course_graph_t *graph,
                                        rr_course_graph_cursor_t *cursor,
                                        rr_u32 delta_8_8);
int rr_course_graph_cursor_finish_distance(
    const rr_course_graph_t *graph,
    const rr_course_graph_cursor_t *cursor,
    int *distance_8_8);
int rr_course_graph_cursor_curvature(const rr_course_graph_t *graph,
                                     const rr_course_graph_cursor_t *cursor,
                                     int *curvature);
unsigned int rr_course_graph_curve_speed(const rr_course_graph_t *graph,
                                          const rr_course_graph_cursor_t *cursor,
                                          unsigned int base_speed);

#endif

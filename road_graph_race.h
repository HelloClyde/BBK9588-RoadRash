#ifndef ROAD_RASH_GRAPH_RACE_H
#define ROAD_RASH_GRAPH_RACE_H

#include "road_core.h"
#include "road_course_graph.h"

typedef struct rr_graph_race {
    const rr_course_graph_t *graph;
    rr_course_graph_cursor_t opponents[ROAD_OPPONENT_COUNT];
    rr_u32 previous_distance[ROAD_OPPONENT_COUNT];
    rr_u32 path_prefix[RR_COURSE_GRAPH_MAX_NODES];
    rr_u32 player_branch_mask;
    rr_u8 active;
} rr_graph_race_t;

/* Called after a race begins. Rebinding the player's selected RNOD path only
 * changes the mask passed to sync; opponent cursors retain their own links. */
int rr_graph_race_reset(rr_graph_race_t *race,
                        const rr_course_graph_t *graph,
                        road_game_t *game);
int rr_graph_race_sync(rr_graph_race_t *race, road_game_t *game,
                       rr_u32 player_branch_mask);
void rr_graph_race_disable(rr_graph_race_t *race, road_game_t *game);

#endif

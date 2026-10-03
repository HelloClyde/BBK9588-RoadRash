#include "road_graph_race.h"

#define RR_GRAPH_RACE_NO_PREFIX 0xffffffffu
#define RR_GRAPH_RACE_ACTIVE_DEPTH (33u * 256u)

static int graph_race_near_player(rr_u32 rider, rr_u32 player)
{
    return rider > player ?
        rider - player <= RR_GRAPH_RACE_ACTIVE_DEPTH :
        player - rider <= RR_GRAPH_RACE_ACTIVE_DEPTH;
}

static int graph_race_advance_opponent(const rr_course_graph_t *graph,
                                       rr_course_graph_cursor_t *cursor,
                                       rr_u32 delta_8_8, int lane,
                                       rr_u32 rider_distance,
                                       rr_u32 player_distance)
{
    if (graph_race_near_player(rider_distance, player_distance))
        return rr_course_graph_cursor_advance_lane(graph, cursor,
                                                   delta_8_8, lane);
    /* Upstream offscreen AI traverses the RNOD main fork; the physical
     * rider chooses a connected lane when activated near the player. */
    return rr_course_graph_cursor_advance_main(graph, cursor,
                                                delta_8_8);
}

static int graph_race_build_path(rr_graph_race_t *race, rr_u32 mask)
{
    rr_u32 i, prefix = 0u, fork = 0u;
    rr_u8 cursor;
    const rr_course_graph_t *graph = race->graph;
    if (!graph || !graph->node_count) return 0;
    for (i = 0u; i < RR_COURSE_GRAPH_MAX_NODES; ++i)
        race->path_prefix[i] = RR_GRAPH_RACE_NO_PREFIX;
    cursor = graph->root;
    for (i = 0u; i < graph->node_count; ++i) {
        const rr_course_graph_node_t *node;
        if (cursor >= graph->node_count ||
            race->path_prefix[cursor] != RR_GRAPH_RACE_NO_PREFIX)
            return 0;
        race->path_prefix[cursor] = prefix;
        node = &graph->nodes[cursor];
        if (node->kind == 4u) {
            race->player_branch_mask = mask;
            return 1;
        }
        if (node->kind == 0u) {
            if (prefix > 0xffffffffu - node->sample_count)
                return 0;
            prefix += node->sample_count;
            cursor = node->next[0];
        } else if (node->kind == 1u) {
            if (fork >= 8u) return 0;
            cursor = node->next[(mask & (1u << fork++)) ? 1u : 0u];
        } else if (node->kind == 2u) cursor = node->next[0];
        else return 0;
    }
    return 0;
}

void rr_graph_race_disable(rr_graph_race_t *race, road_game_t *game)
{
    unsigned int i;
    if (race) race->active = 0u;
    if (!game) return;
    game->graph_rank_enabled = 0u;
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
    {
        game->opponents[i].route_visible = 1u;
        game->opponents[i].graph_curve_speed =
            game->opponents[i].base_speed;
    }
}

int rr_graph_race_reset(rr_graph_race_t *race,
                        const rr_course_graph_t *graph,
                        road_game_t *game)
{
    unsigned int i;
    if (!race || !graph || !game) return 0;
    race->graph = graph;
    race->active = 0u;
    race->player_branch_mask = RR_GRAPH_RACE_NO_PREFIX;
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i) {
        rr_course_graph_cursor_t *cursor = &race->opponents[i];
        if (!rr_course_graph_cursor_init(graph, cursor) ||
            !graph_race_advance_opponent(graph, cursor,
                game->opponents[i].distance,
                game->opponents[i].lane,
                game->opponents[i].distance,
                game->distance)) {
            rr_graph_race_disable(race, game);
            return 0;
        }
        race->previous_distance[i] = game->opponents[i].distance;
    }
    race->active = 1u;
    if (!rr_graph_race_sync(race, game, 0u)) {
        rr_graph_race_disable(race, game);
        return 0;
    }
    return 1;
}

int rr_graph_race_sync(rr_graph_race_t *race, road_game_t *game,
                       rr_u32 player_branch_mask)
{
    unsigned int i, place = 0u;
    int player_remaining;
    rr_u32 finish;
    if (!race || !race->active || !race->graph || !game) return 0;
    if (race->player_branch_mask != player_branch_mask ||
        race->path_prefix[race->graph->root] == RR_GRAPH_RACE_NO_PREFIX)
        if (!graph_race_build_path(race, player_branch_mask)) return 0;
    finish = rr_course_graph_route_finish(race->graph,
                                          player_branch_mask);
    if (!finish || finish > 0x7fffffu) return 0;
    player_remaining = (int)(finish * 256u) - (int)game->distance;
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i) {
        road_rider_t *opponent = &game->opponents[i];
        rr_course_graph_cursor_t *cursor = &race->opponents[i];
        rr_u32 prefix, delta;
        int opponent_remaining;
        if (opponent->distance < race->previous_distance[i]) return 0;
        delta = opponent->distance - race->previous_distance[i];
        if (!graph_race_advance_opponent(race->graph, cursor,
                delta, opponent->lane, opponent->distance,
                game->distance) ||
            !rr_course_graph_cursor_finish_distance(race->graph,
                cursor, &opponent_remaining)) return 0;
        prefix = race->path_prefix[cursor->node];
        opponent->route_visible =
            prefix != RR_GRAPH_RACE_NO_PREFIX ? 1u : 0u;
        opponent->graph_curve_speed = opponent->route_visible ?
            opponent->base_speed :
            rr_course_graph_curve_speed(race->graph, cursor,
                                        opponent->base_speed);
        if (opponent->route_visible &&
            race->graph->nodes[cursor->node].kind == 0u) {
            if (prefix > (0xffffffffu - cursor->local_8_8) / 256u)
                return 0;
            opponent->distance = prefix * 256u + cursor->local_8_8;
        }
        race->previous_distance[i] = opponent->distance;
        /* The upstream rank compares track position for riders sharing a
         * lane topology and uses finish distance across separate lanes. */
        if (opponent->route_visible ?
            opponent->distance > game->distance :
            opponent_remaining < player_remaining) ++place;
    }
    game->graph_rank = place;
    game->graph_rank_enabled = 1u;
    return 1;
}

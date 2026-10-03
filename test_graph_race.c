#include <assert.h>
#include <stdio.h>
#include "road_graph_race.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *f = (FILE *)user;
    return fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, f) == size;
}

int main(int argc, char **argv)
{
    static rr_course_graph_t graph;
    static rr_graph_race_t race;
    static road_game_t game;
    rr_rsrc_file_t catalog;
    rr_u32 fork_prefix = 0u, alternate_run = 0u;
    rr_u8 cursor, alternate;
    int fork_edge;
    rr_u32 steps;
    FILE *file;
    long size;
    int remaining;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file,
                                     (rr_u32)size));
    assert(rr_course_graph_load(&catalog, 0u, &graph));
    cursor = graph.root;
    for (steps = 0u; steps < graph.node_count; ++steps) {
        const rr_course_graph_node_t *node = &graph.nodes[cursor];
        if (node->kind == 1u) break;
        assert(node->kind == 0u || node->kind == 2u);
        if (node->kind == 0u) fork_prefix += node->sample_count;
        cursor = node->next[0];
    }
    assert(steps < graph.node_count && fork_prefix > 1u);
    fork_edge = graph.nodes[cursor].fork_edge_lane;
    alternate = graph.nodes[cursor].next[1];
    cursor = alternate;
    for (steps = 0u; steps < graph.node_count; ++steps) {
        const rr_course_graph_node_t *node = &graph.nodes[cursor];
        if (node->kind == 2u) break;
        assert(node->kind == 0u);
        alternate_run += node->sample_count;
        cursor = node->next[0];
    }
    assert(steps < graph.node_count && alternate_run > 3u);
    {
        rr_u32 chosen, other, swapped, back;
        assert(rr_course_graph_branch_span(&graph, 0u, 0u,
                                           &chosen, &other));
        assert(other == alternate_run);
        assert(rr_course_graph_branch_span(&graph, 1u, 0u,
                                           &swapped, &back));
        assert(chosen == back && other == swapped);
    }

    road_game_init(&game);
    game.opponents[0].distance = (fork_prefix - 1u) * 256u;
    game.opponents[0].lane = fork_edge;
    game.opponents[1].distance = (fork_prefix - 1u) * 256u;
    game.opponents[1].lane = fork_edge - 1;
    assert(rr_graph_race_reset(&race, &graph, &game));
    assert(game.graph_rank_enabled);
    game.distance = (fork_prefix + 1u) * 256u;
    game.opponents[0].distance += 3u * 256u;
    game.opponents[1].distance += 3u * 256u;
    assert(rr_graph_race_sync(&race, &game, 0u));
    assert(!game.opponents[0].route_visible);
    assert(game.opponents[1].route_visible);
    assert(race.opponents[0].node == alternate);
    assert(game.opponents[0].graph_curve_speed ==
           rr_course_graph_curve_speed(&graph, &race.opponents[0],
               game.opponents[0].base_speed));
    assert(rr_course_graph_cursor_finish_distance(&graph,
        &race.opponents[0], &remaining));
    assert(remaining < (int)(graph.root_finish_samples * 256u));
    assert(road_game_finish_place(&game) == game.graph_rank);

    assert(rr_graph_race_sync(&race, &game, 1u));
    assert(game.opponents[0].route_visible);
    assert(!game.opponents[1].route_visible);
    assert(game.opponents[0].distance ==
           (fork_prefix + 2u) * 256u);
    game.opponents[0].distance += (alternate_run - 2u) * 256u;
    assert(rr_graph_race_sync(&race, &game, 0u));
    assert(game.opponents[0].route_visible);
    assert(game.opponents[0].distance ==
           race.path_prefix[race.opponents[0].node] * 256u +
           race.opponents[0].local_8_8);
    for (steps = 0u; steps < 20u; ++steps) {
        road_game_step(&game, ROAD_ACCEL, 33u);
        assert(rr_graph_race_sync(&race, &game, 0u));
        assert(road_game_finish_place(&game) <= ROAD_OPPONENT_COUNT);
    }
    rr_graph_race_disable(&race, &game);
    assert(!game.graph_rank_enabled &&
           game.opponents[0].route_visible &&
           game.opponents[1].route_visible);
    road_game_init(&game);
    game.opponents[0].distance = (fork_prefix - 1u) * 256u;
    game.opponents[0].lane = fork_edge;
    assert(rr_graph_race_reset(&race, &graph, &game));
    game.opponents[0].distance += 3u * 256u;
    assert(rr_graph_race_sync(&race, &game, 0u));
    assert(game.opponents[0].route_visible);
    assert(race.opponents[0].node != alternate);
    {
        rr_u8 node_index = graph.root;
        rr_u32 prefix = 0u, fork_number = 0u;
        int safe_lane = -110;
        for (steps = 0u; steps < graph.node_count; ++steps) {
            const rr_course_graph_node_t *node = &graph.nodes[node_index];
            if (node->kind == 4u) break;
            if (node->kind == 0u) prefix += node->sample_count;
            if (node->kind == 1u) {
                rr_u32 chosen, other, swapped, back;
                rr_u32 mask;
                assert(rr_course_graph_branch_span(&graph, 0u,
                    fork_number, &chosen, &other));
                assert(rr_course_graph_branch_span(&graph,
                    1u << fork_number, fork_number, &swapped, &back));
                assert(chosen == back && other == swapped);
                for (mask = 0u; mask < 8u; ++mask) {
                    rr_u32 route_span, opposite_span;
                    assert(rr_course_graph_branch_span(&graph, mask,
                        fork_number, &route_span, &opposite_span));
                    assert(route_span ==
                        ((mask & (1u << fork_number)) ? other : chosen));
                    assert(opposite_span ==
                        ((mask & (1u << fork_number)) ? chosen : other));
                }
                printf("  fork %u: branch spans %u / %u samples\n",
                       fork_number, chosen, other);
                assert(prefix > 1u && fork_number < 8u);
                road_game_init(&game);
                game.distance = (prefix - 1u) * 256u;
                game.opponents[0].distance = game.distance;
                game.opponents[0].lane = safe_lane;
                assert(rr_graph_race_reset(&race, &graph, &game));
                game.opponents[0].lane = node->fork_edge_lane;
                game.distance = (prefix + 1u) * 256u;
                game.opponents[0].distance += 3u * 256u;
                assert(rr_graph_race_sync(&race, &game, 0u));
                assert(!game.opponents[0].route_visible);
                assert(rr_graph_race_sync(&race, &game,
                                           1u << fork_number));
                assert(game.opponents[0].route_visible);
                ++fork_number;
            }
            node_index = node->next[0];
            assert(node_index < graph.node_count);
        }
        assert(fork_number > 0u);
    }
    fclose(file);
    puts("Independent graph opponents, fork visibility and rejoin passed.");
    return 0;
}

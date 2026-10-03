#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "road_course_runtime.h"
#include "road_course_graph.h"
#include "road_career.h"
#include "cel_runtime.h"

static signed char curvature[20000], elevation[20000];
static signed char route_curvature[8][10000];
static rr_u32 route_sample_count[8];
static rr_u32 route_branch_samples[8][8];
static rr_u32 route_branch_count[8];
static signed char junction_primary[20000], junction_other[20000];
static signed char junction_primary_raw[20000], junction_other_raw[20000];
static unsigned short left_width[20000], right_width[20000];
static rr_u8 left_margin[20000], right_margin[20000];
static rr_u8 left_depth[20000], right_depth[20000];
static rr_u8 left_edge_resource[20000], right_edge_resource[20000];
static unsigned short left_edge_family[20000], right_edge_family[20000];
static unsigned short all_margin_family[128];
static rr_u8 all_margin_selector[128];
static rr_u32 all_margin_key_count;
static rr_u8 terrain_mode[20000];
static rr_u8 surface_flags[20000];
static road_hill_profile_t hill_profiles[512];
typedef struct hill_key {
    unsigned short family;
    unsigned char selector;
    unsigned char band;
} hill_key_t;
static hill_key_t route_hill_keys[8][64];
static unsigned int route_hill_counts[8];
static road_scenery_placement_t objects[20000];
static road_static_placement_t hazards[5000];
static road_effect_placement_t effects[5000];
static road_crossing_zone_t crossing_zones[500];
static rr_u32 families[128];
static rr_u32 hazard_families[64];
static rr_u8 hazard_frames[64];
static rr_u8 scratch[65536];
static unsigned short sprite_pixels[65536];
static rr_u8 backdrop[360u * 94u];
static unsigned short backdrop_palette[256];

static rr_u32 scenery_capacity(rr_u32 id)
{
    if (id == 168u || id == 242u) return 32768u;
    if (id == 69u || id == 164u || id == 185u) return 24576u;
    return 4096u;
}

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *f = (FILE *)user;
    return fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, f) == size;
}

static void collect_margin_keys(rr_u32 samples)
{
    rr_u32 sample, side;
    for (sample = 0u; sample < samples; ++sample)
        for (side = 0u; side < 2u; ++side) {
            unsigned short family = side ?
                right_edge_family[sample] : left_edge_family[sample];
            rr_u8 selector = side ?
                right_edge_resource[sample] : left_edge_resource[sample];
            rr_u32 key;
            if (!family || selector == 255u) continue;
            for (key = 0u; key < all_margin_key_count; ++key)
                if (all_margin_family[key] == family &&
                    all_margin_selector[key] == selector) break;
            if (key < all_margin_key_count) continue;
            assert(all_margin_key_count < 128u);
            all_margin_family[all_margin_key_count] = family;
            all_margin_selector[all_margin_key_count++] = selector;
        }
}

static void collect_hill_keys(unsigned int mask,
    unsigned int profile_count)
{
    unsigned int p, side, band;
    hill_key_t *keys = route_hill_keys[mask];
    unsigned int *count = &route_hill_counts[mask];
    *count = 0u;
    for (p = 0u; p < profile_count; ++p)
        for (side = 0u; side < 2u; ++side)
            for (band = 0u; band < 3u; ++band) {
                hill_key_t key;
                unsigned int i;
                key.family = hill_profiles[p].family_id[side];
                key.selector = hill_profiles[p].selector[side] & 63u;
                key.band = (unsigned char)band;
                if (!key.family) continue;
                for (i = 0u; i < *count; ++i)
                    if (keys[i].family == key.family &&
                        keys[i].selector == key.selector &&
                        keys[i].band == key.band) break;
                if (i == *count) {
                    assert(*count < 64u);
                    keys[(*count)++] = key;
                }
            }
}

static void check_hill_cache_pairs(void)
{
    unsigned int mask, branch;
    unsigned int maximum = 0u;
    for (mask = 0u; mask < 8u; ++mask)
        for (branch = 0u; branch < 3u; ++branch) {
            unsigned int other = mask ^ (1u << branch);
            unsigned int count = route_hill_counts[mask];
            unsigned int j;
            for (j = 0u; j < route_hill_counts[other]; ++j) {
                const hill_key_t *key = &route_hill_keys[other][j];
                unsigned int i;
                for (i = 0u; i < route_hill_counts[mask]; ++i)
                    if (route_hill_keys[mask][i].family == key->family &&
                        route_hill_keys[mask][i].selector == key->selector &&
                        route_hill_keys[mask][i].band == key->band) break;
                if (i == route_hill_counts[mask]) ++count;
            }
            assert(count <= 32u);
            if (count > maximum) maximum = count;
        }
    printf("  main/alternate RHIL cache maximum: %u/32\n", maximum);
}

static void snapshot_route_curvature(unsigned int mask,
    const rr_course_buffer_t *course)
{
    assert(mask < 8u && course->sample_count <= 10000u);
    memcpy(route_curvature[mask], curvature, course->sample_count);
    route_sample_count[mask] = course->sample_count;
    route_branch_count[mask] = course->branch_count;
    memcpy(route_branch_samples[mask], course->branch_samples,
        course->branch_count * sizeof(course->branch_samples[0]));
}

static void check_bridge_threshold_pairs(const rr_course_graph_t *graph)
{
    unsigned int mask, branch, maximum = 0u;
    unsigned int first_hit = 0u;
    unsigned int reverse_hits = 0u;
    for (mask = 0u; mask < 8u; ++mask)
        for (branch = 0u; branch < route_branch_count[mask]; ++branch) {
            unsigned int other = mask ^ (1u << branch);
            unsigned int fork = route_branch_samples[mask][branch];
            unsigned int at;
            const signed char *first = (mask & (1u << branch)) ?
                route_curvature[other] : route_curvature[mask];
            const signed char *second = (mask & (1u << branch)) ?
                route_curvature[mask] : route_curvature[other];
            int left_step = 0;
            int right_step;
            assert(fork == route_branch_samples[other][branch]);
            assert(fork + 81u <= route_sample_count[mask] &&
                fork + 81u <= route_sample_count[other]);
            right_step = (int)second[fork] - (int)first[fork];
            for (at = fork + 1u; at < fork + 81u; ++at) {
                left_step += right_step;
                right_step += (int)second[at] - (int)first[at];
                if (left_step > 0x18) {
                    ++maximum;
                    if (!mask && !branch && !first_hit)
                        first_hit = at - fork;
                }
            }
            {
                rr_u32 chosen_span, other_span;
                rr_u32 chosen_join, other_join, offset;
                int backward_left, backward_right, diff;
                assert(rr_course_graph_branch_span(graph, mask, branch,
                    &chosen_span, &other_span));
                chosen_join = fork + chosen_span;
                other_join = fork + other_span;
                assert(chosen_join <= route_sample_count[mask] &&
                    other_join <= route_sample_count[other] &&
                    chosen_join >= fork + 81u &&
                    other_join >= fork + 81u);
                diff = (int)second[mask & (1u << branch) ?
                    chosen_join - 1u : other_join - 1u] -
                    (int)first[mask & (1u << branch) ?
                    other_join - 1u : chosen_join - 1u];
                backward_left = diff;
                backward_right = -diff;
                reverse_hits += backward_left > 0x18;
                for (offset = 1u; offset < 81u; ++offset) {
                    diff = (int)second[mask & (1u << branch) ?
                        chosen_join - 1u - offset :
                        other_join - 1u - offset] -
                        (int)first[mask & (1u << branch) ?
                        other_join - 1u - offset :
                        chosen_join - 1u - offset];
                    backward_right -= diff;
                    backward_left -= backward_right;
                    reverse_hits += backward_left > 0x18;
                }
            }
        }
    assert(maximum > 0u && reverse_hits > 0u && first_hit > 0u &&
           first_hit < 81u);
    printf("  bridge-threshold hits: forward %u, reverse %u, first fork at %u\n",
        maximum, reverse_hits, first_hit);
}

static rr_u32 margin_child_tag(const rr_rsrc_file_t *file,
    unsigned int family_id, unsigned int selector, unsigned int child,
    rr_u8 *flags)
{
    rr_rsrc_record_t record;
    rr_u8 word[16];
    rr_u32 count, level_at, entry_at, child_at;
    if (!rr_rsrc_find(file, RR_RSRC_TAG('F','A','M',' '),
                      family_id, &record) ||
        !rr_rsrc_read(file, &record, 0u, word, 4u)) return 0u;
    count = rr_rsrc_be32(word);
    if (count <= 5u ||
        !rr_rsrc_read(file, &record, 24u, word, 4u)) return 0u;
    level_at = rr_rsrc_be32(word);
    if (!rr_rsrc_read(file, &record, level_at, word, 4u) ||
        selector >= rr_rsrc_be32(word) ||
        !rr_rsrc_read(file, &record,
            level_at + 4u + selector * 4u, word, 4u)) return 0u;
    entry_at = level_at + rr_rsrc_be32(word);
    if (!rr_rsrc_read(file, &record, entry_at, word, 4u) ||
        child >= rr_rsrc_be32(word) ||
        !rr_rsrc_read(file, &record,
            entry_at + 4u + child * 4u, word, 4u)) return 0u;
    child_at = entry_at + rr_rsrc_be32(word);
    if (!rr_rsrc_read(file, &record, child_at, word, 16u))
        return 0u;
    if (flags) *flags = word[8u];
    return rr_rsrc_be32(word);
}

static void check_graph_cursor(const rr_course_graph_t *graph,
                               rr_u32 mask, rr_u32 finish_sample)
{
    rr_course_graph_cursor_t cursor;
    int remaining;
    assert(rr_course_graph_cursor_init(graph, &cursor));
    assert(rr_course_graph_cursor_advance(graph, &cursor,
                                          finish_sample * 256u, mask));
    assert(rr_course_graph_cursor_finish_distance(graph, &cursor,
                                                  &remaining));
    assert(remaining == 0);
    assert(rr_course_graph_cursor_advance(graph, &cursor, 256u, mask));
    assert(rr_course_graph_cursor_finish_distance(graph, &cursor,
                                                  &remaining));
    assert(remaining == -256);
}

static void check_graph_curves(const rr_course_graph_t *graph,
                               rr_u32 mask, const signed char *path,
                               rr_u32 count)
{
    rr_course_graph_cursor_t cursor;
    rr_u32 sample;
    assert(rr_course_graph_cursor_init(graph, &cursor));
    for (sample = 0u; sample < count; ++sample) {
        int curve;
        assert(rr_course_graph_cursor_advance(graph, &cursor,
                                              sample ? 256u : 0u, mask));
        assert(rr_course_graph_cursor_curvature(graph, &cursor, &curve));
        assert(curve == path[sample]);
        if (cursor.local_8_8 / 256u + 11u <
            graph->nodes[cursor.node].sample_count) {
            rr_u32 i, sum = 0u, reduction;
            unsigned int expected;
            for (i = 8u; i < 12u; ++i) {
                int value = path[sample + i];
                sum += (rr_u32)(value < 0 ? -value : value);
            }
            sum /= 4u;
            reduction = sum > 12u ? (sum - 12u) * 100u / 128u : 0u;
            if (reduction > 50u) reduction = 50u;
            expected = 100u - reduction;
            assert(rr_course_graph_curve_speed(graph, &cursor,
                                                100u) == expected);
        }
    }
}

int main(int argc, char **argv)
{
    static const char *const course_names[5] = {
        "Highway", "Canyon", "City", "Napa", "Medley"
    };
    static const rr_u32 expected_finish[5][ROAD_CAREER_LEVELS] = {
        {1810u, 2650u, 3295u, 4205u, 5566u},
        {1862u, 2719u, 3685u, 4215u, 5361u},
        {1821u, 2840u, 3865u, 4510u, 5419u},
        {1715u, 2565u, 3570u, 4280u, 6164u},
        {1818u, 2719u, 3642u, 4963u, 5532u}
    };
    static const rr_u32 expected_short_zones[5] = {0u, 2u, 0u, 3u, 1u};
    static const rr_u32 expected_no_right_child[5] =
        {0u, 3u, 1u, 0u, 5u};
    static const rr_u32 expected_no_left_child[5] =
        {0u, 0u, 10u, 0u, 0u};
    rr_rsrc_file_t catalog;
    rr_course_buffer_t course = {0};
    rr_course_graph_t graph;
    rr_u32 backdrop_width, backdrop_height;
    FILE *f;
    long size;
    unsigned int course_index;
    assert(argc == 2 || argc == 3);
    for (course_index = 0u; course_index < 5u; ++course_index)
        if (strstr(argv[1], course_names[course_index])) break;
    assert(course_index < 5u);
    f = fopen(argv[1], "rb");
    assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, f, (rr_u32)size));
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
    course.hill_profiles = hill_profiles;
    course.hill_profile_capacity = 512u;
    course.objects = objects;
    course.object_capacity = 20000u;
    course.hazards = hazards;
    course.hazard_capacity = 5000u;
    course.effects = effects;
    course.effect_capacity = 5000u;
    course.crossing_zones = crossing_zones;
    course.crossing_capacity = 500u;
    course.difficulty_level = 0u;
    course.family_ids = families;
    course.family_capacity = 128u;
    course.family_count = 0u;
    course.hazard_family_ids = hazard_families;
    course.hazard_frame_ids = hazard_frames;
    course.hazard_family_capacity = 64u;
    course.hazard_family_count = 0u;
    course.capacity = 20000u;
    memset(left_edge_resource, 0xff, sizeof(left_edge_resource));
    memset(right_edge_resource, 0xff, sizeof(right_edge_resource));
    assert(rr_course_load_primary(&catalog, &course));
    snapshot_route_curvature(0u, &course);
    collect_hill_keys(0u, course.hill_profile_count);
    collect_margin_keys(course.sample_count);
    {
        rr_u32 sample, entry = 0u, pre_end = 0u;
        for (sample = 0u; sample < course.sample_count; ++sample)
            if (terrain_mode[sample] == 0u) {
                entry += surface_flags[sample] == 0x08u;
                pre_end += surface_flags[sample] == 0x10u;
            }
        printf("  textured RSEC margin transitions: entry %u, pre-end %u\n",
            entry, pre_end);
    }
    {
        rr_u32 i, selected = 0u, resident = 0u;
        for (i = 0u; i < course.sample_count; ++i)
        {
            selected += left_edge_resource[i] != 255u &&
                right_edge_resource[i] != 255u;
            resident += left_edge_family[i] != 0u &&
                right_edge_family[i] != 0u;
        }
        assert(selected > 0u);
        assert(resident > 0u);
        printf("  RSLD edge resources: %u IDs, %u resident pairs / %u samples; first %u/%u in families %u/%u\n",
            selected, resident, course.sample_count,
            left_edge_resource[0], right_edge_resource[0],
            left_edge_family[0], right_edge_family[0]);
    }
    assert(course.finish_sample > 0u &&
           course.finish_sample <= course.sample_count);
    assert(course.finish_sample == expected_finish[course_index][0]);
    assert(rr_course_graph_load(&catalog, 0u, &graph));
    {
        rr_rsrc_record_t segments;
        rr_u8 segment[52], rsld[52];
        const rr_course_graph_node_t *root = &graph.nodes[graph.root];
        assert(root->kind == 0u);
        assert(rr_rsrc_find(&catalog,
            RR_RSRC_TAG('S','G','S',' '), 1u, &segments));
        assert(rr_rsrc_read(&catalog, &segments,
            16u + root->segment_index * 52u, segment,
            sizeof(segment)));
        assert(rr_rsrc_read(&catalog, &segments,
            rr_rsrc_be32(segment + 20u), rsld, sizeof(rsld)));
        assert(rr_rsrc_be32(rsld) == RR_RSRC_TAG('R','S','L','D'));
        assert(left_edge_resource[0] == rsld[16u + 16u]);
        assert(right_edge_resource[0] == rsld[16u + 17u]);
    }
    assert(graph.node_count > course.segment_count);
    assert(rr_course_graph_route_finish(&graph, 0u) ==
           course.finish_sample);
    assert(graph.root_finish_samples == course.finish_sample);
    check_graph_cursor(&graph, 0u, course.finish_sample);
    check_graph_curves(&graph, 0u, curvature, course.sample_count);
    {
        rr_u32 branch;
        assert(course.branch_count > 0u);
        for (branch = 0u; branch < course.branch_count; ++branch) {
            rr_u8 node_index = graph.root;
            rr_u32 sample = course.branch_samples[branch], step;
            rr_u32 primary_count = course.sample_count;
            rr_u32 visited_forks = 0u;
            unsigned int old_left, first_left, first_right;
            unsigned int main_channel =
                course.branch_main_channels[branch];
            assert(sample > 0u);
            memcpy(junction_primary, elevation, primary_count);
            old_left = left_width[sample - 1u];
            first_left = left_width[sample];
            first_right = right_width[sample];
            for (step = 0u; step < graph.node_count; ++step) {
                const rr_course_graph_node_t *node =
                    &graph.nodes[node_index];
                if (node->kind == 1u) {
                    if (visited_forks++ == branch) break;
                }
                node_index = node->next[0];
                assert(node_index < graph.node_count);
            }
            assert(step < graph.node_count);
            {
                rr_rsrc_record_t nodes;
                rr_u8 raw[32], join = graph.nodes[node_index].next[0];
                rr_u32 hops;
                for (hops = 0u; hops < graph.node_count; ++hops) {
                    const rr_course_graph_node_t *entry = &graph.nodes[join];
                    if (entry->kind == 2u) break;
                    assert(entry->kind == 0u);
                    join = entry->next[0];
                    assert(join < graph.node_count);
                }
                assert(hops < graph.node_count);
                assert(rr_rsrc_find(&catalog,
                    RR_RSRC_TAG('N','O','D',' '), 1u, &nodes));
                assert(rr_rsrc_read(&catalog, &nodes,
                    graph.nodes[join].resource_offset, raw, sizeof(raw)));
                assert(raw[24] == main_channel);
                assert(raw[25] == (unsigned char)(1u - main_channel));
            }
            assert(rr_course_load_selected(&catalog, &course,
                                           1u << branch));
            collect_margin_keys(course.sample_count);
            {
                rr_u32 chosen_span, other_span, i;
                rr_u32 other_count = course.sample_count;
                memcpy(junction_other, elevation, other_count);
                memcpy(junction_primary_raw, junction_primary,
                       primary_count);
                memcpy(junction_other_raw, junction_other, other_count);
                assert(rr_course_graph_branch_span(&graph, 0u, branch,
                                                   &chosen_span,
                                                   &other_span));
                assert(rr_course_reconcile_fork_elevation(
                    junction_primary, primary_count,
                    junction_other, other_count, sample,
                    chosen_span, other_span, main_channel == 0u));
                for (i = 0u; i < RR_COURSE_JUNCTION_SAMPLES; ++i) {
                    assert(junction_primary[sample + i] ==
                           junction_other[sample + i]);
                    assert(junction_primary[sample + chosen_span -
                        RR_COURSE_JUNCTION_SAMPLES + i] ==
                           junction_other[sample + other_span -
                        RR_COURSE_JUNCTION_SAMPLES + i]);
                }
                assert(junction_primary[sample - 1u] ==
                       elevation[sample - 1u]);
                assert(rr_course_reconcile_fork_elevation(
                    junction_other_raw, other_count,
                    junction_primary_raw, primary_count, sample,
                    other_span, chosen_span, 0));
                for (i = 0u; i < RR_COURSE_JUNCTION_SAMPLES; ++i) {
                    assert(junction_other_raw[sample + i] ==
                           junction_primary_raw[sample + i]);
                    assert(junction_other_raw[sample + other_span -
                        RR_COURSE_JUNCTION_SAMPLES + i] ==
                           junction_primary_raw[sample + chosen_span -
                        RR_COURSE_JUNCTION_SAMPLES + i]);
                }
            }
            if (main_channel == 1u) {
                first_left = left_width[sample];
                first_right = right_width[sample];
            }
            assert(graph.nodes[node_index].fork_edge_lane ==
                ((int)first_left + (int)first_right - (int)old_left) *
                111 / 512);
            assert(rr_course_load_primary(&catalog, &course));
        }
        printf("  graph fork width thresholds: %u streamed matches\n",
               course.branch_count);
    }
    {
        rr_u32 node_index, post_finish = 0u, prefix = 0u, step;
        for (node_index = 0u; node_index < graph.node_count;
             ++node_index)
            if (graph.nodes[node_index].kind == 0u &&
                graph.nodes[node_index].finish_distance_samples < 0)
                ++post_finish;
        assert(post_finish > 0u);
        node_index = graph.root;
        for (step = 0u; step < graph.node_count; ++step) {
            const rr_course_graph_node_t *node = &graph.nodes[node_index];
            if (node->kind == 4u) break;
            if (node->kind == 0u) {
                assert(node->finish_distance_samples ==
                       (int)graph.root_finish_samples - (int)prefix);
                prefix += node->sample_count;
            }
            node_index = node->next[node->kind == 1u ?
                                    node->main_channel : 0u];
            assert(node_index < graph.node_count);
        }
        assert(step < graph.node_count);
        printf("  graph nodes: %u, RPTH curves: %u, post-finish segments: %u\n",
               graph.node_count, graph.curve_count, post_finish);
    }
    printf("  level 1 finish: %u / %u samples\n",
           course.finish_sample, course.sample_count);
    {
        rr_u32 sample;
        for (sample = 0u; sample < course.sample_count; ++sample)
            assert(terrain_mode[sample] == 0u ||
                   terrain_mode[sample] == 1u ||
                   terrain_mode[sample] == 4u ||
                   terrain_mode[sample] == 255u);
    }
    assert(course.effect_count > 0u && course.effect_count <= 1000u);
    assert(course.crossing_count > 0u && course.crossing_count <= 256u);
    {
        rr_u32 zone_index, short_count = 0u;
        rr_u32 no_right_child = 0u, no_left_child = 0u;
        for (zone_index = 0u; zone_index < course.crossing_count;
             ++zone_index) {
            assert(course.crossing_zones[zone_index].mode >= 1u &&
                   course.crossing_zones[zone_index].mode <= 3u);
            assert(course.crossing_zones[zone_index].start_sample <=
                   course.crossing_zones[zone_index].end_sample);
            if (zone_index) {
                assert(course.crossing_zones[zone_index - 1u]
                       .start_sample <=
                       course.crossing_zones[zone_index].start_sample);
                assert(course.crossing_zones[zone_index - 1u]
                       .end_sample <=
                       course.crossing_zones[zone_index].end_sample);
            }
            if (course.crossing_zones[zone_index].short_timing) {
                ++short_count;
                assert(!course.crossing_zones[zone_index]
                       .preserve_child_updates);
                assert(terrain_mode[course.crossing_zones[zone_index]
                       .start_sample] == 4u);
            }
            if (course.crossing_zones[zone_index].start_sample <
                course.sample_count &&
                right_width[course.crossing_zones[zone_index]
                    .start_sample] < 256u)
                ++no_right_child;
            if (course.crossing_zones[zone_index].end_sample &&
                course.crossing_zones[zone_index].end_sample - 1u <
                course.sample_count &&
                left_width[course.crossing_zones[zone_index]
                    .end_sample - 1u] < 256u)
                ++no_left_child;
        }
        printf("  short RSEC phases: %u/%u zones\n", short_count,
               course.crossing_count);
        printf("  RSEC width-gated children: right %u, left %u\n",
               no_right_child, no_left_child);
        assert(short_count == expected_short_zones[course_index]);
        assert(no_right_child == expected_no_right_child[course_index]);
        assert(no_left_child == expected_no_left_child[course_index]);
    }
    {
        rr_u32 effect_index;
        for (effect_index = 0u; effect_index < course.effect_count;
             ++effect_index) {
            assert(course.effects[effect_index].sample <
                   course.sample_count);
            assert(course.effects[effect_index].travel_mode <= 15u);
            if (effect_index)
                assert(course.effects[effect_index - 1u].sample <=
                       course.effects[effect_index].sample);
        }
        for (effect_index = 1u; effect_index < course.hazard_count;
             ++effect_index)
            assert(course.hazards[effect_index - 1u].sample <=
                   course.hazards[effect_index].sample);
    }
    {
        rr_u32 branch;
        for (branch = 0u; branch < course.branch_count; ++branch) {
            assert(course.branch_main_channels[branch] <= 1u);
            assert(course.branch_alternate_channels[branch] <= 1u);
            assert(course.branch_main_channels[branch] !=
                   course.branch_alternate_channels[branch]);
        }
    }
    printf("%s: %u samples, %u segments, %u objects, %u hazards, %u effects, %u crossing zones, %u families, %u hazard variants\n",
           argv[1], course.sample_count, course.segment_count,
           course.object_count, course.hazard_count, course.effect_count,
           course.crossing_count,
           course.family_count,
           course.hazard_family_count);
    {
        rr_u32 e, j, unique_count = 0u;
        unsigned short unique_ids[1000];
        for (e = 0u; e < course.effect_count; ++e) {
            unsigned short id = course.effects[e].family_id;
            if (course.effects[e].travel_mode >= 11u || !id) continue;
            for (j = 0u; j < unique_count; ++j)
                if (unique_ids[j] == id) break;
            if (j == unique_count) unique_ids[unique_count++] = id;
        }
        printf("  rider families:");
        for (j = 0u; j < unique_count; ++j)
            printf(" %u", (unsigned int)unique_ids[j]);
        printf("\n");
    }
    {
        rr_u32 reachable = 0u, h;
        for (h = 0u; h < course.hazard_count; ++h)
            if (course.hazards[h].lateral >= -153 &&
                course.hazards[h].lateral <= 153)
                reachable++;
        printf("  lane-reachable hazard placements: %u\n", reachable);
    }
    assert(rr_cel_load_backdrop(&catalog, 2u, backdrop, sizeof(backdrop),
                                 backdrop_palette, &backdrop_width,
                                 &backdrop_height));
    printf("  backdrop: %ux%u\n", backdrop_width, backdrop_height);
    {
        rr_u32 mask;
        for (mask = 1u; mask < 8u; ++mask) {
            assert(rr_course_load_selected(&catalog, &course, mask));
            snapshot_route_curvature(mask, &course);
            collect_hill_keys(mask, course.hill_profile_count);
            collect_margin_keys(course.sample_count);
            {
                rr_u32 branch;
                for (branch = 0u; branch < course.branch_count; ++branch) {
                    rr_u32 chosen, other, fork, join, sample;
                    assert(course.branch_main_channels[branch] !=
                           course.branch_alternate_channels[branch]);
                    assert(rr_course_graph_branch_span(&graph, mask,
                        branch, &chosen, &other));
                    fork = course.branch_samples[branch];
                    join = fork + chosen;
                    for (sample = fork;
                         sample < fork + 81u &&
                         sample < course.sample_count; ++sample)
                        assert(terrain_mode[sample] != 1u);
                    for (sample = join >= 81u ? join - 81u : 0u;
                         sample < join && sample < course.sample_count;
                         ++sample) assert(terrain_mode[sample] != 1u);
                }
            }
            printf("  branch mask %u: %u segments, %u samples, %u objects, %u hazards, %u forks\n",
                   mask, course.segment_count, course.sample_count,
                   course.object_count, course.hazard_count,
                   course.branch_count);
            assert(course.sample_count <= 10000u);
            assert(course.finish_sample > 0u &&
                   course.finish_sample <= course.sample_count);
            assert(course.object_count <= 8000u);
            assert(course.hazard_count <= 1000u);
            assert(course.effect_count <= 1000u);
            {
                rr_u32 sorted_index;
                for (sorted_index = 1u;
                     sorted_index < course.effect_count; ++sorted_index)
                    assert(course.effects[sorted_index - 1u].sample <=
                           course.effects[sorted_index].sample);
                for (sorted_index = 1u;
                     sorted_index < course.hazard_count; ++sorted_index)
                    assert(course.hazards[sorted_index - 1u].sample <=
                           course.hazards[sorted_index].sample);
            }
            assert(course.family_count <= 32u);
            assert(course.hazard_family_count <= 64u);
            {
                rr_u32 level;
                rr_u32 level_zero_finish = course.finish_sample;
                assert(course.finish_sample < course.sample_count);
                assert(rr_course_graph_load(&catalog, 0u, &graph));
                assert(rr_course_graph_route_finish(&graph, mask) ==
                       level_zero_finish);
                check_graph_curves(&graph, mask, curvature,
                                   course.sample_count);
                check_graph_cursor(&graph, mask, level_zero_finish);
                for (level = 1u; level < ROAD_CAREER_LEVELS; ++level) {
                    course.difficulty_level = level;
                    assert(rr_course_load_selected(&catalog, &course,
                                                   mask));
                    assert(rr_course_graph_load(&catalog, level, &graph));
                    assert(rr_course_graph_route_finish(&graph, mask) ==
                           course.finish_sample);
                    assert(graph.root_finish_samples ==
                           expected_finish[course_index][level]);
                    check_graph_cursor(&graph, mask,
                                       course.finish_sample);
                    assert(course.finish_sample > 0u &&
                           course.finish_sample < course.sample_count);
                }
                course.difficulty_level = 0u;
            }
        }
        {
            rr_u32 level;
            for (level = 1u; level < ROAD_CAREER_LEVELS; ++level) {
                course.difficulty_level = level;
                assert(rr_course_load_primary(&catalog, &course));
                assert(course.sample_count <= 10000u);
                assert(course.finish_sample > 0u &&
                       course.finish_sample <= course.sample_count);
                assert(course.finish_sample ==
                       expected_finish[course_index][level]);
                printf("  level %u finish: %u / %u samples\n",
                       level + 1u, course.finish_sample,
                       course.sample_count);
                assert(course.object_count <= 8000u);
                assert(course.hazard_count <= 1000u);
                assert(course.effect_count <= 1000u);
            }
            course.difficulty_level = 0u;
        }
        check_hill_cache_pairs();
        assert(rr_course_graph_load(&catalog, 0u, &graph));
        check_bridge_threshold_pairs(&graph);
        assert(rr_course_load_primary(&catalog, &course));
    }
    printf("  all-route RSLD resource keys: %u\n",
           all_margin_key_count);
    fclose(f);
    if (argc == 3) {
        rr_rsrc_file_t family_catalog;
        rr_cel_image_t image;
        rr_u32 i, decoded = 0u;
        f = fopen(argv[2], "rb");
        assert(f && fseek(f, 0, SEEK_END) == 0);
        size = ftell(f);
        assert(size > 0 && rr_rsrc_open(&family_catalog, read_at, f,
                                         (rr_u32)size));
        image.pixels = sprite_pixels;
        {
            unsigned short keys_family[128];
            rr_u8 keys_selector[128];
            rr_u32 key_count = 0u, primary_ok = 0u, fill_ok = 0u;
            rr_u32 sample, side;
            for (sample = 0u; sample < course.sample_count; ++sample)
                for (side = 0u; side < 2u; ++side) {
                    unsigned short family = side ?
                        right_edge_family[sample] :
                        left_edge_family[sample];
                    rr_u8 selector = side ?
                        right_edge_resource[sample] :
                        left_edge_resource[sample];
                    rr_u32 key, child;
                    if (!family || selector == 255u) continue;
                    for (key = 0u; key < key_count; ++key)
                        if (keys_family[key] == family &&
                            keys_selector[key] == selector) break;
                    if (key < key_count) continue;
                    assert(key_count < 128u);
                    keys_family[key_count] = family;
                    keys_selector[key_count++] = selector;
                    for (child = 2u; child <= 3u; ++child) {
                        int decoded =
                            rr_family_decode_surface_tile_group_size(
                                &family_catalog, family, 5u,
                                selector & 63u, child, 32u, 16u,
                                &image, 65536u, scratch,
                                sizeof(scratch));
                        rr_u8 flags = 0u;
                        rr_u32 tag = margin_child_tag(&family_catalog,
                            family, selector & 63u, child, &flags);
                        assert(decoded ==
                            (tag == RR_RSRC_TAG('C','L','G','P')));
                        /* The five EU courses use height-threshold CELs
                         * for both margin children. */
                        if (decoded) assert(flags == 0xe1u);
                        if (decoded) {
                            if (child == 2u) ++primary_ok;
                            else ++fill_ok;
                        }
                    }
                }
            printf("  RSLD CEL child 2/3: %u/%u, %u/%u unique keys\n",
                primary_ok, key_count, fill_ok, key_count);
        }
        for (i = 0u; i < course.family_count; ++i) {
            int prefer_last = families[i] == 171u;
            rr_u32 capacity = scenery_capacity(families[i]);
            if (rr_family_load_frame(&family_catalog, families[i],
                                     prefer_last, &image, capacity,
                                     scratch, sizeof(scratch)) ||
                rr_family_load_frame(&family_catalog, families[i],
                                     !prefer_last, &image, capacity,
                                     scratch, sizeof(scratch))) {
                ++decoded;
            } else {
                printf("  undecoded family %u\n", families[i]);
            }
        }
        printf("  decoded scenery: %u/%u\n", decoded,
               course.family_count);
        assert(decoded == course.family_count);
        {
            rr_u32 e, rider_decoded = 0u, rider_second = 0u;
            rr_u32 rider_pixels = 0u;
            rr_u32 standing = 0u, falling = 0u, moving = 0u;
            rr_u32 named_pixels = 0u;
            rr_u32 minimum_ticks = 0xffffffffu, maximum_ticks = 0u;
            static const rr_u32 state_keys[29] = {
                RR_RSRC_TAG('S','t','n','d'),
                RR_RSRC_TAG('M','o','v','1'),
                RR_RSRC_TAG('M','o','v','2'),
                RR_RSRC_TAG('M','o','v','3'),
                RR_RSRC_TAG('M','o','v','4'),
                RR_RSRC_TAG('M','o','v','5'),
                RR_RSRC_TAG('M','o','v','6'),
                RR_RSRC_TAG('S','h','k','1'),
                RR_RSRC_TAG('S','h','k','2'),
                RR_RSRC_TAG('F','a','l','1'),
                RR_RSRC_TAG('F','a','l','2'),
                RR_RSRC_TAG('F','a','l','3'),
                RR_RSRC_TAG('F','a','l','4'),
                RR_RSRC_TAG('F','a','l','5'),
                RR_RSRC_TAG('F','a','l','6'),
                RR_RSRC_TAG('A','t','k','1'),
                RR_RSRC_TAG('A','t','k','2'),
                RR_RSRC_TAG('F','s','t','1'),
                RR_RSRC_TAG('F','s','t','2'),
                RR_RSRC_TAG('F','s','t','3'),
                RR_RSRC_TAG('F','s','t','4'),
                RR_RSRC_TAG('F','s','t','5'),
                RR_RSRC_TAG('F','s','t','6'),
                RR_RSRC_TAG('F','l','g','1'),
                RR_RSRC_TAG('F','l','g','2'),
                RR_RSRC_TAG('F','l','g','3'),
                RR_RSRC_TAG('F','l','g','4'),
                RR_RSRC_TAG('F','l','g','5'),
                RR_RSRC_TAG('F','l','g','6')
            };
            unsigned short ids[1000];
            rr_u32 indices[1000], unique_count = 0u;
            for (e = 0u; e < course.effect_count; ++e) {
                rr_u32 j, index = course.effects[e].family_inventory & 63u;
                unsigned short id = course.effects[e].family_id;
                rr_rsrc_record_t record;
                rr_u8 *family;
                if (!id || course.effects[e].travel_mode >= 11u) continue;
                for (j = 0u; j < unique_count; ++j)
                    if (ids[j] == id && indices[j] == index) break;
                if (j < unique_count) continue;
                ids[unique_count] = id;
                indices[unique_count++] = index;
                if (!rr_rsrc_find(&family_catalog,
                                  RR_RSRC_TAG('F','A','M',' '),
                                  id, &record)) continue;
                family = (rr_u8 *)malloc(record.size);
                assert(family && rr_rsrc_read(&family_catalog, &record, 0u,
                                               family, record.size));
                if (rr_family_decode_rider_frame(family, record.size,
                        index, 0u, &image, 65536u)) {
                    rider_decoded++;
                    rider_pixels += image.width * image.height;
                    if (rr_family_decode_rider_frame(family, record.size,
                            index, 1u, &image, 65536u)) {
                        rider_second++;
                        rider_pixels += image.width * image.height;
                    }
                }
                if (rr_family_decode_rider_named_frame(family,
                        record.size, index,
                        RR_RSRC_TAG('S','t','n','d'),
                        &image, 65536u, 0)) standing++;
                if (rr_family_decode_rider_named_frame(family,
                        record.size, index,
                        RR_RSRC_TAG('F','a','l','1'),
                        &image, 65536u, 0)) falling++;
                if (rr_family_decode_rider_named_frame(family,
                        record.size, index,
                        RR_RSRC_TAG('M','o','v','1'),
                        &image, 65536u, 0)) moving++;
                for (j = 0u; j < 29u; ++j) {
                    rr_u32 ticks = 0u;
                    if (rr_family_decode_rider_named_frame(family,
                            record.size, index, state_keys[j],
                            &image, 65536u, &ticks)) {
                        named_pixels += image.width * image.height;
                        assert(ticks > 0u);
                        if (ticks < minimum_ticks) minimum_ticks = ticks;
                        if (ticks > maximum_ticks) maximum_ticks = ticks;
                    }
                }
                free(family);
            }
            printf("  rider frame pairs: %u/%u, second frames %u, RGB565 bytes %u\n",
                   rider_decoded, unique_count, rider_second,
                   rider_pixels * 2u);
            printf("  CANS rider keys: stand %u, move %u, fall %u\n",
                   standing, moving, falling);
            printf("  selected CANS frame bytes: %u\n",
                   named_pixels * 2u);
            printf("  CANS frame duration ticks: %u-%u\n",
                   minimum_ticks, maximum_ticks);
            assert(rider_decoded > 0u && rider_second > 0u);
            assert(standing > 0u && falling > 0u);
        }
        decoded = 0u;
        {
        rr_u32 fallback = 0u, oversized = 0u;
        rr_u32 hotspot_variants = 0u, hotspot_boxes = 0u;
        rr_u32 scale_counts[3] = {0u, 0u, 0u};
        rr_u32 anchor_counts[3] = {0u, 0u, 0u};
        rr_u32 extra_scale_bytes = 0u;
        for (i = 0u; i < course.hazard_family_count; ++i) {
            rr_rsrc_record_t record;
            rr_u8 *family;
            if (!rr_rsrc_find(&family_catalog,
                              RR_RSRC_TAG('F','A','M',' '),
                              hazard_families[i], &record)) continue;
            family = (rr_u8 *)malloc(record.size);
            assert(family && rr_rsrc_read(&family_catalog, &record, 0u,
                                          family, record.size));
            {
                rr_u32 pixel, visible = 0u;
                rr_cel_hotspot_box_t boxes[2];
                rr_u32 box_count;
                int kind = rr_family_decode_static_frame(family,
                    record.size, hazard_frames[i], &image, 4096u) ? 1 : 0;
                if (!kind && rr_family_decode_static_frame(family,
                    record.size, hazard_frames[i], &image, 8192u)) {
                    kind = 1;
                    ++oversized;
                }
                if (!kind)
                    kind = rr_family_decode_hazard_preview(
                        &family_catalog, family, record.size,
                        hazard_families[i], hazard_frames[i],
                        &image, 4096u, scratch, sizeof(scratch));
                if (kind == 1) ++decoded;
                else if (kind == 2) {
                    ++fallback;
                    printf("    family preview FAM %u frame %u\n",
                        hazard_families[i], hazard_frames[i]);
                }
                assert(kind && image.width * image.height > 1u);
                for (pixel = 0u; pixel < image.width * image.height;
                     ++pixel)
                    if (image.pixels[pixel] != RR_CEL_TRANSPARENT)
                        ++visible;
                assert(visible);
                box_count = rr_family_decode_static_hotspots(family,
                    record.size, hazard_families[i], hazard_frames[i],
                    boxes);
                if (box_count) ++hotspot_variants;
                hotspot_boxes += box_count;
                for (pixel = 0u; pixel < 3u; ++pixel) {
                    rr_cel_anchor_t anchor;
                    if (rr_family_decode_static_frame_bucket(family,
                            record.size, hazard_frames[i], pixel,
                            &image, 32768u)) {
                        ++scale_counts[pixel];
                        if (pixel != 1u)
                            extra_scale_bytes +=
                                image.width * image.height * 2u;
                    }
                    if (rr_family_decode_static_anchor_bucket(family,
                            record.size, hazard_frames[i], pixel,
                            &anchor))
                        ++anchor_counts[pixel];
                }
            }
            free(family);
        }
        printf("  decoded static hazard variants: %u/%u (%u large); family fallback: %u\n",
               decoded, course.hazard_family_count, oversized, fallback);
        printf("  nearest-frame HSPT collision boxes: %u variants, %u boxes\n",
               hotspot_variants, hotspot_boxes);
        printf("  roadside CANS scales: %u/%u/%u frames; %u extra RGB565 bytes\n",
               scale_counts[0], scale_counts[1], scale_counts[2],
               extra_scale_bytes);
        printf("  roadside frame anchors: %u/%u/%u\n",
               anchor_counts[0], anchor_counts[1], anchor_counts[2]);
        assert(scale_counts[0] == scale_counts[1] &&
               scale_counts[1] == scale_counts[2]);
        assert(scale_counts[0] + (course_index == 0u ? 1u : 0u) ==
               course.hazard_family_count);
        assert(decoded + fallback == course.hazard_family_count);
        }
        {
            rr_rsrc_file_t route_catalog;
            FILE *route = fopen(argv[1], "rb");
            rr_u32 mask;
            assert(route && fseek(route, 0, SEEK_END) == 0);
            size = ftell(route);
            assert(size > 0 && rr_rsrc_open(&route_catalog, read_at,
                                             route, (rr_u32)size));
            for (mask = 1u; mask < 8u; ++mask) {
                rr_u32 matched = 0u;
                assert(rr_course_load_selected(&route_catalog, &course,
                                                mask));
                for (i = 0u; i < course.family_count; ++i) {
                    int prefer_last = families[i] == 171u;
                    rr_u32 capacity = scenery_capacity(families[i]);
                    if (!rr_family_load_frame(&family_catalog, families[i],
                                              prefer_last, &image, capacity,
                                              scratch, sizeof(scratch)) &&
                        !rr_family_load_frame(&family_catalog, families[i],
                                              !prefer_last, &image, capacity,
                                              scratch, sizeof(scratch))) {
                        fprintf(stderr, "undecoded branch family %u mask %u\n",
                                families[i], mask);
                        return 2;
                    }
                    ++matched;
                }
                printf("  branch mask %u scenery: %u/%u\n", mask,
                       matched, course.family_count);
            }
            fclose(route);
        }
        fclose(f);
    }
    return 0;
}

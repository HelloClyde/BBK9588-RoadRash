#include "road_course_graph.h"

#define RR_GRAPH_SEGMENT_BYTES 52u
#define RR_GRAPH_NODE_BYTES 32u
#define RR_GRAPH_MAX_RLAN_ENTRIES 64u

static int graph_segment_width(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               rr_u32 segment_index, rr_u32 sample_count,
                               rr_u32 target_sample,
                               unsigned int *left, unsigned int *right)
{
    rr_u8 segment[RR_GRAPH_SEGMENT_BYTES], head[16], entry[20], next[20];
    rr_u32 at, count, index = 0u, sample;
    int left_acc, right_acc;
    if (target_sample >= sample_count ||
        !rr_rsrc_read(file, segments,
            16u + segment_index * RR_GRAPH_SEGMENT_BYTES,
            segment, sizeof(segment))) return 0;
    at = rr_rsrc_be32(segment + 24u);
    if (!rr_rsrc_read(file, segments, at, head, sizeof(head)) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','L','A','N')) return 0;
    count = rr_rsrc_be32(head + 8u);
    if (!count || count > RR_GRAPH_MAX_RLAN_ENTRIES ||
        rr_rsrc_be32(head + 4u) != 16u + count * 20u ||
        !rr_rsrc_read(file, segments, at + 16u, entry,
                      sizeof(entry))) return 0;
    left_acc = (int)entry[8] << 16;
    right_acc = (int)entry[9] << 16;
    if (count > 1u && !rr_rsrc_read(file, segments,
        at + 36u, next, sizeof(next))) return 0;
    for (sample = 1u; sample <= target_sample; ++sample) {
        rr_u32 end = rr_rsrc_be32(entry + 4u);
        if (sample > end) {
            if (index + 1u < count &&
                sample == rr_rsrc_be32(next)) {
                rr_u32 byte;
                for (byte = 0u; byte < sizeof(entry); ++byte)
                    entry[byte] = next[byte];
                ++index;
                if (index + 1u < count &&
                    !rr_rsrc_read(file, segments,
                        at + 16u + (index + 1u) * 20u,
                        next, sizeof(next))) return 0;
            }
        } else if (sample < end) {
            left_acc += (int)rr_rsrc_be32(entry + 12u);
            right_acc += (int)rr_rsrc_be32(entry + 16u);
        } else {
            left_acc = (int)entry[10] << 16;
            right_acc = (int)entry[11] << 16;
        }
    }
    if (left_acc < 0 || right_acc < 0 ||
        left_acc / 256 > 65535 || right_acc / 256 > 65535)
        return 0;
    *left = (unsigned int)(left_acc >> 8);
    *right = (unsigned int)(right_acc >> 8);
    return 1;
}

static int graph_find_or_add(rr_course_graph_t *graph, rr_u32 offset,
                             rr_u32 resource_size, rr_u8 *index)
{
    rr_u32 i;
    if (offset < 20u || offset > resource_size - 8u) return 0;
    for (i = 0u; i < graph->node_count; ++i)
        if (graph->nodes[i].resource_offset == offset) {
            *index = (rr_u8)i;
            return 1;
        }
    if (graph->node_count >= RR_COURSE_GRAPH_MAX_NODES) return 0;
    *index = (rr_u8)graph->node_count++;
    graph->nodes[*index].resource_offset = offset;
    graph->nodes[*index].next[0] = RR_COURSE_GRAPH_NONE;
    graph->nodes[*index].next[1] = RR_COURSE_GRAPH_NONE;
    return 1;
}

static int graph_segment_gate(const rr_rsrc_file_t *file,
                              const rr_rsrc_record_t *segments,
                              rr_u32 segment_index, rr_u32 sample_count,
                              unsigned int difficulty_level,
                              rr_u32 *gate)
{
    rr_u8 row[RR_GRAPH_SEGMENT_BYTES], head[20], entry[12];
    rr_u32 path_at, schedule_at, count, i;
    if (!rr_rsrc_read(file, segments,
                      16u + segment_index * RR_GRAPH_SEGMENT_BYTES,
                      row, sizeof(row))) return 0;
    path_at = rr_rsrc_be32(row + 4u);
    if (!rr_rsrc_read(file, segments, path_at, head, 16u) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','P','T','H') ||
        rr_rsrc_be32(head + 8u) != sample_count ||
        rr_rsrc_be32(head + 4u) != 16u + sample_count * 2u)
        return 0;
    *gate = 0u;
    schedule_at = rr_rsrc_be32(row + 36u);
    if (!schedule_at) return 1;
    if (!rr_rsrc_read(file, segments, schedule_at, head, sizeof(head)) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','S','E','C') ||
        rr_rsrc_be32(head + 12u) != sample_count)
        return 0;
    count = rr_rsrc_be32(head + 8u);
    if (count > 256u || rr_rsrc_be32(head + 4u) != 20u + count * 12u)
        return 0;
    for (i = 0u; i < count; ++i) {
        rr_u32 start;
        if (!rr_rsrc_read(file, segments,
                          schedule_at + 20u + i * 12u,
                          entry, sizeof(entry))) return 0;
        start = rr_rsrc_be32(entry);
        if (entry[8] == 6u &&
            entry[9] == difficulty_level + 1u &&
            start && start <= sample_count &&
            (!*gate || start < *gate)) *gate = start;
    }
    return 1;
}

static int graph_load_segment_curves(const rr_rsrc_file_t *file,
                                     const rr_rsrc_record_t *segments,
                                     rr_course_graph_t *graph,
                                     rr_course_graph_node_t *node)
{
    rr_u8 row[RR_GRAPH_SEGMENT_BYTES], block[512];
    rr_u32 path_at, sample;
    if (node->sample_count >
        RR_COURSE_GRAPH_MAX_CURVES - graph->curve_count ||
        !rr_rsrc_read(file, segments,
            16u + node->segment_index * RR_GRAPH_SEGMENT_BYTES,
            row, sizeof(row))) return 0;
    path_at = rr_rsrc_be32(row + 4u);
    node->curve_offset = graph->curve_count;
    for (sample = 0u; sample < node->sample_count;) {
        rr_u32 i, count = node->sample_count - sample;
        if (count > sizeof(block) / 2u)
            count = sizeof(block) / 2u;
        if (!rr_rsrc_read(file, segments,
            path_at + 16u + sample * 2u,
            block, count * 2u)) return 0;
        for (i = 0u; i < count; ++i)
            graph->curves[graph->curve_count + sample + i] =
                (signed char)block[i * 2u];
        sample += count;
    }
    graph->curve_count += node->sample_count;
    return 1;
}

static int graph_compute_finish(rr_course_graph_t *graph, rr_u8 index,
                                int distance_after_gate, int *result)
{
    rr_course_graph_node_t *node = &graph->nodes[index];
    int downstream = 0, branch_result;
    rr_u8 next;
    if (node->visit == 2u) {
        *result = node->finish_distance_samples;
        return 1;
    }
    if (node->visit == 1u) return 0;
    node->visit = 1u;
    if (node->kind == 4u) {
        node->finish_distance_samples = 0;
        node->fork_edge_lane = 0;
    } else {
        next = node->next[node->kind == 1u ? node->main_channel : 0u];
        if (next == RR_COURSE_GRAPH_NONE) return 0;
        if (node->kind == 0u) {
            if (distance_after_gate > 0) {
                node->finish_distance_samples = -distance_after_gate;
                if (!graph_compute_finish(graph, next,
                    distance_after_gate + (int)node->sample_count,
                    &downstream)) return 0;
            } else if (node->finish_gate) {
                node->finish_distance_samples = (int)node->finish_gate;
                if (!graph_compute_finish(graph, next,
                    (int)(node->sample_count - node->finish_gate),
                    &downstream)) return 0;
            } else {
                if (!graph_compute_finish(graph, next, 0,
                                          &downstream)) return 0;
                node->finish_distance_samples =
                    downstream + (int)node->sample_count;
            }
        } else if (node->kind == 1u) {
            if (!graph_compute_finish(graph, next,
                distance_after_gate, &downstream)) return 0;
            next = node->next[node->alternate_channel];
            if (next == RR_COURSE_GRAPH_NONE ||
                !graph_compute_finish(graph, next,
                    distance_after_gate, &branch_result)) return 0;
            node->finish_distance_samples = downstream;
        } else {
            if (!graph_compute_finish(graph, next,
                distance_after_gate, &downstream)) return 0;
            node->finish_distance_samples = downstream;
        }
    }
    node->visit = 2u;
    *result = node->finish_distance_samples;
    return 1;
}

int rr_course_graph_load(const rr_rsrc_file_t *file,
                         unsigned int difficulty_level,
                         rr_course_graph_t *graph)
{
    rr_rsrc_record_t segments, nodes;
    rr_u8 head[20], raw[RR_GRAPH_NODE_BYTES];
    rr_u32 count, i;
    if (!file || !graph || difficulty_level > 4u ||
        !rr_rsrc_find(file, RR_RSRC_TAG('S','G','S',' '), 1u, &segments) ||
        !rr_rsrc_find(file, RR_RSRC_TAG('N','O','D',' '), 1u, &nodes) ||
        segments.size < 16u || nodes.size < 28u ||
        !rr_rsrc_read(file, &segments, 0u, head, 16u) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','S','G','S'))
        return 0;
    count = rr_rsrc_be32(head + 8u);
    if (!count || count > (segments.size - 16u) / RR_GRAPH_SEGMENT_BYTES ||
        !rr_rsrc_read(file, &nodes, 0u, head, sizeof(head)) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','N','O','D'))
        return 0;
    graph->node_count = 0u;
    graph->segment_count = count;
    graph->curve_count = 0u;
    if (!graph_find_or_add(graph, rr_rsrc_be32(head + 16u),
                           nodes.size, &graph->root)) return 0;
    for (i = 0u; i < graph->node_count; ++i) {
        rr_course_graph_node_t *node = &graph->nodes[i];
        rr_u32 offset = node->resource_offset;
        rr_u32 link[2], segment_index;
        rr_u8 index;
        if (!rr_rsrc_read(file, &nodes, offset, raw, 8u)) return 0;
        if (rr_rsrc_be32(raw) > 4u) return 0;
        node->kind = (rr_u8)rr_rsrc_be32(raw);
        node->visit = 0u;
        node->sample_count = 0u;
        node->finish_gate = 0u;
        node->finish_distance_samples = 0;
        node->segment_index = 0u;
        node->main_channel = 0u;
        node->alternate_channel = 1u;
        if (node->kind == 4u) continue;
        if (node->kind > 2u || offset > nodes.size - RR_GRAPH_NODE_BYTES ||
            !rr_rsrc_read(file, &nodes, offset, raw, sizeof(raw)))
            return 0;
        link[0] = rr_rsrc_be32(raw + (node->kind == 2u ? 16u : 8u));
        link[1] = rr_rsrc_be32(raw + 12u);
        if (node->kind == 0u) {
            segment_index = rr_rsrc_be32(raw + 16u);
            node->sample_count = rr_rsrc_be32(raw + 20u);
            if (segment_index >= count || !node->sample_count ||
                node->sample_count > 32767u ||
                !graph_segment_gate(file, &segments, segment_index,
                    node->sample_count, difficulty_level,
                    &node->finish_gate)) return 0;
            node->segment_index = segment_index;
            if (!graph_load_segment_curves(file, &segments, graph, node))
                return 0;
        } else if (node->kind == 1u) {
            node->main_channel = raw[24];
            node->alternate_channel = raw[25];
            if (node->main_channel > 1u ||
                node->alternate_channel > 1u ||
                node->main_channel == node->alternate_channel)
                return 0;
        }
        if (!graph_find_or_add(graph, link[0], nodes.size, &index))
            return 0;
        node->next[0] = index;
        if (node->kind == 1u) {
            if (!graph_find_or_add(graph, link[1], nodes.size, &index))
                return 0;
            node->next[1] = index;
        }
    }
    for (i = 0u; i < graph->node_count; ++i) {
        rr_course_graph_node_t *branch = &graph->nodes[i];
        const rr_course_graph_node_t *before = 0, *first;
        unsigned int old_left, unused_right, first_left, first_right;
        rr_u32 predecessor;
        rr_u8 first_link;
        if (branch->kind != 1u) continue;
        for (predecessor = 0u; predecessor < graph->node_count;
             ++predecessor)
            if (graph->nodes[predecessor].kind == 0u &&
                graph->nodes[predecessor].next[0] == i) {
                before = &graph->nodes[predecessor];
                break;
            }
        first_link = branch->main_channel == 0u ?
            branch->next[0] : branch->next[1];
        if (first_link >= graph->node_count) return 0;
        first = &graph->nodes[first_link];
        if (!before || first->kind != 0u ||
            !graph_segment_width(file, &segments,
                before->segment_index, before->sample_count,
                before->sample_count - 1u,
                &old_left, &unused_right) ||
            !graph_segment_width(file, &segments,
                first->segment_index, first->sample_count,
                0u, &first_left, &first_right)) return 0;
        branch->fork_edge_lane =
            ((int)first_left + (int)first_right -
             (int)old_left) * 111 / 512;
    }
    {
        int finish;
        if (!graph_compute_finish(graph, graph->root, 0, &finish) ||
            finish <= 0) return 0;
        graph->root_finish_samples = (rr_u32)finish;
    }
    for (i = 0u; i < graph->node_count; ++i)
        if (graph->nodes[i].visit != 2u) return 0;
    return 1;
}

rr_u32 rr_course_graph_route_finish(const rr_course_graph_t *graph,
                                     rr_u32 branch_mask)
{
    rr_u32 distance = 0u, fork = 0u, steps;
    rr_u8 index;
    if (!graph || !graph->node_count) return 0u;
    index = graph->root;
    for (steps = 0u; steps < graph->node_count; ++steps) {
        const rr_course_graph_node_t *node;
        if (index >= graph->node_count) return 0u;
        node = &graph->nodes[index];
        if (node->kind == 4u) return distance;
        if (node->kind == 0u) {
            rr_u32 run = node->finish_gate ?
                node->finish_gate : node->sample_count;
            if (run > 0xffffffffu - distance) return 0u;
            distance += run;
            if (node->finish_gate) return distance;
        }
        if (node->kind == 1u) {
            if (fork >= 8u) return 0u;
            index = node->next[(branch_mask & (1u << fork++)) ? 1u : 0u];
        } else index = node->next[0];
    }
    return 0u;
}

int rr_course_graph_branch_span(const rr_course_graph_t *graph,
                                rr_u32 branch_mask, unsigned int branch_index,
                                rr_u32 *chosen_span, rr_u32 *other_span)
{
    rr_u32 first_distance[RR_COURSE_GRAPH_MAX_NODES];
    rr_u32 distance = 0u, steps, fork = 0u;
    rr_u8 index, first, second;
    const rr_course_graph_node_t *node;
    unsigned int i;
    if (!graph || !chosen_span || !other_span || !graph->node_count ||
        graph->node_count > RR_COURSE_GRAPH_MAX_NODES ||
        graph->root >= graph->node_count || branch_index >= 8u)
        return 0;
    index = graph->root;
    for (steps = 0u; steps < graph->node_count; ++steps) {
        if (index >= graph->node_count) return 0;
        node = &graph->nodes[index];
        if (node->kind == 1u) {
            if (fork == branch_index) break;
            index = node->next[(branch_mask & (1u << fork)) ? 1u : 0u];
            ++fork;
        } else if (node->kind == 4u) return 0;
        else index = node->next[0];
    }
    if (steps == graph->node_count) return 0;
    first = node->next[0];
    second = node->next[1];
    for (i = 0u; i < RR_COURSE_GRAPH_MAX_NODES; ++i)
        first_distance[i] = 0xffffffffu;
    index = first;
    for (steps = 0u; steps < graph->node_count; ++steps) {
        if (index >= graph->node_count ||
            first_distance[index] != 0xffffffffu) return 0;
        first_distance[index] = distance;
        node = &graph->nodes[index];
        if (node->kind == 2u) break;
        if (node->kind == 4u) return 0;
        if (node->kind == 1u ||
            node->sample_count > 0xffffffffu - distance) return 0;
        distance += node->sample_count;
        index = node->next[0];
    }
    index = second;
    distance = 0u;
    for (steps = 0u; steps < graph->node_count; ++steps) {
        rr_u32 spans[2];
        if (index >= graph->node_count) return 0;
        if (first_distance[index] != 0xffffffffu) {
            spans[0] = first_distance[index];
            spans[1] = distance;
            *chosen_span = spans[(branch_mask &
                                  (1u << branch_index)) ? 1u : 0u];
            *other_span = spans[(branch_mask &
                                 (1u << branch_index)) ? 0u : 1u];
            return *chosen_span != 0u && *other_span != 0u;
        }
        node = &graph->nodes[index];
        if (node->kind == 4u || node->kind == 2u || node->kind == 1u ||
            node->sample_count > 0xffffffffu - distance) return 0;
        distance += node->sample_count;
        index = node->next[0];
    }
    return 0;
}

int rr_course_graph_cursor_init(const rr_course_graph_t *graph,
                                rr_course_graph_cursor_t *cursor)
{
    if (!graph || !cursor || !graph->node_count ||
        graph->root >= graph->node_count) return 0;
    cursor->node = graph->root;
    cursor->local_8_8 = 0u;
    cursor->fork_index = 0u;
    return 1;
}

static int graph_cursor_advance(const rr_course_graph_t *graph,
                                rr_course_graph_cursor_t *cursor,
                                rr_u32 delta_8_8, rr_u32 branch_mask,
                                int use_lane, int lane)
{
    rr_u32 steps;
    if (!graph || !cursor) return 0;
    for (steps = 0u; steps <= graph->node_count; ++steps) {
        const rr_course_graph_node_t *node;
        rr_u8 next;
        if (cursor->node >= graph->node_count) return 0;
        node = &graph->nodes[cursor->node];
        if (node->kind == 4u) return 1;
        if (node->kind == 0u) {
            rr_u32 length = node->sample_count * 256u;
            rr_u32 available;
            if (cursor->local_8_8 > length) return 0;
            available = length - cursor->local_8_8;
            if (delta_8_8 < available) {
                cursor->local_8_8 += delta_8_8;
                return 1;
            }
            delta_8_8 -= available;
            cursor->local_8_8 = 0u;
            next = node->next[0];
        } else if (node->kind == 1u) {
            rr_u32 alternate;
            if (cursor->fork_index >= 8u) return 0;
            alternate = use_lane == 2 ? node->main_channel != 0u :
                use_lane ?
                (unsigned int)(lane >= node->fork_edge_lane) !=
                    node->main_channel :
                (branch_mask & (1u << cursor->fork_index)) != 0u;
            ++cursor->fork_index;
            next = node->next[alternate ? 1u : 0u];
        } else if (node->kind == 2u) {
            next = node->next[0];
        } else return 0;
        if (next >= graph->node_count) return 0;
        cursor->node = next;
    }
    return 0;
}

int rr_course_graph_cursor_advance(const rr_course_graph_t *graph,
                                   rr_course_graph_cursor_t *cursor,
                                   rr_u32 delta_8_8, rr_u32 branch_mask)
{
    return graph_cursor_advance(graph, cursor, delta_8_8,
                                branch_mask, 0, 0);
}

int rr_course_graph_cursor_advance_lane(const rr_course_graph_t *graph,
                                        rr_course_graph_cursor_t *cursor,
                                        rr_u32 delta_8_8, int lane)
{
    return graph_cursor_advance(graph, cursor, delta_8_8,
                                0u, 1, lane);
}

int rr_course_graph_cursor_advance_main(const rr_course_graph_t *graph,
                                        rr_course_graph_cursor_t *cursor,
                                        rr_u32 delta_8_8)
{
    return graph_cursor_advance(graph, cursor, delta_8_8,
                                0u, 2, 0);
}

int rr_course_graph_cursor_finish_distance(
    const rr_course_graph_t *graph,
    const rr_course_graph_cursor_t *cursor,
    int *distance_8_8)
{
    const rr_course_graph_node_t *node;
    if (!graph || !cursor || !distance_8_8 ||
        cursor->node >= graph->node_count) return 0;
    node = &graph->nodes[cursor->node];
    if (node->kind != 0u && node->kind != 4u) return 0;
    *distance_8_8 = node->finish_distance_samples * 256 -
        (int)cursor->local_8_8;
    return 1;
}

int rr_course_graph_cursor_curvature(const rr_course_graph_t *graph,
                                     const rr_course_graph_cursor_t *cursor,
                                     int *curvature)
{
    const rr_course_graph_node_t *node;
    rr_u32 sample;
    if (!graph || !cursor || !curvature ||
        cursor->node >= graph->node_count) return 0;
    node = &graph->nodes[cursor->node];
    sample = cursor->local_8_8 / 256u;
    if (node->kind != 0u || sample >= node->sample_count ||
        node->curve_offset > graph->curve_count ||
        sample >= graph->curve_count - node->curve_offset)
        return 0;
    *curvature = graph->curves[node->curve_offset + sample];
    return 1;
}

unsigned int rr_course_graph_curve_speed(const rr_course_graph_t *graph,
                                          const rr_course_graph_cursor_t *cursor,
                                          unsigned int base_speed)
{
    rr_course_graph_cursor_t ahead;
    unsigned int curve = 0u, i, reduction;
    if (!graph || !cursor) return base_speed;
    ahead = *cursor;
    if (!rr_course_graph_cursor_advance_main(graph, &ahead, 0x800u))
        return base_speed;
    for (i = 0u; i < 4u; ++i) {
        int value;
        if (i && !rr_course_graph_cursor_advance_main(graph, &ahead,
                                                       256u))
            return base_speed;
        if (!rr_course_graph_cursor_curvature(graph, &ahead, &value))
            return base_speed;
        curve += (unsigned int)(value < 0 ? -value : value);
    }
    curve /= 4u;
    if (curve <= 12u) return base_speed;
    reduction = (curve - 12u) * base_speed / 128u;
    if (reduction > base_speed / 2u)
        reduction = base_speed / 2u;
    return base_speed - reduction;
}

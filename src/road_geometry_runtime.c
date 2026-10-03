#include "road_geometry_runtime.h"
#include "local-data/upstream-src/track_traversal_runtime.h"

void *initialize_road_lane_width_traversal(RoadRlanTraversalState *,
                                           const RoadRlanResource *, int);
void advance_road_lane_width_traversal(RoadRlanTraversalState *, int);

#define MAX_RLAN_ENTRIES 64u
static unsigned int g_rsl_words[(16u + MAX_RLAN_ENTRIES * 20u + 3u) / 4u];

#define MAX_RSLD_ENTRIES 64u
static rr_u8 g_rsld_entries[MAX_RSLD_ENTRIES][36];
static const rr_u8 g_rsld_start_at[4] = {8u, 9u, 12u, 13u};
static const rr_u8 g_rsld_end_at[4] = {10u, 11u, 14u, 15u};

int rr_road_slopes_for_segment(const rr_rsrc_file_t *file,
                              const rr_rsrc_record_t *segments,
                              rr_u32 segment_relative_offset,
                              rr_u32 samples,
                              rr_u8 *left_margin, rr_u8 *right_margin,
                              rr_u8 *left_depth, rr_u8 *right_depth,
                              rr_u8 *left_edge_resource,
                              rr_u8 *right_edge_resource)
{
    rr_u8 header[16];
    rr_u32 count, i, kept = 0u, entry_index = 0u;
    int values[4], steps[4];
    if (!file || !segments || !segment_relative_offset || !samples ||
        !left_margin || !right_margin ||
        (!!left_depth != !!right_depth) ||
        (!!left_edge_resource != !!right_edge_resource) ||
        !rr_rsrc_read(file, segments, segment_relative_offset,
                      header, sizeof(header)) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('R','S','L','D'))
        return 0;
    count = rr_rsrc_be32(header + 8u);
    if (!count || count > MAX_RSLD_ENTRIES ||
        rr_rsrc_be32(header + 4u) != 16u + count * 36u)
        return 0;
    for (i = 0u; i < count; ++i) {
        rr_u8 *entry = g_rsld_entries[kept];
        rr_u32 start, end;
        if (!rr_rsrc_read(file, segments,
                          segment_relative_offset + 16u + i * 36u,
                          entry, 36u)) return 0;
        start = rr_rsrc_be32(entry);
        end = rr_rsrc_be32(entry + 4u);
        if (start > end || end > samples ||
            (i == 0u && start != 0u))
            return 0;
        /* Napa contains an overlapping duplicate interval. The upstream
         * forward cursor cannot enter the later duplicate because its start
         * precedes the current entry's end, so retain the first interval. */
        if (kept && start < rr_rsrc_be32(g_rsld_entries[kept - 1u] + 4u))
            continue;
        ++kept;
    }
    count = kept;
    if (rr_rsrc_be32(g_rsld_entries[count - 1u] + 4u) != samples)
        return 0;
    for (i = 0u; i < 4u; ++i) {
        values[i] = (int)g_rsld_entries[0][g_rsld_start_at[i]] << 16;
        steps[i] = (int)rr_rsrc_be32(g_rsld_entries[0] +
                                         20u + i * 4u);
    }
    for (i = 0u; i < samples; ++i) {
        rr_u8 *entry = g_rsld_entries[entry_index];
        rr_u32 j, end = rr_rsrc_be32(entry + 4u);
        if (entry_index + 1u < count &&
            i == rr_rsrc_be32(g_rsld_entries[entry_index + 1u])) {
            entry = g_rsld_entries[++entry_index];
            end = rr_rsrc_be32(entry + 4u);
            for (j = 0u; j < 4u; ++j) {
                values[j] = (int)entry[g_rsld_start_at[j]] << 16;
                steps[j] = (int)rr_rsrc_be32(entry + 20u + j * 4u);
            }
        } else if (i > rr_rsrc_be32(entry) && i == end) {
            for (j = 0u; j < 4u; ++j)
                values[j] = (int)entry[g_rsld_end_at[j]] << 16;
        } else if (i > rr_rsrc_be32(entry) && i < end) {
            for (j = 0u; j < 4u; ++j) values[j] += steps[j];
        }
        left_margin[i] = (rr_u8)(values[0] >> 16);
        right_margin[i] = (rr_u8)(values[1] >> 16);
        if (left_depth) {
            left_depth[i] = (rr_u8)(values[2] >> 16);
            right_depth[i] = (rr_u8)(values[3] >> 16);
        }
        if (left_edge_resource) {
            left_edge_resource[i] = entry[16u];
            right_edge_resource[i] = entry[17u];
        }
    }
    return 1;
}

int rr_road_widths_for_segment(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               rr_u32 segment_relative_offset,
                               rr_u32 samples,
                               unsigned short *left,
                               unsigned short *right)
{
    rr_u8 header[16];
    rr_u8 row[20];
    RoadRlanResource *resource = (RoadRlanResource *)g_rsl_words;
    RoadRlanTraversalState cursor;
    rr_u32 count, i;
    if (!file || !segments || !left || !right || !samples ||
        !rr_rsrc_read(file, segments, segment_relative_offset,
                      header, sizeof(header)) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('R','L','A','N'))
        return 0;
    count = rr_rsrc_be32(header + 8);
    if (!count || count > MAX_RLAN_ENTRIES ||
        rr_rsrc_be32(header + 4) != 16u + count * 20u)
        return 0;
    resource->tag = RR_RSRC_TAG('R','L','A','N');
    resource->byte_size = 16u + count * 20u;
    resource->entry_count = (int)count;
    resource->reserved_0c = 0u;
    for (i = 0u; i < count; ++i) {
        RoadRlanEntry *entry = &resource->entries[i];
        if (!rr_rsrc_read(file, segments,
                          segment_relative_offset + 16u + i * 20u,
                          row, sizeof(row)))
            return 0;
        entry->start_sample = (int)rr_rsrc_be32(row);
        entry->end_sample = (int)rr_rsrc_be32(row + 4);
        entry->start_left_width = row[8];
        entry->start_right_width = row[9];
        entry->end_left_width = row[10];
        entry->end_right_width = row[11];
        entry->left_width_step = (int)rr_rsrc_be32(row + 12);
        entry->right_width_step = (int)rr_rsrc_be32(row + 16);
        /* City carries branch-only width entries after the main path and
         * closes with a sentinel at the primary route length. */
        if (entry->start_sample < 0 || entry->end_sample <
            entry->start_sample)
            return 0;
    }
    if ((rr_u32)resource->entries[count - 1u].end_sample != samples)
        return 0;
    initialize_road_lane_width_traversal(&cursor, resource,
                                          TRACK_TRAVERSAL_FORWARD);
    for (i = 0u; i < samples; ++i) {
        int l = cursor.left_width_accumulator >> 8;
        int r = cursor.right_width_accumulator >> 8;
        if (l < 0 || r < 0 || l > 65535 || r > 65535) return 0;
        left[i] = (unsigned short)l;
        right[i] = (unsigned short)r;
        if (i + 1u < samples)
            advance_road_lane_width_traversal(&cursor, 256);
    }
    return 1;
}

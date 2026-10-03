#include "road_course_runtime.h"
#include "road_geometry_runtime.h"
#include "road_placement_runtime.h"


#define SEGMENT_RECORD_BYTES 52u
#define NODE_RECORD_BYTES 32u
#define SAMPLE_BLOCK_BYTES 512u
static rr_u8 g_edge_entry_bytes[2][256][16];
static rr_u8 g_section_entries[256][12];

static rr_u32 hill_random(rr_u32 *state, rr_u32 range)
{
    *state = *state * 1664525u + 1013904223u;
    return range ? (*state >> 1u) % range : 0u;
}

static int hill_definition_read(const rr_rsrc_file_t *file,
                                const rr_rsrc_record_t *segments,
                                rr_u32 terrain_at, rr_u32 index,
                                rr_u8 *definition)
{
    return rr_rsrc_read(file, segments,
        terrain_at + 20u + index * 76u, definition, 76u);
}

static int load_hill_sample_shapes(const rr_rsrc_file_t *file,
                                    const rr_rsrc_record_t *segments,
                                    rr_u32 terrain_at, rr_u32 definitions,
                                    rr_u32 count, rr_course_buffer_t *course)
{
    rr_u8 definition[76], next_definition[76];
    int current[2][4][2], step[2][4][2];
    rr_u32 interval_end[2][4];
    rr_u32 seed = 0x93e3ab45u ^ (terrain_at * 17u) ^
        course->sample_count;
    rr_u32 index = 0u, next_start = 0xffffffffu;
    rr_u32 sample, side, point, axis;
    if (!definitions || !hill_definition_read(file, segments,
        terrain_at, 0u, definition)) return 0;
    if (definitions > 1u) {
        if (!hill_definition_read(file, segments, terrain_at,
            1u, next_definition)) return 0;
        next_start = rr_rsrc_be32(next_definition);
    }
    for (side = 0u; side < 2u; ++side)
        for (point = 0u; point < 4u; ++point) {
            rr_u32 offset = 12u + side * 32u + point * 8u;
            int minimum[2] = {
                (int)definition[offset] * 32,
                (int)(signed char)definition[offset + 3u] * 16
            };
            int maximum[2] = {
                (int)definition[offset + 1u] * 32,
                (int)(signed char)definition[offset + 4u] * 16
            };
            for (axis = 0u; axis < 2u; ++axis) {
                if (maximum[axis] < minimum[axis]) return 0;
                current[side][point][axis] = minimum[axis] +
                    (int)hill_random(&seed, (rr_u32)
                        (maximum[axis] - minimum[axis] + 1));
                step[side][point][axis] = 0;
            }
            interval_end[side][point] = 0u;
        }
    for (sample = 0u; sample < count; ++sample) {
        road_hill_sample_t *shape = &course->hill_samples[
            course->sample_count + sample];
        while (index + 1u < definitions &&
               (int)next_start <= (int)sample) {
            for (axis = 0u; axis < 76u; ++axis)
                definition[axis] = next_definition[axis];
            ++index;
            next_start = 0xffffffffu;
            if (index + 1u < definitions) {
                if (!hill_definition_read(file, segments, terrain_at,
                    index + 1u, next_definition)) return 0;
                next_start = rr_rsrc_be32(next_definition);
            }
        }
        for (side = 0u; side < 2u; ++side)
            for (point = 0u; point < 4u; ++point) {
                rr_u32 offset = 12u + side * 32u + point * 8u;
                if (sample >= interval_end[side][point]) {
                    rr_u32 minimum_run = definition[offset + 6u];
                    rr_u32 maximum_run = definition[offset + 7u];
                    rr_u32 run;
                    if (maximum_run < minimum_run) return 0;
                    run = minimum_run + hill_random(&seed,
                        maximum_run - minimum_run + 1u);
                    if (run > count - sample) run = count - sample;
                    if (rr_rsrc_be32(definition + 4u) > sample &&
                        run > rr_rsrc_be32(definition + 4u) - sample)
                        run = rr_rsrc_be32(definition + 4u) - sample;
                    if (!run) run = 1u;
                    interval_end[side][point] = sample + run;
                    for (axis = 0u; axis < 2u; ++axis) {
                        int minimum = axis == 0u ?
                            (int)definition[offset] * 32 :
                            (int)(signed char)definition[offset + 3u] * 16;
                        int maximum = axis == 0u ?
                            (int)definition[offset + 1u] * 32 :
                            (int)(signed char)definition[offset + 4u] * 16;
                        int radius = (int)definition[offset +
                            (axis == 0u ? 2u : 5u)] * 8;
                        int delta = (int)hill_random(&seed,
                            (rr_u32)(radius * 2 + 1)) - radius;
                        int target = current[side][point][axis] +
                            (int)run * delta;
                        if (target < minimum)
                            delta = (minimum -
                                current[side][point][axis]) / (int)run;
                        else if (target > maximum)
                            delta = (maximum -
                                current[side][point][axis]) / (int)run;
                        step[side][point][axis] = delta;
                    }
                }
                if (point < 3u) {
                    int x = current[side][point][0] / 32;
                    int y = current[side][point][1] / 16;
                    if (x < 0) x = 0;
                    if (x > 255) x = 255;
                    if (y < -128) y = -128;
                    if (y > 127) y = 127;
                    shape->x[side][point] = (unsigned char)x;
                    shape->y[side][point] = (signed char)y;
                }
                for (axis = 0u; axis < 2u; ++axis)
                    current[side][point][axis] +=
                        step[side][point][axis];
            }
    }
    return 1;
}

static int load_crossing_zones(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               rr_u32 offset, rr_u32 segment_samples,
                               rr_course_buffer_t *course)
{
    rr_u8 head[20], entry[12];
    rr_u32 count, i;
    if (course->surface_flags)
        for (i = 0u; i < segment_samples; ++i)
            course->surface_flags[course->sample_count + i] = 0u;
    if (!offset) return 1;
    if (!rr_rsrc_read(file, segments, offset, head, sizeof(head)) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','S','E','C') ||
        rr_rsrc_be32(head + 12u) != segment_samples)
        return 0;
    count = rr_rsrc_be32(head + 8u);
    if (count > 256u || rr_rsrc_be32(head + 4u) != 20u + count * 12u)
        return 0;
    for (i = 0u; i < count; ++i) {
        rr_u32 start, end;
        road_crossing_zone_t *zone;
        if (!rr_rsrc_read(file, segments, offset + 20u + i * 12u,
                          entry, sizeof(entry))) return 0;
        if (course->surface_flags) {
            rr_u32 byte;
            for (byte = 0u; byte < sizeof(entry); ++byte)
                g_section_entries[i][byte] = entry[byte];
        }
        start = rr_rsrc_be32(entry);
        end = rr_rsrc_be32(entry + 4u);
        if (entry[8] == 6u &&
            entry[9] == course->difficulty_level + 1u &&
            start && start <= segment_samples) {
            rr_u32 finish = course->sample_count + start;
            if (!course->finish_sample || finish < course->finish_sample)
                course->finish_sample = finish;
        }
        if (entry[8] > 2u || !course->crossing_zones) continue;
        if (start > end || end > segment_samples ||
            course->crossing_count >= course->crossing_capacity ||
            course->sample_count + end > 65535u) return 0;
        zone = &course->crossing_zones[course->crossing_count++];
        zone->start_sample =
            (unsigned short)(course->sample_count + start);
        zone->end_sample =
            (unsigned short)(course->sample_count + end);
        zone->mode = (unsigned char)(entry[8] + 1u);
        zone->spread = (signed char)entry[9];
        zone->simulation_scale = (signed char)entry[10];
        zone->preserve_child_updates = entry[11];
        zone->short_timing = 0u;
    }
    if (course->surface_flags && count) {
        rr_u32 active = 0u, sample;
        for (sample = 0u; sample < segment_samples; ++sample) {
            const rr_u8 *section;
            rr_u32 start, end, kind;
            rr_u8 flags = 0u;
            /* The upstream forward cursor crosses only one RSEC entry
             * per sample, even if two starts coincide. */
            if (active + 1u < count &&
                rr_rsrc_be32(g_section_entries[active + 1u]) <= sample)
                ++active;
            section = g_section_entries[active];
            start = rr_rsrc_be32(section);
            end = rr_rsrc_be32(section + 4u);
            kind = (rr_u32)section[8u] + 1u;
            if (sample >= start && sample <= end) {
                if (kind == 4u) flags = 0x40u;
                else if (kind != 5u && kind != 7u) {
                    if (sample == end) flags = 0u;
                    else if (sample == start) flags = 0x08u;
                    else if (sample + 1u == end) flags = 0x10u;
                    else if (kind == 1u) {
                        rr_u32 to_end = end - sample;
                        rr_u32 from_start = sample - start;
                        rr_u32 result = to_end < from_start + 1u;
                        if (to_end == from_start ||
                            to_end == from_start + 1u) result = 2u;
                        flags = (rr_u8)(0x20u + result);
                    } else flags = 0x21u;
                }
            }
            course->surface_flags[course->sample_count + sample] = flags;
        }
    }
    return 1;
}

static int load_edge_profiles(const rr_rsrc_file_t *file,
                              const rr_rsrc_record_t *segments,
                              rr_u32 terrain_at, rr_u32 sample_count,
                              rr_course_buffer_t *course)
{
    rr_u8 header[24], entry[16];
    rr_u32 counts[2], valid_counts[2] = {0u, 0u};
    rr_u32 bases[2], side, i, at, sample;
    rr_u32 active[2] = {0u, 0u};
    rr_u32 seed;
    int inner_state[2], height_state[2], outer_state[2];
    if (!course->edge_profiles[0] || !course->edge_profiles[1]) return 1;
    if (!rr_rsrc_read(file, segments, terrain_at, header,
                      sizeof(header))) return 0;
    if (rr_rsrc_be32(header) != RR_RSRC_TAG('R','M','T','N') &&
        rr_rsrc_be32(header) != RR_RSRC_TAG('R','D','W','D')) return 1;
    counts[0] = rr_rsrc_be32(header + 12u);
    counts[1] = rr_rsrc_be32(header + 16u);
    if (!counts[0] || !counts[1] || counts[0] > 256u ||
        counts[1] > 256u || counts[0] + counts[1] > 512u ||
        rr_rsrc_be32(header + 4u) !=
            24u + (counts[0] + counts[1]) * 16u +
            sample_count * 2u ||
        course->edge_profile_count[0] + counts[0] >
            course->edge_profile_capacity[0] ||
        course->edge_profile_count[1] + counts[1] >
            course->edge_profile_capacity[1]) return 0;
    bases[0] = course->edge_profile_count[0];
    bases[1] = course->edge_profile_count[1];
    seed = 0x61ac4d39u ^ (terrain_at * 17u) ^ course->sample_count;
    at = 0u;
    for (side = 0u; side < 2u; ++side) {
        rr_u32 previous = 0u;
        for (i = 0u; i < counts[side]; ++i, ++at) {
            road_edge_profile_t *profile;
            rr_u32 start;
            if (!rr_rsrc_read(file, segments, terrain_at + 24u +
                at * 16u, entry, sizeof(entry))) return 0;
            start = rr_rsrc_be32(entry);
            if (course->sample_count + sample_count > 65535u)
                return 0;
            /* The source traversal can advance only one entry per sample,
             * even when resource starts repeat or move backwards. */
            if (i && start <= previous) start = previous + 1u;
            previous = start;
            /* Some resources retain entries beyond the route segment's
             * shortened RPTH; they never become active on that segment. */
            if (start >= sample_count) continue;
            profile = &course->edge_profiles[side][
                bases[side] + valid_counts[side]];
            for (rr_u32 byte = 0u; byte < 16u; ++byte)
                g_edge_entry_bytes[side][valid_counts[side]][byte] =
                    entry[byte];
            ++valid_counts[side];
            profile->start_sample = (unsigned short)(
                course->sample_count + start);
        }
        if (!valid_counts[side]) return 0;
        inner_state[side] = g_edge_entry_bytes[side][0][6u];
        height_state[side] =
            (int)g_edge_entry_bytes[side][0][11u] * 2;
        outer_state[side] =
            (int)g_edge_entry_bytes[side][0][8u] * 2;
    }
    for (sample = 0u; sample < sample_count; ++sample) {
        for (side = 0u; side < 2u; ++side) {
            rr_u8 *active_entry;
            road_edge_profile_t *profile;
            int outer, maximum;
            int changed = sample == 0u;
            if (sample > 0u && active[side] + 1u <
                valid_counts[side] &&
                course->edge_profiles[side][bases[side] +
                    active[side] + 1u].start_sample <=
                    course->sample_count + sample) {
                ++active[side];
                changed = 1;
            }
            active_entry = g_edge_entry_bytes[side][active[side]];
            if (sample > 0u) {
                /* The upstream traversal consumes three rand() calls per
                 * side.  Its inner/height draws do not change those state
                 * values; only the outer radius changes the silhouette. */
                (void)hill_random(&seed,
                    (rr_u32)active_entry[5u] * 2u + 1u);
                (void)hill_random(&seed,
                    (rr_u32)active_entry[10u] * 2u + 1u);
                outer_state[side] += (int)hill_random(&seed,
                    (rr_u32)active_entry[7u] * 2u + 1u) -
                    (int)active_entry[7u];
                maximum = (int)active_entry[6u] * 2;
                if (inner_state[side] < 0) inner_state[side] = 0;
                if (inner_state[side] > maximum)
                    inner_state[side] = maximum;
                maximum = (int)active_entry[11u] * 4;
                if (height_state[side] < 0) height_state[side] = 0;
                if (height_state[side] > maximum)
                    height_state[side] = maximum;
                maximum = (int)active_entry[8u] * 4;
                if (outer_state[side] < 0) outer_state[side] = 0;
                if (outer_state[side] > maximum)
                    outer_state[side] = maximum;
            }
            outer = (int)active_entry[6u] * 2 +
                (int)active_entry[9u] * 4 + outer_state[side];
            if (course->edge_outer_samples[side])
                course->edge_outer_samples[side][
                    course->sample_count + sample] = (short)outer;
            if (!changed) continue;
            profile = &course->edge_profiles[side][
                bases[side] + active[side]];
            profile->inner_offset = (short)inner_state[side];
            profile->outer_offset = (short)outer;
            profile->height = (short)((int)rr_rsrc_be32(
                active_entry + 12u) + height_state[side]);
        }
    }
    course->edge_profile_count[0] += valid_counts[0];
    course->edge_profile_count[1] += valid_counts[1];
    return 1;
}

static int load_segment(const rr_rsrc_file_t *file,
                        const rr_rsrc_record_t *segments,
                        rr_u32 segment_count, rr_u32 index,
                        rr_u32 expected_samples, rr_course_buffer_t *course)
{
    rr_u8 row[SEGMENT_RECORD_BYTES];
    rr_u8 header[16];
    rr_u8 block[SAMPLE_BLOCK_BYTES];
    rr_u32 offset, bytes, count, at;
    if (index >= segment_count ||
        !rr_rsrc_read(file, segments, 16u + index * SEGMENT_RECORD_BYTES,
                      row, sizeof(row)))
        return 0;
    offset = rr_rsrc_be32(row + 4);
    if (!rr_rsrc_read(file, segments, offset, header, sizeof(header)) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('R','P','T','H'))
        return 0;
    bytes = rr_rsrc_be32(header + 4);
    count = rr_rsrc_be32(header + 8);
    if (count == 0u || count != expected_samples ||
        count > (0xffffffffu - 16u) / 2u ||
        bytes != 16u + count * 2u ||
        count > course->capacity - course->sample_count)
        return 0;
    if (course->left_width && course->right_width &&
        !rr_road_widths_for_segment(
            file, segments, rr_rsrc_be32(row + 24), count,
            course->left_width + course->sample_count,
            course->right_width + course->sample_count))
        return 0;
    if (course->left_margin && course->right_margin &&
        !rr_road_slopes_for_segment(
            file, segments, rr_rsrc_be32(row + 20), count,
            course->left_margin + course->sample_count,
            course->right_margin + course->sample_count,
            course->left_depth ?
                course->left_depth + course->sample_count : 0,
            course->right_depth ?
                course->right_depth + course->sample_count : 0,
            course->left_edge_resource ?
                course->left_edge_resource + course->sample_count : 0,
            course->right_edge_resource ?
                course->right_edge_resource + course->sample_count : 0))
        return 0;
    if (course->left_edge_family && course->right_edge_family) {
        rr_u8 schedule_head[24], events[64u * 12u];
        rr_u32 event_count, event_index = 0u;
        rr_u32 active_family[4] = {0u, 0u, 0u, 0u};
        rr_u32 schedule_at = rr_rsrc_be32(row + 12u);
        if (!course->left_edge_resource ||
            !course->right_edge_resource ||
            !rr_rsrc_read(file, segments, schedule_at,
                          schedule_head, sizeof(schedule_head)) ||
            rr_rsrc_be32(schedule_head) !=
                RR_RSRC_TAG('R','R','S','M')) return 0;
        event_count = rr_rsrc_be32(schedule_head + 8u);
        if (event_count > 64u ||
            rr_rsrc_be32(schedule_head + 4u) !=
                24u + event_count * 12u ||
            !rr_rsrc_read(file, segments, schedule_at + 24u,
                          events, event_count * 12u)) return 0;
        for (at = 0u; at < count; ++at) {
            rr_u32 side;
            while (event_index < event_count &&
                (int)rr_rsrc_be32(events + event_index * 12u) <=
                (int)at) {
                const rr_u8 *event = events + event_index * 12u;
                if (event[4u] == 1u && event[5u] < 4u)
                    active_family[event[5u]] =
                        rr_rsrc_be32(event + 8u) & 0xffffu;
                ++event_index;
            }
            for (side = 0u; side < 2u; ++side) {
                rr_u8 selector = side ?
                    course->right_edge_resource[course->sample_count + at] :
                    course->left_edge_resource[course->sample_count + at];
                unsigned short *family = side ?
                    course->right_edge_family :
                    course->left_edge_family;
                family[course->sample_count + at] = selector == 255u ?
                    0u : (unsigned short)active_family[selector >> 6u];
            }
        }
    }
    if (course->terrain_mode) {
        rr_u8 terrain_head[4];
        rr_u8 mode;
        if (!rr_rsrc_read(file, segments, rr_rsrc_be32(row + 16),
                          terrain_head, sizeof(terrain_head))) return 0;
        mode = rr_rsrc_be32(terrain_head) ==
            RR_RSRC_TAG('R','B','L','D') ? 0u :
            rr_rsrc_be32(terrain_head) ==
            RR_RSRC_TAG('R','H','I','L') ? 4u :
            (rr_rsrc_be32(terrain_head) ==
                RR_RSRC_TAG('R','M','T','N') ||
             rr_rsrc_be32(terrain_head) ==
                RR_RSRC_TAG('R','D','W','D')) ? 1u : 255u;
        for (at = 0u; at < count; ++at)
            course->terrain_mode[course->sample_count + at] = mode;
    }
    if (!load_edge_profiles(file, segments, rr_rsrc_be32(row + 16u),
                            count, course)) return 0;
    if (course->surface_samples) {
        rr_u32 terrain_at = rr_rsrc_be32(row + 16u);
        rr_u8 terrain_head[24];
        rr_u32 terrain_tag, surface_at;
        if (!rr_rsrc_read(file, segments, terrain_at,
                          terrain_head, sizeof(terrain_head))) return 0;
        terrain_tag = rr_rsrc_be32(terrain_head);
        surface_at = terrain_at + 16u;
        if (terrain_tag == RR_RSRC_TAG('R','M','T','N') ||
            terrain_tag == RR_RSRC_TAG('R','D','W','D'))
            surface_at = terrain_at + 24u +
                (rr_rsrc_be32(terrain_head + 12u) +
                 rr_rsrc_be32(terrain_head + 16u)) * 16u;
        for (at = 0u; at < count; ++at) {
            road_surface_sample_t *sample = &course->surface_samples[
                course->sample_count + at];
            sample->family_id[0] = sample->family_id[1] = 0u;
            sample->selector[0] = sample->selector[1] = 255u;
            sample->tile_index[0] = sample->tile_index[1] = 255u;
        }
        if (terrain_tag == RR_RSRC_TAG('R','B','L','D') ||
            terrain_tag == RR_RSRC_TAG('R','M','T','N') ||
            terrain_tag == RR_RSRC_TAG('R','D','W','D')) {
            rr_u8 schedule_head[24], events[64u * 12u];
            rr_u32 schedule_at = rr_rsrc_be32(row + 12u);
            rr_u32 event_count, event_index = 0u;
            rr_u32 active_family[4] = {0u, 0u, 0u, 0u};
            if (rr_rsrc_be32(terrain_head + 8u) != count ||
                rr_rsrc_be32(terrain_head + 4u) !=
                    surface_at - terrain_at + count * 2u ||
                !rr_rsrc_read(file, segments, schedule_at,
                              schedule_head, sizeof(schedule_head)) ||
                rr_rsrc_be32(schedule_head) !=
                    RR_RSRC_TAG('R','R','S','M')) return 0;
            event_count = rr_rsrc_be32(schedule_head + 8u);
            if (event_count > 64u ||
                rr_rsrc_be32(schedule_head + 4u) !=
                    24u + event_count * 12u ||
                !rr_rsrc_read(file, segments, schedule_at + 24u,
                              events, event_count * 12u)) return 0;
            for (at = 0u; at < count;) {
                rr_u32 n = count - at, i;
                if (n > SAMPLE_BLOCK_BYTES / 2u)
                    n = SAMPLE_BLOCK_BYTES / 2u;
                if (!rr_rsrc_read(file, segments,
                    surface_at + at * 2u, block, n * 2u)) return 0;
                for (i = 0u; i < n; ++i) {
                    rr_u32 position = at + i, side;
                    road_surface_sample_t *sample =
                        &course->surface_samples[
                            course->sample_count + position];
                    while (event_index < event_count &&
                        (int)rr_rsrc_be32(events +
                            event_index * 12u) <= (int)position) {
                        const rr_u8 *event = events + event_index * 12u;
                        if (event[4u] == 1u && event[5u] < 4u)
                            active_family[event[5u]] =
                                rr_rsrc_be32(event + 8u) & 0xffffu;
                        ++event_index;
                    }
                    for (side = 0u; side < 2u; ++side) {
                        rr_u8 selector = block[i * 2u + side];
                        sample->selector[side] = selector;
                        if (selector != 255u)
                            sample->family_id[side] =
                                (unsigned short)active_family[
                                    selector >> 6u];
                    }
                }
                at += n;
            }
        }
    }
    if (course->hill_profiles) {
        rr_u32 terrain_at = rr_rsrc_be32(row + 16u);
        rr_u8 terrain_head[20];
        if (!rr_rsrc_read(file, segments, terrain_at,
                          terrain_head, sizeof(terrain_head))) return 0;
        if (rr_rsrc_be32(terrain_head) ==
            RR_RSRC_TAG('R','H','I','L')) {
            rr_u32 definition_count = rr_rsrc_be32(terrain_head + 8u);
            rr_u8 schedule_head[24], schedule_events[64u * 12u];
            rr_u32 schedule_at = rr_rsrc_be32(row + 12u);
            rr_u32 event_count, event_index = 0u;
            rr_u32 active_family[4] = {0u, 0u, 0u, 0u};
            rr_u32 j;
            if (definition_count >
                    course->hill_profile_capacity -
                    course->hill_profile_count ||
                rr_rsrc_be32(terrain_head + 4u) !=
                    20u + definition_count * 76u) return 0;
            if (!rr_rsrc_read(file, segments, schedule_at,
                              schedule_head, sizeof(schedule_head)) ||
                rr_rsrc_be32(schedule_head) !=
                    RR_RSRC_TAG('R','R','S','M')) return 0;
            event_count = rr_rsrc_be32(schedule_head + 8u);
            if (event_count > 64u ||
                rr_rsrc_be32(schedule_head + 4u) !=
                    24u + event_count * 12u ||
                !rr_rsrc_read(file, segments, schedule_at + 24u,
                              schedule_events, event_count * 12u))
                return 0;
            for (j = 0u; j < definition_count; ++j) {
                rr_u8 definition[76];
                road_hill_profile_t *profile;
                rr_u32 side, point;
                int start, end;
                if (!rr_rsrc_read(file, segments,
                    terrain_at + 20u + j * 76u,
                    definition, sizeof(definition))) return 0;
                start = (int)rr_rsrc_be32(definition);
                end = (int)rr_rsrc_be32(definition + 4u);
                while (event_index < event_count &&
                    (int)rr_rsrc_be32(schedule_events +
                        event_index * 12u) <= start) {
                    const rr_u8 *event = schedule_events +
                        event_index * 12u;
                    if (event[4u] == 1u && event[5u] < 4u)
                        active_family[event[5u]] =
                            rr_rsrc_be32(event + 8u) & 0xffffu;
                    ++event_index;
                }
                if (start < 0) start = 0;
                if (end > (int)count) end = (int)count;
                if (start >= end) continue;
                profile = &course->hill_profiles[
                    course->hill_profile_count++];
                profile->start_sample = course->sample_count +
                    (rr_u32)start;
                profile->end_sample = course->sample_count +
                    (rr_u32)end;
                for (side = 0u; side < 2u; ++side)
                {
                    profile->selector[side] = definition[8u + side];
                    profile->family_id[side] = (unsigned short)
                        active_family[profile->selector[side] >> 6u];
                    for (point = 0u; point < 3u; ++point) {
                        rr_u32 control = 12u + side * 32u + point * 8u;
                        profile->x[side][point] = (rr_u8)(
                            ((unsigned int)definition[control] +
                             definition[control + 1u]) / 2u);
                        profile->y[side][point] = (signed char)(
                            ((int)(signed char)definition[control + 3u] +
                             (int)(signed char)definition[control + 4u]) / 2);
                        profile->tile_index[side][point] = 255u;
                    }
                }
            }
            if (course->hill_samples &&
                !load_hill_sample_shapes(file, segments, terrain_at,
                    definition_count, count, course)) return 0;
        }
    }
    if (!load_crossing_zones(file, segments, rr_rsrc_be32(row + 36u),
                             count, course)) return 0;
    if (!rr_placements_for_segment(file, segments, row, count,
                                    course->sample_count, course))
        return 0;
    for (at = 0u; at < count;) {
        rr_u32 n = count - at;
        rr_u32 i;
        if (n > SAMPLE_BLOCK_BYTES / 2u) n = SAMPLE_BLOCK_BYTES / 2u;
        if (!rr_rsrc_read(file, segments, offset + 16u + at * 2u,
                          block, n * 2u))
            return 0;
        for (i = 0u; i < n; ++i) {
            course->curvature[course->sample_count + at + i] =
                (signed char)block[i * 2u];
            course->elevation[course->sample_count + at + i] =
                (signed char)block[i * 2u + 1u];
        }
        at += n;
    }
    course->sample_count += count;
    course->segment_count++;
    return 1;
}

int rr_course_load_selected(const rr_rsrc_file_t *file,
                            rr_course_buffer_t *course,
                            rr_u32 branch_mask)
{
    rr_rsrc_record_t segments, nodes;
    rr_u8 head[20];
    rr_u8 node[NODE_RECORD_BYTES];
    rr_u32 segment_count, cursor, steps, branch_index = 0u;
    rr_u32 open_branch = 0u;
    int branch_is_open = 0;
    if (!file || !course || !course->curvature || !course->elevation ||
        course->capacity == 0u ||
        !rr_rsrc_find(file, RR_RSRC_TAG('S','G','S',' '), 1u, &segments) ||
        !rr_rsrc_find(file, RR_RSRC_TAG('N','O','D',' '), 1u, &nodes) ||
        !rr_rsrc_read(file, &segments, 0u, head, 16u) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','S','G','S'))
        return 0;
    segment_count = rr_rsrc_be32(head + 8);
    if (segment_count == 0u || segment_count >
        (segments.size - 16u) / SEGMENT_RECORD_BYTES ||
        !rr_rsrc_read(file, &nodes, 0u, head, sizeof(head)) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','N','O','D'))
        return 0;
    cursor = rr_rsrc_be32(head + 16);
    course->sample_count = 0u;
    course->finish_sample = 0u;
    course->segment_count = 0u;
    course->branch_count = 0u;
    course->object_count = 0u;
    course->hazard_count = 0u;
    course->effect_count = 0u;
    course->crossing_count = 0u;
    course->hill_profile_count = 0u;
    course->edge_profile_count[0] = 0u;
    course->edge_profile_count[1] = 0u;
    if (course->family_ids) course->family_count = 0u;
    if (course->hazard_family_ids) course->hazard_family_count = 0u;
    for (steps = 0u; steps < 128u; ++steps) {
        rr_u32 kind, primary, payload;
        if (cursor < 20u ||
            !rr_rsrc_read(file, &nodes, cursor, node, 8u))
            return 0;
        kind = rr_rsrc_be32(node);
        if (kind == 4u) {
            if (!course->finish_sample)
                course->finish_sample = course->sample_count;
            return course->segment_count > 0u;
        }
        if (kind > 2u ||
            !rr_rsrc_read(file, &nodes, cursor, node,
                          NODE_RECORD_BYTES))
            return 0;
        primary = rr_rsrc_be32(node + 8);
        payload = rr_rsrc_be32(node + 16);
        if (kind == 0u) {
            if (!load_segment(file, &segments, segment_count, payload,
                              rr_rsrc_be32(node + 20), course))
                return 0;
            cursor = primary;
        } else if (kind == 1u) {
            rr_u32 alternate = rr_rsrc_be32(node + 12);
            rr_u8 main_channel = node[24];
            rr_u8 alternate_channel = node[25];
            if (branch_index >= 8u) return 0;
            if (main_channel > 1u || alternate_channel > 1u ||
                main_channel == alternate_channel) return 0;
            course->branch_samples[branch_index] = course->sample_count;
            course->branch_main_channels[branch_index] = main_channel;
            course->branch_alternate_channels[branch_index] =
                alternate_channel;
            course->branch_count = branch_index + 1u;
            open_branch = branch_index;
            branch_is_open = 1;
            cursor = (branch_mask & (1u << branch_index)) ?
                alternate : primary;
            branch_index++;
        } else {
            if (branch_is_open && course->terrain_mode) {
                rr_u32 first = course->branch_samples[open_branch];
                rr_u32 join = course->sample_count;
                rr_u32 zone_index;
                /* In the original parent, resource_kind 2 means a two-lane
                 * segment. That exists in the first and last 81 fork cells;
                 * geometry mode 4 is the RHIL profile. */
                for (zone_index = 0u;
                     zone_index < course->crossing_count; ++zone_index) {
                    road_crossing_zone_t *zone =
                        &course->crossing_zones[zone_index];
                    rr_u32 sample = zone->start_sample;
                    if (!zone->preserve_child_updates &&
                        sample >= first && sample < join &&
                        (sample - first < RR_COURSE_JUNCTION_SAMPLES ||
                         join - sample <= RR_COURSE_JUNCTION_SAMPLES) &&
                        course->terrain_mode[sample] == 4u)
                        zone->short_timing = 1u;
                }
            }
            branch_is_open = 0;
            cursor = payload;
        }
    }
    return 0;
}

int rr_course_load_primary(const rr_rsrc_file_t *file,
                           rr_course_buffer_t *course)
{
    return rr_course_load_selected(file, course, 0u);
}

int rr_course_reconcile_fork_elevation(
    signed char *selected, rr_u32 selected_count,
    signed char *other, rr_u32 other_count,
    rr_u32 fork_sample, rr_u32 selected_span, rr_u32 other_span,
    int selected_is_source)
{
    rr_u32 selected_join, other_join, i;
    if (!selected || !other ||
        selected_span < RR_COURSE_JUNCTION_SAMPLES ||
        other_span < RR_COURSE_JUNCTION_SAMPLES ||
        fork_sample > selected_count || fork_sample > other_count ||
        selected_span > selected_count - fork_sample ||
        other_span > other_count - fork_sample)
        return 0;
    selected_join = fork_sample + selected_span;
    other_join = fork_sample + other_span;
    for (i = 0u; i < RR_COURSE_JUNCTION_SAMPLES; ++i) {
        rr_u32 selected_start = fork_sample + i;
        rr_u32 other_start = fork_sample + i;
        rr_u32 selected_end = selected_join -
            RR_COURSE_JUNCTION_SAMPLES + i;
        rr_u32 other_end = other_join -
            RR_COURSE_JUNCTION_SAMPLES + i;
        if (selected_is_source) {
            other[other_start] = selected[selected_start];
            other[other_end] = selected[selected_end];
        } else {
            selected[selected_start] = other[other_start];
            selected[selected_end] = other[other_end];
        }
    }
    return 1;
}

#include "road_placement_runtime.h"

#define MAX_EVENTS 64u
#define MAX_OBJECT_ENTRIES 2048u
#define MAX_HAZARD_ENTRIES 256u

static rr_u8 g_events[MAX_EVENTS * 12u];
static rr_u8 g_objects[MAX_OBJECT_ENTRIES * 4u];
static rr_u8 g_hazards[MAX_HAZARD_ENTRIES * 8u];

static rr_u32 hazard_random(rr_u32 *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 1u;
}

rr_u32 rr_schedule_hazard_entry(rr_u32 control, rr_u32 detail,
                                 unsigned int level, rr_u32 *sample,
                                 rr_u32 *random_state, int *enabled)
{
    rr_u32 spread, point, mode;
    int threshold;
    if (!sample || !random_state || !enabled) return 0u;
    *sample += (control >> 24u) & 0x7fu;
    spread = (control >> 15u) & 0x1eu;
    point = *sample + hazard_random(random_state) % (spread + 1u);
    threshold = (int)level + (int)((control >> 20u) & 15u);
    if ((control & 0x80000000u) && (control & 15u) >= 11u)
        threshold -= 4;
    mode = (detail >> 12u) & 7u;
    *enabled = 0;
    if (mode == 0u || mode - 1u == level)
        *enabled = threshold >=
            (int)(hazard_random(random_state) % 10u);
    return point;
}

static int event_sample(rr_u32 index)
{
    return (int)rr_rsrc_be32(g_events + index * 12u);
}

static void advance_events(rr_u32 *event_index, rr_u32 event_count,
                           rr_u32 sample, rr_u32 active[4])
{
    while (*event_index < event_count &&
           event_sample(*event_index) <= (int)sample) {
        const rr_u8 *event = g_events + *event_index * 12u;
        if (event[4] == 1u && event[5] < 4u)
            active[event[5]] = rr_rsrc_be32(event + 8u);
        (*event_index)++;
    }
}

static int family_variant(rr_course_buffer_t *course, rr_u32 family)
{
    static const rr_u32 ids[9] =
        {101u, 102u, 148u, 149u, 169u, 171u, 174u, 175u, 240u};
    unsigned int i;
    if (!family) return -1;
    if (course->family_ids) {
        for (i = 0u; i < course->family_count; ++i)
            if (course->family_ids[i] == family) return (int)i;
        if (course->family_count >= course->family_capacity ||
            course->family_count >= 256u) return -1;
        i = course->family_count++;
        course->family_ids[i] = family;
        return (int)i;
    }
    for (i = 0u; i < 9u; ++i)
        if (ids[i] == family) return (int)i;
    return -1;
}

static int hazard_variant(rr_course_buffer_t *course, rr_u32 family,
                          rr_u32 frame)
{
    rr_u32 i;
    if (!family || frame >= 63u) return -1;
    if (!course->hazard_family_ids || !course->hazard_frame_ids)
        return ((family == 148u || family == 149u) && frame < 6u) ?
            (int)frame : -1;
    for (i = 0u; i < course->hazard_family_count; ++i)
        if (course->hazard_family_ids[i] == family &&
            course->hazard_frame_ids[i] == frame) return (int)i;
    if (course->hazard_family_count >= course->hazard_family_capacity ||
        course->hazard_family_count >= 256u) return -1;
    i = course->hazard_family_count++;
    course->hazard_family_ids[i] = family;
    course->hazard_frame_ids[i] = (rr_u8)frame;
    return (int)i;
}

int rr_placements_for_segment(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               const rr_u8 *segment_row,
                               rr_u32 segment_samples,
                               rr_u32 global_start,
                               rr_course_buffer_t *course)
{
    rr_u8 head[24];
    rr_u32 schedule_at, object_at, hazard_at, event_count;
    rr_u32 object_entries, hazard_entries, i;
    rr_u32 active[4] = {0u, 0u, 0u, 0u};
    rr_u32 event_index = 0u, sample = 0u;
    rr_u32 hazard_random_state = 0x5a17u ^
        (global_start * 2654435761u) ^ segment_samples;
    if (!course->objects && !course->hazards && !course->effects) return 1;
    schedule_at = rr_rsrc_be32(segment_row + 12u);
    object_at = rr_rsrc_be32(segment_row + 8u);
    hazard_at = rr_rsrc_be32(segment_row + 28u);
    if (!rr_rsrc_read(file, segments, schedule_at, head, 24u) ||
        rr_rsrc_be32(head) != RR_RSRC_TAG('R','R','S','M'))
        return 0;
    event_count = rr_rsrc_be32(head + 8u);
    if (event_count > MAX_EVENTS ||
        rr_rsrc_be32(head + 4u) != 24u + 12u * event_count ||
        !rr_rsrc_read(file, segments, schedule_at + 24u,
                      g_events, event_count * 12u))
        return 0;
    if (course->objects && object_at != 0u) {
        if (!rr_rsrc_read(file, segments, object_at, head, 20u) ||
            rr_rsrc_be32(head) != RR_RSRC_TAG('R','R','O','B'))
            return 0;
        object_entries = rr_rsrc_be32(head + 8u);
        if (object_entries > MAX_OBJECT_ENTRIES ||
            rr_rsrc_be32(head + 12u) != segment_samples ||
            rr_rsrc_be32(head + 4u) != 20u + 4u * object_entries ||
            !rr_rsrc_read(file, segments, object_at + 20u,
                          g_objects, object_entries * 4u))
            return 0;
        for (i = 0u; i < object_entries && sample < segment_samples; ++i) {
            rr_u32 packed = rr_rsrc_be32(g_objects + i * 4u);
            rr_u32 rows = (packed >> 24u) & 15u;
            rr_u32 row_spacing = rows ? ((packed >> 12u) & 15u) + 1u : 1u;
            rr_u32 end = sample + rows * row_spacing + 1u;
            rr_u32 selector = (packed >> 16u) & 255u;
            rr_u32 group = selector >> 6u;
            rr_u32 spacing_code = rows ? packed & 15u :
                ((packed >> 8u) & 0xf0u) + (packed & 15u);
            rr_u32 spacing = (packed & (1u << 29u)) ?
                1024u + (spacing_code << (rows ? 8u : 5u)) :
                spacing_code << (rows ? 6u : 3u);
            rr_u32 scale = (packed >> 4u) & 15u;
            rr_u32 point;
            if (end > segment_samples) end = segment_samples;
            for (point = sample; point < end; ++point) {
                int variant;
                rr_u32 side, columns, column;
                advance_events(&event_index, event_count, point, active);
                if ((point - sample) % row_spacing ||
                    (selector & 63u) == 63u)
                    continue;
                variant = family_variant(course, active[group]);
                if (variant < 0) continue;
                side = (packed >> 28u) & 1u;
                if ((packed & 0x80000000u) &&
                    ((point - sample) & 1u) == 0u)
                    side ^= 1u;
                columns = ((packed >> 8u) & 7u) + 1u;
                for (column = 0u; column < columns; ++column) {
                    road_scenery_placement_t *out;
                    rr_u32 lateral = spacing * (column + 1u);
                    if (course->object_count >= course->object_capacity ||
                        global_start + point > 65535u) return 0;
                    if (lateral > 65535u) lateral = 65535u;
                    out = &course->objects[course->object_count++];
                    out->sample = (unsigned short)(global_start + point);
                    out->side = (unsigned char)side;
                    out->variant = (unsigned char)variant;
                    out->scale = (unsigned char)scale;
                    out->lateral = (unsigned short)lateral;
                }
            }
            sample += rows * row_spacing + 1u;
        }
    }
    if ((course->hazards || course->effects) && hazard_at != 0u) {
        if (!rr_rsrc_read(file, segments, hazard_at, head, 16u) ||
            rr_rsrc_be32(head) != RR_RSRC_TAG('R','H','Z','D'))
            return 0;
        hazard_entries = rr_rsrc_be32(head + 8u);
        if (hazard_entries > MAX_HAZARD_ENTRIES ||
            rr_rsrc_be32(head + 4u) != 16u + 8u * hazard_entries ||
            !rr_rsrc_read(file, segments, hazard_at + 16u,
                          g_hazards, hazard_entries * 8u))
            return 0;
        active[0] = active[1] = active[2] = active[3] = 0u;
        event_index = 0u;
        sample = 0u;
        for (i = 0u; i < hazard_entries; ++i) {
            rr_u32 control = rr_rsrc_be32(g_hazards + i * 8u);
            rr_u32 detail = rr_rsrc_be32(g_hazards + i * 8u + 4u);
            rr_u32 selector = (control >> 8u) & 255u;
            rr_u32 variant = selector & 63u;
            rr_u32 point;
            int enabled;
            int lateral;
            int sprite;
            road_static_placement_t *out;
            point = rr_schedule_hazard_entry(control, detail,
                course->difficulty_level, &sample,
                &hazard_random_state, &enabled);
            advance_events(&event_index, event_count, point, active);
            if (point >= segment_samples || !enabled)
                continue;
            lateral = (int)(detail & 0xfffu);
            if (lateral & 0x800) lateral -= 0x1000;
            if (control & 0x80000000u) {
                road_effect_placement_t *effect;
                if (!course->effects) continue;
                if (course->effect_count >= course->effect_capacity ||
                    global_start + point > 65535u) return 0;
                effect = &course->effects[course->effect_count++];
                effect->sample = (unsigned short)(global_start + point);
                effect->lateral = (short)lateral;
                effect->travel_mode = (unsigned char)(control & 15u);
                effect->family_inventory =
                    (unsigned char)selector;
                effect->track_offset = (unsigned char)(detail >> 24u);
                effect->family_id =
                    (unsigned short)active[selector >> 6u];
                effect->sprite_index = 255u;
                {
                    rr_u32 position = course->effect_count - 1u;
                    while (position &&
                           course->effects[position - 1u].sample >
                           course->effects[position].sample) {
                        road_effect_placement_t *left =
                            &course->effects[position - 1u];
                        road_effect_placement_t *right =
                            &course->effects[position];
                        unsigned short sample_swap = left->sample;
                        short lateral_swap = left->lateral;
                        unsigned char mode_swap = left->travel_mode;
                        unsigned char family_swap = left->family_inventory;
                        unsigned char offset_swap = left->track_offset;
                        unsigned short id_swap = left->family_id;
                        unsigned char sprite_swap = left->sprite_index;
                        left->sample = right->sample;
                        left->lateral = right->lateral;
                        left->travel_mode = right->travel_mode;
                        left->family_inventory = right->family_inventory;
                        left->track_offset = right->track_offset;
                        left->family_id = right->family_id;
                        left->sprite_index = right->sprite_index;
                        right->sample = sample_swap;
                        right->lateral = lateral_swap;
                        right->travel_mode = mode_swap;
                        right->family_inventory = family_swap;
                        right->track_offset = offset_swap;
                        right->family_id = id_swap;
                        right->sprite_index = sprite_swap;
                        --position;
                    }
                }
                continue;
            }
            sprite = hazard_variant(course, active[selector >> 6u],
                                    variant);
            if (sprite < 0) continue;
            if (!course->hazards) continue;
            if (course->hazard_count >= course->hazard_capacity ||
                global_start + point > 65535u) return 0;
            out = &course->hazards[course->hazard_count++];
            out->sample = (unsigned short)(global_start + point);
            out->lateral = (short)lateral;
            out->variant = (unsigned char)sprite;
            out->visibility_group = (unsigned char)(control & 15u);
            out->mirrored = (unsigned char)((control & 0x80u) != 0u);
            {
                rr_u32 position = course->hazard_count - 1u;
                while (position &&
                       course->hazards[position - 1u].sample >
                       course->hazards[position].sample) {
                    road_static_placement_t *left =
                        &course->hazards[position - 1u];
                    road_static_placement_t *right =
                        &course->hazards[position];
                    unsigned short sample_swap = left->sample;
                    short lateral_swap = left->lateral;
                    unsigned char variant_swap = left->variant;
                    unsigned char visibility_swap = left->visibility_group;
                    unsigned char mirrored_swap = left->mirrored;
                    left->sample = right->sample;
                    left->lateral = right->lateral;
                    left->variant = right->variant;
                    left->visibility_group = right->visibility_group;
                    left->mirrored = right->mirrored;
                    right->sample = sample_swap;
                    right->lateral = lateral_swap;
                    right->variant = variant_swap;
                    right->visibility_group = visibility_swap;
                    right->mirrored = mirrored_swap;
                    --position;
                }
            }
        }
    }
    return 1;
}

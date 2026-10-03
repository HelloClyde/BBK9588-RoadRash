#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "rsrc_reader.h"
#include "road_course_runtime.h"
#include "road_placement_runtime.h"
#include "local-data/upstream-src/track_traversal_runtime.h"
#include "local-data/road_course_data.h"
#include "local-data/road_object_data.h"
#include "local-data/road_static_data.h"

void advance_road_path_traversal(RoadPathTraversalState *, int);

static int file_read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

static int memory_read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    const rr_u8 *data = (const rr_u8 *)user;
    rr_u32 i;
    for (i = 0u; i < size; ++i) ((rr_u8 *)dst)[i] = data[offset + i];
    return 1;
}

int main(int argc, char **argv)
{
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t record;
    rr_u8 segment[24];
    rr_u8 header[16];
    rr_u8 *path;
    rr_u32 offset, count, size, i;
    RoadPathTraversalState traversal;
    FILE *file;
    long file_size;
    rr_u8 malformed[24] = {0};
    signed char curvature[10000];
    signed char elevation[10000];
    unsigned short left_width[10000];
    unsigned short right_width[10000];
    static rr_u8 left_margin[10000], right_margin[10000];
    static rr_u8 left_depth[10000], right_depth[10000];
    static rr_u8 terrain_mode[10000];
    road_scenery_placement_t objects[8000];
    road_static_placement_t hazards[1000];
    road_effect_placement_t effects[1000];
    rr_course_buffer_t course = {0};

    assert(argc == 2);
    assert(!rr_rsrc_open(&catalog, memory_read_at, malformed,
                         sizeof(malformed)));
    file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    file_size = ftell(file);
    assert(file_size > 0);
    assert(rr_rsrc_open(&catalog, file_read_at, file, (rr_u32)file_size));
    assert(catalog.resource_count == 17u);
    assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('S','G','S',' '), 1u,
                        &record));
    assert(record.size == 76184u);
    assert(!rr_rsrc_read(&catalog, &record, record.size, header, 1u));
    assert(rr_rsrc_read(&catalog, &record, 16u, segment,
                        sizeof(segment)));
    offset = rr_rsrc_be32(segment + 4);
    assert(rr_rsrc_read(&catalog, &record, offset, header,
                        sizeof(header)));
    assert(rr_rsrc_be32(header) == RR_RSRC_TAG('R','P','T','H'));
    count = rr_rsrc_be32(header + 8);
    size = rr_rsrc_be32(header + 4);
    assert(count == 900u && size == 16u + count * 2u);
    path = (rr_u8 *)malloc(size);
    assert(path);
    assert(rr_rsrc_read(&catalog, &record, offset, path, size));
    traversal.resource = (const RoadPathResource *)path;
    traversal.elevation = 0;
    traversal.track_position = 0;
    for (i = 0u; i < count; ++i)
        advance_road_path_traversal(&traversal, 256);
    assert(traversal.elevation == 8796);
    assert(traversal.track_position == (int)(count * 256u));
    course.curvature = curvature;
    course.elevation = elevation;
    course.left_width = left_width;
    course.right_width = right_width;
    course.left_margin = left_margin;
    course.right_margin = right_margin;
    course.left_depth = left_depth;
    course.right_depth = right_depth;
    course.terrain_mode = terrain_mode;
    course.objects = objects;
    course.object_capacity = 8000u;
    course.object_count = 0u;
    course.hazards = hazards;
    course.hazard_capacity = 1000u;
    course.hazard_count = 0u;
    course.effects = effects;
    course.effect_capacity = 1000u;
    course.crossing_zones = 0;
    course.crossing_capacity = 0u;
    course.effect_count = 0u;
    course.difficulty_level = 0u;
    course.family_ids = 0;
    course.family_capacity = 0u;
    course.family_count = 0u;
    course.hazard_family_ids = 0;
    course.hazard_frame_ids = 0;
    course.hazard_family_capacity = 0u;
    course.hazard_family_count = 0u;
    course.capacity = sizeof(curvature);
    course.sample_count = 0u;
    course.segment_count = 0u;
    if (!rr_course_load_primary(&catalog, &course)) {
        fprintf(stderr, "route decode stopped after %u segments/%u samples\n",
                course.segment_count, course.sample_count);
        return 1;
    }
    assert(course.sample_count == ROAD_COURSE_SAMPLE_COUNT);
    assert(course.segment_count == ROAD_COURSE_SEGMENT_COUNT);
    assert(left_margin[0] == 224u && right_margin[0] == 224u);
    assert(left_margin[1] == 255u && right_margin[1] == 255u);
    assert(left_depth[0] == 0u && right_depth[0] == 0u);
    assert(terrain_mode[0] == 4u);
    assert(course.object_count == ROAD_OBJECT_COUNT);
    assert(course.hazard_count > 0u &&
           course.hazard_count <= ROAD_STATIC_COUNT);
    for (i = 0u; i < course.sample_count; ++i) {
        assert(curvature[i] == g_original_course_curvature[i]);
        assert(elevation[i] == g_original_course_elevation[i]);
        assert(left_width[i] <= 2048u);
        assert(right_width[i] <= 2048u);
    }
    for (i = 0u; i < course.object_count; ++i) {
        const road_scenery_placement_t *want = &g_original_object_placements[i];
        const road_scenery_placement_t *got = &objects[i];
        assert(got->sample == want->sample && got->side == want->side &&
               got->variant == want->variant && got->scale == want->scale &&
               got->lateral == want->lateral);
    }
    for (i = 0u; i < course.hazard_count; ++i) {
        const road_static_placement_t *got = &hazards[i];
        assert(got->sample < course.sample_count && got->variant < 6u &&
               got->visibility_group <= 15u);
        if (i) assert(hazards[i - 1u].sample <= got->sample);
    }
    {
        rr_u32 sample_a = 0u, sample_b = 0u;
        rr_u32 seed_a = 123u, seed_b = 123u;
        rr_u32 point_a, point_b;
        int enabled_a, enabled_b;
        rr_u32 control = 0x05000000u | (15u << 20u) |
                         (2u << 15u);
        point_a = rr_schedule_hazard_entry(control, 1u << 12u, 0u,
                                           &sample_a, &seed_a, &enabled_a);
        point_b = rr_schedule_hazard_entry(control, 1u << 12u, 0u,
                                           &sample_b, &seed_b, &enabled_b);
        assert(point_a == point_b && point_a >= 5u && point_a <= 9u);
        assert(enabled_a && enabled_b && sample_a == 5u);
        (void)rr_schedule_hazard_entry(control, 2u << 12u, 0u,
                                       &sample_a, &seed_a, &enabled_a);
        assert(!enabled_a && sample_a == 10u);
        (void)rr_schedule_hazard_entry(0x8000000bu, 0u, 0u,
                                       &sample_a, &seed_a, &enabled_a);
        assert(!enabled_a);
    }
    printf("RSRC %u resources; upstream RPTH traversal %u samples, "
           "elevation %d; live route %u segments/%u samples: PASS\n",
           catalog.resource_count, count, traversal.elevation,
           course.segment_count, course.sample_count);
    free(path);
    fclose(file);
    return 0;
}

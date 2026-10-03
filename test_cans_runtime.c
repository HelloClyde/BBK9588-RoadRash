#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cans_runtime.h"
#include "cel_runtime.h"

static unsigned short pixels[16384];

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t anim, cans;
    road_car_frame_map_t map;
    road_car_frame_map_t maps[16];
    road_car_animation_t animations[16];
    FILE *file;
    long size;
    rr_u32 animation_id, cans_id = 1u, found = 0u, bound = 0u;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    for (animation_id = 1u; animation_id <= 16u; ++animation_id) {
        rr_u8 *anim_bytes;
        rr_u32 frame_count, row, column;
        rr_u32 widths[ROAD_CAR_MAX_FRAMES];
        rr_u32 heights[ROAD_CAR_MAX_FRAMES];
        rr_cel_image_t image;
        road_car_animation_t animation;
        if (!rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                          animation_id, &anim)) break;
        anim_bytes = (rr_u8 *)malloc(anim.size);
        assert(anim_bytes && rr_rsrc_read(&catalog, &anim, 0u,
                                          anim_bytes, anim.size));
        frame_count = rr_animation_frame_count(anim_bytes, anim.size);
        image.pixels = pixels;
        for (row = 0u; row < frame_count; ++row) {
            assert(rr_animation_decode_frame(anim_bytes, anim.size,
                                             row, 0, &image, 16384u));
            widths[row] = image.width;
            heights[row] = image.height;
        }
        free(anim_bytes);
        assert(frame_count == 3u || frame_count == 12u);
        if (rr_rsrc_find(&catalog, RR_RSRC_TAG('C','A','N','S'),
                         cans_id, &cans)) {
            rr_u8 *cans_bytes = (rr_u8 *)malloc(cans.size);
            assert(cans_bytes && rr_rsrc_read(&catalog, &cans, 0u,
                                              cans_bytes, cans.size));
            assert(rr_cans_parse_car_frames(cans_bytes, cans.size,
                                            frame_count, &map));
            free(cans_bytes);
            ++cans_id;
        } else {
            assert(animation_id > 1u);
        }
        for (row = 0u; row < 3u; ++row)
            for (column = 0u; column < map.column_count; ++column) {
                assert(map.frames[row][column] < frame_count);
                ++bound;
            }
        for (column = 0u; column < map.column_count; ++column) {
            assert(widths[map.frames[2u][column]] >
                   widths[map.frames[0u][column]]);
            assert(widths[map.frames[0u][column]] >
                   widths[map.frames[1u][column]]);
        }
        assert(map.column_count == 2u || map.column_count == 9u ||
               map.column_count == 16u);
        assert(map.direction_flags == 0x100000u ||
               map.direction_flags == 0x80000u ||
               map.direction_flags == 0x40000u);
        assert(map.scale_multiplier[0] == 2u);
        assert(map.scale_multiplier[1] == 4u);
        assert(map.scale_multiplier[2] == 1u);
        maps[found] = map;
        animations[found].frames = 0;
        animations[found].frame_count = frame_count;
        animations[found].frame_map = &maps[found];
        animations[found].anchors = 0;
        animation.frames = 0;
        animation.frame_count = frame_count;
        animation.frame_map = &map;
        animation.anchors = 0;
        column = map.column_count / 2u;
        assert(road_car_select_mapped_frame(&animation, 0x500u) ==
               map.frames[2][column]);
        assert(road_car_select_mapped_frame(&animation, 0x501u) ==
               map.frames[0][column]);
        assert(road_car_select_mapped_frame(&animation, 0xa00u) ==
               map.frames[0][column]);
        assert(road_car_select_mapped_frame(&animation, 0xa01u) ==
               map.frames[1][column]);
        {
            road_sprite_t sprite;
            int near_height, middle_height, far_height;
            sprite.width = widths[map.frames[2][column]];
            sprite.height = heights[map.frames[2][column]];
            sprite.pixels = 0;
            near_height = road_car_projected_height(
                &animation, &sprite, 0x500u);
            sprite.width = widths[map.frames[0][column]];
            sprite.height = heights[map.frames[0][column]];
            middle_height = road_car_projected_height(
                &animation, &sprite, 0x501u);
            sprite.width = widths[map.frames[1][column]];
            sprite.height = heights[map.frames[1][column]];
            far_height = road_car_projected_height(
                &animation, &sprite, 0xa01u);
            assert(near_height > 0 && middle_height > 0 && far_height > 0);
            assert(abs(near_height - middle_height) <= 8);
        }
        ++found;
    }
    assert(found >= 10u && cans_id > 1u);
    for (animation_id = 0u; animation_id < 4u; ++animation_id) {
        rr_u32 choice;
        rr_u32 mask = animation_id & 1u ? 0x40000u :
                      animation_id == 2u ? 0x80000u : 0x100000u;
        for (choice = 0u; choice < found * 2u; ++choice) {
            rr_u32 selected = road_car_select_animation(
                animations, found, animation_id, choice);
            assert(selected < found - 1u);
            assert(maps[selected].direction_flags == mask);
        }
    }
    printf("%s: %u car CANS bindings, %u frame references\n",
           argv[1], found, bound);
    fclose(file);
    return 0;
}

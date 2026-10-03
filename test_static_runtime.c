#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cel_runtime.h"
#include "road_core.h"
#include "local-data/road_static_data.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

static rr_u32 pixel_hash(const unsigned short *pixels, rr_u32 count)
{
    rr_u32 hash = 2166136261u, i;
    for (i = 0u; i < count; ++i)
        hash = (hash ^ pixels[i]) * 16777619u;
    return hash;
}

int main(int argc, char **argv)
{
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t record;
    rr_cel_image_t image;
    unsigned short pixels[8192];
    rr_u8 *scratch;
    rr_u8 *family;
    FILE *file;
    long size;
    rr_u32 id, slot, i;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0);
    assert(rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    scratch = (rr_u8 *)malloc(65536u);
    assert(scratch);
    image.pixels = pixels;
    for (id = 148u; id <= 149u; ++id) {
        assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('F','A','M',' '), id,
                            &record));
        family = (rr_u8 *)malloc(record.size);
        assert(family && rr_rsrc_read(&catalog, &record, 0u,
                                      family, record.size));
        for (slot = 0u; slot < ROAD_STATIC_SPRITE_COUNT; ++slot) {
            const road_sprite_t *expected = &g_original_static_sprites[slot];
            assert(rr_family_decode_static_frame(family, record.size, slot,
                                                  &image, 4096u));
            assert(image.width == expected->width);
            assert(image.height == expected->height);
            for (i = 0u; i < image.width * image.height; ++i)
                assert(pixels[i] == expected->pixels[i]);
        }
        free(family);
    }
    {
        static const struct {
            rr_u32 family_id, variant, width, height, hash;
            int marker;
        } cases[] = {
            {65u, 0u, 34u, 95u, 0x397d78d6u, 0},
            {171u, 0u, 19u, 35u, 0x6af3c978u, 1},
            {69u, 0u, 35u, 23u, 0xe7d57c8au, 0},
            {27u, 5u, 130u, 50u, 0x45a7fdb9u, 0}
        };
        for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            rr_cel_hotspot_box_t boxes[2];
            rr_u32 box_count;
            assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('F','A','M',' '),
                                cases[i].family_id, &record));
            family = (rr_u8 *)malloc(record.size);
            assert(family && rr_rsrc_read(&catalog, &record, 0u,
                                          family, record.size));
            if (cases[i].marker)
                assert(rr_family_decode_hazard_preview(&catalog, family,
                    record.size, cases[i].family_id, cases[i].variant,
                    &image, 8192u, scratch, 65536u) == 1);
            else
                assert(rr_family_decode_static_frame(family, record.size,
                    cases[i].variant, &image, 8192u));
            assert(image.width == cases[i].width &&
                   image.height == cases[i].height);
            assert(pixel_hash(pixels, image.width * image.height) ==
                   cases[i].hash);
            box_count = rr_family_decode_static_hotspots(family,
                record.size, cases[i].family_id, cases[i].variant, boxes);
            if (cases[i].family_id == 65u) {
                assert(box_count == 2u);
                assert(boxes[0].left == -3 && boxes[0].top == -64 &&
                       boxes[0].right == 6 && boxes[0].bottom == 1);
                assert(boxes[1].left == -11 && boxes[1].top == -92 &&
                       boxes[1].right == 12 && boxes[1].bottom == -64);
            } else if (cases[i].family_id == 171u) {
                assert(box_count == 1u);
                assert(boxes[0].left == -1 && boxes[0].top == -17 &&
                       boxes[0].right == 3 && boxes[0].bottom == 1);
            } else if (cases[i].family_id == 69u) {
                assert(box_count == 1u);
                assert(boxes[0].left == -25 && boxes[0].top == -38 &&
                       boxes[0].right == 28 && boxes[0].bottom == -7);
            } else assert(box_count == 0u);
            free(family);
        }
    }
    {
        static const rr_u32 expected_width[3] = {17u, 35u, 70u};
        static const rr_u32 expected_height[3] = {11u, 23u, 45u};
        static const short anchor_x[3] = {10, 20, 40};
        static const short anchor_y[3] = {10, 21, 42};
        rr_cel_hotspot_box_t boxes[2];
        assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('F','A','M',' '),
                            69u, &record));
        family = (rr_u8 *)malloc(record.size);
        assert(family && rr_rsrc_read(&catalog, &record, 0u,
                                      family, record.size));
        for (slot = 0u; slot < 3u; ++slot) {
            rr_cel_anchor_t anchor;
            assert(rr_family_decode_static_frame_bucket(family,
                record.size, 0u, slot, &image, 8192u));
            assert(image.width == expected_width[slot]);
            assert(image.height == expected_height[slot]);
            assert(rr_family_decode_static_anchor_bucket(family,
                record.size, 0u, slot, &anchor));
            assert(anchor.x == anchor_x[slot] &&
                   anchor.y == anchor_y[slot]);
        }
        assert(rr_family_decode_static_hotspots_bucket(family,
            record.size, 69u, 0u, 2u, boxes) == 1u);
        assert(boxes[0].left == -25 && boxes[0].top == -38 &&
               boxes[0].right == 28 && boxes[0].bottom == -7);
        free(family);
    }
    free(scratch);
    printf("FAM 148/149: six debris sprites each matched original pixels: PASS\n");
    printf("FAM 65/171/69/27: nested PDAT/RPDT pixels matched source: PASS\n");
    printf("FAM 65/171/69: nearest-frame HSPT bounds matched source: PASS\n");
    printf("FAM 69: all three original roadside scale frames decoded: PASS\n");
    fclose(file);
    return 0;
}

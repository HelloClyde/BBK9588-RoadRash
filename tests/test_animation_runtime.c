#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "cel_runtime.h"
#include "road_core.h"
#include "local-data/road_bike_data.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    static const rr_u32 extra_frames[] = {
        1u, 2u, 5u, 6u, 13u, 14u, 18u, 19u, 319u,
        241u, 242u, 243u, 244u, 245u, 246u, 247u,
        282u, 283u, 284u, 285u, 286u, 289u,
        303u, 304u, 305u, 306u, 307u, 308u,
        309u, 310u, 311u, 312u, 313u,
        213u, 214u, 215u, 216u, 217u, 219u,
        225u, 226u, 227u, 228u, 229u, 230u,
        1u, 31u, 32u
    };
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t anim;
    rr_cel_image_t image;
    unsigned short pixels[16384];
    rr_u8 *record;
    FILE *file;
    long file_size;
    rr_u32 slot, i;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    file_size = ftell(file);
    assert(file_size > 0);
    assert(rr_rsrc_open(&catalog, read_at, file, (rr_u32)file_size));
    image.pixels = pixels;
    for (slot = 0u; slot < ROAD_BIKE_COUNT; ++slot) {
        rr_u32 animation_id = slot == ROAD_BIKE_OPPONENT_INDEX ||
            slot >= ROAD_BIKE_OPPONENT_HIT_FIRST ? 3u : 2u;
        rr_u32 frame_index = slot == 0u ? 3u :
            slot == 1u ? 20u : slot == 2u ? 0u :
            slot == ROAD_BIKE_OPPONENT_INDEX ? 0u :
            slot >= ROAD_BIKE_TURN_FIRST ?
            extra_frames[slot - ROAD_BIKE_TURN_FIRST] :
            100u + slot - 3u;
        const road_sprite_t *expected = &g_original_bikes[slot];
        assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                            animation_id, &anim));
        record = (rr_u8 *)malloc(anim.size);
        assert(record);
        assert(rr_rsrc_read(&catalog, &anim, 0u, record, anim.size));
        assert(rr_animation_decode_frame(record, anim.size, frame_index,
                                         animation_id == 3u, &image,
                                         16384u));
        assert(image.width == expected->width);
        assert(image.height == expected->height);
        for (i = 0u; i < image.width * image.height; ++i)
            assert(pixels[i] == expected->pixels[i]);
        free(record);
    }
    {
        static const unsigned int widths[9] =
            {128u, 64u, 32u, 16u, 16u, 16u, 8u, 8u, 8u};
        static const unsigned int heights[9] =
            {32u, 16u, 8u, 8u, 4u, 2u, 4u, 2u, 1u};
        static const unsigned int reference_hashes[3] =
            {0x2022f844u, 0xe4da208au, 0x46642419u};
        for (slot = 0u; slot < 27u; ++slot) {
            unsigned int hash = 2166136261u;
            assert(rr_cel_load_road_surface(&catalog, slot + 1u,
                                             &image, 16384u));
            assert(image.width == widths[slot % 9u]);
            assert(image.height == heights[slot % 9u]);
            for (i = 0u; i < image.width * image.height; ++i)
                hash = (hash ^ pixels[i]) * 16777619u;
            if (slot % 9u == 0u)
                assert(hash == reference_hashes[slot / 9u]);
        }
    }
    {
        static unsigned short hud_pixels[300u * 58u];
        static rr_u8 scratch[12000u];
        unsigned int hash = 2166136261u;
        image.pixels = hud_pixels;
        assert(rr_cel_load_packed_image(&catalog, 55u, &image,
                                         300u * 58u,
                                         scratch, sizeof(scratch)));
        assert(image.width == 300u && image.height == 58u);
        for (i = 0u; i < 300u * 58u; ++i)
            hash = (hash ^ hud_pixels[i]) * 16777619u;
        assert(hash == 0xdf4d9261u);
        assert(rr_cel_load_packed_image(&catalog, 59u, &image,
                                         300u * 58u,
                                         scratch, sizeof(scratch)));
        assert(image.width == 43u && image.height == 14u);
        for (slot = 57u; slot <= 58u; ++slot) {
            unsigned int visible = 0u;
            image.pixels = hud_pixels;
            assert((slot == 57u ? rr_cel_load_uncoded6(&catalog,
                slot, &image, 300u * 58u) : rr_cel_load_uncoded1(
                &catalog, slot, &image, 300u * 58u)));
            for (i = 0u; i < image.width * image.height; ++i)
                visible += hud_pixels[i] != RR_CEL_TRANSPARENT;
            assert(visible > 0u && visible < image.width * image.height);
            printf("HUD needle CEL %u: %ux%u\n", slot,
                   image.width, image.height);
        }
    }
    {
        rr_rsrc_record_t gauge;
        static unsigned short gauge_pixels[32u * 8u];
        assert(rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                            1u, &gauge));
        record = (rr_u8 *)malloc(gauge.size);
        assert(record);
        assert(rr_rsrc_read(&catalog, &gauge, 0u, record, gauge.size));
        assert(rr_animation_frame_count(record, gauge.size) == 32u);
        image.pixels = gauge_pixels;
        for (slot = 0u; slot < 32u; ++slot) {
            assert(rr_animation_decode_rect_frame(record, gauge.size,
                slot, &image, 32u * 8u));
            assert(image.width == 32u && image.height == 8u);
        }
        free(record);
    }
    printf("%u original ANIM CEL frames matched offline RGB565 pixels: PASS\n",
           (unsigned int)ROAD_BIKE_COUNT);
    puts("27 original road-strip CELs decoded; near-strip hashes matched: PASS");
    puts("Original packed race HUD matched independent RGB565 decode: PASS");
    puts("32 original default-layout health gauge frames decoded: PASS");
    fclose(file);
    return 0;
}

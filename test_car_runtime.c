#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    rr_rsrc_record_t record;
    rr_cel_image_t image;
    FILE *file;
    long size;
    rr_u32 i, found = 0u, decoded = 0u, frames = 0u;
    rr_u32 explicit_anchors = 0u;
    rr_u32 pixel_bytes = 0u;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    image.pixels = pixels;
    for (i = 0u; i < catalog.resource_count; ++i) {
        rr_u8 *bytes;
        assert(rr_rsrc_record_at(&catalog, i, &record));
        if (record.type != RR_RSRC_TAG('A','N','I','M')) continue;
        ++found;
        bytes = (rr_u8 *)malloc(record.size);
        assert(bytes && rr_rsrc_read(&catalog, &record, 0u,
                                     bytes, record.size));
        {
            rr_u32 frame, count = rr_animation_frame_count(bytes, record.size);
            rr_u32 widths[12], heights[12];
            assert(count == 3u || count == 12u);
            for (frame = 0u; frame < count; ++frame) {
            rr_u32 pixel, opaque = 0u;
            rr_cel_anchor_t anchor;
            assert(rr_animation_decode_frame(bytes, record.size, frame, 0,
                                             &image, 16384u));
            if (rr_animation_decode_frame_anchor(bytes, record.size,
                                                  frame, &anchor)) {
                ++explicit_anchors;
                if (record.id == 3u && frame == 0u &&
                    strstr(argv[1], "Medley")) {
                    assert(anchor.x == 16 && anchor.y == 25);
                }
            }
            for (pixel = 0u; pixel < image.width * image.height; ++pixel)
                if (pixels[pixel] != RR_CEL_TRANSPARENT) ++opaque;
            assert(opaque > 0u);
            ++frames;
            pixel_bytes += image.width * image.height * 2u;
            widths[frame] = image.width;
            heights[frame] = image.height;
            }
            for (frame = 0u; frame < count; frame += 3u) {
                assert(widths[frame] > widths[frame + 1u]);
                assert(widths[frame + 1u] > widths[frame + 2u]);
                assert(heights[frame] >= heights[frame + 1u]);
                assert(heights[frame + 1u] >= heights[frame + 2u]);
            }
            if (record.id == 1u)
                printf("    center frames: near %ux%u, middle %ux%u, far %ux%u\n",
                       widths[0], heights[0], widths[1], heights[1],
                       widths[2], heights[2]);
            ++decoded;
            printf("  ANIM %u: %u frames\n", record.id, count);
        }
        free(bytes);
    }
    printf("%s: decoded %u/%u car animations, %u frames, %u RGB565 bytes\n",
           argv[1], decoded, found, frames, pixel_bytes);
    assert(explicit_anchors == (strstr(argv[1], "Medley") ? 12u : 0u));
    fclose(file);
    return found > 0u && decoded == found ? 0 : 1;
}

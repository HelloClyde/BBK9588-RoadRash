#include <assert.h>
#include <stdio.h>
#include "cel_runtime.h"

static unsigned short pixels[320u * 240u];
static rr_u8 scratch[160000u];

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

static void check(const char *path, const rr_u32 *ids, rr_u32 count)
{
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    FILE *file = fopen(path, "rb");
    long size;
    rr_u32 i;
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    image.pixels = pixels;
    for (i = 0u; i < count; ++i) {
        rr_u32 pixel, nonzero = 0u;
        assert(rr_cel_load_rgb16(&catalog, ids[i], &image,
                                 320u * 240u, scratch, sizeof(scratch)));
        assert(image.width == 320u && image.height == 240u);
        for (pixel = 0u; pixel < image.width * image.height; ++pixel)
            if (pixels[pixel] != 0u &&
                pixels[pixel] != RR_CEL_TRANSPARENT) ++nonzero;
        assert(nonzero > 500u);
        if (ids[i] == 16u) {
            /* Independently decoded RGB555 title backdrop samples. */
            assert(pixels[0u] == 0u);
            assert(pixels[100u * 320u + 100u] == 0x5a8eu);
            assert(pixels[100u * 320u + 200u] == 0x6208u);
            assert(pixels[200u * 320u + 300u] == 0x0802u);
        }
        printf("  CEL %u: %u nonblack pixels\n", ids[i], nonzero);
    }
    fclose(file);
}

int main(int argc, char **argv)
{
    static const rr_u32 splash[] = {1u};
    static const rr_u32 front[] = {1u, 13u, 14u, 15u, 16u};
    assert(argc == 3);
    check(argv[1], splash, sizeof(splash) / sizeof(splash[0]));
    check(argv[2], front, sizeof(front) / sizeof(front[0]));
    puts("Packed 16-bit front-end CELs: PASS");
    return 0;
}

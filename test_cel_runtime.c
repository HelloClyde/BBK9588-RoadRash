#include <assert.h>
#include <stdio.h>
#include "cel_runtime.h"
#include "road_core.h"
#include "local-data/road_scenery_data.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    static const rr_u32 ids[9] = {101u,102u,148u,149u,169u,171u,174u,175u,240u};
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    unsigned short pixels[4096];
    rr_u8 scratch[65536];
    FILE *file;
    long size;
    rr_u32 slot, i;
    assert(argc == 2);
    file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0);
    assert(rr_rsrc_open(&catalog, read_at, file, (rr_u32)size));
    image.pixels = pixels;
    for (slot = 0u; slot < 9u; ++slot) {
        const road_sprite_t *expected = &g_original_scenery[slot];
        assert(rr_family_load_frame(&catalog, ids[slot], ids[slot] == 171u,
                                    &image, 4096u, scratch,
                                    sizeof(scratch)));
        assert(image.width == expected->width);
        assert(image.height == expected->height);
        for (i = 0u; i < image.width * image.height; ++i)
            assert(pixels[i] == expected->pixels[i]);
    }
    printf("9 original FAM CEL images matched offline RGB565 pixels: PASS\n");
    fclose(file);
    return 0;
}

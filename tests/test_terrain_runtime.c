#include <assert.h>
#include <stdio.h>
#include "cel_runtime.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
        fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    static unsigned short pixels[16384];
    static rr_u8 scratch[32768];
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    FILE *file;
    long size;
    unsigned int hash, i;
    assert(argc == 2 || argc == 3);
    file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, file,
                                     (rr_u32)size));
    image.pixels = pixels;
    assert(rr_family_decode_surface_tile(&catalog, 171u, 2u, 0u,
        &image, 16384u, scratch, sizeof(scratch)));
    hash = 2166136261u;
    for (i = 0u; i < image.width * image.height; ++i)
        hash = (hash ^ pixels[i]) * 16777619u;
    printf("RHIL CLGP: %ux%u %08x\n", image.width,
           image.height, hash);
    assert(image.width == 32u && image.height == 16u &&
           hash == 0x7889e573u);
    assert(rr_family_decode_surface_tile(&catalog, 101u, 0u,
        0xffffffffu, &image, 16384u, scratch, sizeof(scratch)));
    hash = 2166136261u;
    for (i = 0u; i < image.width * image.height; ++i)
        hash = (hash ^ pixels[i]) * 16777619u;
    printf("RBLD CLGP: %ux%u %08x\n", image.width,
           image.height, hash);
    assert(image.width == 32u && image.height == 32u &&
           hash == 0x7ec37142u);
    assert(rr_family_decode_surface_tile(&catalog, 3u, 0u,
        0xffffffffu, &image, 16384u, scratch, sizeof(scratch)));
    hash = 2166136261u;
    for (i = 0u; i < image.width * image.height; ++i)
        hash = (hash ^ pixels[i]) * 16777619u;
    printf("City 6bpp CLGP: %ux%u %08x\n", image.width,
           image.height, hash);
    assert(image.width == 32u && image.height == 32u &&
           hash == 0xb310c579u);
    {
        unsigned int row;
        for (row = 1u; row <= 8u; row <<= 1u) {
            unsigned int opaque = 0u;
            assert(rr_family_decode_surface_tile_size(&catalog,
                15u, 0u, 1u, 128u, row, &image, 16384u,
                scratch, sizeof(scratch)));
            assert(image.width == 128u && image.height == row);
            for (i = 0u; i < image.width * image.height; ++i)
                if (pixels[i] != RR_CEL_TRANSPARENT) ++opaque;
            assert(opaque > image.width * image.height / 2u);
        }
        printf("RMTN threshold CLGP: 128x1/2/4/8 decoded\n");
    }
    if (argc == 3) {
        FILE *output = fopen(argv[2], "wb");
        assert(output);
        assert(fprintf(output, "P6\n%u %u\n255\n",
            image.width, image.height) > 0);
        for (i = 0u; i < image.width * image.height; ++i) {
            unsigned short color = pixels[i];
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(((color >> 11u) & 31u) * 255u / 31u);
            rgb[1] = (unsigned char)(((color >> 5u) & 63u) * 255u / 63u);
            rgb[2] = (unsigned char)((color & 31u) * 255u / 31u);
            assert(fwrite(rgb, 1u, 3u, output) == 3u);
        }
        assert(fclose(output) == 0);
    }
    fclose(file);
    return 0;
}

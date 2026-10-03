#include <assert.h>
#include <stdio.h>
#include "cel_runtime.h"
#include "road_core.h"
#include "local-data/road_backdrop_data.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    rr_rsrc_file_t file;
    rr_u8 indices[360u * 94u];
    unsigned short palette[256];
    rr_u32 width, height, i;
    FILE *input;
    long size;
    assert(argc == 2);
    input = fopen(argv[1], "rb");
    assert(input && fseek(input, 0, SEEK_END) == 0);
    size = ftell(input);
    assert(size > 0);
    assert(rr_rsrc_open(&file, read_at, input, (rr_u32)size));
    assert(rr_cel_load_backdrop(&file, 2u, indices, sizeof(indices),
                                palette, &width, &height));
    assert(width == ROAD_BACKDROP_WIDTH && height == ROAD_BACKDROP_HEIGHT);
    for (i = 0u; i < width * height; ++i)
        assert(indices[i] == g_original_backdrop_indices[i]);
    for (i = 0u; i < 256u; ++i)
        assert(palette[i] == g_original_backdrop_palette[i]);
    printf("Original 360x94 CEL backdrop matched pixels and palette: PASS\n");
    fclose(input);
    return 0;
}

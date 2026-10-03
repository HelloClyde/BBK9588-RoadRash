#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "aiff_runtime.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *f = (FILE *)user;
    return fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, f) == size;
}

int main(int argc, char **argv)
{
    rr_rsrc_file_t catalog;
    rr_aiff_sample_t sample;
    rr_u32 id;
    short *pcm;
    FILE *f;
    long size;
    assert(argc == 2);
    f = fopen(argv[1], "rb");
    assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(size > 0 && rr_rsrc_open(&catalog, read_at, f, (rr_u32)size));
    for (id = 1u; id <= 19u; ++id) {
        if (!rr_aiff_find_sample(&catalog, id, &sample)) {
            fprintf(stderr, "Failed AIFF %u\n", id);
            return 1;
        }
        pcm = (short *)malloc(sample.pcm_bytes);
        assert(pcm);
        assert(rr_aiff_read_frames(&catalog, &sample, 0u, pcm,
                                   sample.frame_count));
        printf("AIFF %u: %u Hz, %u frames\n", id,
               sample.source_rate, sample.frame_count);
        free(pcm);
    }
    assert(!rr_aiff_find_sample(&catalog, 20u, &sample));
    fclose(f);
    return 0;
}

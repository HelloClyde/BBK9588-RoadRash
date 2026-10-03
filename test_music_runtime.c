#include <assert.h>
#include <stdio.h>
#include "music_runtime.h"

static unsigned int large_reads;

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *f = (FILE *)user;
    if (size >= 1024u) ++large_reads;
    return fseek(f, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1, size, f) == size;
}

int main(int argc, char **argv)
{
    rr_music_stream_t stream;
    short pcm[512];
    FILE *f;
    long size;
    unsigned int i;
    long energy = 0;
    rr_u32 random_a = 0x12345678u, random_b = 0x12345678u;
    rr_u32 previous = 14u;
    assert(argc == 2 || argc == 3);
    assert(rr_music_choose_next(&random_a, previous, 0u) == 0u);
    assert(rr_music_choose_next(&random_a, previous, 1u) == 0u);
    random_a = random_b;
    for (i = 0u; i < 128u; ++i) {
        rr_u32 selected = rr_music_choose_next(&random_a, previous, 14u);
        assert(selected < 14u && selected != previous);
        assert(selected == rr_music_choose_next(&random_b,
            previous, 14u));
        previous = selected;
    }
    f = fopen(argv[1], "rb");
    assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(size > 0);
    assert(rr_music_open(&stream, read_at, f, (rr_u32)size));
    assert(rr_music_read_mono(&stream, pcm, 512u) == 512u);
    for (i = 0u; i < 512u; ++i)
        energy += pcm[i] < 0 ? -(long)pcm[i] : pcm[i];
    printf("SDX2 first 512 frames: energy %ld\n", energy);
    if (argc == 3) {
        FILE *out = fopen(argv[2], "wb");
        assert(out && fwrite(pcm, sizeof(short), 512u, out) == 512u);
        fclose(out);
    }
    for (i = 0u; i < 64u; ++i)
        assert(rr_music_read_mono(&stream, pcm, 512u) == 512u);
    assert(large_reads < 30u);
    fclose(f);
    return 0;
}

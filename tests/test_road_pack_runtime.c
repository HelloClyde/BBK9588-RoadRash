#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "road_pack_runtime.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    rr_pack_t pack;
    FILE *packed;
    unsigned int i;
    long size;
    assert(argc == 3);
    packed = fopen(argv[1], "rb");
    assert(packed);
    assert(fseek(packed, 0, SEEK_END) == 0);
    size = ftell(packed);
    assert(size > 0 && rr_pack_open(&pack, read_at, packed,
                                   (rr_u32)size));
    assert(pack.count == 32u);
    assert(rr_pack_find(&pack, "rashOpt.rsrc") >= 0);
    assert(rr_pack_find(&pack,
        "Streams\\bgaudio\\SG.RustyCage_sw22.stream") >= 0);
    assert(rr_pack_find(&pack, "missing.rsrc") == -1);
    for (i = 0u; i < pack.count; ++i) {
        char path[256];
        FILE *original;
        rr_u32 places[3];
        unsigned int j;
        unsigned char a[32], b[32];
        int length = snprintf(path, sizeof(path), "%s/%s", argv[2],
                              pack.entries[i].name);
        assert(length > 0 && (unsigned int)length < sizeof(path));
        original = fopen(path, "rb");
        assert(original);
        assert(fseek(original, 0, SEEK_END) == 0);
        assert((rr_u32)ftell(original) == pack.entries[i].size);
        places[0] = 0u;
        places[1] = pack.entries[i].size / 2u;
        places[2] = pack.entries[i].size > sizeof(a) ?
            pack.entries[i].size - sizeof(a) : 0u;
        for (j = 0u; j < 3u; ++j) {
            rr_u32 count = pack.entries[i].size - places[j];
            if (count > sizeof(a)) count = sizeof(a);
            assert(rr_pack_read(&pack, i, places[j], a, count));
            assert(fseek(original, (long)places[j], SEEK_SET) == 0);
            assert(fread(b, 1u, count, original) == count);
            assert(memcmp(a, b, count) == 0);
        }
        assert(!rr_pack_read(&pack, i, pack.entries[i].size, a, 1u));
        fclose(original);
    }
    fclose(packed);
    puts("Single-file pack index and random reads: PASS");
    return 0;
}

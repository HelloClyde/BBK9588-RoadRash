#include <stdio.h>
#include <stdlib.h>
#include "rsrc_reader.h"

static int read_at(void *user, rr_u32 offset, void *dst, rr_u32 size)
{
    FILE *file = (FILE *)user;
    return fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(dst, 1u, size, file) == size;
}

int main(int argc, char **argv)
{
    unsigned int total = 0u;
    int arg;
    if (argc < 2) return 2;
    for (arg = 1; arg < argc; ++arg) {
        rr_rsrc_file_t catalog;
        rr_rsrc_record_t record;
        rr_u32 i;
        long size;
        FILE *file = fopen(argv[arg], "rb");
        if (!file || fseek(file, 0, SEEK_END) != 0 ||
            (size = ftell(file)) < 0 ||
            !rr_rsrc_open(&catalog, read_at, file, (rr_u32)size)) {
            fprintf(stderr, "Invalid RSRC: %s\n", argv[arg]);
            return 1;
        }
        for (i = 0u; i < catalog.resource_count; ++i) {
            rr_rsrc_record_t found;
            if (!rr_rsrc_record_at(&catalog, i, &record)) {
                fprintf(stderr, "Invalid record %u: %s\n", i, argv[arg]);
                return 1;
            }
            if (!rr_rsrc_find(&catalog, record.type, record.id, &found) ||
                found.offset != record.offset || found.size != record.size) {
                fprintf(stderr, "Index lookup mismatch %u: %s\n", i,
                        argv[arg]);
                return 1;
            }
        }
        total += catalog.resource_count;
        fclose(file);
    }
    printf("%d RSRC catalogs, %u indexed records: PASS\n", argc - 1, total);
    return 0;
}

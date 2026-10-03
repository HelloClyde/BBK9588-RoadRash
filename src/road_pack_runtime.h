#ifndef ROAD_RASH_PACK_RUNTIME_H
#define ROAD_RASH_PACK_RUNTIME_H

#include "rsrc_reader.h"

#define RR_PACK_MAX_FILES 40u
#define RR_PACK_NAME_BYTES 48u
#define RR_PACK_ENTRY_BYTES 56u

typedef struct rr_pack_entry {
    char name[RR_PACK_NAME_BYTES];
    rr_u32 offset;
    rr_u32 size;
} rr_pack_entry_t;

typedef struct rr_pack {
    rr_read_at_t read_at;
    void *user;
    rr_u32 file_size;
    rr_u32 count;
    rr_pack_entry_t entries[RR_PACK_MAX_FILES];
} rr_pack_t;

int rr_pack_open(rr_pack_t *pack, rr_read_at_t read_at, void *user,
                 rr_u32 file_size);
int rr_pack_find(const rr_pack_t *pack, const char *name);
int rr_pack_read(const rr_pack_t *pack, unsigned int index,
                 rr_u32 offset, void *dst, rr_u32 size);

#endif

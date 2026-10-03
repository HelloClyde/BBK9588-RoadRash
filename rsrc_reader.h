#ifndef ROAD_RASH_RSRC_READER_H
#define ROAD_RASH_RSRC_READER_H

/* Disk-format adapter for the 3DO resource catalog used by the upstream game.
 * The caller supplies random-access I/O; no 3DO OS or host libc is needed. */
typedef unsigned char rr_u8;
typedef unsigned int rr_u32;

typedef int (*rr_read_at_t)(void *user, rr_u32 offset, void *dst, rr_u32 size);

typedef struct rr_rsrc_file {
    rr_read_at_t read_at;
    void *user;
    rr_u32 file_size;
    rr_u32 table_offset;
    rr_u32 resource_count;
} rr_rsrc_file_t;

typedef struct rr_rsrc_record {
    rr_u32 type;
    rr_u32 id;
    rr_u32 offset;
    rr_u32 size;
    rr_u32 flags;
} rr_rsrc_record_t;

#define RR_RSRC_TAG(a,b,c,d) \
    (((rr_u32)(a) << 24) | ((rr_u32)(b) << 16) | \
     ((rr_u32)(c) << 8) | (rr_u32)(d))

int rr_rsrc_open(rr_rsrc_file_t *file, rr_read_at_t read_at,
                 void *user, rr_u32 file_size);
int rr_rsrc_record_at(const rr_rsrc_file_t *file, rr_u32 index,
                      rr_rsrc_record_t *record);
int rr_rsrc_find(const rr_rsrc_file_t *file, rr_u32 type, rr_u32 id,
                 rr_rsrc_record_t *record);
int rr_rsrc_read(const rr_rsrc_file_t *file, const rr_rsrc_record_t *record,
                 rr_u32 relative_offset, void *dst, rr_u32 size);
rr_u32 rr_rsrc_be32(const rr_u8 *bytes);

#endif

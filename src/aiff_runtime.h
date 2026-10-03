#ifndef ROAD_RASH_AIFF_RUNTIME_H
#define ROAD_RASH_AIFF_RUNTIME_H

#include "rsrc_reader.h"

typedef struct rr_aiff_sample {
    rr_u32 source_rate;
    rr_u32 frame_count;
    rr_u32 pcm_offset;
    rr_u32 pcm_bytes;
} rr_aiff_sample_t;

/* Locate a 16-bit mono sample inside the original Rash.AIFF RSRC file. */
int rr_aiff_find_sample(const rr_rsrc_file_t *file, rr_u32 resource_id,
                        rr_aiff_sample_t *sample);

/* Read a bounded frame range into host-endian PCM. */
int rr_aiff_read_frames(const rr_rsrc_file_t *file,
                        const rr_aiff_sample_t *sample,
                        rr_u32 first_frame, short *pcm, rr_u32 frame_count);

#endif

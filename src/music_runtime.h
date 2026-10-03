#ifndef ROAD_RASH_MUSIC_RUNTIME_H
#define ROAD_RASH_MUSIC_RUNTIME_H

#include "rsrc_reader.h"

#define RR_MUSIC_READ_CACHE_BYTES 4096u

typedef struct rr_music_stream {
    rr_read_at_t read_at;
    void *user;
    rr_u32 file_size;
    rr_u32 first_data_chunk;
    rr_u32 next_chunk;
    rr_u32 data_offset;
    rr_u32 data_remaining;
    rr_u32 cache_offset;
    rr_u32 cache_bytes;
    rr_u8 cache[RR_MUSIC_READ_CACHE_BYTES];
    int predictor[2];
} rr_music_stream_t;

/* Read 3DO SNDS/SSMP stereo SDX2 streams as 22050 Hz mono PCM. */
int rr_music_open(rr_music_stream_t *stream, rr_read_at_t read_at,
                  void *user, rr_u32 file_size);
rr_u32 rr_music_read_mono(rr_music_stream_t *stream, short *pcm,
                           rr_u32 frames);
/* Choose from the original background track list without immediately
 * repeating the previous track. A previous index >= count starts a session. */
rr_u32 rr_music_choose_next(rr_u32 *random_state, rr_u32 previous,
                            rr_u32 count);

#endif

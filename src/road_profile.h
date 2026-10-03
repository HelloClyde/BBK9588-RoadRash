#ifndef ROAD_PROFILE_H
#define ROAD_PROFILE_H
#include "road_career.h"

/* Portable 9588 adapter save. The original 3DO record lives in NVRAM and
 * includes progression fields that this port does not yet simulate. */
#define ROAD_PROFILE_V1_BYTES 52u
#define ROAD_PROFILE_V2_BYTES 72u
#define ROAD_PROFILE_BYTES 76u
#define ROAD_PROFILE_COURSES 5u

typedef struct road_profile {
    unsigned int sequence;
    unsigned int course;
    unsigned int finished_mask;
    unsigned int race_count;
    unsigned int best_ms[ROAD_PROFILE_COURSES];
    road_career_t career;
    unsigned int last_rank;
    unsigned int music_enabled;
} road_profile_t;

void road_profile_init(road_profile_t *profile);
unsigned int road_profile_finish(road_profile_t *profile,
                                 unsigned int course,
                                 unsigned int elapsed_ms,
                                 unsigned int rank);
int road_profile_encode(const road_profile_t *profile,
                        unsigned char bytes[ROAD_PROFILE_BYTES]);
int road_profile_decode(road_profile_t *profile, const unsigned char *bytes,
                        unsigned int size);

#endif

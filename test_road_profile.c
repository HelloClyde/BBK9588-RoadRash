#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "road_profile.h"

int main(void)
{
    road_profile_t profile;
    road_profile_t loaded;
    unsigned char bytes[ROAD_PROFILE_BYTES];
    unsigned char corrupt[ROAD_PROFILE_BYTES];
    unsigned char legacy[ROAD_PROFILE_V1_BYTES];
    unsigned char v2[ROAD_PROFILE_V2_BYTES];
    unsigned int hash, i;
    road_profile_init(&profile);
    profile.course = 4u;
    profile.sequence = 17u;
    assert(road_profile_finish(&profile, 4u, 123456u, 0u) == 1000u);
    assert(road_profile_finish(&profile, 4u, 128000u, 3u) == 400u);
    assert(road_profile_finish(&profile, 4u, 120000u, 2u) == 500u);
    assert(profile.race_count == 3u);
    assert(profile.finished_mask == 16u);
    assert(profile.best_ms[4] == 120000u);
    assert(road_profile_encode(&profile, bytes));
    assert(road_profile_decode(&loaded, bytes, ROAD_PROFILE_BYTES));
    assert(loaded.music_enabled == 1u);
    assert(loaded.sequence == 17u && loaded.course == 4u);
    assert(loaded.career.bike == 1u && loaded.best_ms[4] == 120000u);
    assert(loaded.career.balance == 2400 &&
           loaded.career.completed_courses == 16u);
    profile.music_enabled = 0u;
    assert(road_profile_encode(&profile, bytes));
    assert(road_profile_decode(&loaded, bytes, ROAD_PROFILE_BYTES));
    assert(loaded.music_enabled == 0u);
    memcpy(corrupt, bytes, sizeof(corrupt));
    corrupt[30] ^= 0x40u;
    assert(!road_profile_decode(&loaded, corrupt, ROAD_PROFILE_BYTES));
    assert(loaded.sequence == 17u);
    corrupt[30] ^= 0x40u;
    corrupt[4] = 4u;
    assert(!road_profile_decode(&loaded, corrupt, ROAD_PROFILE_BYTES));
    memcpy(v2, bytes, 68u);
    v2[4] = 2u;
    hash = 2166136261u;
    for (i = 0u; i < 68u; ++i)
        hash = (hash ^ v2[i]) * 16777619u;
    for (i = 0u; i < 4u; ++i)
        v2[68u + i] = (unsigned char)(hash >> (i * 8u));
    assert(road_profile_decode(&loaded, v2, ROAD_PROFILE_V2_BYTES));
    assert(loaded.music_enabled == 1u);
    memcpy(legacy, bytes, 48u);
    legacy[4] = 1u;
    hash = 2166136261u;
    for (i = 0u; i < 48u; ++i)
        hash = (hash ^ legacy[i]) * 16777619u;
    for (i = 0u; i < 4u; ++i)
        legacy[48u + i] = (unsigned char)(hash >> (i * 8u));
    assert(road_profile_decode(&loaded, legacy,
                               ROAD_PROFILE_V1_BYTES));
    assert(loaded.career.balance == 500 && loaded.career.bike == 1u);
    assert(loaded.career.level == 0u && loaded.race_count == 3u);
    assert(loaded.music_enabled == 1u);
    profile.course = 5u;
    assert(!road_profile_encode(&profile, bytes));
    puts("Profile round-trip, progress, and corruption checks: PASS");
    return 0;
}

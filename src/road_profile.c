#include "road_profile.h"

static void put_u32(unsigned char *dst, unsigned int value)
{
    dst[0] = (unsigned char)value;
    dst[1] = (unsigned char)(value >> 8);
    dst[2] = (unsigned char)(value >> 16);
    dst[3] = (unsigned char)(value >> 24);
}

static unsigned int get_u32(const unsigned char *src)
{
    return (unsigned int)src[0] | ((unsigned int)src[1] << 8) |
           ((unsigned int)src[2] << 16) | ((unsigned int)src[3] << 24);
}

static unsigned int checksum(const unsigned char *bytes, unsigned int size)
{
    unsigned int hash = 2166136261u;
    unsigned int i;
    for (i = 0u; i < size - 4u; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

void road_profile_init(road_profile_t *profile)
{
    unsigned int i;
    profile->sequence = 0u;
    profile->course = 0u;
    profile->finished_mask = 0u;
    profile->race_count = 0u;
    profile->last_rank = 0u;
    profile->music_enabled = 1u;
    road_career_init(&profile->career);
    for (i = 0u; i < ROAD_PROFILE_COURSES; ++i)
        profile->best_ms[i] = 0u;
}

unsigned int road_profile_finish(road_profile_t *profile,
                                 unsigned int course,
                                 unsigned int elapsed_ms,
                                 unsigned int rank)
{
    unsigned int award;
    if (!profile || course >= ROAD_PROFILE_COURSES ||
        rank >= 15u || !elapsed_ms) return 0u;
    profile->course = course;
    profile->finished_mask |= 1u << course;
    if (profile->race_count != 0xffffffffu) ++profile->race_count;
    if (!profile->best_ms[course] || elapsed_ms < profile->best_ms[course])
        profile->best_ms[course] = elapsed_ms;
    profile->last_rank = rank;
    award = road_career_finish(&profile->career, course, rank);
    return award;
}

int road_profile_encode(const road_profile_t *profile,
                        unsigned char bytes[ROAD_PROFILE_BYTES])
{
    unsigned int i;
    if (!profile || !bytes || profile->course >= ROAD_PROFILE_COURSES ||
        profile->career.bike >= ROAD_CAREER_BIKES ||
        profile->career.level >= ROAD_CAREER_LEVELS ||
        profile->career.balance < 0 ||
        (profile->career.completed_courses & ~31u) ||
        profile->career.champion > 1u || profile->last_rank >= 15u ||
        profile->music_enabled > 1u ||
        (profile->finished_mask & ~31u)) return 0;
    bytes[0] = 'R'; bytes[1] = 'R'; bytes[2] = '5'; bytes[3] = '8';
    put_u32(bytes + 4u, 3u);
    put_u32(bytes + 8u, profile->sequence);
    put_u32(bytes + 12u, profile->course);
    put_u32(bytes + 16u, profile->career.bike);
    put_u32(bytes + 20u, profile->finished_mask);
    put_u32(bytes + 24u, profile->race_count);
    for (i = 0u; i < ROAD_PROFILE_COURSES; ++i)
        put_u32(bytes + 28u + i * 4u, profile->best_ms[i]);
    put_u32(bytes + 48u, profile->career.level);
    put_u32(bytes + 52u, (unsigned int)profile->career.balance);
    put_u32(bytes + 56u, profile->career.completed_courses);
    put_u32(bytes + 60u, profile->career.champion);
    put_u32(bytes + 64u, profile->last_rank);
    put_u32(bytes + 68u, profile->music_enabled);
    put_u32(bytes + ROAD_PROFILE_BYTES - 4u,
            checksum(bytes, ROAD_PROFILE_BYTES));
    return 1;
}

int road_profile_decode(road_profile_t *profile, const unsigned char *bytes,
                        unsigned int size)
{
    road_profile_t decoded;
    unsigned int version;
    unsigned int i;
    if (!profile || !bytes ||
        (size != ROAD_PROFILE_BYTES && size != ROAD_PROFILE_V2_BYTES &&
         size != ROAD_PROFILE_V1_BYTES) ||
        bytes[0] != 'R' || bytes[1] != 'R' ||
        bytes[2] != '5' || bytes[3] != '8')
        return 0;
    version = get_u32(bytes + 4u);
    if (!((version == 1u && size == ROAD_PROFILE_V1_BYTES) ||
          (version == 2u && size == ROAD_PROFILE_V2_BYTES) ||
          (version == 3u && size == ROAD_PROFILE_BYTES)) ||
        get_u32(bytes + size - 4u) != checksum(bytes, size)) return 0;
    road_profile_init(&decoded);
    decoded.sequence = get_u32(bytes + 8u);
    decoded.course = get_u32(bytes + 12u);
    decoded.career.bike = get_u32(bytes + 16u);
    decoded.finished_mask = get_u32(bytes + 20u);
    decoded.race_count = get_u32(bytes + 24u);
    for (i = 0u; i < ROAD_PROFILE_COURSES; ++i)
        decoded.best_ms[i] = get_u32(bytes + 28u + i * 4u);
    if (version >= 2u) {
        decoded.career.level = get_u32(bytes + 48u);
        decoded.career.balance = (int)get_u32(bytes + 52u);
        decoded.career.completed_courses = get_u32(bytes + 56u);
        decoded.career.champion = get_u32(bytes + 60u);
        decoded.last_rank = get_u32(bytes + 64u);
    }
    if (version == 3u)
        decoded.music_enabled = get_u32(bytes + 68u);
    if (decoded.course >= ROAD_PROFILE_COURSES ||
        decoded.career.bike >= ROAD_CAREER_BIKES ||
        decoded.career.level >= ROAD_CAREER_LEVELS ||
        decoded.career.balance < 0 ||
        (decoded.career.completed_courses & ~31u) ||
        decoded.career.champion > 1u || decoded.last_rank >= 15u ||
        decoded.music_enabled > 1u ||
        (decoded.finished_mask & ~31u)) return 0;
    profile->sequence = decoded.sequence;
    profile->course = decoded.course;
    profile->finished_mask = decoded.finished_mask;
    profile->race_count = decoded.race_count;
    for (i = 0u; i < ROAD_PROFILE_COURSES; ++i)
        profile->best_ms[i] = decoded.best_ms[i];
    profile->career.level = decoded.career.level;
    profile->career.bike = decoded.career.bike;
    profile->career.completed_courses = decoded.career.completed_courses;
    profile->career.balance = decoded.career.balance;
    profile->career.champion = decoded.career.champion;
    profile->last_rank = decoded.last_rank;
    profile->music_enabled = decoded.music_enabled;
    return 1;
}

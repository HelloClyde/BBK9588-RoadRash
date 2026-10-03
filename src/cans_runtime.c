#include "cans_runtime.h"

int rr_cans_parse_car_frames(const rr_u8 *bytes, rr_u32 size,
                             rr_u32 animation_frames,
                             road_car_frame_map_t *map)
{
    rr_u32 at = 16u, group, columns = 0u;
    unsigned int seen = 0u;
    if (!bytes || !map || size < 16u ||
        rr_rsrc_be32(bytes) != RR_RSRC_TAG('C','A','N','S') ||
        (rr_rsrc_be32(bytes + 8u) != 3u &&
         rr_rsrc_be32(bytes + 8u) != 4u) ||
        rr_rsrc_be32(bytes + 12u) != 3u ||
        (animation_frames != 3u && animation_frames != 12u))
        return 0;
    for (group = 0u; group < 3u; ++group) {
        rr_u32 entry, variants, row, column;
        if (at > size || size - at < 16u) return 0;
        entry = rr_rsrc_be32(bytes + at);
        variants = rr_rsrc_be32(bytes + at + 4u);
        if (entry < 1u || entry > 3u ||
            (seen & (1u << (entry - 1u))) ||
            (variants != 2u && variants != 9u && variants != 16u) ||
            (columns && columns != variants) ||
            variants > (size - at - 16u) / 28u)
            return 0;
        row = entry - 1u;
        seen |= 1u << row;
        columns = variants;
        for (column = 0u; column < variants; ++column) {
            rr_u32 offset = at + 16u + column * 28u;
            rr_u32 frame = ((rr_u32)bytes[offset + 2u] << 8) |
                           (rr_u32)bytes[offset + 3u];
            if (!frame || frame > animation_frames) return 0;
            map->frames[row][column] = (unsigned char)(frame - 1u);
        }
        at += 16u + variants * 28u;
    }
    if (seen != 7u) return 0;
    /* get_cans_animation_flag_table follows CANS -> SRCN -> ATTR.
     * The first ATTR frame has the direction mask returned by the original
     * get_car_animation_output_flags. Later ANIMs may reuse this CANS. */
    at = 0u;
    for (group = 0u; group < 2u; ++group) {
        rr_u32 jump;
        if (at > size || size - at < 12u) return 0;
        jump = rr_rsrc_be32(bytes + at + 4u);
        if (jump < 12u || jump > size - at - 12u) return 0;
        at += jump;
    }
    if (size - at < 20u ||
        rr_rsrc_be32(bytes + at) != RR_RSRC_TAG('A','T','T','R'))
        return 0;
    map->column_count = columns;
    at += 12u;
    for (group = 0u; group < 3u; ++group) {
        rr_u32 flags, scale;
        if (at > size || size - at < 4u ||
            rr_rsrc_be32(bytes + at) != columns ||
            columns > (size - at - 4u) / 4u)
            return 0;
        flags = rr_rsrc_be32(bytes + at + 4u);
        if (group == 0u)
            map->direction_flags = flags & 0x1c0000u;
        scale = 1u;
        if (flags & 0x10000u) scale <<= 1u;
        if (flags & 0x20000u) scale <<= 2u;
        map->scale_multiplier[group] = (unsigned char)scale;
        at += 4u + columns * 4u;
    }
    return 1;
}

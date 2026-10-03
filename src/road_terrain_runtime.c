#include "road_terrain_runtime.h"

static void load_hill_mip(const rr_rsrc_file_t *families,
    unsigned int family, unsigned int selector, unsigned int band,
    road_sprite_t *mip, road_pixel_t *pixels,
    unsigned int width, unsigned int height,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity)
{
    rr_cel_image_t image;
    unsigned int x, y;
    mip->width = 0u;
    mip->height = 0u;
    mip->pixels = 0;
    image.pixels = temporary;
    if (!rr_family_decode_surface_tile_size(families, family, selector,
        band, width, height, &image,
        temporary_capacity, scratch, scratch_capacity)) return;
    for (y = 0u; y < height; ++y)
        for (x = 0u; x < width; ++x)
            pixels[y * width + x] =
                temporary[(y * image.height / height) * image.width +
                          x * image.width / width];
    mip->width = width;
    mip->height = height;
    mip->pixels = pixels;
}

unsigned int road_terrain_load_hill_tiles(
    const rr_rsrc_file_t *families, road_hill_profile_t *profiles,
    unsigned int profile_count, road_sprite_t *tiles,
    road_sprite_t *mip_tiles, road_sprite_t *narrow_tiles,
    road_pixel_t *tile_pixels, road_pixel_t *mip_pixels,
    road_pixel_t *narrow_pixels,
    unsigned short *family_keys, unsigned char *selector_keys,
    unsigned char *band_keys,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity)
{
    unsigned int count = tile_count, p, side, band;
    if (!families || !profiles || !tiles || !mip_tiles ||
        !narrow_tiles || !tile_pixels || !mip_pixels ||
        !narrow_pixels || !family_keys || !selector_keys ||
        !band_keys || !temporary || !scratch ||
        tile_count > tile_capacity) return tile_count;
    if (tile_capacity > ROAD_HILL_TILE_LIMIT)
        tile_capacity = ROAD_HILL_TILE_LIMIT;
    for (p = 0u; p < profile_count; ++p) {
        for (side = 0u; side < 2u; ++side) {
            unsigned short family = profiles[p].family_id[side];
            unsigned char selector = profiles[p].selector[side] & 63u;
            for (band = 0u; band < 3u; ++band) {
                unsigned int i, x, y;
                rr_cel_image_t image;
                road_pixel_t *destination;
                profiles[p].tile_index[side][band] = 255u;
                if (!family) continue;
                for (i = 0u; i < count; ++i)
                    if (family_keys[i] == family &&
                        selector_keys[i] == selector &&
                        band_keys[i] == band) break;
                if (i < count) {
                    profiles[p].tile_index[side][band] =
                        (unsigned char)i;
                    continue;
                }
                if (count >= tile_capacity) continue;
                image.pixels = temporary;
                if (!rr_family_decode_surface_tile_size(families, family,
                    selector, band, ROAD_HILL_TILE_WIDTH,
                    ROAD_HILL_TILE_HEIGHT, &image, temporary_capacity,
                    scratch, scratch_capacity)) continue;
                destination = tile_pixels + count * ROAD_HILL_TILE_PIXELS;
                for (y = 0u; y < ROAD_HILL_TILE_HEIGHT; ++y)
                    for (x = 0u; x < ROAD_HILL_TILE_WIDTH; ++x)
                        destination[y * ROAD_HILL_TILE_WIDTH + x] =
                            temporary[(y * image.height /
                                ROAD_HILL_TILE_HEIGHT) * image.width +
                                x * image.width / ROAD_HILL_TILE_WIDTH];
                family_keys[count] = family;
                selector_keys[count] = selector;
                band_keys[count] = (unsigned char)band;
                tiles[count].width = ROAD_HILL_TILE_WIDTH;
                tiles[count].height = ROAD_HILL_TILE_HEIGHT;
                tiles[count].pixels = destination;
                {
                    unsigned int level, offset = 0u, narrow_offset = 0u;
                    unsigned int height = 8u;
                    for (level = 0u; level < 4u; ++level) {
                        unsigned int width_level, width = 16u;
                        load_hill_mip(families, family, selector, band,
                            &mip_tiles[count * 4u + level],
                            mip_pixels + count * ROAD_HILL_MIP_PIXELS +
                                offset, ROAD_HILL_TILE_WIDTH, height,
                            temporary,
                            temporary_capacity, scratch, scratch_capacity);
                        offset += ROAD_HILL_TILE_WIDTH * height;
                        for (width_level = 0u; width_level < 3u;
                             ++width_level) {
                            load_hill_mip(families, family, selector,
                                band, &narrow_tiles[count * 12u +
                                    level * 3u + width_level],
                                narrow_pixels +
                                    count * ROAD_HILL_NARROW_MIP_PIXELS +
                                    narrow_offset, width, height,
                                temporary, temporary_capacity,
                                scratch, scratch_capacity);
                            narrow_offset += width * height;
                            width <<= 1u;
                        }
                        height >>= 1u;
                    }
                }
                profiles[p].tile_index[side][band] =
                    (unsigned char)count;
                ++count;
            }
        }
    }
    return count;
}

unsigned int road_terrain_load_roadside_tiles(
    const rr_rsrc_file_t *families, road_surface_sample_t *samples,
    const unsigned char *terrain_mode, unsigned int sample_count,
    road_sprite_t *tiles,
    unsigned short *tile_heights, unsigned char *repeat_indices,
    road_sprite_t *repeat_mips,
    road_pixel_t *tile_pixels,
    unsigned short *family_keys,
    unsigned char *selector_keys,
    unsigned char *child_keys,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity)
{
    unsigned int count = tile_count, sample_index, side;
    if (!families || !samples || !terrain_mode || !tiles || !tile_heights ||
        !repeat_indices || !repeat_mips || !tile_pixels ||
        !family_keys || !selector_keys || !child_keys ||
        !temporary || !scratch || tile_count > tile_capacity)
        return tile_count;
    if (tile_capacity > ROAD_ROADSIDE_TILE_LIMIT)
        tile_capacity = ROAD_ROADSIDE_TILE_LIMIT;
    for (sample_index = 0u; sample_index < sample_count; ++sample_index)
        for (side = 0u; side < 2u; ++side) {
            road_surface_sample_t *sample = &samples[sample_index];
            unsigned short family = sample->family_id[side];
            unsigned char selector = sample->selector[side] & 63u;
            unsigned char child = terrain_mode[sample_index] == 1u ?
                0u : 255u;
            unsigned int i, x, y;
            rr_cel_image_t image;
            road_pixel_t *destination;
            sample->tile_index[side] = 255u;
            if (!family || sample->selector[side] == 255u ||
                (terrain_mode[sample_index] != 0u &&
                 terrain_mode[sample_index] != 1u)) continue;
            for (i = 0u; i < count; ++i)
                if (family_keys[i] == family &&
                    selector_keys[i] == selector &&
                    child_keys[i] == child) break;
            if (i < count) {
                sample->tile_index[side] = (unsigned char)i;
                continue;
            }
            if (count >= tile_capacity) continue;
            image.pixels = temporary;
            if (child == 255u) {
                if (!rr_family_decode_surface_tile(families, family,
                    selector, 0xffffffffu, &image,
                    temporary_capacity, scratch, scratch_capacity))
                    continue;
            } else if (!rr_family_decode_surface_tile_size(families,
                family, selector, 0u, ROAD_ROADSIDE_TILE_WIDTH,
                ROAD_ROADSIDE_TILE_HEIGHT, &image, temporary_capacity,
                scratch, scratch_capacity)) continue;
            destination = tile_pixels +
                count * ROAD_ROADSIDE_TILE_PIXELS;
            for (y = 0u; y < ROAD_ROADSIDE_TILE_HEIGHT; ++y)
                for (x = 0u; x < ROAD_ROADSIDE_TILE_WIDTH; ++x)
                    destination[y * ROAD_ROADSIDE_TILE_WIDTH + x] =
                        temporary[(y * image.height /
                            ROAD_ROADSIDE_TILE_HEIGHT) * image.width +
                            x * image.width / ROAD_ROADSIDE_TILE_WIDTH];
            family_keys[count] = family;
            selector_keys[count] = selector;
            child_keys[count] = child;
            tiles[count].width = ROAD_ROADSIDE_TILE_WIDTH;
            tiles[count].height = ROAD_ROADSIDE_TILE_HEIGHT;
            tiles[count].pixels = destination;
            tile_heights[count] = (unsigned short)image.surface_height;
            sample->tile_index[side] = (unsigned char)count;
            repeat_indices[count] = (unsigned char)count;
            for (i = 0u; i < ROAD_ROADSIDE_REPEAT_LEVELS; ++i)
                repeat_mips[count * ROAD_ROADSIDE_REPEAT_LEVELS + i] =
                    tiles[count];
            ++count;
            /* The source renderer selects child 1 for repeated raised
             * outer bands. Its threshold CLGP has four exact heights:
             * 128x1/2/4/8. Pack them into two existing 1024-pixel slots. */
            if (child == 0u && count + 1u < tile_capacity) {
                unsigned int base = count - 1u;
                unsigned int level, valid = 1u;
                unsigned int small_offset = 0u;
                for (level = 0u; level <
                    ROAD_ROADSIDE_REPEAT_LEVELS; ++level) {
                    unsigned int pixel, target_height = 1u << level;
                    unsigned int slot = level == 3u ? count : count + 1u;
                    road_pixel_t *pixels = tile_pixels +
                        slot * ROAD_ROADSIDE_TILE_PIXELS +
                        (level == 3u ? 0u : small_offset);
                    if (!rr_family_decode_surface_tile_size(families,
                        family, selector, 1u, 128u, target_height,
                        &image, temporary_capacity, scratch,
                        scratch_capacity) || image.width != 128u ||
                        image.height != target_height) {
                        valid = 0u;
                        break;
                    }
                    for (pixel = 0u; pixel < image.width * image.height;
                         ++pixel) pixels[pixel] = temporary[pixel];
                    repeat_mips[base * ROAD_ROADSIDE_REPEAT_LEVELS +
                        level].width = image.width;
                    repeat_mips[base * ROAD_ROADSIDE_REPEAT_LEVELS +
                        level].height = image.height;
                    repeat_mips[base * ROAD_ROADSIDE_REPEAT_LEVELS +
                        level].pixels = pixels;
                    if (level != 3u)
                        small_offset += image.width * image.height;
                }
                if (valid) {
                    family_keys[count] = family;
                    selector_keys[count] = selector;
                    child_keys[count] = 1u;
                    tiles[count] = repeat_mips[base *
                        ROAD_ROADSIDE_REPEAT_LEVELS + 3u];
                    tile_heights[count] = 0u;
                    repeat_indices[count] = (unsigned char)count;
                    family_keys[count + 1u] = family;
                    selector_keys[count + 1u] = selector;
                    child_keys[count + 1u] = 2u;
                    tiles[count + 1u].width = 0u;
                    tiles[count + 1u].height = 0u;
                    tiles[count + 1u].pixels = 0;
                    tile_heights[count + 1u] = 0u;
                    repeat_indices[count + 1u] =
                        (unsigned char)(count + 1u);
                    repeat_indices[base] = (unsigned char)count;
                    count += 2u;
                } else for (level = 0u; level <
                    ROAD_ROADSIDE_REPEAT_LEVELS; ++level)
                    repeat_mips[base * ROAD_ROADSIDE_REPEAT_LEVELS +
                        level] = tiles[base];
            }
        }
    return count;
}

unsigned int road_terrain_load_margin_tiles(
    const rr_rsrc_file_t *families,
    const rr_u8 *left_selector, const rr_u8 *right_selector,
    const unsigned short *left_family,
    const unsigned short *right_family,
    unsigned int sample_count,
    road_margin_tile_t *tiles, road_pixel_t *pixel_pool,
    unsigned int *pixel_used, unsigned int pixel_capacity,
    unsigned int tile_count, unsigned int tile_capacity,
    road_pixel_t *temporary, unsigned int temporary_capacity,
    rr_u8 *scratch, unsigned int scratch_capacity)
{
    unsigned int sample, side;
    if (!families || !left_selector || !right_selector ||
        !left_family || !right_family || !tiles || !pixel_pool ||
        !pixel_used || !temporary || !scratch ||
        tile_count > tile_capacity || *pixel_used > pixel_capacity)
        return tile_count;
    for (sample = 0u; sample < sample_count; ++sample)
        for (side = 0u; side < 2u; ++side) {
            unsigned short family = side ?
                right_family[sample] : left_family[sample];
            rr_u8 selector = side ?
                right_selector[sample] : left_selector[sample];
            unsigned int key, child;
            if (!family || selector == 255u) continue;
            for (key = 0u; key < tile_count; ++key)
                if (tiles[key].family_id == family &&
                    tiles[key].selector == selector) break;
            if (key < tile_count) continue;
            if (tile_count >= tile_capacity) {
                *pixel_used = pixel_capacity + 1u;
                return tile_count;
            }
            tiles[key].family_id = family;
            tiles[key].selector = selector;
            for (child = 0u; child < 2u; ++child) {
                road_sprite_t *variants = child ?
                    tiles[key].fill_mips : tiles[key].primary_mips;
                unsigned int level;
                for (level = 0u; level < 8u; ++level) {
                    rr_cel_image_t image;
                    unsigned int prior, pixels, pixel;
                    variants[level].width = variants[level].height = 0u;
                    variants[level].pixels = 0;
                    image.pixels = temporary;
                    if (!rr_family_decode_surface_tile_group_size(families,
                        family, 5u, selector & 63u, child + 2u,
                        32u, 1u << level, &image, temporary_capacity,
                        scratch, scratch_capacity)) continue;
                    pixels = image.width * image.height;
                    for (prior = 0u; prior < level; ++prior) {
                        int same = 1;
                        if (!variants[prior].pixels ||
                            variants[prior].width != image.width ||
                            variants[prior].height != image.height)
                            continue;
                        for (pixel = 0u; pixel < pixels; ++pixel)
                            if (variants[prior].pixels[pixel] !=
                                temporary[pixel]) {
                                same = 0;
                                break;
                            }
                        if (same) break;
                    }
                    if (prior < level) {
                        variants[level] = variants[prior];
                        continue;
                    }
                    if (pixels > pixel_capacity - *pixel_used) {
                        *pixel_used = pixel_capacity + 1u;
                        return tile_count;
                    }
                    variants[level].width = image.width;
                    variants[level].height = image.height;
                    variants[level].pixels = pixel_pool + *pixel_used;
                    for (pixel = 0u; pixel < pixels; ++pixel)
                        pixel_pool[*pixel_used + pixel] = temporary[pixel];
                    *pixel_used += pixels;
                }
            }
            ++tile_count;
        }
    return tile_count;
}

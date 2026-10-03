#include "cel_runtime.h"

typedef struct rr_bit_cursor {
    const rr_u8 *data;
    rr_u32 bit;
    rr_u32 end_bit;
} rr_bit_cursor_t;

static int read_bits(rr_bit_cursor_t *cursor, rr_u32 count, rr_u32 *value)
{
    rr_u32 out = 0u;
    rr_u32 i;
    if (cursor->bit > cursor->end_bit ||
        count > cursor->end_bit - cursor->bit) return 0;
    for (i = 0u; i < count; ++i) {
        out = (out << 1) |
            ((cursor->data[cursor->bit >> 3] >>
              (7u - (cursor->bit & 7u))) & 1u);
        cursor->bit++;
    }
    *value = out;
    return 1;
}

static unsigned short convert_color(rr_u32 value,
                                    const unsigned short *palette,
                                    rr_u32 palette_count, rr_u32 bpp,
                                    rr_u32 divisor)
{
    rr_u32 color = palette[value & (palette_count - 1u)];
    rr_u32 red = (color >> 10) & 31u;
    rr_u32 green = (color >> 5) & 31u;
    rr_u32 blue = color & 31u;
    if (bpp == 8u) {
        rr_u32 intensity = (value >> 5) + 1u;
        red = red * intensity / divisor;
        green = green * intensity / divisor;
        blue = blue * intensity / divisor;
        if (red > 31u) red = 31u;
        if (green > 31u) green = 31u;
        if (blue > 31u) blue = 31u;
    }
    return (unsigned short)((red << 11) |
                            (((green << 1) | (green >> 4)) << 5) | blue);
}

int rr_cel_decode_packed(const rr_u8 *packed, rr_u32 packed_bytes,
                         rr_u32 width, rr_u32 height, rr_u32 bits_per_pixel,
                         const unsigned short *palette, rr_u32 palette_count,
                         rr_u32 divisor, unsigned short *pixels,
                         rr_u32 pixel_capacity)
{
    rr_u32 y, offset = 0u;
    if (!packed || !palette || !pixels || !width || width > 512u ||
        !height || height > 256u || width * height > pixel_capacity ||
        (bits_per_pixel != 4u && bits_per_pixel != 6u &&
         bits_per_pixel != 8u) ||
        (palette_count != 16u && palette_count != 32u) || !divisor)
        return 0;
    for (y = 0u; y < height; ++y) {
        rr_bit_cursor_t cursor;
        rr_u32 row_words, row_bytes, x = 0u;
        if (offset >= packed_bytes) return 0;
        cursor.data = packed;
        cursor.bit = offset * 8u;
        cursor.end_bit = packed_bytes * 8u;
        if (!read_bits(&cursor, bits_per_pixel == 8u ? 16u : 8u,
                       &row_words))
            return 0;
        row_bytes = (row_words + 2u) * 4u;
        if (row_bytes > packed_bytes - offset) return 0;
        cursor.end_bit = (offset + row_bytes) * 8u;
        while (x < width) {
            rr_u32 kind, run, value, i;
            if (!read_bits(&cursor, 2u, &kind)) return 0;
            if (kind == 0u) break;
            if (!read_bits(&cursor, 6u, &run)) return 0;
            run++;
            if (kind == 3u &&
                !read_bits(&cursor, bits_per_pixel, &value)) return 0;
            for (i = 0u; i < run; ++i) {
                unsigned short color = RR_CEL_TRANSPARENT;
                if (kind == 1u &&
                    !read_bits(&cursor, bits_per_pixel, &value)) return 0;
                if (kind != 2u)
                    color = convert_color(value, palette, palette_count,
                                          bits_per_pixel, divisor);
                if (x < width) pixels[y * width + x] = color;
                x++;
            }
        }
        while (x < width) pixels[y * width + x++] = RR_CEL_TRANSPARENT;
        offset += row_bytes;
    }
    return offset == packed_bytes;
}

int rr_cel_decode_packed_rgb16(const rr_u8 *packed, rr_u32 packed_bytes,
                               rr_u32 width, rr_u32 height,
                               unsigned short *pixels,
                               rr_u32 pixel_capacity)
{
    rr_u32 y, offset = 0u;
    if (!packed || !pixels || !width || width > 512u ||
        !height || height > 256u || width * height > pixel_capacity)
        return 0;
    for (y = 0u; y < height; ++y) {
        rr_bit_cursor_t cursor;
        rr_u32 row_words, row_bytes, x = 0u;
        if (offset >= packed_bytes) return 0;
        cursor.data = packed;
        cursor.bit = offset * 8u;
        cursor.end_bit = packed_bytes * 8u;
        if (!read_bits(&cursor, 16u, &row_words)) return 0;
        row_bytes = (row_words + 2u) * 4u;
        if (row_bytes > packed_bytes - offset) return 0;
        cursor.end_bit = (offset + row_bytes) * 8u;
        while (x < width) {
            rr_u32 kind, run, value = 0u, i;
            if (!read_bits(&cursor, 2u, &kind)) return 0;
            if (kind == 0u) break;
            if (!read_bits(&cursor, 6u, &run)) return 0;
            run++;
            if (kind == 3u && !read_bits(&cursor, 16u, &value))
                return 0;
            for (i = 0u; i < run; ++i) {
                unsigned short color = RR_CEL_TRANSPARENT;
                if (kind == 1u && !read_bits(&cursor, 16u, &value))
                    return 0;
                if (kind != 2u) {
                    rr_u32 rgb555 = value & 0x7fffu;
                    rr_u32 red = (rgb555 >> 10u) & 31u;
                    rr_u32 green = (rgb555 >> 5u) & 31u;
                    rr_u32 blue = rgb555 & 31u;
                    color = (unsigned short)((red << 11u) |
                        (((green << 1u) | (green >> 4u)) << 5u) | blue);
                }
                if (x < width) pixels[y * width + x] = color;
                ++x;
            }
        }
        while (x < width) pixels[y * width + x++] = RR_CEL_TRANSPARENT;
        offset += row_bytes;
    }
    return offset == packed_bytes;
}

int rr_cel_load_rgb16(const rr_rsrc_file_t *file, rr_u32 cel_id,
                       rr_cel_image_t *image, rr_u32 pixel_capacity,
                       rr_u8 *scratch, rr_u32 scratch_capacity)
{
    rr_rsrc_record_t cel;
    rr_u8 chunk[80];
    rr_u32 at = 0u, width = 0u, height = 0u;
    int have_ccb = 0, have_pdat = 0;
    if (!file || !image || !image->pixels || !scratch ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel))
        return 0;
    while (at < cel.size) {
        rr_u32 tag, bytes;
        if (!rr_rsrc_read(file, &cel, at, chunk, 8u)) return 0;
        tag = rr_rsrc_be32(chunk);
        bytes = rr_rsrc_be32(chunk + 4u);
        if (bytes < 8u || bytes > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('C','C','B',' ') && bytes >= 80u) {
            if (!rr_rsrc_read(file, &cel, at, chunk, 80u)) return 0;
            width = rr_rsrc_be32(chunk + 72u);
            height = rr_rsrc_be32(chunk + 76u);
            if ((rr_rsrc_be32(chunk + 12u) & 0x200u) == 0u ||
                (rr_rsrc_be32(chunk + 64u) & 7u) != 6u)
                return 0;
            have_ccb = 1;
        } else if (tag == RR_RSRC_TAG('P','D','A','T')) {
            rr_u32 data_bytes = bytes - 8u;
            if (!data_bytes || data_bytes > scratch_capacity ||
                !rr_rsrc_read(file, &cel, at + 8u,
                              scratch, data_bytes))
                return 0;
            if (!have_ccb || !rr_cel_decode_packed_rgb16(
                    scratch, data_bytes, width, height,
                    image->pixels, pixel_capacity))
                return 0;
            have_pdat = 1;
        }
        at += bytes;
    }
    if (!have_ccb || !have_pdat) return 0;
    image->width = width;
    image->height = height;
    return 1;
}

static rr_u32 be16(const rr_u8 *bytes)
{
    return (rr_u32)bytes[0] << 8 | bytes[1];
}

int rr_cel_load_road_surface(const rr_rsrc_file_t *file, rr_u32 cel_id,
                             rr_cel_image_t *image, rr_u32 pixel_capacity)
{
    rr_rsrc_record_t cel;
    rr_u8 header[80], palette_chunk[44], row[128];
    rr_u32 ccb_at, at, width, height, pdat_at = 0u, pdat_bytes = 0u;
    rr_u32 stride, y, x;
    unsigned short palette[16];
    int have_palette = 0;
    if (!file || !image || !image->pixels ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel) ||
        !rr_rsrc_read(file, &cel, 0u, header, 8u)) return 0;
    ccb_at = rr_rsrc_be32(header) == RR_RSRC_TAG('O','F','S','T') ?
        rr_rsrc_be32(header + 4u) : 0u;
    if (ccb_at > cel.size || cel.size - ccb_at < 80u ||
        !rr_rsrc_read(file, &cel, ccb_at, header, 80u) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('C','C','B',' ') ||
        rr_rsrc_be32(header + 4u) != 80u ||
        (rr_rsrc_be32(header + 12u) & 0x200u) != 0u ||
        (rr_rsrc_be32(header + 64u) & 7u) != 3u)
        return 0;
    width = rr_rsrc_be32(header + 72u);
    height = rr_rsrc_be32(header + 76u);
    if (!width || width > 256u || !height || height > 145u ||
        width * height > pixel_capacity) return 0;
    at = ccb_at + 80u;
    while (at < cel.size) {
        rr_u32 tag, bytes;
        if (!rr_rsrc_read(file, &cel, at, header, 8u)) return 0;
        tag = rr_rsrc_be32(header);
        bytes = rr_rsrc_be32(header + 4u);
        if (bytes < 8u || bytes > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('P','L','U','T')) {
            if (bytes != sizeof(palette_chunk) ||
                !rr_rsrc_read(file, &cel, at, palette_chunk,
                              sizeof(palette_chunk)) ||
                rr_rsrc_be32(palette_chunk + 8u) != 16u)
                return 0;
            have_palette = 1;
        } else if (tag == RR_RSRC_TAG('P','D','A','T')) {
            pdat_at = at + 8u;
            pdat_bytes = bytes - 8u;
        }
        at += bytes;
    }
    if (!pdat_at || !pdat_bytes || pdat_bytes % height != 0u ||
        !have_palette)
        return 0;
    stride = pdat_bytes / height;
    if (stride < (width + 1u) / 2u || stride > sizeof(row)) return 0;
    for (x = 0u; x < 16u; ++x) {
        rr_u32 color = be16(palette_chunk + 12u + x * 2u);
        rr_u32 red = (color >> 10u) & 31u;
        rr_u32 green = (color >> 5u) & 31u;
        rr_u32 blue = color & 31u;
        palette[x] = (unsigned short)((red << 11u) |
            (((green << 1u) | (green >> 4u)) << 5u) | blue);
    }
    for (y = 0u; y < height; ++y) {
        if (!rr_rsrc_read(file, &cel, pdat_at + y * stride,
                          row, stride)) return 0;
        for (x = 0u; x < width; ++x) {
            rr_u8 packed = row[x / 2u];
            rr_u8 index = (rr_u8)((x & 1u) ? packed & 15u : packed >> 4u);
            image->pixels[y * width + x] = palette[index];
        }
    }
    image->width = width;
    image->height = height;
    return 1;
}

int rr_cel_load_uncoded6(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_cel_image_t *image, rr_u32 pixel_capacity)
{
    rr_rsrc_record_t cel;
    rr_u8 chunk[80], row[256];
    unsigned short palette[32];
    rr_u32 at, ccb_at, width, height, pdat_at = 0u, pdat_bytes = 0u;
    rr_u32 stride = 0u, y, x;
    int have_palette = 0;
    if (!file || !image || !image->pixels ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel) ||
        !rr_rsrc_read(file, &cel, 0u, chunk, 8u)) return 0;
    ccb_at = rr_rsrc_be32(chunk) == RR_RSRC_TAG('O','F','S','T') ?
        rr_rsrc_be32(chunk + 4u) : 0u;
    if (ccb_at > cel.size || cel.size - ccb_at < 80u ||
        !rr_rsrc_read(file, &cel, ccb_at, chunk, 80u) ||
        rr_rsrc_be32(chunk) != RR_RSRC_TAG('C','C','B',' ') ||
        rr_rsrc_be32(chunk + 4u) != 80u ||
        (rr_rsrc_be32(chunk + 12u) & 0x200u) != 0u ||
        (rr_rsrc_be32(chunk + 64u) & 7u) != 4u) return 0;
    width = rr_rsrc_be32(chunk + 72u);
    height = rr_rsrc_be32(chunk + 76u);
    if (!width || width > 256u || !height || height > 64u ||
        width * height > pixel_capacity) return 0;
    at = ccb_at + 80u;
    while (at < cel.size) {
        rr_u32 tag, bytes;
        if (!rr_rsrc_read(file, &cel, at, chunk, 8u)) return 0;
        tag = rr_rsrc_be32(chunk);
        bytes = rr_rsrc_be32(chunk + 4u);
        if (bytes < 8u || bytes > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('P','L','U','T')) {
            if (bytes != 76u ||
                !rr_rsrc_read(file, &cel, at, chunk, 76u) ||
                rr_rsrc_be32(chunk + 8u) != 32u) return 0;
            for (x = 0u; x < 32u; ++x)
                palette[x] = (unsigned short)be16(chunk + 12u + x * 2u);
            have_palette = 1;
        } else if (tag == RR_RSRC_TAG('P','D','A','T')) {
            rr_u32 pre1;
            if (bytes < 16u ||
                !rr_rsrc_read(file, &cel, at + 8u, chunk, 8u) ||
                (rr_rsrc_be32(chunk) & 7u) != 4u) return 0;
            pre1 = rr_rsrc_be32(chunk + 4u);
            stride = (((pre1 >> 24u) & 255u) + 2u) * 4u;
            pdat_at = at + 16u;
            pdat_bytes = bytes - 16u;
        }
        at += bytes;
    }
    if (!have_palette || !pdat_at || !stride ||
        stride > sizeof(row) || stride < (width * 6u + 7u) / 8u ||
        pdat_bytes != stride * height) return 0;
    for (y = 0u; y < height; ++y) {
        rr_bit_cursor_t cursor;
        if (!rr_rsrc_read(file, &cel, pdat_at + y * stride,
                          row, stride)) return 0;
        cursor.data = row;
        cursor.bit = 0u;
        cursor.end_bit = stride * 8u;
        for (x = 0u; x < width; ++x) {
            rr_u32 value;
            if (!read_bits(&cursor, 6u, &value)) return 0;
            image->pixels[y * width + x] = value == 0u ?
                RR_CEL_TRANSPARENT : convert_color(value, palette, 32u,
                                                    6u, 16u);
        }
    }
    image->width = width;
    image->height = height;
    return 1;
}

int rr_cel_load_uncoded1(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_cel_image_t *image, rr_u32 pixel_capacity)
{
    rr_rsrc_record_t cel;
    rr_u8 chunk[80], row[128];
    rr_u32 ccb_at, at, width, height, color = 0u;
    rr_u32 pdat_at = 0u, pdat_bytes = 0u, stride = 0u, y, x;
    int have_palette = 0;
    if (!file || !image || !image->pixels ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel) ||
        !rr_rsrc_read(file, &cel, 0u, chunk, 8u)) return 0;
    ccb_at = rr_rsrc_be32(chunk) == RR_RSRC_TAG('O','F','S','T') ?
        rr_rsrc_be32(chunk + 4u) : 0u;
    if (ccb_at > cel.size || cel.size - ccb_at < 80u ||
        !rr_rsrc_read(file, &cel, ccb_at, chunk, 80u) ||
        rr_rsrc_be32(chunk) != RR_RSRC_TAG('C','C','B',' ') ||
        rr_rsrc_be32(chunk + 4u) != 80u ||
        (rr_rsrc_be32(chunk + 12u) & 0x200u) != 0u ||
        (rr_rsrc_be32(chunk + 64u) & 7u) != 1u) return 0;
    width = rr_rsrc_be32(chunk + 72u);
    height = rr_rsrc_be32(chunk + 76u);
    if (!width || width > 128u || !height || height > 64u ||
        width * height > pixel_capacity) return 0;
    at = ccb_at + 80u;
    while (at < cel.size) {
        rr_u32 tag, bytes;
        if (!rr_rsrc_read(file, &cel, at, chunk, 8u)) return 0;
        tag = rr_rsrc_be32(chunk);
        bytes = rr_rsrc_be32(chunk + 4u);
        if (bytes < 8u || bytes > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('P','L','U','T')) {
            unsigned short palette[2];
            if (bytes != 16u ||
                !rr_rsrc_read(file, &cel, at, chunk, 16u) ||
                rr_rsrc_be32(chunk + 8u) != 2u) return 0;
            palette[0] = (unsigned short)be16(chunk + 12u);
            palette[1] = (unsigned short)be16(chunk + 14u);
            color = convert_color(1u, palette, 2u, 1u, 16u);
            have_palette = 1;
        } else if (tag == RR_RSRC_TAG('R','P','D','T')) {
            if (bytes < 24u ||
                !rr_rsrc_read(file, &cel, at + 16u, chunk, 8u) ||
                (rr_rsrc_be32(chunk) & 7u) != 1u) return 0;
            stride = ((rr_rsrc_be32(chunk + 4u) & 0x3ffu) + 1u) / 2u;
            pdat_at = at + 24u;
            pdat_bytes = bytes - 24u;
        }
        at += bytes;
    }
    if (!have_palette || !pdat_at || !stride ||
        stride > sizeof(row) || stride < (width + 7u) / 8u ||
        pdat_bytes != stride * height) return 0;
    for (y = 0u; y < height; ++y) {
        if (!rr_rsrc_read(file, &cel, pdat_at + y * stride,
                          row, stride)) return 0;
        for (x = 0u; x < width; ++x)
            image->pixels[y * width + x] =
                row[x >> 3u] & (0x80u >> (x & 7u)) ?
                (unsigned short)color : RR_CEL_TRANSPARENT;
    }
    image->width = width;
    image->height = height;
    return 1;
}

int rr_cel_load_packed_image(const rr_rsrc_file_t *file, rr_u32 cel_id,
                             rr_cel_image_t *image, rr_u32 pixel_capacity,
                             rr_u8 *scratch, rr_u32 scratch_capacity)
{
    rr_rsrc_record_t cel;
    rr_u8 header[80], palette_chunk[76];
    unsigned short palette[32];
    rr_u32 at, width, height, bpp, divisor, color_count = 0u;
    rr_u32 pdat_at = 0u, pdat_bytes = 0u, i;
    if (!file || !image || !image->pixels || !scratch ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel) ||
        !rr_rsrc_read(file, &cel, 0u, header, 8u)) return 0;
    at = rr_rsrc_be32(header) == RR_RSRC_TAG('O','F','S','T') ?
        rr_rsrc_be32(header + 4u) : 0u;
    if (at > cel.size || cel.size - at < 80u ||
        !rr_rsrc_read(file, &cel, at, header, 80u) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('C','C','B',' ') ||
        rr_rsrc_be32(header + 4u) != 80u ||
        (rr_rsrc_be32(header + 12u) & 0x200u) == 0u) return 0;
    width = rr_rsrc_be32(header + 72u);
    height = rr_rsrc_be32(header + 76u);
    bpp = rr_rsrc_be32(header + 64u) & 7u;
    bpp = bpp == 3u ? 4u : bpp == 4u ? 6u : bpp == 5u ? 8u : 0u;
    divisor = rr_rsrc_be32(header + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    if (!bpp || !width || width > 512u || !height || height > 256u ||
        width * height > pixel_capacity) return 0;
    at += 80u;
    while (at < cel.size) {
        rr_u32 tag, bytes;
        if (!rr_rsrc_read(file, &cel, at, header, 8u)) return 0;
        tag = rr_rsrc_be32(header);
        bytes = rr_rsrc_be32(header + 4u);
        if (bytes < 8u || bytes > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('P','L','U','T')) {
            if (bytes != 44u && bytes != 76u) return 0;
            if (!rr_rsrc_read(file, &cel, at, palette_chunk, bytes))
                return 0;
            color_count = rr_rsrc_be32(palette_chunk + 8u);
            if ((color_count != 16u && color_count != 32u) ||
                bytes != 12u + color_count * 2u) return 0;
            for (i = 0u; i < color_count; ++i)
                palette[i] = (unsigned short)be16(
                    palette_chunk + 12u + i * 2u);
        } else if (tag == RR_RSRC_TAG('P','D','A','T') ||
                   tag == RR_RSRC_TAG('R','P','D','T')) {
            /* PDAT starts with PRE0; RPDT has a 12-byte rectangle/PRE0
             * header after its 8-byte chunk header. */
            rr_u32 skip = tag == RR_RSRC_TAG('P','D','A','T') ? 12u : 20u;
            if (bytes < skip) return 0;
            pdat_at = at + skip;
            pdat_bytes = bytes - skip;
        }
        at += bytes;
    }
    if (!color_count || !pdat_at || !pdat_bytes ||
        pdat_bytes > scratch_capacity ||
        !rr_rsrc_read(file, &cel, pdat_at, scratch, pdat_bytes) ||
        !rr_cel_decode_packed(scratch, pdat_bytes, width, height,
                              bpp, palette, color_count, divisor,
                              image->pixels, pixel_capacity)) return 0;
    image->width = width;
    image->height = height;
    return 1;
}

int rr_family_load_frame(const rr_rsrc_file_t *file, rr_u32 family_id,
                         int use_last_pdat, rr_cel_image_t *image,
                         rr_u32 pixel_capacity, rr_u8 *scratch,
                         rr_u32 scratch_capacity)
{
    rr_rsrc_record_t family;
    rr_u8 start[512], ccb[80], plut[76], frame[20];
    unsigned short palette[32];
    rr_u32 scan_bytes, ofst = 0u, table_bytes, ccb_at = 0u;
    rr_u32 plut_at = 0u, frame_at = 0u, i, bpp, divisor;
    rr_u32 width, height, chunk_bytes, header_bytes, color_count;
    if (!file || !image || !image->pixels || !scratch ||
        !rr_rsrc_find(file, RR_RSRC_TAG('F','A','M',' '), family_id,
                      &family)) return 0;
    scan_bytes = family.size < sizeof(start) ? family.size : sizeof(start);
    if (scan_bytes < 32u ||
        !rr_rsrc_read(file, &family, 0u, start, scan_bytes)) return 0;
    for (i = 0u; i + 12u <= scan_bytes; i += 4u) {
        if (rr_rsrc_be32(start + i) == RR_RSRC_TAG('O','F','S','T')) {
            ofst = i;
            break;
        }
    }
    if (!ofst) return 0;
    table_bytes = rr_rsrc_be32(start + ofst + 4u);
    if (table_bytes < 8u || table_bytes > 512u - ofst ||
        (table_bytes & 3u)) return 0;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 chunk_at = ofst + rr_rsrc_be32(start + ofst + i);
        rr_u8 tag[4];
        rr_u32 type;
        if (!rr_rsrc_read(file, &family, chunk_at, tag, sizeof(tag)))
            return 0;
        type = rr_rsrc_be32(tag);
        if (type == RR_RSRC_TAG('C','C','B',' ') && !ccb_at)
            ccb_at = chunk_at;
        else if (type == RR_RSRC_TAG('P','L','U','T') && !plut_at)
            plut_at = chunk_at;
        else if (!use_last_pdat && type == RR_RSRC_TAG('R','P','D','T') &&
                 !frame_at)
            frame_at = chunk_at;
        else if (use_last_pdat && type == RR_RSRC_TAG('P','D','A','T'))
            frame_at = chunk_at;
    }
    if (!ccb_at || !plut_at || !frame_at ||
        !rr_rsrc_read(file, &family, ccb_at, ccb, sizeof(ccb)) ||
        !rr_rsrc_read(file, &family, plut_at, plut, sizeof(plut)) ||
        !rr_rsrc_read(file, &family, frame_at, frame, sizeof(frame)) ||
        (rr_rsrc_be32(ccb + 12u) & 0x200u) == 0u)
        return 0;
    bpp = rr_rsrc_be32(ccb + 64u) & 7u;
    bpp = bpp == 3u ? 4u : bpp == 4u ? 6u : bpp == 5u ? 8u : 0u;
    color_count = rr_rsrc_be32(plut + 8u);
    if (!bpp || (color_count != 16u && color_count != 32u)) return 0;
    for (i = 0u; i < color_count; ++i)
        palette[i] = (unsigned short)be16(plut + 12u + i * 2u);
    divisor = rr_rsrc_be32(ccb + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    chunk_bytes = rr_rsrc_be32(frame + 4u);
    if (use_last_pdat) {
        width = rr_rsrc_be32(ccb + 72u);
        height = rr_rsrc_be32(ccb + 76u);
        header_bytes = 12u;
    } else {
        rr_u32 top = be16(frame + 8u);
        rr_u32 left = be16(frame + 10u);
        rr_u32 bottom = be16(frame + 12u);
        rr_u32 right = be16(frame + 14u);
        width = (right - left) & 0xffffu;
        height = (bottom - top) & 0xffffu;
        header_bytes = 20u;
    }
    if (chunk_bytes < header_bytes ||
        chunk_bytes - header_bytes > scratch_capacity ||
        width == 0u || height == 0u || width > 256u || height > 256u ||
        width * height > pixel_capacity ||
        !rr_rsrc_read(file, &family, frame_at + header_bytes, scratch,
                      chunk_bytes - header_bytes) ||
        !rr_cel_decode_packed(scratch, chunk_bytes - header_bytes,
                              width, height, bpp, palette, color_count,
                              divisor, image->pixels, pixel_capacity))
        return 0;
    image->width = width;
    image->height = height;
    return 1;
}

static int rr_family_word(const rr_rsrc_file_t *file,
                          const rr_rsrc_record_t *family,
                          rr_u32 offset, rr_u32 *value)
{
    rr_u8 word[4];
    if (!rr_rsrc_read(file, family, offset, word, 4u)) return 0;
    *value = rr_rsrc_be32(word);
    return 1;
}

int rr_family_decode_surface_tile_group_size(const rr_rsrc_file_t *file,
                                        rr_u32 family_id,
                                        rr_u32 family_index,
                                        rr_u32 entry_index,
                                        rr_u32 child_index,
                                        rr_u32 target_width,
                                        rr_u32 target_height,
                                        rr_cel_image_t *image,
                                        rr_u32 pixel_capacity,
                                        rr_u8 *scratch,
                                        rr_u32 scratch_capacity)
{
    rr_rsrc_record_t family;
    rr_u8 header[160];
    unsigned short palette[32];
    rr_u32 root_count, level_at, level_count, resource_at, child_count;
    rr_u32 relative, chunk_bytes, descriptor_at, table_at, row_offset;
    rr_u32 column_offset, payload_offset, payload_bytes, read_bytes;
    rr_u32 width, height, h_index = 0u, v_index = 0u, h_count, v_count;
    rr_u32 h_base, v_base, palette_count, bpp, divisor, ccb_flags, i;
    rr_u32 kind, selection_index = 0u, palette_end;
    rr_u32 pre0, pre1;
    int packed;
    if (!file || !image || !image->pixels || !scratch ||
        !target_width || target_width > 128u ||
        !target_height || target_height > 128u ||
        !rr_rsrc_find(file, RR_RSRC_TAG('F','A','M',' '), family_id,
                      &family) ||
        !rr_family_word(file, &family, 0u, &root_count) ||
        root_count <= family_index || root_count > 64u ||
        !rr_family_word(file, &family,
                        4u + family_index * 4u, &relative)) return 0;
    level_at = relative;
    if (!rr_family_word(file, &family, level_at, &level_count) ||
        entry_index >= level_count || level_count > 64u ||
        !rr_family_word(file, &family,
                        level_at + 4u + entry_index * 4u,
                        &relative)) return 0;
    resource_at = level_at + relative;
    if (child_index != 0xffffffffu) {
        if (!rr_family_word(file, &family, resource_at, &child_count) ||
            child_count > 16u || child_index >= child_count ||
            !rr_family_word(file, &family,
                            resource_at + 4u + child_index * 4u,
                            &relative)) return 0;
        resource_at += relative;
    }
    if (!rr_rsrc_read(file, &family, resource_at,
                      header, 16u) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('C','L','G','P')) return 0;
    chunk_bytes = rr_rsrc_be32(header + 4u);
    if (chunk_bytes < 148u || resource_at > family.size ||
        chunk_bytes > family.size - resource_at ||
        !rr_rsrc_read(file, &family, resource_at,
                      header, 148u)) return 0;
    descriptor_at = resource_at + 8u;
    kind = header[8u] & 3u;
    if (header[9u] < 27u || header[9u] > 63u ||
        (kind != 0u && kind != 1u)) return 0;
    h_count = header[12u];
    v_count = header[13u];
    h_base = header[14u];
    v_base = header[15u];
    if (kind == 0u) {
        if (h_count > 8u || v_count > 8u ||
            h_base > 7u || v_base > 7u) return 0;
        while (h_index < h_count &&
               (1u << (h_base + h_index)) < target_width) ++h_index;
        while (v_index < v_count &&
               (1u << (v_base + v_index)) < target_height) ++v_index;
        width = 1u << (h_base + h_index);
        height = 1u << (v_base + v_index);
        table_at = descriptor_at + (rr_u32)header[9u] * 4u;
        palette_end = (rr_u32)header[9u] * 4u;
    } else {
        rr_u8 dimensions[2];
        rr_u32 requested = (header[8u] & 0x20u) ?
            target_height : target_width;
        /* The original threshold selector compares ceil(log2(size)) to
         * the second byte of each pair when USE_HEIGHT is set. */
        if (!(header[8u] & 0x40u) || h_count > 15u ||
            header[10u] < 27u || header[10u] > header[9u]) return 0;
        if (requested >= 256u) requested = 8u;
        else {
            rr_u32 exponent = 0u, power = 1u;
            while (power < requested) { power <<= 1u; ++exponent; }
            requested = exponent;
        }
        for (i = 1u; i <= h_count; ++i) {
            rr_u8 candidate[2];
            rr_u32 at = descriptor_at + (rr_u32)header[10u] * 4u +
                i * 2u;
            if (at + 2u > resource_at + chunk_bytes ||
                !rr_rsrc_read(file, &family, at, candidate, 2u))
                return 0;
            if (candidate[(header[8u] & 0x20u) ? 1u : 0u] <=
                requested) selection_index = i;
            else break;
        }
        relative = descriptor_at + (rr_u32)header[10u] * 4u +
            selection_index * 2u;
        if (relative + 2u > resource_at + chunk_bytes ||
            !rr_rsrc_read(file, &family, relative, dimensions, 2u) ||
            dimensions[0] > 7u || dimensions[1] > 7u)
            return 0;
        width = 1u << dimensions[0];
        height = 1u << dimensions[1];
        table_at = descriptor_at + (rr_u32)header[9u] * 4u;
        palette_end = (rr_u32)header[10u] * 4u;
    }
    if (width > 128u || height > 128u ||
        width * height > pixel_capacity || palette_end < 76u)
        return 0;
    palette_count = (palette_end - 76u) / 2u;
    if ((palette_count != 16u && palette_count != 32u) ||
        palette_end != 76u + palette_count * 2u)
        return 0;
    for (i = 0u; i < palette_count; ++i)
        palette[i] = (unsigned short)be16(header + 84u + i * 2u);
    ccb_flags = rr_rsrc_be32(header + 16u);
    packed = (ccb_flags & 0x200u) != 0u;
    divisor = rr_rsrc_be32(header + 68u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    if (kind == 0u) {
        if (!rr_family_word(file, &family,
                            table_at + v_index * 4u,
                            &row_offset) ||
            !rr_family_word(file, &family,
                            descriptor_at + row_offset + h_index * 4u,
                            &column_offset)) return 0;
        payload_offset = column_offset;
    } else if (!rr_family_word(file, &family,
                              table_at + selection_index * 4u,
                              &payload_offset)) return 0;
    if (payload_offset > chunk_bytes - 8u) return 0;
    payload_bytes = chunk_bytes - 8u - payload_offset;
    read_bytes = payload_bytes < scratch_capacity ?
        payload_bytes : scratch_capacity;
    if (read_bytes < 8u ||
        !rr_rsrc_read(file, &family, descriptor_at + payload_offset,
                      scratch, read_bytes)) return 0;
    pre0 = rr_rsrc_be32(scratch);
    pre1 = rr_rsrc_be32(scratch + 4u);
    bpp = pre0 & 7u;
    bpp = bpp == 3u ? 4u : bpp == 4u ? 6u :
        bpp == 5u ? 8u : 0u;
    if (!bpp) return 0;
    if (packed) {
        rr_u32 used = 0u;
        for (i = 0u; i < height; ++i) {
            rr_u32 words, row_bytes;
            if (used + (bpp == 8u ? 2u : 1u) > read_bytes - 4u)
                return 0;
            words = bpp == 8u ?
                be16(scratch + 4u + used) : scratch[4u + used];
            row_bytes = (words + 2u) * 4u;
            if (row_bytes > read_bytes - 4u - used) return 0;
            used += row_bytes;
        }
        if (!rr_cel_decode_packed(scratch + 4u, used,
                width, height, bpp, palette, palette_count,
                divisor, image->pixels, pixel_capacity)) return 0;
    } else if (bpp == 6u) {
        rr_u32 stride = (((pre1 >> 24u) & 255u) + 2u) * 4u;
        rr_u32 y, x;
        if (stride < (width * 6u + 7u) / 8u ||
            stride > (read_bytes - 8u) / height) return 0;
        for (y = 0u; y < height; ++y) {
            rr_bit_cursor_t cursor;
            cursor.data = scratch + 8u + y * stride;
            cursor.bit = 0u;
            cursor.end_bit = stride * 8u;
            for (x = 0u; x < width; ++x) {
                rr_u32 value;
                if (!read_bits(&cursor, 6u, &value)) return 0;
                image->pixels[y * width + x] = convert_color(
                    value, palette, palette_count, bpp, divisor);
            }
        }
    } else {
        rr_u32 stride = ((pre1 & 0x3ffu) + 1u) / 2u;
        rr_u32 y, x;
        if (bpp != 4u || stride < (width + 1u) / 2u ||
            stride > (read_bytes - 8u) / height) return 0;
        for (y = 0u; y < height; ++y)
            for (x = 0u; x < width; ++x) {
                rr_u8 pair = scratch[8u + y * stride + x / 2u];
                rr_u32 index = (x & 1u) ? pair & 15u : pair >> 4u;
                image->pixels[y * width + x] = convert_color(
                    index, palette, palette_count, bpp, divisor);
            }
    }
    image->width = width;
    image->height = height;
    image->surface_height = (rr_u32)header[9u] << 4u;
    return 1;
}

int rr_family_decode_surface_tile_size(const rr_rsrc_file_t *file,
                                        rr_u32 family_id,
                                        rr_u32 entry_index,
                                        rr_u32 child_index,
                                        rr_u32 target_width,
                                        rr_u32 target_height,
                                        rr_cel_image_t *image,
                                        rr_u32 pixel_capacity,
                                        rr_u8 *scratch,
                                        rr_u32 scratch_capacity)
{
    return rr_family_decode_surface_tile_group_size(file, family_id,
        1u, entry_index, child_index, target_width, target_height,
        image, pixel_capacity, scratch, scratch_capacity);
}

int rr_family_decode_surface_tile(const rr_rsrc_file_t *file,
                                   rr_u32 family_id, rr_u32 entry_index,
                                   rr_u32 child_index,
                                   rr_cel_image_t *image,
                                   rr_u32 pixel_capacity,
                                   rr_u8 *scratch,
                                   rr_u32 scratch_capacity)
{
    return rr_family_decode_surface_tile_size(file, family_id,
        entry_index, child_index, 32u, 16u, image, pixel_capacity,
        scratch, scratch_capacity);
}

static rr_u32 find_tag(const rr_u8 *data, rr_u32 size,
                       rr_u32 tag, rr_u32 from)
{
    rr_u32 i;
    for (i = from; i + 4u <= size; ++i)
        if (rr_rsrc_be32(data + i) == tag) return i;
    return size;
}

rr_u32 rr_animation_frame_count(const rr_u8 *record, rr_u32 record_bytes)
{
    rr_u32 at = 0u, count = 0u;
    if (!record) return 0u;
    while ((at = find_tag(record, record_bytes,
                          RR_RSRC_TAG('R','P','D','T'), at)) < record_bytes) {
        rr_u32 bytes;
        if (record_bytes - at < 20u) return 0u;
        bytes = rr_rsrc_be32(record + at + 4u);
        if (bytes < 20u || bytes > record_bytes - at) return 0u;
        ++count;
        at += bytes;
    }
    return count;
}

int rr_animation_decode_frame_anchor(const rr_u8 *record,
                                      rr_u32 record_bytes,
                                      rr_u32 frame_index,
                                      rr_cel_anchor_t *anchor)
{
    rr_u32 hotspot_at, hotspot_bytes, cursor, end, frame_at = 0u;
    rr_u32 i, control, count;
    int x, y;
    if (!record || !anchor) return 0;
    hotspot_at = find_tag(record, record_bytes,
                          RR_RSRC_TAG('H','S','P','T'), 0u);
    if (hotspot_at >= record_bytes ||
        record_bytes - hotspot_at < 24u) return 0;
    hotspot_bytes = rr_rsrc_be32(record + hotspot_at + 4u);
    if (hotspot_bytes < 24u || hotspot_bytes > record_bytes - hotspot_at ||
        frame_index >= rr_rsrc_be32(record + hotspot_at + 8u))
        return 0;
    cursor = hotspot_at + 16u;
    end = hotspot_at + hotspot_bytes;
    for (i = 0u; i <= frame_index; ++i) {
        if (cursor > end || end - cursor < 8u) return 0;
        control = rr_rsrc_be32(record + cursor);
        count = control >> 24u;
        if (!count || count > (end - cursor) / 8u) return 0;
        if (i == frame_index) break;
        cursor += count * 8u;
    }
    if (!(control & 0x10000u)) return 0;
    x = (short)be16(record + cursor + 6u);
    y = (short)be16(record + cursor + 4u);
    for (i = 0u; i <= frame_index; ++i) {
        frame_at = find_tag(record, record_bytes,
                            RR_RSRC_TAG('R','P','D','T'), frame_at);
        if (frame_at >= record_bytes ||
            record_bytes - frame_at < 20u) return 0;
        if (i != frame_index) frame_at += 4u;
    }
    x -= (short)be16(record + frame_at + 10u);
    y -= (short)be16(record + frame_at + 8u);
    anchor->x = (short)x;
    anchor->y = (short)y;
    return 1;
}

int rr_animation_decode_rect_frame(const rr_u8 *record,
                                    rr_u32 record_bytes,
                                    rr_u32 frame_index,
                                    rr_cel_image_t *image,
                                    rr_u32 pixel_capacity)
{
    rr_u32 ccb_at, plut_at, frame_at = 0u, frame_bytes, current = 0u;
    rr_u32 width, height, divisor, i;
    unsigned short palette[16];
    if (!record || !image || !image->pixels) return 0;
    ccb_at = find_tag(record, record_bytes,
                      RR_RSRC_TAG('C','C','B',' '), 0u);
    plut_at = find_tag(record, record_bytes,
                       RR_RSRC_TAG('P','L','U','T'), 0u);
    if (ccb_at > record_bytes || record_bytes - ccb_at < 80u ||
        plut_at > record_bytes || record_bytes - plut_at < 44u ||
        (rr_rsrc_be32(record + ccb_at + 12u) & 0x200u) == 0u ||
        (rr_rsrc_be32(record + ccb_at + 64u) & 7u) != 3u ||
        rr_rsrc_be32(record + plut_at + 8u) != 16u) return 0;
    for (i = 0u; i < 16u; ++i)
        palette[i] = (unsigned short)be16(record + plut_at + 12u + i * 2u);
    divisor = rr_rsrc_be32(record + ccb_at + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    for (;;) {
        frame_at = find_tag(record, record_bytes,
                            RR_RSRC_TAG('R','P','D','T'), frame_at);
        if (frame_at >= record_bytes || record_bytes - frame_at < 20u)
            return 0;
        if (current++ == frame_index) break;
        frame_at += 4u;
    }
    frame_bytes = rr_rsrc_be32(record + frame_at + 4u);
    width = (be16(record + frame_at + 14u) -
             be16(record + frame_at + 10u)) & 0xffffu;
    height = (be16(record + frame_at + 12u) -
              be16(record + frame_at + 8u)) & 0xffffu;
    if (frame_bytes < 20u || frame_bytes > record_bytes - frame_at ||
        !width || width > 128u || !height || height > 128u ||
        width * height > pixel_capacity ||
        !rr_cel_decode_packed(record + frame_at + 20u, frame_bytes - 20u,
                              width, height, 4u, palette, 16u,
                              divisor, image->pixels, pixel_capacity))
        return 0;
    image->width = width;
    image->height = height;
    return 1;
}

int rr_animation_decode_frame(const rr_u8 *record, rr_u32 record_bytes,
                              rr_u32 frame_index, int black_transparent,
                              rr_cel_image_t *image, rr_u32 pixel_capacity)
{
    rr_u32 ccb_at, plut_at, frame_at = 0u, current = 0u;
    rr_u32 color_count, divisor, width, height, frame_bytes, i;
    unsigned short palette[32];
    if (!record || !image || !image->pixels) return 0;
    ccb_at = find_tag(record, record_bytes,
                      RR_RSRC_TAG('C','C','B',' '), 0u);
    plut_at = find_tag(record, record_bytes,
                       RR_RSRC_TAG('P','L','U','T'), 0u);
    if (ccb_at > record_bytes || record_bytes - ccb_at < 80u ||
        plut_at > record_bytes || record_bytes - plut_at < 76u ||
        (rr_rsrc_be32(record + ccb_at + 12u) & 0x200u) == 0u ||
        (rr_rsrc_be32(record + ccb_at + 64u) & 7u) != 5u)
        return 0;
    color_count = rr_rsrc_be32(record + plut_at + 8u);
    if (color_count != 32u) return 0;
    for (i = 0u; i < color_count; ++i)
        palette[i] = (unsigned short)be16(record + plut_at + 12u + i * 2u);
    divisor = rr_rsrc_be32(record + ccb_at + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    for (;;) {
        frame_at = find_tag(record, record_bytes,
                            RR_RSRC_TAG('R','P','D','T'), frame_at);
        if (frame_at >= record_bytes || record_bytes - frame_at < 20u)
            return 0;
        if (current++ == frame_index) break;
        frame_at += 4u;
    }
    frame_bytes = rr_rsrc_be32(record + frame_at + 4u);
    width = rr_rsrc_be32(record + frame_at + 8u);
    height = ((rr_rsrc_be32(record + frame_at + 16u) >> 6) & 0x3ffu) + 1u;
    if (width > 128u || height > 128u) {
        /* Some car ANIM entries start with a reduced rectangle instead of
         * a full frame. Decode its visible patch for the 9588 preview. */
        width = (be16(record + frame_at + 14u) -
                 be16(record + frame_at + 10u)) & 0xffffu;
        height = (be16(record + frame_at + 12u) -
                  be16(record + frame_at + 8u)) & 0xffffu;
    }
    if (frame_bytes < 20u || frame_bytes > record_bytes - frame_at ||
        !width || width > 128u || height > 128u ||
        width * height > pixel_capacity ||
        !rr_cel_decode_packed(record + frame_at + 20u, frame_bytes - 20u,
                              width, height, 8u, palette, color_count,
                              divisor, image->pixels, pixel_capacity))
        return 0;
    if (black_transparent) {
        for (i = 0u; i < width * height; ++i)
            if (image->pixels[i] == 0u)
                image->pixels[i] = RR_CEL_TRANSPARENT;
    }
    image->width = width;
    image->height = height;
    return 1;
}

int rr_cel_load_backdrop(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_u8 *indices, rr_u32 index_capacity,
                         unsigned short *palette256,
                         rr_u32 *width, rr_u32 *height)
{
    rr_rsrc_record_t cel;
    rr_u8 chunk[80];
    rr_u8 ccb[80];
    rr_u8 plut[76];
    unsigned short colors[32];
    rr_u32 at = 0u, ccb_at = 0xffffffffu;
    rr_u32 plut_at = 0xffffffffu, pdat_at = 0xffffffffu;
    rr_u32 pdat_bytes = 0u, w, h, stride, count, divisor, i;
    if (!file || !indices || !palette256 || !width || !height ||
        !rr_rsrc_find(file, RR_RSRC_TAG('C','E','L',' '), cel_id, &cel))
        return 0;
    while (at < cel.size) {
        rr_u32 tag, size;
        if (!rr_rsrc_read(file, &cel, at, chunk, 8u)) return 0;
        tag = rr_rsrc_be32(chunk);
        size = rr_rsrc_be32(chunk + 4u);
        if (size < 8u || size > cel.size - at) return 0;
        if (tag == RR_RSRC_TAG('C','C','B',' ')) ccb_at = at;
        if (tag == RR_RSRC_TAG('P','L','U','T')) plut_at = at;
        if (tag == RR_RSRC_TAG('P','D','A','T')) {
            pdat_at = at;
            pdat_bytes = size;
        }
        at += size;
    }
    if (ccb_at == 0xffffffffu || plut_at == 0xffffffffu ||
        pdat_at == 0xffffffffu || pdat_bytes < 8u ||
        !rr_rsrc_read(file, &cel, ccb_at, ccb, sizeof(ccb)) ||
        !rr_rsrc_read(file, &cel, plut_at, plut, sizeof(plut)))
        return 0;
    w = rr_rsrc_be32(ccb + 72u);
    h = rr_rsrc_be32(ccb + 76u);
    count = rr_rsrc_be32(plut + 8u);
    stride = h ? (pdat_bytes - 8u) / h : 0u;
    if (!w || !h || w > 512u || h > 128u ||
        (rr_rsrc_be32(ccb + 64u) & 7u) != 5u ||
        count != 32u || w * h > index_capacity ||
        (pdat_bytes - 8u) % h != 0u ||
        stride < w || stride > w + 3u)
        return 0;
    if (stride == w) {
        if (!rr_rsrc_read(file, &cel, pdat_at + 8u, indices, w * h))
            return 0;
    } else {
        for (i = 0u; i < h; ++i)
            if (!rr_rsrc_read(file, &cel, pdat_at + 8u + i * stride,
                              indices + i * w, w)) return 0;
    }
    for (i = 0u; i < 32u; ++i)
        colors[i] = (unsigned short)be16(plut + 12u + 2u * i);
    divisor = rr_rsrc_be32(ccb + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    for (i = 0u; i < 256u; ++i)
        palette256[i] = convert_color(i, colors, 32u, 8u, divisor);
    *width = w;
    *height = h;
    return 1;
}

static int family_child(const rr_u8 *family, rr_u32 size,
                        rr_u32 parent, rr_u32 index, rr_u32 *child)
{
    rr_u32 count, offset;
    if (parent > size || size - parent < 8u) return 0;
    count = rr_rsrc_be32(family + parent);
    if (count > 64u || index >= count ||
        size - parent < 4u + 4u * count) return 0;
    offset = rr_rsrc_be32(family + parent + 4u + index * 4u);
    if (offset > size - parent || size - parent - offset < 8u)
        return 0;
    *child = parent + offset;
    return 1;
}

int rr_family_decode_rider_frame(const rr_u8 *family, rr_u32 family_bytes,
                                 rr_u32 rider_index, rr_u32 frame_index,
                                 rr_cel_image_t *image,
                                 rr_u32 pixel_capacity)
{
    rr_u32 group, entry, table, table_bytes, i;
    rr_u32 ccb_at = family_bytes, plut_at = family_bytes;
    rr_u32 selected = family_bytes, visible = 0u;
    rr_u32 width, height, color_count, divisor, chunk_bytes;
    unsigned short palette[32];
    if (!family || family_bytes < 80u || !image || !image->pixels ||
        rider_index >= 64u ||
        !family_child(family, family_bytes, 0u, 2u, &group) ||
        !family_child(family, family_bytes, group, rider_index, &entry) ||
        !family_child(family, family_bytes, entry, 0u, &table) ||
        table > family_bytes - 8u ||
        rr_rsrc_be32(family + table) != RR_RSRC_TAG('O','F','S','T'))
        return 0;
    table_bytes = rr_rsrc_be32(family + table + 4u);
    if (table_bytes < 8u || (table_bytes & 3u) ||
        table_bytes > family_bytes - table) return 0;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 offset = rr_rsrc_be32(family + table + i);
        rr_u32 at, tag;
        if (offset > family_bytes - table ||
            family_bytes - table - offset < 8u) return 0;
        at = table + offset;
        tag = rr_rsrc_be32(family + at);
        if (tag == RR_RSRC_TAG('C','C','B',' ') && ccb_at == family_bytes)
            ccb_at = at;
        else if (tag == RR_RSRC_TAG('P','L','U','T') &&
                 plut_at == family_bytes)
            plut_at = at;
        else if (tag == RR_RSRC_TAG('R','P','D','T') &&
                 family_bytes - at >= 20u) {
            width = (be16(family + at + 14u) -
                     be16(family + at + 10u)) & 0xffffu;
            height = (be16(family + at + 12u) -
                      be16(family + at + 8u)) & 0xffffu;
            if (width > 1u && height > 1u &&
                width <= 128u && height <= 128u &&
                width * height <= pixel_capacity) {
                if (visible++ == frame_index) selected = at;
            }
        }
    }
    if (selected == family_bytes ||
        ccb_at > family_bytes - 80u ||
        plut_at > family_bytes - 76u ||
        (rr_rsrc_be32(family + ccb_at + 12u) & 0x200u) == 0u)
        return 0;
    color_count = rr_rsrc_be32(family + plut_at + 8u);
    if (color_count != 32u ||
        (rr_rsrc_be32(family + ccb_at + 64u) & 7u) != 5u)
        return 0;
    for (i = 0u; i < 32u; ++i)
        palette[i] = (unsigned short)be16(family + plut_at + 12u + i * 2u);
    divisor = rr_rsrc_be32(family + ccb_at + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    chunk_bytes = rr_rsrc_be32(family + selected + 4u);
    width = (be16(family + selected + 14u) -
             be16(family + selected + 10u)) & 0xffffu;
    height = (be16(family + selected + 12u) -
              be16(family + selected + 8u)) & 0xffffu;
    if (chunk_bytes < 20u || chunk_bytes > family_bytes - selected ||
        !rr_cel_decode_packed(family + selected + 20u,
                              chunk_bytes - 20u, width, height, 8u,
                              palette, 32u, divisor, image->pixels,
                              pixel_capacity)) return 0;
    image->width = width;
    image->height = height;
    return 1;
}

int rr_family_decode_rider_named_frame(const rr_u8 *family,
                                       rr_u32 family_bytes,
                                       rr_u32 rider_index,
                                       rr_u32 cans_key,
                                       rr_cel_image_t *image,
                                       rr_u32 pixel_capacity,
                                       rr_u32 *duration_ticks)
{
    rr_u32 root, entry, ofst, cans, section, names, name_count;
    rr_u32 group_count, group_at, key_index, frame_number = 0u;
    rr_u32 i, table_bytes, raw_frame = 0u, visible_frame = 0u;
    if (!family || family_bytes < 80u || rider_index >= 64u ||
        !family_child(family, family_bytes, 0u, 2u, &root) ||
        !family_child(family, family_bytes, root, rider_index, &entry) ||
        !family_child(family, family_bytes, entry, 0u, &ofst) ||
        !family_child(family, family_bytes, entry, 1u, &cans) ||
        rr_rsrc_be32(family + ofst) != RR_RSRC_TAG('O','F','S','T') ||
        rr_rsrc_be32(family + cans) != RR_RSRC_TAG('C','A','N','S'))
        return 0;
    section = cans;
    for (i = 0u; i < 4u; ++i) {
        rr_u32 next;
        if (section > family_bytes - 12u) return 0;
        next = rr_rsrc_be32(family + section + 4u);
        if (next < 12u || next > family_bytes - section) return 0;
        section += next;
    }
    if (section > family_bytes - 12u) return 0;
    name_count = rr_rsrc_be32(family + section + 8u);
    names = section + 12u;
    if (name_count > 128u || name_count >
        (family_bytes - names) / 16u) return 0;
    for (key_index = 0u; key_index < name_count; ++key_index)
        if (rr_rsrc_be32(family + names + key_index * 16u) ==
            cans_key) break;
    if (key_index == name_count) return 0;
    if (rr_rsrc_be32(family + cans + 8u) == 1u) {
        if (cans > family_bytes - 48u) return 0;
        group_count = rr_rsrc_be32(family + cans + 44u);
        group_at = cans + 48u;
    } else {
        group_count = rr_rsrc_be32(family + cans + 12u);
        group_at = cans + 16u;
    }
    if (group_count > 128u) return 0;
    for (i = 0u; i < group_count; ++i) {
        rr_u32 entry_number, variants;
        if (group_at > family_bytes - 16u) return 0;
        entry_number = rr_rsrc_be32(family + group_at);
        variants = rr_rsrc_be32(family + group_at + 4u);
        if (!variants || variants > 64u ||
            variants > (family_bytes - group_at - 16u) / 28u)
            return 0;
        if (entry_number == key_index + 1u) {
            frame_number = be16(family + group_at + 18u);
            if (duration_ticks) {
                rr_u32 ticks = rr_rsrc_be32(family + group_at + 8u) / 20u;
                *duration_ticks = ticks ? ticks : 1u;
            }
            break;
        }
        group_at += 16u + variants * 28u;
    }
    if (!frame_number) return 0;
    table_bytes = rr_rsrc_be32(family + ofst + 4u);
    if (table_bytes < 8u || (table_bytes & 3u) ||
        table_bytes > family_bytes - ofst) return 0;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 offset = rr_rsrc_be32(family + ofst + i);
        rr_u32 at, width, height;
        if (offset > family_bytes - ofst ||
            family_bytes - ofst - offset < 20u) continue;
        at = ofst + offset;
        if (rr_rsrc_be32(family + at) !=
            RR_RSRC_TAG('R','P','D','T')) continue;
        width = (be16(family + at + 14u) -
                 be16(family + at + 10u)) & 0xffffu;
        height = (be16(family + at + 12u) -
                  be16(family + at + 8u)) & 0xffffu;
        if (++raw_frame == frame_number) {
            if (width <= 1u || height <= 1u ||
                width > 128u || height > 128u) return 0;
            return rr_family_decode_rider_frame(family, family_bytes,
                rider_index, visible_frame, image, pixel_capacity);
        }
        if (width > 1u && height > 1u &&
            width <= 128u && height <= 128u &&
            width * height <= pixel_capacity) visible_frame++;
    }
    return 0;
}

static int decode_static_table_frame(const rr_u8 *family,
                                     rr_u32 family_bytes, rr_u32 table,
                                     rr_cel_image_t *image,
                                     rr_u32 pixel_capacity,
                                     rr_u32 bucket, int explicit_bucket)
{
    rr_u32 table_bytes, i;
    rr_u32 ccb_at = family_bytes, plut_at = family_bytes;
    rr_u32 frame_at[3] = {family_bytes, family_bytes, family_bytes};
    rr_u32 pdat_frames[3] = {family_bytes, family_bytes, family_bytes};
    rr_u32 frames = 0u, pdats = 0u;
    rr_u32 pdat_at = family_bytes;
    rr_u32 width, height, color_count, divisor, chunk_bytes, bpp;
    unsigned short palette[32];
    if (table > family_bytes || family_bytes - table < 8u ||
        rr_rsrc_be32(family + table) != RR_RSRC_TAG('O','F','S','T'))
        return 0;
    table_bytes = rr_rsrc_be32(family + table + 4u);
    if (table_bytes < 8u || (table_bytes & 3u) ||
        table_bytes > family_bytes - table) return 0;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 relative = rr_rsrc_be32(family + table + i);
        rr_u32 at;
        rr_u32 tag;
        if (relative > family_bytes - table ||
            family_bytes - table - relative < 8u) return 0;
        at = table + relative;
        tag = rr_rsrc_be32(family + at);
        if (tag == RR_RSRC_TAG('C','C','B',' ') && ccb_at == family_bytes)
            ccb_at = at;
        else if (tag == RR_RSRC_TAG('P','L','U','T') &&
                 plut_at == family_bytes)
            plut_at = at;
        else if (tag == RR_RSRC_TAG('R','P','D','T')) {
            if (frames < 3u) frame_at[frames] = at;
            frames++;
        } else if (tag == RR_RSRC_TAG('P','D','A','T')) {
            if (pdats < 3u) pdat_frames[pdats] = at;
            pdats++;
            pdat_at = at;
        }
    }
    if (explicit_bucket) {
        if (bucket >= 3u) return 0;
        pdat_at = pdat_frames[bucket];
    }
    if (ccb_at > family_bytes || family_bytes - ccb_at < 80u ||
        plut_at > family_bytes || family_bytes - plut_at < 12u ||
        ((explicit_bucket ? frame_at[bucket] : frame_at[0]) ==
             family_bytes && pdat_at == family_bytes) ||
        (rr_rsrc_be32(family + ccb_at + 12u) & 0x200u) == 0u)
        return 0;
    bpp = rr_rsrc_be32(family + ccb_at + 64u) & 7u;
    bpp = bpp == 3u ? 4u : bpp == 4u ? 6u : bpp == 5u ? 8u : 0u;
    color_count = rr_rsrc_be32(family + plut_at + 8u);
    if (!bpp || (color_count != 16u && color_count != 32u) ||
        family_bytes - plut_at < 12u + color_count * 2u) return 0;
    for (i = 0u; i < color_count; ++i)
        palette[i] = (unsigned short)be16(family + plut_at + 12u + i * 2u);
    divisor = rr_rsrc_be32(family + ccb_at + 60u) & 0x300u;
    divisor = divisor == 0u ? 16u :
        divisor == 0x100u ? 2u : divisor == 0x200u ? 4u : 8u;
    /* The second RPDT is the visible static frame in the original debris
       layout. A few larger families require the smaller first frame within
       the caller's fixed sprite capacity. */
    for (i = 0u; i < (explicit_bucket ? 1u : 2u); ++i) {
        rr_u32 selected = explicit_bucket ? frame_at[bucket] :
            frame_at[i == 0u ? 1u : 0u];
        if (selected > family_bytes || family_bytes - selected < 20u)
            continue;
        chunk_bytes = rr_rsrc_be32(family + selected + 4u);
        width = (be16(family + selected + 14u) -
                 be16(family + selected + 10u)) & 0xffffu;
        height = (be16(family + selected + 12u) -
                  be16(family + selected + 8u)) & 0xffffu;
        if (chunk_bytes < 20u || chunk_bytes > family_bytes - selected ||
            !width || !height || width > 256u || height > 192u ||
            width * height > pixel_capacity ||
            !rr_cel_decode_packed(family + selected + 20u,
                                  chunk_bytes - 20u, width, height, bpp,
                                  palette, color_count, divisor,
                                  image->pixels, pixel_capacity))
            continue;
        image->width = width;
        image->height = height;
        return 1;
    }
    if (pdat_at <= family_bytes && family_bytes - pdat_at >= 12u) {
        chunk_bytes = rr_rsrc_be32(family + pdat_at + 4u);
        width = rr_rsrc_be32(family + ccb_at + 72u);
        height = rr_rsrc_be32(family + ccb_at + 76u);
        if (chunk_bytes >= 12u &&
            chunk_bytes <= family_bytes - pdat_at &&
            width && height && width <= 256u && height <= 192u &&
            width * height <= pixel_capacity &&
            rr_cel_decode_packed(family + pdat_at + 12u,
                                 chunk_bytes - 12u, width, height, bpp,
                                 palette, color_count, divisor,
                                 image->pixels, pixel_capacity)) {
            image->width = width;
            image->height = height;
            return 1;
        }
    }
    return 0;
}

static int decode_static_table(const rr_u8 *family, rr_u32 family_bytes,
                               rr_u32 table, rr_cel_image_t *image,
                               rr_u32 pixel_capacity)
{
    return decode_static_table_frame(family, family_bytes, table,
                                     image, pixel_capacity, 0u, 0);
}

int rr_family_decode_static_frame(const rr_u8 *family, rr_u32 family_bytes,
                                   rr_u32 static_index, rr_cel_image_t *image,
                                   rr_u32 pixel_capacity)
{
    rr_u32 group, entry, count, child_index;
    if (!family || !image || !image->pixels || static_index >= 64u ||
        !family_child(family, family_bytes, 0u, 4u, &group) ||
        !family_child(family, family_bytes, group, static_index, &entry))
        return 0;
    count = rr_rsrc_be32(family + entry);
    if (count > 64u) return 0;
    for (child_index = count; child_index > 0u; --child_index) {
        rr_u32 table;
        if (family_child(family, family_bytes, entry,
                         child_index - 1u, &table) &&
            decode_static_table(family, family_bytes, table,
                                image, pixel_capacity))
            return 1;
    }
    return 0;
}

int rr_family_decode_static_frame_bucket(const rr_u8 *family,
                                         rr_u32 family_bytes,
                                         rr_u32 static_index,
                                         rr_u32 bucket,
                                         rr_cel_image_t *image,
                                         rr_u32 pixel_capacity)
{
    rr_u32 group, entry, table, child_index, count;
    if (!family || !image || !image->pixels ||
        static_index >= 64u || bucket >= 3u ||
        !family_child(family, family_bytes, 0u, 4u, &group) ||
        !family_child(family, family_bytes, group,
                      static_index, &entry))
        return 0;
    count = rr_rsrc_be32(family + entry);
    if (count > 64u) return 0;
    for (child_index = count; child_index > 0u; --child_index)
        if (family_child(family, family_bytes, entry,
                         child_index - 1u, &table) &&
            decode_static_table_frame(family, family_bytes, table,
                                      image, pixel_capacity, bucket, 1))
            return 1;
    return 0;
}

static int signed_family_word16(const rr_u8 *bytes)
{
    int value = (int)be16(bytes);
    return value & 0x8000 ? value - 0x10000 : value;
}

static int decode_static_anchor_from_table(const rr_u8 *family,
                                           rr_u32 family_bytes,
                                           rr_u32 table, rr_u32 bucket,
                                           rr_cel_anchor_t *anchor)
{
    rr_u32 table_bytes, i, hotspot_at = family_bytes;
    rr_u32 frame_at[3] = {family_bytes, family_bytes, family_bytes};
    rr_u32 frame_count = 0u, pixel_count = 0u;
    rr_u32 hotspot_bytes, hotspot_frames, cursor;
    int left = 0, top = 0, x, y;
    if (table > family_bytes || family_bytes - table < 8u ||
        rr_rsrc_be32(family + table) != RR_RSRC_TAG('O','F','S','T'))
        return 0;
    table_bytes = rr_rsrc_be32(family + table + 4u);
    if (table_bytes < 8u || (table_bytes & 3u) ||
        table_bytes > family_bytes - table) return 0;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 relative = rr_rsrc_be32(family + table + i);
        rr_u32 at, tag;
        if (relative > family_bytes - table ||
            family_bytes - table - relative < 8u) return 0;
        at = table + relative;
        tag = rr_rsrc_be32(family + at);
        if (tag == RR_RSRC_TAG('H','S','P','T'))
            hotspot_at = at;
        else if (tag == RR_RSRC_TAG('R','P','D','T')) {
            if (frame_count < 3u) frame_at[frame_count] = at;
            ++frame_count;
        } else if (tag == RR_RSRC_TAG('P','D','A','T'))
            ++pixel_count;
    }
    if (hotspot_at == family_bytes ||
        (frame_at[bucket] == family_bytes && pixel_count <= bucket))
        return 0;
    if (frame_at[bucket] != family_bytes) {
        rr_u32 frame_bytes;
        if (family_bytes - frame_at[bucket] < 20u) return 0;
        frame_bytes = rr_rsrc_be32(family + frame_at[bucket] + 4u);
        if (frame_bytes < 20u ||
            frame_bytes > family_bytes - frame_at[bucket]) return 0;
        top = signed_family_word16(family + frame_at[bucket] + 8u);
        left = signed_family_word16(family + frame_at[bucket] + 10u);
    }
    if (family_bytes - hotspot_at < 24u) return 0;
    hotspot_bytes = rr_rsrc_be32(family + hotspot_at + 4u);
    hotspot_frames = rr_rsrc_be32(family + hotspot_at + 8u);
    if (hotspot_bytes < 24u ||
        hotspot_bytes > family_bytes - hotspot_at ||
        hotspot_frames <= bucket || hotspot_frames > 64u) return 0;
    cursor = hotspot_at + 16u;
    for (i = 0u; i <= bucket; ++i) {
        rr_u32 run;
        if (cursor > hotspot_at + hotspot_bytes ||
            hotspot_at + hotspot_bytes - cursor < 8u) return 0;
        run = rr_rsrc_be32(family + cursor) >> 24u;
        if (!run || run > (hotspot_at + hotspot_bytes - cursor) / 8u)
            return 0;
        if (i == bucket) {
            if (!(rr_rsrc_be32(family + cursor) & 0x10000u))
                return 0;
            x = signed_family_word16(family + cursor + 6u) - left;
            y = signed_family_word16(family + cursor + 4u) - top;
            if (x < -256 || x > 256 || y < -256 || y > 256)
                return 0;
            anchor->x = (short)x;
            anchor->y = (short)y;
            return 1;
        }
        cursor += run * 8u;
    }
    return 0;
}

int rr_family_decode_static_anchor_bucket(const rr_u8 *family,
                                           rr_u32 family_bytes,
                                           rr_u32 static_index,
                                           rr_u32 bucket,
                                           rr_cel_anchor_t *anchor)
{
    rr_u32 group, entry, count, child_index;
    if (!family || !anchor || static_index >= 64u || bucket >= 3u ||
        !family_child(family, family_bytes, 0u, 4u, &group) ||
        !family_child(family, family_bytes, group, static_index, &entry))
        return 0;
    count = rr_rsrc_be32(family + entry);
    if (count > 64u) return 0;
    for (child_index = count; child_index > 0u; --child_index) {
        rr_u32 table;
        if (family_child(family, family_bytes, entry,
                         child_index - 1u, &table) &&
            decode_static_anchor_from_table(family, family_bytes,
                                            table, bucket, anchor))
            return 1;
    }
    return 0;
}

static rr_u32 decode_hotspots_from_table(const rr_u8 *family,
                                         rr_u32 family_bytes,
                                         rr_u32 table,
                                         rr_cel_hotspot_box_t boxes[2],
                                         rr_u32 bucket, int explicit_bucket)
{
    rr_u32 table_bytes, i, hotspot_at = family_bytes;
    rr_u32 chunk_bytes, frame_count, cursor, frame, count = 0u;
    if (table > family_bytes || family_bytes - table < 8u ||
        rr_rsrc_be32(family + table) != RR_RSRC_TAG('O','F','S','T'))
        return 0u;
    table_bytes = rr_rsrc_be32(family + table + 4u);
    if (table_bytes < 8u || (table_bytes & 3u) ||
        table_bytes > family_bytes - table) return 0u;
    for (i = 4u; i < table_bytes; i += 4u) {
        rr_u32 relative = rr_rsrc_be32(family + table + i);
        rr_u32 at;
        if (relative > family_bytes - table ||
            family_bytes - table - relative < 8u) return 0u;
        at = table + relative;
        if (rr_rsrc_be32(family + at) ==
            RR_RSRC_TAG('H','S','P','T')) {
            hotspot_at = at;
            break;
        }
    }
    if (hotspot_at == family_bytes ||
        family_bytes - hotspot_at < 24u) return 0u;
    chunk_bytes = rr_rsrc_be32(family + hotspot_at + 4u);
    frame_count = rr_rsrc_be32(family + hotspot_at + 8u);
    if (chunk_bytes < 24u || chunk_bytes > family_bytes - hotspot_at ||
        frame_count == 0u || frame_count > 64u) return 0u;
    if (explicit_bucket && bucket >= frame_count) return 0u;
    cursor = hotspot_at + 16u;
    for (frame = 0u; frame < frame_count; ++frame) {
        rr_u32 run, j;
        int center_x, center_y;
        if (cursor > hotspot_at + chunk_bytes ||
            hotspot_at + chunk_bytes - cursor < 8u) return 0u;
        run = rr_rsrc_be32(family + cursor) >> 24u;
        if (!run || run > (hotspot_at + chunk_bytes - cursor) / 8u)
            return 0u;
        if (frame != (explicit_bucket ? bucket : frame_count - 1u)) {
            cursor += run * 8u;
            continue;
        }
        if (run < 3u ||
            !(rr_rsrc_be32(family + cursor) & 0x10000u)) return 0u;
        center_y = signed_family_word16(family + cursor + 4u);
        center_x = signed_family_word16(family + cursor + 6u);
        for (j = 1u; j + 1u < run && count < 2u; j += 2u) {
            rr_u32 first = cursor + j * 8u;
            rr_u32 second = first + 8u;
            int left, right, top, bottom;
            if (!(rr_rsrc_be32(family + first) & 0x10000u) ||
                !(rr_rsrc_be32(family + second) & 0x10000u))
                continue;
            left = signed_family_word16(family + first + 6u) - center_x;
            top = signed_family_word16(family + first + 4u) - center_y;
            right = signed_family_word16(family + second + 6u) - center_x;
            bottom = signed_family_word16(family + second + 4u) - center_y;
            if (left >= right || top >= bottom ||
                left < -256 || right > 256 ||
                top < -256 || bottom > 256) continue;
            boxes[count].left = (short)left;
            boxes[count].top = (short)top;
            boxes[count].right = (short)right;
            boxes[count].bottom = (short)bottom;
            ++count;
        }
        return count;
    }
    return 0u;
}

rr_u32 rr_family_decode_static_hotspots(const rr_u8 *family,
                                       rr_u32 family_bytes,
                                       rr_u32 family_id,
                                       rr_u32 static_index,
                                       rr_cel_hotspot_box_t boxes[2])
{
    rr_u32 group, entry, count, child_index;
    if (!family || !boxes || static_index >= 64u) return 0u;
    if (family_id == 171u && static_index == 0u) {
        rr_u32 table;
        if (!family_child(family, family_bytes, 0u, 0u, &group) ||
            !family_child(family, family_bytes, group, 0u, &table))
            return 0u;
        return decode_hotspots_from_table(family, family_bytes,
                                          table, boxes, 0u, 0);
    }
    if (!family_child(family, family_bytes, 0u, 4u, &group) ||
        !family_child(family, family_bytes, group, static_index, &entry))
        return 0u;
    count = rr_rsrc_be32(family + entry);
    if (count > 64u) return 0u;
    for (child_index = count; child_index > 0u; --child_index) {
        rr_u32 table, found;
        if (!family_child(family, family_bytes, entry,
                          child_index - 1u, &table)) continue;
        found = decode_hotspots_from_table(family, family_bytes,
                                           table, boxes, 0u, 0);
        if (found) return found;
    }
    return 0u;
}

rr_u32 rr_family_decode_static_hotspots_bucket(const rr_u8 *family,
                                               rr_u32 family_bytes,
                                               rr_u32 family_id,
                                               rr_u32 static_index,
                                               rr_u32 bucket,
                                               rr_cel_hotspot_box_t boxes[2])
{
    rr_u32 group, entry, table, count, child_index;
    if (!family || !boxes || static_index >= 64u || bucket >= 3u)
        return 0u;
    if (family_id == 171u && static_index == 0u) {
        if (!family_child(family, family_bytes, 0u, 0u, &group) ||
            !family_child(family, family_bytes, group, 0u, &table))
            return 0u;
        return decode_hotspots_from_table(family, family_bytes,
                                          table, boxes, bucket, 1);
    }
    if (!family_child(family, family_bytes, 0u, 4u, &group) ||
        !family_child(family, family_bytes, group,
                      static_index, &entry))
        return 0u;
    count = rr_rsrc_be32(family + entry);
    if (count > 64u) return 0u;
    for (child_index = count; child_index > 0u; --child_index) {
        rr_u32 found;
        if (!family_child(family, family_bytes, entry,
                          child_index - 1u, &table)) continue;
        found = decode_hotspots_from_table(family, family_bytes,
                                           table, boxes, bucket, 1);
        if (found) return found;
    }
    return 0u;
}

int rr_family_decode_hazard_preview(const rr_rsrc_file_t *file,
                                    const rr_u8 *family,
                                    rr_u32 family_bytes,
                                    rr_u32 family_id,
                                    rr_u32 frame_index,
                                    rr_cel_image_t *image,
                                    rr_u32 pixel_capacity,
                                    rr_u8 *scratch,
                                    rr_u32 scratch_capacity)
{
    if (rr_family_decode_static_frame(family, family_bytes,
                                      frame_index, image, pixel_capacity))
        return 1;
    if (family_id == 171u && frame_index == 0u &&
        rr_family_load_frame(file, family_id, 1, image, pixel_capacity,
                             scratch, scratch_capacity) &&
        image->width * image->height > 1u)
        return 1;
    if (rr_family_load_frame(file, family_id, 0, image, pixel_capacity,
                             scratch, scratch_capacity) &&
        image->width * image->height > 1u)
        return 2;
    if (rr_family_load_frame(file, family_id, 1, image, pixel_capacity,
                             scratch, scratch_capacity) &&
        image->width * image->height > 1u)
        return 2;
    return 0;
}

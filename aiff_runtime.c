#include "aiff_runtime.h"

static rr_u32 rr_aiff_be16(const rr_u8 *p)
{
    return ((rr_u32)p[0] << 8) | p[1];
}

static rr_u32 extended_rate(const rr_u8 *p)
{
    rr_u32 exponent = rr_aiff_be16(p) & 0x7fffu;
    rr_u32 mantissa = rr_aiff_be16(p + 2);
    rr_u32 i;
    if ((p[0] & 0x80u) || exponent < 16383u || exponent > 16397u ||
        p[4] || p[5] || p[6] || p[7] || p[8] || p[9])
        return 0u;
    exponent -= 16383u;
    if (exponent >= 15u) return mantissa << (exponent - 15u);
    i = 1u << (15u - exponent);
    return (mantissa + i / 2u) / i;
}

int rr_aiff_find_sample(const rr_rsrc_file_t *file, rr_u32 resource_id,
                        rr_aiff_sample_t *sample)
{
    rr_rsrc_record_t record;
    rr_u8 form[12];
    rr_u8 chunk[8];
    rr_u8 common[18];
    rr_u8 sound[8];
    rr_u32 cursor, limit, channels = 0u, frames = 0u, bits = 0u, rate = 0u;
    rr_u32 pcm_offset = 0u, pcm_bytes = 0u;
    if (!file || !sample ||
        !rr_rsrc_find(file, RR_RSRC_TAG('A','I','F','F'), resource_id,
                      &record) || record.size < 12u ||
        !rr_rsrc_read(file, &record, 0u, form, sizeof(form)) ||
        rr_rsrc_be32(form) != RR_RSRC_TAG('F','O','R','M') ||
        rr_rsrc_be32(form + 8) != RR_RSRC_TAG('A','I','F','F') ||
        rr_rsrc_be32(form + 4) > record.size - 8u)
        return 0;
    cursor = 12u;
    limit = rr_rsrc_be32(form + 4) + 8u;
    while (cursor <= limit && limit - cursor >= 8u) {
        rr_u32 size, next;
        if (!rr_rsrc_read(file, &record, cursor, chunk, sizeof(chunk)))
            return 0;
        size = rr_rsrc_be32(chunk + 4);
        if (size > limit - cursor - 8u) return 0;
        next = cursor + 8u + size + (size & 1u);
        if (next > limit) return 0;
        if (rr_rsrc_be32(chunk) == RR_RSRC_TAG('C','O','M','M')) {
            if (size < sizeof(common) ||
                !rr_rsrc_read(file, &record, cursor + 8u, common,
                              sizeof(common))) return 0;
            channels = rr_aiff_be16(common);
            frames = rr_rsrc_be32(common + 2);
            bits = rr_aiff_be16(common + 6);
            rate = extended_rate(common + 8);
        } else if (rr_rsrc_be32(chunk) == RR_RSRC_TAG('S','S','N','D')) {
            rr_u32 offset;
            if (size < sizeof(sound) ||
                !rr_rsrc_read(file, &record, cursor + 8u, sound,
                              sizeof(sound))) return 0;
            offset = rr_rsrc_be32(sound);
            if (offset > size - 8u) return 0;
            pcm_offset = cursor + 16u + offset;
            pcm_bytes = size - 8u - offset;
        }
        cursor = next;
    }
    if (channels != 1u || bits != 16u || rate < 1000u ||
        rate > 22050u || !pcm_offset || !frames || frames > pcm_bytes / 2u)
        return 0;
    sample->source_rate = rate;
    sample->frame_count = frames;
    sample->pcm_offset = record.offset + pcm_offset;
    sample->pcm_bytes = frames * 2u;
    return 1;
}

int rr_aiff_read_frames(const rr_rsrc_file_t *file,
                        const rr_aiff_sample_t *sample,
                        rr_u32 first_frame, short *pcm, rr_u32 frame_count)
{
    rr_u8 raw[512];
    rr_u32 done = 0u;
    if (!file || !sample || !pcm || first_frame > sample->frame_count ||
        frame_count > sample->frame_count - first_frame) return 0;
    while (done < frame_count) {
        rr_u32 i, block = frame_count - done;
        if (block > 256u) block = 256u;
        if (!file->read_at(file->user,
                           sample->pcm_offset + (first_frame + done) * 2u,
                           raw, block * 2u)) return 0;
        for (i = 0u; i < block; ++i)
            pcm[done + i] = (short)rr_aiff_be16(raw + i * 2u);
        done += block;
    }
    return 1;
}

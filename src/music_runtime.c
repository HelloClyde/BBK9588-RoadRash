#include "music_runtime.h"

rr_u32 rr_music_choose_next(rr_u32 *random_state, rr_u32 previous,
                            rr_u32 count)
{
    rr_u32 value, index;
    if (!random_state || !count) return 0u;
    value = *random_state ? *random_state : 0x9e3779b9u;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    *random_state = value;
    if (count == 1u) return 0u;
    if (previous >= count) return value % count;
    index = value % (count - 1u);
    return index >= previous ? index + 1u : index;
}

static int rr_music_chunk(rr_music_stream_t *stream, rr_u32 offset,
                           rr_u8 *header, rr_u32 *size)
{
    if (offset > stream->file_size ||
        stream->file_size - offset < 8u ||
        !stream->read_at(stream->user, offset, header, 8u)) return 0;
    *size = rr_rsrc_be32(header + 4);
    return *size >= 8u && *size <= stream->file_size - offset;
}

int rr_music_open(rr_music_stream_t *stream, rr_read_at_t read_at,
                  void *user, rr_u32 file_size)
{
    rr_u32 cursor = 0u;
    rr_u8 header[48];
    rr_u32 size;
    if (!stream || !read_at || file_size < 64u) return 0;
    stream->read_at = read_at;
    stream->user = user;
    stream->file_size = file_size;
    stream->data_offset = 0u;
    stream->data_remaining = 0u;
    stream->cache_offset = 0u;
    stream->cache_bytes = 0u;
    stream->predictor[0] = stream->predictor[1] = 0;
    while (cursor < file_size) {
        if (!rr_music_chunk(stream, cursor, header, &size)) return 0;
        if (rr_rsrc_be32(header) == RR_RSRC_TAG('S','N','D','S') &&
            size >= 64u) {
            if (!read_at(user, cursor + 8u, header, sizeof(header)))
                return 0;
            if (rr_rsrc_be32(header + 8) ==
                    RR_RSRC_TAG('S','H','D','R') &&
                rr_rsrc_be32(header + 36) == 22050u &&
                rr_rsrc_be32(header + 40) == 2u &&
                rr_rsrc_be32(header + 44) ==
                    RR_RSRC_TAG('S','D','X','2')) {
                stream->first_data_chunk = cursor + size;
                stream->next_chunk = stream->first_data_chunk;
                return 1;
            }
        }
        cursor += size;
    }
    return 0;
}

static int rr_music_next_data(rr_music_stream_t *stream)
{
    rr_u8 header[24];
    rr_u32 size;
    rr_u32 steps;
    for (steps = 0u; steps < 128u; ++steps) {
        rr_u32 cursor = stream->next_chunk;
        if (cursor >= stream->file_size) return 0;
        if (!rr_music_chunk(stream, cursor, header, &size)) return 0;
        stream->next_chunk = cursor + size;
        if (rr_rsrc_be32(header) != RR_RSRC_TAG('S','N','D','S') ||
            size <= 24u) continue;
        if (!stream->read_at(stream->user, cursor + 8u, header,
                             sizeof(header))) return 0;
        if (rr_rsrc_be32(header + 8) !=
                RR_RSRC_TAG('S','S','M','P') ||
            rr_rsrc_be32(header + 12) != size - 24u)
            continue;
        stream->data_offset = cursor + 24u;
        stream->data_remaining = size - 24u;
        return 1;
    }
    return 0;
}

static int rr_sdx2_decode(int previous, rr_u8 code)
{
    int signed_code = (int)(signed char)code;
    int delta = signed_code * signed_code * 2;
    int value = ((code & 1u) ? previous : 0) +
                (signed_code < 0 ? -delta : delta);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    return value;
}

rr_u32 rr_music_read_mono(rr_music_stream_t *stream, short *pcm,
                           rr_u32 frames)
{
    rr_u32 done = 0u;
    if (!stream || !pcm) return 0u;
    while (done < frames) {
        rr_u32 block, available, i;
        const rr_u8 *compressed;
        if (stream->data_remaining < 2u &&
            !rr_music_next_data(stream)) break;
        if (stream->data_offset < stream->cache_offset ||
            stream->data_offset - stream->cache_offset >=
                stream->cache_bytes) {
            rr_u32 fetch = stream->data_remaining;
            if (fetch > RR_MUSIC_READ_CACHE_BYTES)
                fetch = RR_MUSIC_READ_CACHE_BYTES;
            if (!stream->read_at(stream->user, stream->data_offset,
                                 stream->cache, fetch)) break;
            stream->cache_offset = stream->data_offset;
            stream->cache_bytes = fetch;
        }
        available = (stream->cache_bytes -
            (stream->data_offset - stream->cache_offset)) / 2u;
        block = frames - done;
        if (block > stream->data_remaining / 2u)
            block = stream->data_remaining / 2u;
        if (block > available) block = available;
        if (!block) break;
        compressed = stream->cache +
            (stream->data_offset - stream->cache_offset);
        for (i = 0u; i < block; ++i) {
            int left = rr_sdx2_decode(stream->predictor[0],
                                      compressed[i * 2u]);
            int right = rr_sdx2_decode(stream->predictor[1],
                                       compressed[i * 2u + 1u]);
            stream->predictor[0] = left;
            stream->predictor[1] = right;
            pcm[done + i] = (short)((left + right) / 2);
        }
        stream->data_offset += block * 2u;
        stream->data_remaining -= block * 2u;
        done += block;
    }
    return done;
}

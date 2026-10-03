#include "road_pack_runtime.h"

static rr_u32 rr_pack_u32(const rr_u8 *p)
{
    return (rr_u32)p[0] | ((rr_u32)p[1] << 8u) |
           ((rr_u32)p[2] << 16u) | ((rr_u32)p[3] << 24u);
}

static int rr_pack_name_equal(const char *left, const char *right)
{
    while (*left && *right) {
        char a = *left++;
        char b = *right++;
        if (a == '\\') a = '/';
        if (b == '\\') b = '/';
        if (a != b) return 0;
    }
    return !*left && !*right;
}

int rr_pack_find(const rr_pack_t *pack, const char *name)
{
    rr_u32 i;
    if (!pack || !name) return -1;
    for (i = 0u; i < pack->count; ++i)
        if (rr_pack_name_equal(pack->entries[i].name, name))
            return (int)i;
    return -1;
}

int rr_pack_open(rr_pack_t *pack, rr_read_at_t read_at, void *user,
                 rr_u32 file_size)
{
    rr_u8 header[16];
    rr_u8 entry[RR_PACK_ENTRY_BYTES];
    rr_u32 count, data_start, i, j;
    if (!pack || !read_at || file_size < 16u ||
        !read_at(user, 0u, header, sizeof(header)) ||
        header[0] != 'R' || header[1] != 'R' ||
        header[2] != 'P' || header[3] != 'K' ||
        rr_pack_u32(header + 4u) != 1u) return 0;
    count = rr_pack_u32(header + 8u);
    if (!count || count > RR_PACK_MAX_FILES ||
        count > (file_size - 16u) / RR_PACK_ENTRY_BYTES) return 0;
    data_start = 16u + count * RR_PACK_ENTRY_BYTES;
    if (rr_pack_u32(header + 12u) != data_start) return 0;
    pack->read_at = read_at;
    pack->user = user;
    pack->file_size = file_size;
    pack->count = 0u;
    for (i = 0u; i < count; ++i) {
        rr_pack_entry_t *item = &pack->entries[i];
        if (!read_at(user, 16u + i * RR_PACK_ENTRY_BYTES,
                     entry, sizeof(entry))) return 0;
        for (j = 0u; j < RR_PACK_NAME_BYTES; ++j)
            item->name[j] = (char)entry[j];
        if (!item->name[0] || item->name[RR_PACK_NAME_BYTES - 1u])
            return 0;
        item->offset = rr_pack_u32(entry + RR_PACK_NAME_BYTES);
        item->size = rr_pack_u32(entry + RR_PACK_NAME_BYTES + 4u);
        if (item->offset < data_start || item->offset > file_size ||
            item->size > file_size - item->offset ||
            rr_pack_find(pack, item->name) >= 0) return 0;
        ++pack->count;
    }
    return 1;
}

int rr_pack_read(const rr_pack_t *pack, unsigned int index,
                 rr_u32 offset, void *dst, rr_u32 size)
{
    const rr_pack_entry_t *item;
    if (!pack || index >= pack->count || !dst) return 0;
    item = &pack->entries[index];
    if (offset > item->size || size > item->size - offset) return 0;
    return pack->read_at(pack->user, item->offset + offset, dst, size);
}

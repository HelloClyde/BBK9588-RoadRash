#include "rsrc_reader.h"

rr_u32 rr_rsrc_be32(const rr_u8 *bytes)
{
    return ((rr_u32)bytes[0] << 24) | ((rr_u32)bytes[1] << 16) |
           ((rr_u32)bytes[2] << 8) | (rr_u32)bytes[3];
}

static int in_bounds(rr_u32 offset, rr_u32 size, rr_u32 limit)
{
    return offset <= limit && size <= limit - offset;
}

int rr_rsrc_open(rr_rsrc_file_t *file, rr_read_at_t read_at,
                 void *user, rr_u32 file_size)
{
    rr_u8 header[24];
    rr_u8 table[16];
    rr_u32 table_offset;
    rr_u32 table_size;
    rr_u32 count;
    if (!file || !read_at || file_size < sizeof(header) ||
        !read_at(user, 0u, header, sizeof(header)) ||
        rr_rsrc_be32(header) != RR_RSRC_TAG('R','S','R','C'))
        return 0;
    /* These two fields are ResourceFileDiskHeader's catalog range. */
    table_offset = rr_rsrc_be32(header + 16);
    table_size = rr_rsrc_be32(header + 20);
    if (!in_bounds(table_offset, table_size, file_size) ||
        table_size < sizeof(table) ||
        !read_at(user, table_offset, table, sizeof(table)) ||
        rr_rsrc_be32(table) != RR_RSRC_TAG('R','T','B','L'))
        return 0;
    count = rr_rsrc_be32(table + 8);
    if (count > (table_size - sizeof(table)) / 32u)
        return 0;
    file->read_at = read_at;
    file->user = user;
    file->file_size = file_size;
    file->table_offset = table_offset;
    file->resource_count = count;
    return 1;
}

int rr_rsrc_record_at(const rr_rsrc_file_t *file, rr_u32 index,
                      rr_rsrc_record_t *record)
{
    rr_u8 raw[32];
    if (!file || !record || index >= file->resource_count ||
        !file->read_at(file->user, file->table_offset + 16u + index * 32u,
                       raw, sizeof(raw)))
        return 0;
    record->type = rr_rsrc_be32(raw);
    record->id = rr_rsrc_be32(raw + 4);
    record->offset = rr_rsrc_be32(raw + 8);
    record->size = rr_rsrc_be32(raw + 12);
    record->flags = rr_rsrc_be32(raw + 16);
    return in_bounds(record->offset, record->size, file->file_size);
}

int rr_rsrc_find(const rr_rsrc_file_t *file, rr_u32 type, rr_u32 id,
                 rr_rsrc_record_t *record)
{
    rr_u32 low, high;
    if (!file || !record) return 0;
    /* RTBL is ordered by (type, ID), as in upstream's resource lookup. */
    low = 0u;
    high = file->resource_count;
    while (low < high) {
        rr_u32 middle = low + (high - low) / 2u;
        if (!rr_rsrc_record_at(file, middle, record)) return 0;
        if (record->type < type ||
            (record->type == type && record->id < id))
            low = middle + 1u;
        else
            high = middle;
    }
    return low < file->resource_count &&
        rr_rsrc_record_at(file, low, record) &&
        record->type == type && record->id == id;
}

int rr_rsrc_read(const rr_rsrc_file_t *file, const rr_rsrc_record_t *record,
                 rr_u32 relative_offset, void *dst, rr_u32 size)
{
    if (!file || !record || !dst ||
        !in_bounds(record->offset, record->size, file->file_size) ||
        !in_bounds(relative_offset, size, record->size))
        return 0;
    return file->read_at(file->user, record->offset + relative_offset,
                         dst, size);
}

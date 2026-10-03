#ifndef ROAD_RASH_GEOMETRY_RUNTIME_H
#define ROAD_RASH_GEOMETRY_RUNTIME_H

#include "rsrc_reader.h"

int rr_road_widths_for_segment(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               rr_u32 segment_relative_offset,
                               rr_u32 samples,
                               unsigned short *left,
                               unsigned short *right);

/* Depth outputs may both be null when only the RSLD margins are needed. */
int rr_road_slopes_for_segment(const rr_rsrc_file_t *file,
                              const rr_rsrc_record_t *segments,
                              rr_u32 segment_relative_offset,
                              rr_u32 samples,
                              rr_u8 *left_margin, rr_u8 *right_margin,
                              rr_u8 *left_depth, rr_u8 *right_depth,
                              rr_u8 *left_edge_resource,
                              rr_u8 *right_edge_resource);

#endif

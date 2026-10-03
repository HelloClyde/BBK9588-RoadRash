#ifndef ROAD_RASH_CANS_RUNTIME_H
#define ROAD_RASH_CANS_RUNTIME_H

#include "road_core.h"
#include "rsrc_reader.h"

/* The car CANS resource has three distance rows. Each channel holds a
 * one-based RPDT index. This is the original frame binding table, separate
 * from the 3DO orientation projection that chooses a column. */
int rr_cans_parse_car_frames(const rr_u8 *bytes, rr_u32 size,
                             rr_u32 animation_frames,
                             road_car_frame_map_t *map);

#endif

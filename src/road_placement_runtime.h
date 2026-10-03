#ifndef ROAD_RASH_PLACEMENT_RUNTIME_H
#define ROAD_RASH_PLACEMENT_RUNTIME_H

#include "road_course_runtime.h"

/* Apply the original RHZD weight/spread, mode and difficulty rules.
 * A deterministic local PRNG replaces the 3DO process-wide rand(). */
rr_u32 rr_schedule_hazard_entry(rr_u32 control, rr_u32 detail,
                                 unsigned int level, rr_u32 *sample,
                                 rr_u32 *random_state, int *enabled);

int rr_placements_for_segment(const rr_rsrc_file_t *file,
                               const rr_rsrc_record_t *segments,
                               const rr_u8 *segment_row,
                               rr_u32 segment_samples,
                               rr_u32 global_start,
                               rr_course_buffer_t *course);

#endif

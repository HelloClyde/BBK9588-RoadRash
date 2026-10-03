#ifndef ROAD_RASH_CEL_RUNTIME_H
#define ROAD_RASH_CEL_RUNTIME_H

#include "rsrc_reader.h"

#define RR_CEL_TRANSPARENT 0xf81fu

typedef struct rr_cel_image {
    rr_u32 width;
    rr_u32 height;
    unsigned short *pixels;
    rr_u32 surface_height;
} rr_cel_image_t;

typedef struct rr_cel_hotspot_box {
    short left;
    short top;
    short right;
    short bottom;
} rr_cel_hotspot_box_t;

typedef struct rr_cel_anchor {
    short x;
    short y;
} rr_cel_anchor_t;

/* CPU replacement for 3DO packed, coded CEL pixel decoding. */
int rr_cel_decode_packed(const rr_u8 *packed, rr_u32 packed_bytes,
                         rr_u32 width, rr_u32 height, rr_u32 bits_per_pixel,
                         const unsigned short *palette, rr_u32 palette_count,
                         rr_u32 divisor, unsigned short *pixels,
                         rr_u32 pixel_capacity);

/* Packed uncoded RGB555 CEL used by the original 3DO front end. */
int rr_cel_decode_packed_rgb16(const rr_u8 *packed, rr_u32 packed_bytes,
                               rr_u32 width, rr_u32 height,
                               unsigned short *pixels,
                               rr_u32 pixel_capacity);
int rr_cel_load_rgb16(const rr_rsrc_file_t *file, rr_u32 cel_id,
                       rr_cel_image_t *image, rr_u32 pixel_capacity,
                       rr_u8 *scratch, rr_u32 scratch_capacity);

/* Read one frame of any original FAM resource by type/ID, without extracting
 * or embedding the FAM tree at build time. use_last_pdat handles full PDAT
 * frames such as the Highway marker. */
int rr_family_load_frame(const rr_rsrc_file_t *file, rr_u32 family_id,
                         int use_last_pdat, rr_cel_image_t *image,
                         rr_u32 pixel_capacity, rr_u8 *scratch,
                         rr_u32 scratch_capacity);

/* Resolve a road-surface selector through the original FAM tree and decode
 * one dimension-selected CLGP CEL. child_index is 0..2 for RHIL bands or
 * 0xffffffff for a direct RBLD terrain entry. */
int rr_family_decode_surface_tile(const rr_rsrc_file_t *file,
                                   rr_u32 family_id, rr_u32 entry_index,
                                   rr_u32 child_index,
                                   rr_cel_image_t *image,
                                   rr_u32 pixel_capacity,
                                   rr_u8 *scratch,
                                   rr_u32 scratch_capacity);
int rr_family_decode_surface_tile_size(const rr_rsrc_file_t *file,
                                        rr_u32 family_id,
                                        rr_u32 entry_index,
                                        rr_u32 child_index,
                                        rr_u32 target_width,
                                        rr_u32 target_height,
                                        rr_cel_image_t *image,
                                        rr_u32 pixel_capacity,
                                        rr_u8 *scratch,
                                        rr_u32 scratch_capacity);
/* The original margin renderer looks up root family index 5; ordinary
 * roadside/RHIL tiles use root index 1. */
int rr_family_decode_surface_tile_group_size(const rr_rsrc_file_t *file,
    rr_u32 family_id, rr_u32 family_index, rr_u32 entry_index,
    rr_u32 child_index, rr_u32 target_width, rr_u32 target_height,
    rr_cel_image_t *image, rr_u32 pixel_capacity,
    rr_u8 *scratch, rr_u32 scratch_capacity);

int rr_animation_decode_frame(const rr_u8 *record, rr_u32 record_bytes,
                              rr_u32 frame_index, int black_transparent,
                              rr_cel_image_t *image, rr_u32 pixel_capacity);
/* An explicit car ANIM HSPT centre, adjusted into its RPDT crop. Returns 0
 * when the frame uses the standard centre/bottom anchor. */
int rr_animation_decode_frame_anchor(const rr_u8 *record,
                                      rr_u32 record_bytes,
                                      rr_u32 frame_index,
                                      rr_cel_anchor_t *anchor);
rr_u32 rr_animation_frame_count(const rr_u8 *record, rr_u32 record_bytes);

/* Rectangular 4bpp frames in rashOpt ANIM 1, the default HUD health bars. */
int rr_animation_decode_rect_frame(const rr_u8 *record,
                                    rr_u32 record_bytes,
                                    rr_u32 frame_index,
                                    rr_cel_image_t *image,
                                    rr_u32 pixel_capacity);

int rr_cel_load_backdrop(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_u8 *indices, rr_u32 index_capacity,
                         unsigned short *palette256,
                         rr_u32 *width, rr_u32 *height);

/* Unpacked 4bpp road-strip CELs 1..27 in rashOpt.rsrc. */
int rr_cel_load_road_surface(const rr_rsrc_file_t *file, rr_u32 cel_id,
                             rr_cel_image_t *image, rr_u32 pixel_capacity);

/* Unpacked coded 6bpp CELs, including the original HUD needles. */
int rr_cel_load_uncoded6(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_cel_image_t *image, rr_u32 pixel_capacity);
int rr_cel_load_uncoded1(const rr_rsrc_file_t *file, rr_u32 cel_id,
                         rr_cel_image_t *image, rr_u32 pixel_capacity);

/* Packed palettized CEL, including the original 300x58 race HUD (id 55).
 * scratch holds the compressed PDAT payload, up to 12 KiB for that HUD. */
int rr_cel_load_packed_image(const rr_rsrc_file_t *file, rr_u32 cel_id,
                             rr_cel_image_t *image, rr_u32 pixel_capacity,
                             rr_u8 *scratch, rr_u32 scratch_capacity);

int rr_family_decode_static_frame(const rr_u8 *family, rr_u32 family_bytes,
                                   rr_u32 static_index, rr_cel_image_t *image,
                                   rr_u32 pixel_capacity);
/* Original roadside CANS selects near/middle/far frame indices 0/1/2
 * according to projected size. Returns zero when that bucket is absent. */
int rr_family_decode_static_frame_bucket(const rr_u8 *family,
                                         rr_u32 family_bytes,
                                         rr_u32 static_index,
                                         rr_u32 bucket,
                                         rr_cel_image_t *image,
                                         rr_u32 pixel_capacity);
int rr_family_decode_static_anchor_bucket(const rr_u8 *family,
                                           rr_u32 family_bytes,
                                           rr_u32 static_index,
                                           rr_u32 bucket,
                                           rr_cel_anchor_t *anchor);

/* HSPT boxes from the nearest static-object frame, in CEL pixels relative
 * to its center. Returns the number of valid boxes (at most two). */
rr_u32 rr_family_decode_static_hotspots(const rr_u8 *family,
                                       rr_u32 family_bytes,
                                       rr_u32 family_id,
                                       rr_u32 static_index,
                                       rr_cel_hotspot_box_t boxes[2]);
rr_u32 rr_family_decode_static_hotspots_bucket(const rr_u8 *family,
                                               rr_u32 family_bytes,
                                               rr_u32 family_id,
                                               rr_u32 static_index,
                                               rr_u32 bucket,
                                               rr_cel_hotspot_box_t boxes[2]);

/* Visible rider frames in FAM group 2, selected by RHZD's low six bits. */
int rr_family_decode_rider_frame(const rr_u8 *family, rr_u32 family_bytes,
                                 rr_u32 rider_index, rr_u32 frame_index,
                                 rr_cel_image_t *image,
                                 rr_u32 pixel_capacity);
int rr_family_decode_rider_named_frame(const rr_u8 *family,
                                       rr_u32 family_bytes,
                                       rr_u32 rider_index,
                                       rr_u32 cans_key,
                                       rr_cel_image_t *image,
                                       rr_u32 pixel_capacity,
                                       rr_u32 *duration_ticks);

/* Return 1 for a matching static frame (or the original full marker), 2 for
 * a generic family preview frame, or 0 when neither layout can be decoded.
 * A single frame does not reproduce the original animated hazard state. */
int rr_family_decode_hazard_preview(const rr_rsrc_file_t *file,
                                    const rr_u8 *family,
                                    rr_u32 family_bytes,
                                    rr_u32 family_id,
                                    rr_u32 frame_index,
                                    rr_cel_image_t *image,
                                    rr_u32 pixel_capacity,
                                    rr_u8 *scratch,
                                    rr_u32 scratch_capacity);

#endif

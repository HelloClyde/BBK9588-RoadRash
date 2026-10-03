#define ROAD_USE_UPSTREAM_PATH 1
#include "bda_sdk.h"
#include "bda_audio.h"
#include "road_core.h"
#include "road_landscape_input.h"
#include "road_virtual_touch.h"
#include "road_ui_font.h"
#include "road_resource_paths.h"
static void service_audio(void);
#define ROAD_RUNTIME_AUDIO_PUMP() service_audio()
#define ROAD_RUNTIME_TRACE(stage, a, b, c) \
    road_diag_render_trace(stage, a, b, c)
static void road_diag_render_trace(const char *stage, unsigned int a,
                                   unsigned int b, unsigned int c);
#include "road_core.c"
#include "road_frontend.h"
#include "road_frontend.c"
#include "road_career.h"
#include "road_career.c"
#include "road_profile.h"
#include "road_profile.c"
#include "local-data/upstream-src/advance_road_path_traversal.c"
#include "local-data/upstream-src/initialize_road_lane_width_traversal.c"
#include "local-data/upstream-src/advance_road_lane_width_traversal.c"
#include "rsrc_reader.h"
#include "rsrc_reader.c"
#include "bike_spec_runtime.h"
#include "bike_spec_runtime.c"
#include "cel_runtime.h"
#include "cel_runtime.c"
#include "cans_runtime.h"
#include "cans_runtime.c"
#include "road_geometry_runtime.h"
#include "road_geometry_runtime.c"
#include "road_placement_runtime.h"
#include "road_placement_runtime.c"
#include "road_course_runtime.h"
#include "road_course_runtime.c"
#include "road_course_graph.h"
#include "road_course_graph.c"
#include "road_graph_race.h"
#include "road_graph_race.c"
#include "road_terrain_runtime.h"
#include "road_terrain_runtime.c"
#include "aiff_runtime.h"
#include "aiff_runtime.c"
#include "road_audio_runtime.h"
#include "road_audio_runtime.c"
#include "music_runtime.h"
#include "music_runtime.c"
#include "road_pack_runtime.h"
#include "road_pack_runtime.c"
#include "course_catalog.h"
#ifndef ROAD_RUNTIME_ASSETS_ONLY
#include "local-data/road_course_data.h"
#include "local-data/road_backdrop_data.h"
#include "local-data/road_scenery_data.h"
#include "local-data/road_object_data.h"
#include "local-data/road_static_data.h"
#include "local-data/road_bike_data.h"
#else
#define ROAD_COURSE_SAMPLE_COUNT 0
#define ROAD_BACKDROP_WIDTH 0
#define ROAD_SCENERY_COUNT 0
#define ROAD_OBJECT_COUNT 0
#define ROAD_STATIC_COUNT 0
#define ROAD_STATIC_SPRITE_COUNT 0
#define ROAD_BIKE_COUNT 0
#endif
#ifndef ROAD_STATIC_SPRITE_COUNT
#define ROAD_STATIC_SPRITE_COUNT 0
#endif
#define PORTRAIT_WIDTH 240
#define PORTRAIT_HEIGHT 320
#define VX_BYTES (24 + PORTRAIT_WIDTH * PORTRAIT_HEIGHT * 2)
#define FRAME_INTERVAL_MS 33u

typedef struct road_display_buffers {
    road_pixel_t landscape[ROAD_PIXELS];
    u8 vx[VX_BYTES] __attribute__((aligned(4)));
} road_display_buffers_t;
typedef struct road_course_buffers {
    road_surface_sample_t surface_samples[10000];
    road_surface_sample_t fork_surface_samples[10000];
    road_hill_sample_t hill_samples[10000];
    road_hill_sample_t fork_hill_samples[10000];
    short edge_outer_samples[2][10000];
    road_scenery_placement_t objects[8000];
} road_course_buffers_t;
typedef struct road_terrain_buffers {
    road_pixel_t margin_pixel_pool[ROAD_MARGIN_PIXEL_POOL_PIXELS];
    road_pixel_t hill_tile_pixels[ROAD_HILL_TILE_LIMIT]
                                 [ROAD_HILL_TILE_PIXELS];
    road_pixel_t hill_mip_pixels[ROAD_HILL_TILE_LIMIT]
                                [ROAD_HILL_MIP_PIXELS];
    road_pixel_t hill_narrow_pixels[ROAD_HILL_TILE_LIMIT]
                                   [ROAD_HILL_NARROW_MIP_PIXELS];
    road_pixel_t roadside_tile_pixels[ROAD_ROADSIDE_TILE_LIMIT]
                                      [ROAD_ROADSIDE_TILE_PIXELS];
    road_pixel_t edge_fill_pixels[2][256u * 32u];
} road_terrain_buffers_t;
typedef struct road_art_buffers {
    unsigned char backdrop_indices[360u * 94u];
    road_pixel_t sky_pixels[80u * 145u];
    road_pixel_t scenery_pixels[32][4096];
    road_pixel_t large_scenery_pixels[3][24576];
    road_pixel_t huge_scenery_pixels[32768];
    road_pixel_t road_pixels[3][5656];
    road_pixel_t hud_pixels[300u * 58u];
    road_pixel_t hud_health_pixels[32][32u * 8u];
} road_art_buffers_t;
typedef struct road_static_buffers {
    road_pixel_t pixels[64][4096];
} road_static_buffers_t;

static road_display_buffers_t *g_display_buffers;
static road_course_buffers_t *g_course_buffers;
static road_terrain_buffers_t *g_terrain_buffers;
static road_art_buffers_t *g_art_buffers;
static road_static_buffers_t *g_static_buffers;

#define g_landscape (g_display_buffers->landscape)
#define g_vx (g_display_buffers->vx)
#define g_runtime_surface_samples (g_course_buffers->surface_samples)
#define g_runtime_fork_surface_samples (g_course_buffers->fork_surface_samples)
#define g_runtime_hill_samples (g_course_buffers->hill_samples)
#define g_runtime_fork_hill_samples (g_course_buffers->fork_hill_samples)
#define g_runtime_edge_outer_samples (g_course_buffers->edge_outer_samples)
#define g_runtime_objects (g_course_buffers->objects)
#define g_runtime_margin_pixel_pool (g_terrain_buffers->margin_pixel_pool)
#define g_runtime_hill_tile_pixels (g_terrain_buffers->hill_tile_pixels)
#define g_runtime_hill_mip_pixels (g_terrain_buffers->hill_mip_pixels)
#define g_runtime_hill_narrow_pixels (g_terrain_buffers->hill_narrow_pixels)
#define g_runtime_roadside_tile_pixels (g_terrain_buffers->roadside_tile_pixels)
#define g_runtime_edge_fill_pixels (g_terrain_buffers->edge_fill_pixels)
#define g_runtime_backdrop_indices (g_art_buffers->backdrop_indices)
#define g_runtime_sky_pixels (g_art_buffers->sky_pixels)
#define g_runtime_scenery_pixels (g_art_buffers->scenery_pixels)
#define g_runtime_large_scenery_pixels (g_art_buffers->large_scenery_pixels)
#define g_runtime_huge_scenery_pixels (g_art_buffers->huge_scenery_pixels)
#define g_runtime_road_pixels (g_art_buffers->road_pixels)
#define g_runtime_hud_pixels (g_art_buffers->hud_pixels)
#define g_runtime_hud_health_pixels (g_art_buffers->hud_health_pixels)
#define g_runtime_static_pixels (g_static_buffers->pixels)

static road_pixel_t *g_menu_background;
static int g_menu_background_ready;
static signed char g_runtime_curvature[10000];
static signed char g_runtime_elevation[10000];
static unsigned short g_runtime_left_width[10000];
static unsigned short g_runtime_right_width[10000];
static signed char g_runtime_fork_curvature[10000];
static signed char g_runtime_fork_elevation[10000];
static unsigned short g_runtime_fork_left_width[10000];
static unsigned short g_runtime_fork_right_width[10000];
static rr_u8 g_runtime_fork_left_margin[10000];
static rr_u8 g_runtime_fork_right_margin[10000];
static rr_u8 g_runtime_fork_left_depth[10000];
static rr_u8 g_runtime_fork_right_depth[10000];
static rr_u8 g_runtime_fork_left_edge_resource[10000];
static rr_u8 g_runtime_fork_right_edge_resource[10000];
static unsigned short g_runtime_fork_left_edge_family[10000];
static unsigned short g_runtime_fork_right_edge_family[10000];
static rr_u8 g_runtime_left_margin[10000];
static rr_u8 g_runtime_right_margin[10000];
static rr_u8 g_runtime_left_edge_resource[10000];
static rr_u8 g_runtime_right_edge_resource[10000];
static unsigned short g_runtime_left_edge_family[10000];
static unsigned short g_runtime_right_edge_family[10000];
static rr_u8 g_runtime_left_depth[10000];
static rr_u8 g_runtime_right_depth[10000];
static rr_u8 g_runtime_terrain_mode[10000];
static rr_u8 g_runtime_surface_flags[10000];
static rr_u8 g_runtime_fork_terrain_mode[10000];
static rr_u8 g_runtime_fork_surface_flags[10000];
static road_margin_tile_t g_runtime_margin_tiles[ROAD_MARGIN_TILE_LIMIT];
static unsigned int g_runtime_margin_pixel_used;
static unsigned int g_runtime_margin_tile_count;
static unsigned int g_runtime_sample_count;
static road_hill_profile_t g_runtime_hill_profiles[512];
static road_hill_profile_t g_runtime_fork_hill_profiles[512];
static road_edge_profile_t g_runtime_edge_profiles[2][256];
static unsigned int g_runtime_hill_profile_count;
static unsigned int g_runtime_fork_hill_profile_count;
static unsigned int g_runtime_hill_tile_count;
static unsigned int g_runtime_hill_main_tile_count;
static unsigned short g_runtime_hill_family_keys[ROAD_HILL_TILE_LIMIT];
static unsigned char g_runtime_hill_selector_keys[ROAD_HILL_TILE_LIMIT];
static unsigned char g_runtime_hill_band_keys[ROAD_HILL_TILE_LIMIT];
static road_sprite_t g_runtime_hill_tiles[ROAD_HILL_TILE_LIMIT];
static road_sprite_t g_runtime_hill_mips[ROAD_HILL_TILE_LIMIT * 4u];
static road_sprite_t g_runtime_hill_narrow_mips[ROAD_HILL_TILE_LIMIT * 12u];
static road_sprite_t g_runtime_roadside_tiles[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned short g_runtime_roadside_family_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char g_runtime_roadside_selector_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char g_runtime_roadside_child_keys[ROAD_ROADSIDE_TILE_LIMIT];
static unsigned int g_runtime_roadside_tile_count;
static road_sprite_t g_runtime_edge_fill[2];
static unsigned short g_runtime_roadside_tile_heights
    [ROAD_ROADSIDE_TILE_LIMIT];
static unsigned char g_runtime_roadside_repeat_indices
    [ROAD_ROADSIDE_TILE_LIMIT];
static road_sprite_t g_runtime_roadside_repeat_mips
    [ROAD_ROADSIDE_TILE_LIMIT * ROAD_ROADSIDE_REPEAT_LEVELS];
static road_static_placement_t g_runtime_hazards[1000];
static road_effect_placement_t g_runtime_effects[1000];
static road_crossing_zone_t g_runtime_crossing_zones[256];
static unsigned int g_runtime_object_count;
static unsigned int g_runtime_hazard_count;
static unsigned int g_runtime_effect_count;
static unsigned int g_runtime_crossing_count;
static unsigned int g_runtime_branch_samples[8];
static rr_u8 g_runtime_branch_main_channels[8];
static unsigned short g_runtime_branch_first_left[8];
static unsigned short g_runtime_branch_first_right[8];
static unsigned short g_runtime_branch_old_left[8];
static unsigned int g_runtime_branch_count;
static rr_course_graph_t g_runtime_course_graph;
static rr_graph_race_t g_runtime_graph_race;
static int g_runtime_graph_valid;
static unsigned int g_active_branch_mask;
static unsigned int g_next_fork;
static unsigned int g_active_fork_end;
static unsigned int g_active_fork_reverse_start;
static unsigned int g_active_fork_selected_join;
static unsigned int g_active_fork_other_join;
/* 1: forward dual, 2: separated single, 3: reverse dual. */
static unsigned int g_active_fork_phase;
static int g_runtime_placements_loaded;
static road_pixel_t g_runtime_backdrop_palette[256];
static int g_runtime_backdrop_loaded;
static road_sprite_t g_runtime_sky;
static road_sprite_t g_runtime_scenery[32];
static rr_u32 g_runtime_family_ids[32];
static rr_u32 g_runtime_family_count;
static road_pixel_t *g_runtime_static_allocations[64];
static road_sprite_t g_runtime_static_sprites[64];
static road_sprite_t g_runtime_static_scale_frames[64][3];
static road_pixel_t *g_runtime_static_scale_allocations[64][3];
static road_static_frame_anchor_t g_runtime_static_frame_anchors[64][3];
static unsigned int g_runtime_reciprocal_table[0x1100];
static road_static_collision_t g_runtime_static_collisions[64];
static rr_u32 g_runtime_hazard_family_ids[64];
static rr_u8 g_runtime_hazard_frame_ids[64];
static unsigned char g_runtime_hazard_motion[64];
static rr_u32 g_runtime_hazard_pair_count;
static int g_runtime_static_loaded;
static road_sprite_t g_runtime_effect_sprites[64]
    [ROAD_EFFECT_SPRITE_FRAMES];
static road_pixel_t *g_runtime_effect_allocations[64]
    [ROAD_EFFECT_SPRITE_FRAMES];
static unsigned char g_runtime_effect_ticks[64]
    [ROAD_EFFECT_SPRITE_FRAMES];
static unsigned short g_runtime_effect_family_ids[64];
static unsigned char g_runtime_effect_variants[64];
static unsigned int g_runtime_effect_sprite_count;
static road_sprite_t g_runtime_bikes[ROAD_BIKE_FRAME_COUNT];
static road_pixel_t *g_runtime_bike_allocations[ROAD_BIKE_FRAME_COUNT];
static road_sprite_t g_runtime_road_textures[27];
static road_sprite_t g_runtime_hud;
static road_pixel_t g_runtime_hud_speed_needle_pixels[48u * 6u];
static road_pixel_t g_runtime_hud_health_needle_pixels[10u * 2u];
static road_sprite_t g_runtime_hud_speed_needle;
static road_sprite_t g_runtime_hud_health_needle;
static road_sprite_t g_runtime_hud_health_frames[32];
static road_pixel_t g_runtime_hud_portrait_pixels[43u * 14u];
static road_sprite_t g_runtime_hud_portrait;
static road_car_animation_t g_runtime_cars[16];
static road_car_frame_map_t g_runtime_car_maps[16];
static road_sprite_t g_runtime_car_frames[16][ROAD_CAR_MAX_FRAMES];
static road_car_frame_anchor_t g_runtime_car_anchors[16][ROAD_CAR_MAX_FRAMES];
static road_pixel_t *g_runtime_car_allocations[16][ROAD_CAR_MAX_FRAMES];
static rr_u8 g_runtime_cans_bytes[4096];
static unsigned int g_runtime_car_count;
static road_game_t g_game;
static rr_audio_mixer_t g_audio_mixer;
static short *g_audio_samples[19];
static short g_audio_pcm[512];
static short g_music_pcm[512];
static int g_audio_open;
static int g_audio_saved_attenuation;
static unsigned int g_audio_pending_bytes;
static unsigned int g_audio_pending_offset;
static unsigned int g_audio_written_blocks;
static unsigned int g_audio_ready_misses;
static unsigned int g_audio_zero_writes;
static unsigned int g_audio_pcm_nonzero;
static int g_music_file;
static int g_music_ready;
static unsigned int g_music_source_drive;
static unsigned int g_sfx_source_drive;
static unsigned int g_music_index;
static rr_u32 g_music_random_state;
static rr_music_stream_t g_music_stream;
static bda_handle_t g_frame;
static bda_handle_t g_draw;
static bda_handle_t g_draw_owner;
static bda_handle_t g_back;
static void *g_brush;
static volatile int g_detached;
static road_frontend_t g_frontend;
static road_profile_t g_profile;
static int g_profile_slot;
static int g_profile_save_failed;
static unsigned int g_last_award;
static rr_bike_spec_t g_bike_specs[15];
static int g_bike_specs_ready;
static int g_course_load_failed;
static const char *g_diag_path;
static const char *g_resource_root = ROAD_RESOURCE_ROOT_A;
static char g_diag_resource_path[ROAD_RESOURCE_PATH_CAP];
static char g_profile_paths[2][ROAD_RESOURCE_PATH_CAP];

#define ROAD_PACK_HANDLE_BASE 0x5a500000u
#define ROAD_PACK_OPEN_SLOTS 16u
typedef struct road_pack_open_file {
    unsigned int used;
    unsigned int index;
    unsigned int position;
} road_pack_open_file_t;
static rr_pack_t g_resource_pack;
static road_pack_open_file_t g_pack_open_files[ROAD_PACK_OPEN_SLOTS];
static int g_pack_file = -1;
static char g_pack_drive;

static int road_pack_read_at(void *user, rr_u32 offset, void *dst,
                             rr_u32 size)
{
    int file = *(int *)user;
    return offset <= 0x7fffffffu &&
        bda_fs_seek_raw(file, (s32)offset, BDA_SEEK_SET) == (int)offset &&
        bda_fs_read_raw(file, dst, size) == (int)size;
}

static int road_pack_try_open(const char *root)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    int file;
    if (!road_resource_join(path, sizeof(path), root, "Rash.pak"))
        return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        info.size > 0x7fffffffu) return 0;
    file = bda_fs_fopen_raw(path, "rb");
    if (!bda_fs_file_is_valid(file)) return 0;
    g_pack_file = file;
    if (!rr_pack_open(&g_resource_pack, road_pack_read_at,
                      &g_pack_file, info.size)) {
        (void)bda_fs_close_raw(file);
        g_pack_file = -1;
        return 0;
    }
    g_pack_drive = root[0];
    return 1;
}

static int road_pack_path_index(const char *path)
{
    const char *root = g_pack_drive == 'B' ?
        ROAD_RESOURCE_ROOT_B : ROAD_RESOURCE_ROOT_A;
    const char *cursor = path;
    if (!g_pack_drive || !path || path[0] != g_pack_drive) return -1;
    while (*root && *cursor == *root) {
        ++root;
        ++cursor;
    }
    return *root || *cursor++ != '\\' ? -1 :
        rr_pack_find(&g_resource_pack, cursor);
}

static int road_pack_slot(int file)
{
    unsigned int id = (unsigned int)file;
    unsigned int slot;
    if ((id & 0xffffff00u) != ROAD_PACK_HANDLE_BASE ||
        !(id & 255u)) return -1;
    slot = (id & 255u) - 1u;
    return slot < ROAD_PACK_OPEN_SLOTS && g_pack_open_files[slot].used ?
        (int)slot : -1;
}

static int road_pack_open_index(unsigned int index)
{
    unsigned int slot;
    for (slot = 0u; slot < ROAD_PACK_OPEN_SLOTS; ++slot)
        if (!g_pack_open_files[slot].used) {
            g_pack_open_files[slot].used = 1u;
            g_pack_open_files[slot].index = index;
            g_pack_open_files[slot].position = 0u;
            return (int)(ROAD_PACK_HANDLE_BASE | (slot + 1u));
        }
    return -1;
}

static int road_fs_path_info(const char *path, bda_fs_path_info_t *info)
{
    int index = road_pack_path_index(path);
    if (index >= 0) {
        bda_fs_path_info_init(info);
        info->size = g_resource_pack.entries[index].size;
        return 0;
    }
    return bda_fs_path_info(path, info);
}

static int road_fs_read_raw(int file, void *dst, bda_size_t size)
{
    int slot = road_pack_slot(file);
    if (slot >= 0) {
        road_pack_open_file_t *opened = &g_pack_open_files[slot];
        unsigned int remaining = g_resource_pack.entries[opened->index].size -
                                 opened->position;
        if (size > remaining) size = remaining;
        if (!size) return 0;
        if (!rr_pack_read(&g_resource_pack, opened->index,
                          opened->position, dst, size)) return -1;
        opened->position += size;
        return (int)size;
    }
    return bda_fs_read_raw(file, dst, size);
}

static int road_fs_seek_raw(int file, s32 offset, int whence)
{
    int slot = road_pack_slot(file);
    if (slot >= 0) {
        road_pack_open_file_t *opened = &g_pack_open_files[slot];
        unsigned int size = g_resource_pack.entries[opened->index].size;
        long long base = whence == BDA_SEEK_SET ? 0 :
            whence == BDA_SEEK_CUR ? opened->position :
            whence == BDA_SEEK_END ? size : -1;
        long long next = base + offset;
        if (base < 0 || next < 0 || next > size) return -1;
        opened->position = (unsigned int)next;
        return (int)next;
    }
    return bda_fs_seek_raw(file, offset, whence);
}

static int road_fs_close_raw(int file)
{
    int slot = road_pack_slot(file);
    if (slot >= 0) {
        g_pack_open_files[slot].used = 0u;
        return 0;
    }
    return bda_fs_close_raw(file);
}

static int road_fs_error(int file)
{
    return road_pack_slot(file) >= 0 ? 0 : bda_fs_error(file);
}

static void road_pack_shutdown(void)
{
    if (bda_fs_file_is_valid(g_pack_file))
        (void)bda_fs_close_raw(g_pack_file);
    g_pack_file = -1;
    g_pack_drive = 0;
}

#define bda_fs_path_info road_fs_path_info
#define bda_fs_read_raw road_fs_read_raw
#define bda_fs_seek_raw road_fs_seek_raw
#define bda_fs_close_raw road_fs_close_raw
#define bda_fs_error road_fs_error

static int road_resource_exists(const char *path)
{
    bda_fs_path_info_t info;
    bda_fs_path_info_init(&info);
    return bda_fs_path_info(path, &info) == 0 &&
           !bda_fs_path_info_is_dir(&info);
}

static int road_disc_path(char path[ROAD_RESOURCE_PATH_CAP],
                          const char *relative)
{
    return road_resource_join(path, ROAD_RESOURCE_PATH_CAP,
                              g_resource_root, relative);
}

static int road_disc_open(char path[ROAD_RESOURCE_PATH_CAP],
                          const char *relative)
{
    int file;
    if (!road_disc_path(path, relative)) return -1;
    {
        int index = road_pack_path_index(path);
        if (index >= 0) return road_pack_open_index((unsigned int)index);
    }
    file = bda_fs_fopen_raw(path, "rb");
    if (!bda_fs_file_is_valid(file) && g_resource_root[0] == 'B' &&
        road_resource_join(path, ROAD_RESOURCE_PATH_CAP,
                           ROAD_RESOURCE_ROOT_A, relative))
        file = bda_fs_fopen_raw(path, "rb");
    return file;
}

static void road_select_resource_root(void)
{
    if (road_pack_try_open(ROAD_RESOURCE_ROOT_B))
        g_resource_root = ROAD_RESOURCE_ROOT_B;
    else if (road_pack_try_open(ROAD_RESOURCE_ROOT_A))
        g_resource_root = ROAD_RESOURCE_ROOT_A;
    else g_resource_root = road_resource_select_root(road_resource_exists);
    (void)road_disc_path(g_diag_resource_path, "RRDEBUG.LOG");
    (void)road_disc_path(g_profile_paths[0], "RRPROF0.DAT");
    (void)road_disc_path(g_profile_paths[1], "RRPROF1.DAT");
}
static unsigned int g_diag_sequence;
static unsigned int g_diag_render_frames;
static unsigned int g_diag_race_frames;
static int g_diag_verbose;
static int g_loading_active;
static unsigned int g_loading_percent;
static const char *g_loading_stage;
static road_virtual_touch_t g_touch;
static u32 g_touch_hit_visual_until_ms;
static u32 g_touch_kick_visual_until_ms;
static u32 g_touch_attack_until_ms;
static int g_touch_escape_suppressed;
static u32 g_touch_escape_until_ms;
static int g_escape_key_seen;
static int g_escape_pending;
static u32 g_escape_due_ms;

static char *road_diag_hex(char *out, unsigned int value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        *out++ = digits[(value >> shift) & 15u];
    return out;
}

static void road_diag_checkpoint(const char *stage, unsigned int a,
                                 unsigned int b, unsigned int c)
{
    char line[96];
    char *out = line;
    int file, written, error;
    if (!g_diag_path) return;
    out = road_diag_hex(out, ++g_diag_sequence);
    *out++ = ' ';
    while (*stage && out < line + sizeof(line) - 31u)
        *out++ = *stage++;
    *out++ = ' ';
    out = road_diag_hex(out, a);
    *out++ = ' ';
    out = road_diag_hex(out, b);
    *out++ = ' ';
    out = road_diag_hex(out, c);
    *out++ = '\r';
    *out++ = '\n';
    file = bda_fs_fopen_raw(g_diag_path, "ab");
    if (!bda_fs_file_is_valid(file)) {
        g_diag_path = 0;
        return;
    }
    written = bda_fs_write_raw(file, line, (bda_size_t)(out - line));
    error = bda_fs_error(file);
    bda_fs_flush_all();
    error |= bda_fs_error(file);
    if (bda_fs_close_raw(file) != 0 ||
        written != (int)(out - line) || error)
        g_diag_path = 0;
}

static void road_diag_live(const char *stage, unsigned int a,
                           unsigned int b, unsigned int c)
{
    if (g_diag_verbose) road_diag_checkpoint(stage, a, b, c);
}

static void road_diag_begin(void)
{
    const char *const paths[] = {
        g_diag_resource_path, "A:\\RRDEBUG.LOG"
    };
    static const char header[] =
        "\r\nRoadRash 9588 diagnostic v9, new run\r\n";
    unsigned int i;
    g_diag_path = 0;
    g_diag_sequence = 0u;
    g_diag_render_frames = 0u;
    g_diag_race_frames = 0u;
    {
        char path[ROAD_RESOURCE_PATH_CAP];
        g_diag_verbose = road_disc_path(path, "RRTRACE.ON") &&
                         road_resource_exists(path);
    }
    for (i = 0u; i < 2u; ++i) {
        bda_fs_path_info_t info;
        int exists;
        int file;
        bda_fs_path_info_init(&info);
        exists = bda_fs_path_info(paths[i], &info) == 0;
        file = bda_fs_fopen_raw(paths[i],
            exists && info.size < 65536u ? "ab" : "wb");
        if (bda_fs_file_is_valid(file)) {
            int written = bda_fs_write_raw(file, header,
                (bda_size_t)(sizeof(header) - 1u));
            int error = bda_fs_error(file);
            bda_fs_flush_all();
            error |= bda_fs_error(file);
            if (bda_fs_close_raw(file) == 0 && !error &&
                written == (int)(sizeof(header) - 1u)) {
                g_diag_path = paths[i];
                break;
            }
        }
    }
    road_diag_checkpoint("BOOT", 0u, 0u, 0u);
    road_diag_checkpoint("RESOURCE_ROOT", g_resource_root[0] == 'B',
                         (unsigned int)g_resource_root[0], 0u);
    road_diag_checkpoint("PACK_OPEN", g_pack_drive,
        g_resource_pack.count, g_resource_pack.file_size);
}

static void release_runtime_buffers(void)
{
    if (g_static_buffers) bda_free(g_static_buffers);
    if (g_art_buffers) bda_free(g_art_buffers);
    if (g_terrain_buffers) bda_free(g_terrain_buffers);
    if (g_course_buffers) bda_free(g_course_buffers);
    if (g_display_buffers) bda_free(g_display_buffers);
    g_static_buffers = 0;
    g_art_buffers = 0;
    g_terrain_buffers = 0;
    g_course_buffers = 0;
    g_display_buffers = 0;
}

static int allocate_runtime_buffers(void)
{
    g_display_buffers = (road_display_buffers_t *)bda_alloc(
        sizeof(*g_display_buffers));
    if (!g_display_buffers) goto fail;
    g_course_buffers = (road_course_buffers_t *)bda_alloc(
        sizeof(*g_course_buffers));
    if (!g_course_buffers) goto fail;
    g_terrain_buffers = (road_terrain_buffers_t *)bda_alloc(
        sizeof(*g_terrain_buffers));
    if (!g_terrain_buffers) goto fail;
    g_art_buffers = (road_art_buffers_t *)bda_alloc(
        sizeof(*g_art_buffers));
    if (!g_art_buffers) goto fail;
    g_static_buffers = (road_static_buffers_t *)bda_alloc(
        sizeof(*g_static_buffers));
    if (!g_static_buffers) goto fail;
    bda_memset(g_display_buffers, 0, sizeof(*g_display_buffers));
    bda_memset(g_course_buffers, 0, sizeof(*g_course_buffers));
    bda_memset(g_terrain_buffers, 0, sizeof(*g_terrain_buffers));
    bda_memset(g_art_buffers, 0, sizeof(*g_art_buffers));
    bda_memset(g_static_buffers, 0, sizeof(*g_static_buffers));
    road_diag_checkpoint("HEAP_READY",
        (unsigned int)sizeof(*g_display_buffers) +
        (unsigned int)sizeof(*g_course_buffers) +
        (unsigned int)sizeof(*g_terrain_buffers) +
        (unsigned int)sizeof(*g_art_buffers) +
        (unsigned int)sizeof(*g_static_buffers),
        (unsigned int)g_display_buffers,
        (unsigned int)g_static_buffers);
    return 1;
fail:
    road_diag_checkpoint("HEAP_FAIL",
        (unsigned int)(g_display_buffers != 0) |
        ((unsigned int)(g_course_buffers != 0) << 1) |
        ((unsigned int)(g_terrain_buffers != 0) << 2) |
        ((unsigned int)(g_art_buffers != 0) << 3) |
        ((unsigned int)(g_static_buffers != 0) << 4),
        (unsigned int)sizeof(*g_art_buffers),
        (unsigned int)sizeof(*g_static_buffers));
    release_runtime_buffers();
    return 0;
}

static void road_diag_render_trace(const char *stage, unsigned int a,
                                   unsigned int b, unsigned int c)
{
    if (g_diag_render_frames) road_diag_checkpoint(stage, a, b, c);
}

static int load_profile_path(const char *path, road_profile_t *profile)
{
    unsigned char bytes[ROAD_PROFILE_BYTES];
    bda_fs_path_info_t info;
    int file;
    int valid;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        (info.size != ROAD_PROFILE_BYTES &&
         info.size != ROAD_PROFILE_V2_BYTES &&
         info.size != ROAD_PROFILE_V1_BYTES)) return 0;
    file = bda_fs_fopen_raw(path, "rb");
    if (!bda_fs_file_is_valid(file)) return 0;
    valid = bda_fs_read_raw(file, bytes, info.size) ==
            (int)info.size && !bda_fs_error(file);
    (void)bda_fs_close_raw(file);
    return valid && road_profile_decode(profile, bytes, info.size);
}

static int load_profile_slot(unsigned int slot, road_profile_t *profile)
{
    return load_profile_path(g_profile_paths[slot], profile);
}

static void load_profile(void)
{
    static const char *const legacy_paths[2] = {
        "A:\\Rash\\RRPROF0.DAT", "A:\\Rash\\RRPROF1.DAT"
    };
    road_profile_t candidate;
    unsigned int slot;
    road_profile_init(&g_profile);
    g_profile_slot = -1;
    for (slot = 0u; slot < 2u; ++slot) {
        if (load_profile_slot(slot, &candidate) &&
            (g_profile_slot < 0 || candidate.sequence > g_profile.sequence)) {
            bda_memcpy(&g_profile, &candidate, sizeof(g_profile));
            g_profile_slot = (int)slot;
        }
    }
    if (g_profile_slot < 0) {
        for (slot = 0u; slot < 2u; ++slot) {
            if (load_profile_path(legacy_paths[slot], &candidate) &&
                (g_profile_slot < 0 ||
                 candidate.sequence > g_profile.sequence)) {
                bda_memcpy(&g_profile, &candidate, sizeof(g_profile));
                g_profile_slot = (int)slot;
            }
        }
        /* The next save writes a copy into the new selected directory. */
        g_profile_slot = -1;
    }
}

static int save_profile(void)
{
    road_profile_t candidate;
    unsigned char bytes[ROAD_PROFILE_BYTES];
    int slot = g_profile_slot == 0 ? 1 : 0;
    int file;
    int written;
    int error;
    bda_memcpy(&candidate, &g_profile, sizeof(candidate));
    candidate.sequence++;
    if (!road_profile_encode(&candidate, bytes)) return 0;
    file = bda_fs_fopen_raw(g_profile_paths[slot], "wb");
    if (!bda_fs_file_is_valid(file)) return 0;
    written = bda_fs_write_raw(file, bytes, ROAD_PROFILE_BYTES);
    error = bda_fs_error(file);
    bda_fs_flush_all();
    error |= bda_fs_error(file);
    if (bda_fs_close_raw(file) != 0) error = 1;
    if (written != (int)ROAD_PROFILE_BYTES || error ||
        !load_profile_slot((unsigned int)slot, &candidate)) return 0;
    g_profile.sequence = candidate.sequence;
    g_profile_slot = slot;
    return 1;
}

static void capture_race_result(void)
{
    unsigned int place = road_game_finish_place(&g_game);
    g_last_award = road_profile_finish(&g_profile, g_frontend.course,
                                       g_game.elapsed_ms, place);
    g_profile_save_failed = !save_profile();
}

static int course_read_at(void *user, rr_u32 offset, void *dst,
                          rr_u32 size)
{
    int file = *(int *)user;
    if (offset > 0x7fffffffu ||
        bda_fs_seek_raw(file, (s32)offset, BDA_SEEK_SET) != (int)offset)
        return 0;
    return bda_fs_read_raw(file, dst, size) == (int)size;
}

static int load_disc_reciprocal_table(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    unsigned char *raw = (unsigned char *)g_runtime_reciprocal_table;
    unsigned int i;
    int file = road_disc_open(path, "HalfOneOver.table");
    int ok = 0;
    road_game_set_reciprocal_table(0, 0u);
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        info.size != sizeof(g_runtime_reciprocal_table) ||
        bda_fs_read_raw(file, raw, info.size) != (int)info.size)
        goto done;
    for (i = 0u; i < 0x1100u; ++i) {
        unsigned int at = i * 4u;
        g_runtime_reciprocal_table[i] =
            ((unsigned int)raw[at] << 24u) |
            ((unsigned int)raw[at + 1u] << 16u) |
            ((unsigned int)raw[at + 2u] << 8u) |
            raw[at + 3u];
    }
    if (g_runtime_reciprocal_table[128u] != 0x28000u ||
        g_runtime_reciprocal_table[256u] != 0x8000u ||
        g_runtime_reciprocal_table[462u] != 0x2ffdu)
        goto done;
    road_game_set_reciprocal_table(g_runtime_reciprocal_table, 0x1100u);
    ok = 1;
done:
    (void)bda_fs_close_raw(file);
    return ok;
}

static void release_disc_audio(void)
{
    unsigned int i;
    if (g_audio_open) {
        unsigned int polls = 0u;
        while (!bda_audio_ready() && polls < 128u) {
            bda_sys_delay(1u);
            ++polls;
        }
        if (polls < 128u) {
            int written;
            bda_memset(g_audio_pcm, 0, sizeof(g_audio_pcm));
            bda_audio_set_attenuation((u32)g_audio_saved_attenuation);
            written = bda_audio_write(g_audio_pcm, sizeof(g_audio_pcm));
            road_diag_checkpoint("AUDIO_RESTORE", (unsigned int)written,
                (unsigned int)g_audio_saved_attenuation, polls);
        } else road_diag_checkpoint("AUDIO_RESTORE_WAIT", polls, 0u, 0u);
        bda_audio_stop();
        g_audio_open = 0;
    }
    for (i = 0u; i < 19u; ++i) {
        if (g_audio_samples[i]) bda_free(g_audio_samples[i]);
        g_audio_samples[i] = 0;
    }
    if (g_music_ready) {
        (void)bda_fs_close_raw(g_music_file);
        g_music_ready = 0;
    }
}

#define ROAD_MUSIC_COUNT 1u

static int load_disc_music(unsigned int index)
{
    static const char *const paths[ROAD_MUSIC_COUNT] = {
        "Streams\\bgaudio\\SG.RustyCage_sw22.stream",
    };
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    int file;
    if (index >= ROAD_MUSIC_COUNT) return 0;
    file = road_disc_open(path, paths[index]);
    if (!bda_fs_file_is_valid(file)) return 0;
    g_music_source_drive = (unsigned int)(unsigned char)path[0];
    g_music_file = file;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_music_open(&g_music_stream, course_read_at, &g_music_file,
                       info.size)) {
        (void)bda_fs_close_raw(file);
        return 0;
    }
    g_music_index = index;
    g_music_ready = 1;
    return 1;
}

static int choose_disc_music(unsigned int previous)
{
    unsigned int attempt;
    unsigned int first = rr_music_choose_next(&g_music_random_state,
        previous, ROAD_MUSIC_COUNT);
    for (attempt = 0u; attempt < ROAD_MUSIC_COUNT; ++attempt) {
        unsigned int index = (first + attempt) % ROAD_MUSIC_COUNT;
        if (index != previous && load_disc_music(index)) return 1;
    }
    return previous < ROAD_MUSIC_COUNT && load_disc_music(previous);
}

static int advance_disc_music(void)
{
    unsigned int previous = g_music_index;
    if (g_music_ready) {
        (void)bda_fs_close_raw(g_music_file);
        g_music_ready = 0;
    }
    return choose_disc_music(previous);
}

static int load_disc_audio(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    unsigned int i;
    int file = road_disc_open(path, "Rash.AIFF");
    if (!bda_fs_file_is_valid(file)) return 0;
    g_sfx_source_drive = (unsigned int)(unsigned char)path[0];
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto fail;
    for (i = 0u; i < 19u; ++i) {
        rr_aiff_sample_t sample;
        short *pcm;
        if (!rr_aiff_find_sample(&catalog, i + 1u, &sample)) goto fail;
        pcm = (short *)bda_alloc(sample.pcm_bytes);
        if (!pcm || (u32)pcm == 0xffffffffu) goto fail;
        g_audio_samples[i] = pcm;
        if (!rr_aiff_read_frames(&catalog, &sample, 0u, pcm,
                                 sample.frame_count)) goto fail;
        rr_audio_set_voice(&g_audio_mixer, i, pcm,
                            sample.frame_count, sample.source_rate);
    }
    (void)bda_fs_close_raw(file);
    return 1;
fail:
    (void)bda_fs_close_raw(file);
    release_disc_audio();
    return 0;
}

static void service_audio(void)
{
    unsigned int attempt;
    if (!g_audio_open) return;
    /* Fill the firmware queue while it has room. One block is only 23 ms;
     * a full video frame can take longer than that on the device. */
    for (attempt = 0u; attempt < 4u; ++attempt) {
        if (!bda_audio_ready()) {
            if (++g_audio_ready_misses == 512u)
                road_diag_checkpoint("AUDIO_WAIT", g_audio_ready_misses,
                    g_audio_written_blocks, g_audio_pending_bytes);
            return;
        }
        g_audio_ready_misses = 0u;
        if (!g_audio_pending_bytes) {
            unsigned int i;
            rr_audio_render(&g_audio_mixer, g_audio_pcm, 512u);
            if (g_music_ready && g_frontend.music_enabled) {
                rr_u32 count = rr_music_read_mono(&g_music_stream,
                                                    g_music_pcm, 512u);
                if (count != 512u) {
                    if (advance_disc_music())
                        count += rr_music_read_mono(&g_music_stream,
                                                     g_music_pcm + count,
                                                     512u - count);
                }
                for (i = 0u; i < count; ++i) {
                    int mixed = (int)g_audio_pcm[i] + (int)g_music_pcm[i] / 2;
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    g_audio_pcm[i] = (short)mixed;
                }
            }
            g_audio_pcm_nonzero = 0u;
            for (i = 0u; i < 512u; ++i)
                if (g_audio_pcm[i]) ++g_audio_pcm_nonzero;
            g_audio_pending_bytes = sizeof(g_audio_pcm);
            g_audio_pending_offset = 0u;
        }
        {
            unsigned int remaining = g_audio_pending_bytes -
                                     g_audio_pending_offset;
            int written = bda_audio_write(
                (const unsigned char *)g_audio_pcm + g_audio_pending_offset,
                remaining);
            if (written < 0 || (unsigned int)written > remaining) {
                road_diag_checkpoint("AUDIO_WRITE_FAIL", (unsigned int)written,
                    g_audio_written_blocks, remaining);
                bda_audio_stop();
                g_audio_open = 0;
                return;
            }
            if (!written) {
                if (++g_audio_zero_writes == 64u)
                    road_diag_checkpoint("AUDIO_WRITE_ZERO",
                        g_audio_zero_writes, g_audio_written_blocks, remaining);
                return;
            }
            g_audio_zero_writes = 0u;
            g_audio_pending_offset += (unsigned int)written;
            if (g_audio_pending_offset == g_audio_pending_bytes) {
                ++g_audio_written_blocks;
                if (g_audio_written_blocks == 1u ||
                    g_audio_written_blocks == 64u)
                    road_diag_live("AUDIO_WRITE", g_audio_written_blocks,
                        g_audio_pcm_nonzero, g_music_ready);
                g_audio_pending_bytes = 0u;
                g_audio_pending_offset = 0u;
            }
        }
    }
}

static int load_disc_course(unsigned int index, unsigned int branch_mask)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_course_buffer_t course;
    int file;
    int ok = 0;
    if (index >= RR_COURSE_COUNT) return 0;
    g_runtime_graph_valid = 0;
    bda_memset(&course, 0, sizeof(course));
    file = road_disc_open(path, rr_course_paths[index]);
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    course.curvature = g_runtime_curvature;
    course.elevation = g_runtime_elevation;
    course.left_width = g_runtime_left_width;
    course.right_width = g_runtime_right_width;
    course.left_margin = g_runtime_left_margin;
    course.right_margin = g_runtime_right_margin;
    course.left_edge_resource = g_runtime_left_edge_resource;
    course.right_edge_resource = g_runtime_right_edge_resource;
    course.left_edge_family = g_runtime_left_edge_family;
    course.right_edge_family = g_runtime_right_edge_family;
    course.left_depth = g_runtime_left_depth;
    course.right_depth = g_runtime_right_depth;
    course.terrain_mode = g_runtime_terrain_mode;
    course.surface_flags = g_runtime_surface_flags;
    course.surface_samples = g_runtime_surface_samples;
    course.hill_profiles = g_runtime_hill_profiles;
    course.hill_samples = g_runtime_hill_samples;
    course.hill_profile_capacity = 512u;
    course.edge_profiles[0] = g_runtime_edge_profiles[0];
    course.edge_profiles[1] = g_runtime_edge_profiles[1];
    course.edge_outer_samples[0] = g_runtime_edge_outer_samples[0];
    course.edge_outer_samples[1] = g_runtime_edge_outer_samples[1];
    course.edge_profile_capacity[0] = 256u;
    course.edge_profile_capacity[1] = 256u;
    course.objects = g_runtime_objects;
    course.object_capacity = 8000u;
    course.object_count = 0u;
    course.hazards = g_runtime_hazards;
    course.hazard_capacity = 1000u;
    course.hazard_count = 0u;
    course.effects = g_runtime_effects;
    course.effect_capacity = 1000u;
    course.effect_count = 0u;
    course.crossing_zones = g_runtime_crossing_zones;
    course.crossing_capacity = 256u;
    course.crossing_count = 0u;
    course.difficulty_level = g_profile.career.level;
    course.family_ids = g_runtime_family_ids;
    course.family_capacity = 32u;
    course.family_count = 0u;
    course.hazard_family_ids = g_runtime_hazard_family_ids;
    course.hazard_frame_ids = g_runtime_hazard_frame_ids;
    course.hazard_family_capacity = 64u;
    course.hazard_family_count = 0u;
    course.capacity = sizeof(g_runtime_curvature);
    course.sample_count = 0u;
    course.segment_count = 0u;
    if (bda_fs_path_info(path, &info) == 0 &&
        rr_rsrc_open(&catalog, course_read_at, &file, info.size) &&
        rr_course_load_selected(&catalog, &course, branch_mask)) {
        road_diag_checkpoint("TEX_PARSED", g_surface_texture_count,
            (unsigned int)g_surface_textures, 0u);
        if (rr_course_graph_load(&catalog, course.difficulty_level,
                                 &g_runtime_course_graph) &&
            rr_course_graph_route_finish(&g_runtime_course_graph,
                                          branch_mask) == course.finish_sample) {
        rr_u32 width = 0u, height = 0u;
        unsigned int branch;
        road_diag_checkpoint("TEX_GRAPH", g_surface_texture_count,
            (unsigned int)g_surface_textures, 0u);
        road_game_set_course(course.curvature, course.sample_count);
        road_game_set_finish_sample(course.finish_sample);
        road_game_set_elevation(course.elevation, course.sample_count);
        road_game_set_widths(course.left_width, course.right_width,
                             course.sample_count);
        road_game_set_cross_section(course.left_margin,
            course.right_margin, course.left_depth,
            course.right_depth, course.terrain_mode,
            course.sample_count);
        road_game_set_hill_profiles(course.hill_profiles,
                                     course.hill_profile_count);
        road_game_set_hill_samples(course.hill_samples,
                                   course.sample_count);
        road_game_set_edge_profiles(course.edge_profiles[0],
            course.edge_profile_count[0], course.edge_profiles[1],
            course.edge_profile_count[1]);
        road_game_set_edge_outer_samples(course.edge_outer_samples[0],
            course.edge_outer_samples[1], course.sample_count);
        road_game_set_surface_samples(course.surface_samples,
                                       course.sample_count);
        road_game_set_margin_resources(0u,
            course.left_edge_resource, course.right_edge_resource,
            course.left_edge_family, course.right_edge_family,
            course.surface_flags, course.terrain_mode,
            course.sample_count);
        g_runtime_sample_count = course.sample_count;
        g_runtime_hill_profile_count = course.hill_profile_count;
        g_runtime_object_count = course.object_count;
        g_runtime_hazard_count = course.hazard_count;
        g_runtime_effect_count = course.effect_count;
        g_runtime_crossing_count = course.crossing_count;
        g_runtime_family_count = course.family_count;
        g_runtime_hazard_pair_count = course.hazard_family_count;
        g_runtime_branch_count = course.branch_count;
        for (branch = 0u; branch < course.branch_count; ++branch) {
            g_runtime_branch_samples[branch] = course.branch_samples[branch];
            g_runtime_branch_main_channels[branch] =
                course.branch_main_channels[branch];
        }
        g_runtime_placements_loaded = 1;
        road_diag_checkpoint("TEX_SETUP", g_surface_texture_count,
            (unsigned int)g_surface_textures, 0u);
        {
            rr_cel_image_t sky;
            sky.pixels = g_runtime_sky_pixels;
            if (rr_cel_load_road_surface(&catalog, 1u, &sky,
                                         80u * 145u)) {
                g_runtime_sky.width = sky.width;
                g_runtime_sky.height = sky.height;
                g_runtime_sky.pixels = g_runtime_sky_pixels;
                road_game_set_sky(&g_runtime_sky);
            }
        }
        road_diag_checkpoint("TEX_SKY", g_surface_texture_count,
            (unsigned int)g_surface_textures, 0u);
        if (rr_cel_load_backdrop(&catalog, 2u,
                                 g_runtime_backdrop_indices,
                                 sizeof(g_runtime_backdrop_indices),
                                 g_runtime_backdrop_palette,
                                 &width, &height)) {
            road_game_set_backdrop(g_runtime_backdrop_indices,
                                   g_runtime_backdrop_palette,
                                   width, height);
            g_runtime_backdrop_loaded = 1;
        }
        road_diag_checkpoint("TEX_BACKDROP", g_surface_texture_count,
            (unsigned int)g_surface_textures, 0u);
        ok = 1;
        g_runtime_graph_valid = 1;
        }
    }
    (void)bda_fs_close_raw(file);
    return ok;
}

static void load_disc_fork_terrain_tiles(unsigned int sample_count);
static int load_fork_preview(unsigned int index, unsigned int branch_mask,
                              unsigned int branch)
{
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_course_buffer_t course;
    char path[ROAD_RESOURCE_PATH_CAP];
    int file;
    int loaded = 0;
    rr_u32 chosen_span, other_span;
    road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
    road_game_set_fork_surface_samples(0, 0u);
    road_game_set_fork_hill_profiles(0, 0u);
    road_game_set_fork_hill_samples(0, 0u);
    road_game_set_margin_resources(1u, 0, 0, 0, 0, 0, 0, 0u);
    if (index >= RR_COURSE_COUNT || branch >= g_runtime_branch_count)
        return 0;
    file = road_disc_open(path, rr_course_paths[index]);
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_memset(&course, 0, sizeof(course));
    bda_fs_path_info_init(&info);
    course.curvature = g_runtime_fork_curvature;
    course.elevation = g_runtime_fork_elevation;
    course.left_width = g_runtime_fork_left_width;
    course.right_width = g_runtime_fork_right_width;
    course.left_margin = g_runtime_fork_left_margin;
    course.right_margin = g_runtime_fork_right_margin;
    course.left_depth = g_runtime_fork_left_depth;
    course.right_depth = g_runtime_fork_right_depth;
    course.left_edge_resource = g_runtime_fork_left_edge_resource;
    course.right_edge_resource = g_runtime_fork_right_edge_resource;
    course.left_edge_family = g_runtime_fork_left_edge_family;
    course.right_edge_family = g_runtime_fork_right_edge_family;
    course.terrain_mode = g_runtime_fork_terrain_mode;
    course.surface_flags = g_runtime_fork_surface_flags;
    course.surface_samples = g_runtime_fork_surface_samples;
    course.hill_profiles = g_runtime_fork_hill_profiles;
    course.hill_samples = g_runtime_fork_hill_samples;
    course.hill_profile_capacity = 512u;
    course.capacity = sizeof(g_runtime_fork_curvature);
    if (bda_fs_path_info(path, &info) == 0 &&
        rr_rsrc_open(&catalog, course_read_at, &file, info.size) &&
        rr_course_load_selected(&catalog, &course,
            branch_mask ^ (1u << branch)) &&
        g_runtime_graph_valid &&
        rr_course_graph_branch_span(&g_runtime_course_graph,
            branch_mask, branch, &chosen_span, &other_span)) {
        unsigned int sample = g_runtime_branch_samples[branch];
        unsigned int selected_channel =
            g_runtime_branch_main_channels[branch] ^
            ((branch_mask >> branch) & 1u);
        int selected_is_source = selected_channel ==
            g_runtime_branch_main_channels[branch];
        if (!rr_course_reconcile_fork_elevation(
                g_runtime_elevation, g_runtime_sample_count,
                course.elevation, course.sample_count,
                sample, chosen_span, other_span,
                selected_is_source)) goto done;
        if (!selected_is_source) {
            road_game_set_elevation(g_runtime_elevation,
                                    g_runtime_sample_count);
            road_game_seek(&g_game, g_game.distance);
        }
        if (sample && sample < course.sample_count &&
            sample < sizeof(g_runtime_curvature)) {
            g_runtime_branch_old_left[branch] =
                g_runtime_left_width[sample - 1u];
            g_runtime_branch_first_left[branch] =
                g_runtime_branch_main_channels[branch] == 0u ?
                g_runtime_left_width[sample] : course.left_width[sample];
            g_runtime_branch_first_right[branch] =
                g_runtime_branch_main_channels[branch] == 0u ?
                g_runtime_right_width[sample] : course.right_width[sample];
        }
        road_game_set_fork_preview(course.curvature, course.elevation,
            course.left_width, course.right_width, course.sample_count,
            g_runtime_branch_samples[branch],
            selected_channel);
        road_game_set_fork_slopes(course.left_margin, course.right_margin,
            course.sample_count);
        road_game_set_fork_depths(course.left_depth, course.right_depth,
            course.sample_count);
        road_game_set_margin_resources(1u,
            course.left_edge_resource, course.right_edge_resource,
            course.left_edge_family, course.right_edge_family,
            course.surface_flags, course.terrain_mode,
            course.sample_count);
        road_game_set_fork_surface_samples(course.surface_samples,
            course.sample_count);
        g_runtime_fork_hill_profile_count = course.hill_profile_count;
        road_game_set_fork_hill_profiles(course.hill_profiles,
            course.hill_profile_count);
        road_game_set_fork_hill_samples(course.hill_samples,
            course.sample_count);
        load_disc_fork_terrain_tiles(course.sample_count);
        loaded = 1;
    }
done:
    (void)bda_fs_close_raw(file);
    return loaded;
}

static int load_disc_scenery(void);
static void load_disc_terrain_tiles(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    road_pixel_t *temporary;
    rr_u8 *scratch;
    unsigned int count;
    int file;
    if (!g_runtime_sample_count) return;
    file = road_disc_open(path, "Families.RSRC");
    if (!bda_fs_file_is_valid(file)) return;
    bda_fs_path_info_init(&info);
    temporary = (road_pixel_t *)bda_alloc(16384u * 2u);
    scratch = (rr_u8 *)bda_alloc(32768u);
    if (temporary && scratch &&
        bda_fs_path_info(path, &info) == 0 &&
        rr_rsrc_open(&catalog, course_read_at, &file, info.size)) {
        count = road_terrain_load_hill_tiles(&catalog,
            g_runtime_hill_profiles, g_runtime_hill_profile_count,
            g_runtime_hill_tiles, g_runtime_hill_mips,
            g_runtime_hill_narrow_mips,
            &g_runtime_hill_tile_pixels[0][0],
            &g_runtime_hill_mip_pixels[0][0],
            &g_runtime_hill_narrow_pixels[0][0],
            g_runtime_hill_family_keys,
            g_runtime_hill_selector_keys,
            g_runtime_hill_band_keys,
            0u, ROAD_HILL_TILE_LIMIT, temporary, 16384u,
            scratch, 32768u);
        g_runtime_hill_tile_count = count;
        g_runtime_hill_main_tile_count = count;
        road_diag_checkpoint("TEX_HILL", g_surface_texture_count,
            (unsigned int)g_surface_textures, count);
        road_game_set_hill_tiles(g_runtime_hill_tiles,
            g_runtime_hill_mips, g_runtime_hill_narrow_mips, count);
        count = road_terrain_load_roadside_tiles(&catalog,
            g_runtime_surface_samples, g_runtime_terrain_mode,
            g_runtime_sample_count,
            g_runtime_roadside_tiles,
            g_runtime_roadside_tile_heights,
            g_runtime_roadside_repeat_indices,
            g_runtime_roadside_repeat_mips,
            &g_runtime_roadside_tile_pixels[0][0],
            g_runtime_roadside_family_keys,
            g_runtime_roadside_selector_keys,
            g_runtime_roadside_child_keys,
            0u, ROAD_ROADSIDE_TILE_LIMIT, temporary, 16384u,
            scratch, 32768u);
        g_runtime_roadside_tile_count = count;
        road_diag_checkpoint("TEX_SIDE", g_surface_texture_count,
            (unsigned int)g_surface_textures, count);
        road_game_set_roadside_tiles(g_runtime_roadside_tiles,
            g_runtime_roadside_tile_heights,
            g_runtime_roadside_repeat_indices,
            g_runtime_roadside_repeat_mips, count);
        g_runtime_margin_tile_count = road_terrain_load_margin_tiles(
            &catalog, g_runtime_left_edge_resource,
            g_runtime_right_edge_resource,
            g_runtime_left_edge_family, g_runtime_right_edge_family,
            g_runtime_sample_count, g_runtime_margin_tiles,
            g_runtime_margin_pixel_pool,
            &g_runtime_margin_pixel_used,
            ROAD_MARGIN_PIXEL_POOL_PIXELS,
            g_runtime_margin_tile_count, ROAD_MARGIN_TILE_LIMIT,
            temporary, 16384u, scratch, 32768u);
        if (g_runtime_margin_pixel_used >
            ROAD_MARGIN_PIXEL_POOL_PIXELS)
            g_runtime_margin_tile_count = 0u;
        road_diag_checkpoint("TEX_MARGIN", g_surface_texture_count,
            (unsigned int)g_surface_textures, g_runtime_margin_tile_count);
        road_game_set_margin_tiles(g_runtime_margin_tiles,
            g_runtime_margin_tile_count);
    }
    if (scratch) bda_free(scratch);
    if (temporary) bda_free(temporary);
    (void)bda_fs_close_raw(file);
}
static void load_disc_fork_terrain_tiles(unsigned int sample_count)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    road_pixel_t *temporary;
    rr_u8 *scratch;
    int file;
    if (!sample_count) return;
    file = road_disc_open(path, "Families.RSRC");
    if (!bda_fs_file_is_valid(file)) return;
    bda_fs_path_info_init(&info);
    temporary = (road_pixel_t *)bda_alloc(16384u * 2u);
    scratch = (rr_u8 *)bda_alloc(32768u);
    if (temporary && scratch &&
        bda_fs_path_info(path, &info) == 0 &&
        rr_rsrc_open(&catalog, course_read_at, &file, info.size)) {
        g_runtime_hill_tile_count = road_terrain_load_hill_tiles(
            &catalog, g_runtime_fork_hill_profiles,
            g_runtime_fork_hill_profile_count,
            g_runtime_hill_tiles, g_runtime_hill_mips,
            g_runtime_hill_narrow_mips,
            &g_runtime_hill_tile_pixels[0][0],
            &g_runtime_hill_mip_pixels[0][0],
            &g_runtime_hill_narrow_pixels[0][0],
            g_runtime_hill_family_keys,
            g_runtime_hill_selector_keys,
            g_runtime_hill_band_keys,
            g_runtime_hill_main_tile_count, ROAD_HILL_TILE_LIMIT,
            temporary, 16384u, scratch, 32768u);
        road_game_set_hill_tiles(g_runtime_hill_tiles,
            g_runtime_hill_mips, g_runtime_hill_narrow_mips,
            g_runtime_hill_tile_count);
        g_runtime_roadside_tile_count =
            road_terrain_load_roadside_tiles(&catalog,
                g_runtime_fork_surface_samples,
                g_runtime_fork_terrain_mode, sample_count,
                g_runtime_roadside_tiles,
                g_runtime_roadside_tile_heights,
                g_runtime_roadside_repeat_indices,
                g_runtime_roadside_repeat_mips,
                &g_runtime_roadside_tile_pixels[0][0],
                g_runtime_roadside_family_keys,
                g_runtime_roadside_selector_keys,
                g_runtime_roadside_child_keys,
                g_runtime_roadside_tile_count,
                ROAD_ROADSIDE_TILE_LIMIT, temporary, 16384u,
                scratch, 32768u);
        road_game_set_roadside_tiles(g_runtime_roadside_tiles,
            g_runtime_roadside_tile_heights,
            g_runtime_roadside_repeat_indices,
            g_runtime_roadside_repeat_mips,
            g_runtime_roadside_tile_count);
        g_runtime_margin_tile_count = road_terrain_load_margin_tiles(
            &catalog, g_runtime_fork_left_edge_resource,
            g_runtime_fork_right_edge_resource,
            g_runtime_fork_left_edge_family,
            g_runtime_fork_right_edge_family,
            sample_count, g_runtime_margin_tiles,
            g_runtime_margin_pixel_pool,
            &g_runtime_margin_pixel_used,
            ROAD_MARGIN_PIXEL_POOL_PIXELS,
            g_runtime_margin_tile_count, ROAD_MARGIN_TILE_LIMIT,
            temporary, 16384u, scratch, 32768u);
        if (g_runtime_margin_pixel_used >
            ROAD_MARGIN_PIXEL_POOL_PIXELS)
            g_runtime_margin_tile_count = 0u;
        road_game_set_margin_tiles(g_runtime_margin_tiles,
            g_runtime_margin_tile_count);
    }
    if (scratch) bda_free(scratch);
    if (temporary) bda_free(temporary);
    (void)bda_fs_close_raw(file);
}
static int load_disc_static(void);
static void release_disc_static(void);
static int load_disc_effect_sprites(void);
static void release_disc_effect_sprites(void);
static int load_disc_cars(unsigned int index);
static int draw_frame(void);
static void loading_progress(unsigned int percent, const char *stage);

static int bind_course_assets(unsigned int index, unsigned int branch_mask)
{
    unsigned int crossing_index;
    road_diag_checkpoint("BIND_BEGIN", index, branch_mask,
        g_game.distance);
    g_runtime_placements_loaded = 0;
    g_runtime_backdrop_loaded = 0;
    g_runtime_static_loaded = 0;
    g_runtime_object_count = 0u;
    g_runtime_hazard_count = 0u;
    g_runtime_effect_count = 0u;
    g_runtime_crossing_count = 0u;
    g_runtime_family_count = 0u;
    g_runtime_hazard_pair_count = 0u;
    g_runtime_branch_count = 0u;
    g_runtime_hill_profile_count = 0u;
    g_runtime_fork_hill_profile_count = 0u;
    g_runtime_hill_tile_count = 0u;
    g_runtime_hill_main_tile_count = 0u;
    g_runtime_sample_count = 0u;
    road_game_set_course(0, 0u);
    road_game_set_elevation(0, 0u);
    road_game_set_widths(0, 0, 0u);
    road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
    road_game_set_cross_section(0, 0, 0, 0, 0, 0u);
    road_game_set_hill_profiles(0, 0u);
    road_game_set_hill_samples(0, 0u);
    road_game_set_fork_hill_profiles(0, 0u);
    road_game_set_fork_hill_samples(0, 0u);
    road_game_set_edge_profiles(0, 0u, 0, 0u);
    road_game_set_edge_outer_samples(0, 0, 0u);
    road_game_set_hill_tiles(0, 0, 0, 0u);
    road_game_set_surface_samples(0, 0u);
    road_game_set_fork_surface_samples(0, 0u);
    g_runtime_roadside_tile_count = 0u;
    road_game_set_margin_resources(0u, 0, 0, 0, 0, 0, 0, 0u);
    road_game_set_margin_resources(1u, 0, 0, 0, 0, 0, 0, 0u);
    road_game_set_margin_tiles(0, 0u);
    g_runtime_margin_tile_count = 0u;
    g_runtime_margin_pixel_used = 0u;
    road_game_set_roadside_tiles(0, 0, 0, 0, 0u);
    road_game_set_backdrop(0, 0, 0u, 0u);
    road_game_set_sky(0);
    road_game_set_scenery(0, 0u);
    road_game_set_scenery_placements(0, 0u);
    road_game_set_static_scenery(0, 0u, 0, 0u);
    release_disc_static();
    road_game_set_effects(&g_game, 0, 0u);
    road_game_set_crossing_zones(0, 0u);
    for (crossing_index = 0u; crossing_index < ROAD_CROSSING_COUNT;
         ++crossing_index)
        g_game.crossing[crossing_index].distance = 0u;
    for (crossing_index = 0u; crossing_index < ROAD_CROSSING_PARENT_COUNT;
         ++crossing_index)
        g_game.crossing_parents[crossing_index].active = 0u;
    release_disc_effect_sprites();
    road_diag_checkpoint("COURSE_BEGIN", index, branch_mask, 0u);
    if (!load_disc_course(index, branch_mask)) {
        road_diag_checkpoint("COURSE_FAIL", index, branch_mask, 0u);
#if ROAD_COURSE_SAMPLE_COUNT > 0
        if (index == 0u && branch_mask == 0u) {
            road_game_set_course(g_original_course_curvature,
                                 ROAD_COURSE_SAMPLE_COUNT);
            road_game_set_elevation(g_original_course_elevation,
                                    ROAD_COURSE_SAMPLE_COUNT);
        } else
#endif
        return 0;
    }
    road_diag_checkpoint("COURSE_DONE", g_runtime_sample_count,
        g_runtime_branch_count, g_runtime_hill_profile_count);
    loading_progress(22u, "ROAD DATA");
    road_diag_checkpoint("TEX_COURSE", g_surface_texture_count,
        (unsigned int)g_surface_textures, 0u);
    road_diag_checkpoint("TILES_BEGIN", index, branch_mask, 0u);
    load_disc_terrain_tiles();
    road_diag_checkpoint("TILES_DONE", g_runtime_hill_tile_count,
        g_runtime_roadside_tile_count, g_runtime_margin_tile_count);
    loading_progress(42u, "ROAD TILES");
    road_diag_checkpoint("TEX_TILES", g_surface_texture_count,
        (unsigned int)g_surface_textures, 0u);
    if (!g_runtime_backdrop_loaded) {
#if ROAD_BACKDROP_WIDTH > 0
        if (index == 0u)
            road_game_set_backdrop(g_original_backdrop_indices,
                                   g_original_backdrop_palette,
                                   ROAD_BACKDROP_WIDTH, ROAD_BACKDROP_HEIGHT);
#endif
    }
    road_diag_checkpoint("SCENERY_BEGIN", index, 0u, 0u);
    if (!load_disc_scenery()) {
#if ROAD_SCENERY_COUNT > 0
        if (index == 0u)
            road_game_set_scenery(g_original_scenery, ROAD_SCENERY_COUNT);
#endif
    }
    road_diag_checkpoint("SCENERY_DONE", g_runtime_object_count,
        g_runtime_family_count, g_runtime_placements_loaded);
    loading_progress(62u, "SCENERY");
    road_diag_checkpoint("TEX_SCENERY", g_surface_texture_count,
        (unsigned int)g_surface_textures, 0u);
    road_diag_checkpoint("STATIC_BEGIN", index,
        g_runtime_hazard_pair_count, 0u);
    (void)load_disc_static();
    road_diag_checkpoint("STATIC_DONE", g_runtime_hazard_pair_count,
        g_runtime_static_loaded, g_runtime_hazard_count);
    loading_progress(76u, "OBSTACLES");
    road_diag_checkpoint("TEX_STATIC", g_surface_texture_count,
        (unsigned int)g_surface_textures, 0u);
    road_diag_checkpoint("EFFECT_BEGIN", index, 0u, 0u);
    (void)load_disc_effect_sprites();
    road_diag_checkpoint("EFFECT_DONE", g_runtime_effect_sprite_count,
        g_runtime_effect_count, g_runtime_crossing_count);
    loading_progress(85u, "EFFECTS");
    road_diag_checkpoint("TEX_EFFECT", g_surface_texture_count,
        (unsigned int)g_surface_textures, 0u);
    if (g_runtime_placements_loaded)
        road_game_set_scenery_placements(g_runtime_objects,
                                         g_runtime_object_count);
#if ROAD_OBJECT_COUNT > 0
    else if (index == 0u)
        road_game_set_scenery_placements(g_original_object_placements,
                                         ROAD_OBJECT_COUNT);
#endif
    if (g_runtime_static_loaded) {
        unsigned int hazard_index;
        road_game_set_static_scenery(g_runtime_static_sprites,
            g_runtime_hazard_pair_count, g_runtime_hazards,
            g_runtime_hazard_count);
        road_game_set_static_scale_frames(
            &g_runtime_static_scale_frames[0][0],
            g_runtime_hazard_pair_count);
        road_game_set_static_frame_anchors(
            &g_runtime_static_frame_anchors[0][0],
            g_runtime_hazard_pair_count);
        road_game_set_static_collisions(g_runtime_static_collisions,
            g_runtime_hazard_pair_count);
        for (hazard_index = 0u;
             hazard_index < g_runtime_hazard_pair_count; ++hazard_index)
            g_runtime_hazard_motion[hazard_index] =
                (unsigned char)(g_runtime_hazard_family_ids[hazard_index] !=
                                148u &&
                                g_runtime_hazard_family_ids[hazard_index] !=
                                149u);
        road_game_set_hazard_motion(g_runtime_hazard_motion,
                                    g_runtime_hazard_pair_count);
    }
#if ROAD_STATIC_SPRITE_COUNT > 0
    else if (index == 0u)
        road_game_set_static_scenery(g_original_static_sprites,
            ROAD_STATIC_SPRITE_COUNT, g_original_static_placements,
            ROAD_STATIC_COUNT);
#endif
    road_game_set_effects(&g_game, g_runtime_effects,
                          g_runtime_effect_count);
    road_game_set_crossing_zones(g_runtime_crossing_zones,
                                  g_runtime_crossing_count);
    g_game.crossing_cursor = 0u;
    road_game_set_effect_sprites(&g_runtime_effect_sprites[0][0],
                                  g_runtime_effect_sprite_count);
    road_game_set_effect_frame_ticks(&g_runtime_effect_ticks[0][0],
                                      g_runtime_effect_sprite_count);
    road_diag_checkpoint("BIND_DONE", index, branch_mask,
        g_runtime_sample_count);
    road_diag_checkpoint("TEX_BIND", g_surface_texture_count,
        (unsigned int)g_surface_textures,
        (unsigned int)g_runtime_road_textures[0].pixels);
    return 1;
}

static int start_course(unsigned int index)
{
    road_diag_checkpoint("RACE_BEGIN", index, g_frontend.bike, 0u);
    if (!bind_course_assets(index, 0u)) {
        road_diag_checkpoint("RACE_BIND_FAIL", index, 0u, 0u);
        return 0;
    }
    road_diag_checkpoint("CARS_BEGIN", index, 0u, 0u);
    if (!load_disc_cars(index)) {
        road_diag_checkpoint("CARS_FAIL", index, 0u, 0u);
        return 0;
    }
    road_diag_checkpoint("CARS_DONE", g_runtime_car_count, index, 0u);
    loading_progress(92u, "TRAFFIC");
    road_game_init(&g_game);
    road_diag_checkpoint("RECIP_BEGIN", index, 0u, 0u);
    (void)load_disc_reciprocal_table();
    road_diag_checkpoint("RECIP_DONE", index, 0u, 0u);
    if (g_runtime_graph_valid)
        (void)rr_graph_race_reset(&g_runtime_graph_race,
                                  &g_runtime_course_graph, &g_game);
    else rr_graph_race_disable(&g_runtime_graph_race, &g_game);
    if (g_bike_specs_ready) {
        const rr_bike_spec_t *spec = &g_bike_specs[g_frontend.bike];
        road_game_set_bike_spec(&g_game, spec);
    }
    g_active_branch_mask = 0u;
    g_next_fork = 0u;
    g_active_fork_end = 0u;
    g_active_fork_phase = 0u;
    road_diag_checkpoint("FORK_BEGIN", index, g_next_fork, 0u);
    (void)load_fork_preview(index, g_active_branch_mask, g_next_fork);
    road_diag_checkpoint("FORK_DONE", index, g_next_fork,
        g_runtime_fork_hill_profile_count);
    loading_progress(100u, "READY");
    g_audio_mixer.engine_pitch = 0u;
    g_course_load_failed = 0;
    road_frontend_race_started(&g_frontend);
    g_diag_render_frames = g_diag_verbose ? 2u : 0u;
    g_diag_race_frames = 0u;
    road_diag_checkpoint("RACE_READY", index, g_frontend.bike,
        g_runtime_sample_count);
    road_diag_checkpoint("TEX_RACE", g_surface_texture_count,
        (unsigned int)g_surface_textures,
        (unsigned int)g_runtime_road_textures[0].pixels);
    return 1;
}

static int choose_pending_forks(void)
{
    unsigned int sample = g_game.distance / 256u;
    if (g_active_fork_phase == 1u &&
        sample >= g_active_fork_end) {
        road_game_set_fork_span(0u);
        g_active_fork_phase = 2u;
    }
    /* The renderer projects 33 nodes ahead, including the reverse junction. */
    if (g_active_fork_phase == 2u &&
        sample + 33u >= g_active_fork_reverse_start &&
        sample < g_active_fork_selected_join) {
        road_game_set_fork_reverse(g_active_fork_selected_join,
                                    g_active_fork_other_join);
        g_active_fork_phase = 3u;
    }
    if (g_active_fork_phase &&
        sample >= g_active_fork_selected_join) {
        g_active_fork_phase = 0u;
        (void)load_fork_preview(g_frontend.course,
                                g_active_branch_mask, g_next_fork);
    }
    while (g_next_fork < g_runtime_branch_count &&
           g_game.distance / 256u >=
               g_runtime_branch_samples[g_next_fork]) {
        unsigned int branch = g_next_fork++;
        rr_u32 chosen_span, other_span;
        int selected_alternate = 0;
        /* Upstream chooses the second link at the first lane's right edge. */
        if (road_fork_channel(g_game.lane,
                g_runtime_branch_old_left[branch],
                g_runtime_branch_first_left[branch],
                g_runtime_branch_first_right[branch]) !=
            g_runtime_branch_main_channels[branch]) {
            unsigned int distance = g_game.distance;
            g_active_branch_mask |= 1u << branch;
            selected_alternate = 1;
            if (!bind_course_assets(g_frontend.course,
                                    g_active_branch_mask))
                return 0;
            road_game_seek(&g_game, distance);
        }
        if (g_runtime_graph_valid &&
            rr_course_graph_branch_span(&g_runtime_course_graph,
                g_active_branch_mask, branch,
                &chosen_span, &other_span) &&
            chosen_span >= RR_COURSE_JUNCTION_SAMPLES * 2u &&
            other_span >= RR_COURSE_JUNCTION_SAMPLES * 2u &&
            (!selected_alternate ||
             load_fork_preview(g_frontend.course,
                g_active_branch_mask, branch))) {
            unsigned int start = g_runtime_branch_samples[branch];
            g_active_fork_end = start + RR_COURSE_JUNCTION_SAMPLES;
            g_active_fork_reverse_start = start + chosen_span -
                RR_COURSE_JUNCTION_SAMPLES;
            g_active_fork_selected_join = start + chosen_span;
            g_active_fork_other_join = start + other_span;
            g_active_fork_phase = 1u;
            road_game_set_fork_span(g_active_fork_end);
        } else {
            g_active_fork_end = 0u;
            g_active_fork_phase = 0u;
            (void)load_fork_preview(g_frontend.course,
                                    g_active_branch_mask, g_next_fork);
        }
    }
    return 1;
}

static int load_disc_scenery(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    rr_u8 *scratch;
    unsigned int i;
    int ok = 0;
    int file = road_disc_open(path, "Families.RSRC");
    if (!bda_fs_file_is_valid(file)) return 0;
    scratch = (rr_u8 *)bda_alloc(65536u);
    if (!scratch || (u32)scratch == 0xffffffffu) {
        (void)bda_fs_close_raw(file);
        return 0;
    }
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto done;
    if (!g_runtime_family_count) goto done;
    for (i = 0u; i < g_runtime_family_count; ++i) {
        rr_u32 id = g_runtime_family_ids[i];
        rr_u32 capacity = 4096u;
        int prefer_last = g_runtime_family_ids[i] == 171u;
        image.pixels = g_runtime_scenery_pixels[i];
        if (id == 69u || id == 164u || id == 185u) {
            unsigned int slot = id == 69u ? 0u : id == 164u ? 1u : 2u;
            image.pixels = g_runtime_large_scenery_pixels[slot];
            capacity = 24576u;
        } else if (id == 168u || id == 242u) {
            image.pixels = g_runtime_huge_scenery_pixels;
            capacity = 32768u;
        }
        if (!rr_family_load_frame(&catalog, id,
                                  prefer_last,
                                  &image, capacity, scratch, 65536u) &&
            !rr_family_load_frame(&catalog, id,
                                  !prefer_last,
                                  &image, capacity, scratch, 65536u)) {
            image.width = 1u;
            image.height = 1u;
            image.pixels[0] = RR_CEL_TRANSPARENT;
        }
        g_runtime_scenery[i].width = image.width;
        g_runtime_scenery[i].height = image.height;
        g_runtime_scenery[i].pixels = image.pixels;
    }
    road_game_set_scenery(g_runtime_scenery, g_runtime_family_count);
    ok = 1;
done:
    bda_free(scratch);
    (void)bda_fs_close_raw(file);
    return ok;
}

static void release_disc_static(void)
{
    unsigned int i, bucket;
    for (i = 0u; i < 64u; ++i) {
        if (g_runtime_static_allocations[i])
            bda_free(g_runtime_static_allocations[i]);
        g_runtime_static_allocations[i] = 0;
        for (bucket = 0u; bucket < 3u; ++bucket) {
            if (g_runtime_static_scale_allocations[i][bucket])
                bda_free(g_runtime_static_scale_allocations[i][bucket]);
            g_runtime_static_scale_allocations[i][bucket] = 0;
            g_runtime_static_scale_frames[i][bucket].pixels = 0;
            g_runtime_static_frame_anchors[i][bucket].valid = 0u;
        }
    }
}

static int load_disc_static(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t family;
    rr_cel_image_t image;
    rr_u8 *record = 0;
    rr_u8 *scratch = 0;
    rr_u32 loaded_id = 0xffffffffu;
    rr_u32 loaded_bytes = 0u;
    unsigned int i;
    int file = road_disc_open(path, "Families.RSRC");
    int ok = 0;
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size) ||
        !g_runtime_hazard_pair_count) goto done;
    scratch = (rr_u8 *)bda_alloc(65536u);
    if ((u32)scratch == 0xffffffffu) scratch = 0;
    for (i = 0u; i < g_runtime_hazard_pair_count; ++i) {
        rr_u32 id = g_runtime_hazard_family_ids[i];
        rr_cel_hotspot_box_t boxes[2];
        rr_u32 box_count, box_index;
        int decoded = 0;
        if (id != loaded_id) {
            if (record) bda_free(record);
            record = 0;
            loaded_id = id;
            loaded_bytes = 0u;
            if (rr_rsrc_find(&catalog, RR_RSRC_TAG('F','A','M',' '),
                             id, &family)) {
                record = (rr_u8 *)bda_alloc(family.size);
                if (!record || (u32)record == 0xffffffffu) {
                    record = 0;
                } else if (!rr_rsrc_read(&catalog, &family, 0u,
                                          record, family.size)) {
                    bda_free(record);
                    record = 0;
                } else {
                    loaded_bytes = family.size;
                }
            }
        }
        image.pixels = g_runtime_static_pixels[i];
        if (record)
            decoded = rr_family_decode_static_frame(record, loaded_bytes,
                g_runtime_hazard_frame_ids[i], &image, 4096u);
        if (record && !decoded) {
            road_pixel_t *large =
                (road_pixel_t *)bda_alloc(8192u * sizeof(road_pixel_t));
            if ((u32)large == 0xffffffffu) large = 0;
            if (large) {
                image.pixels = large;
                decoded = rr_family_decode_static_frame(record,
                    loaded_bytes, g_runtime_hazard_frame_ids[i],
                    &image, 8192u);
                if (decoded) {
                    g_runtime_static_allocations[i] = large;
                } else {
                    bda_free(large);
                    image.pixels = g_runtime_static_pixels[i];
                }
            }
        }
        if (record && !decoded)
            decoded = rr_family_decode_hazard_preview(&catalog, record,
                loaded_bytes, id, g_runtime_hazard_frame_ids[i],
                &image, 4096u, scratch, scratch ? 65536u : 0u);
        if (!decoded) {
            image.width = 1u;
            image.height = 1u;
            image.pixels[0] = RR_CEL_TRANSPARENT;
        }
        if (record && decoded) {
            rr_cel_image_t middle;
            middle.pixels = image.pixels;
            if (rr_family_decode_static_frame_bucket(record,
                    loaded_bytes, g_runtime_hazard_frame_ids[i],
                    1u, &middle,
                    g_runtime_static_allocations[i] ? 8192u : 4096u)) {
                image.width = middle.width;
                image.height = middle.height;
            }
        }
        g_runtime_static_sprites[i].width = image.width;
        g_runtime_static_sprites[i].height = image.height;
        g_runtime_static_sprites[i].pixels = image.pixels;
        g_runtime_static_scale_frames[i][1].width = image.width;
        g_runtime_static_scale_frames[i][1].height = image.height;
        g_runtime_static_scale_frames[i][1].pixels = image.pixels;
        if (record && scratch) {
            unsigned int bucket;
            for (bucket = 0u; bucket < 3u; bucket += 2u) {
                road_pixel_t *pixels;
                rr_u32 count;
                image.pixels = (road_pixel_t *)scratch;
                if (!rr_family_decode_static_frame_bucket(record,
                        loaded_bytes, g_runtime_hazard_frame_ids[i],
                        bucket, &image, 32768u)) continue;
                count = image.width * image.height;
                pixels = (road_pixel_t *)bda_alloc(
                    count * sizeof(road_pixel_t));
                if (!pixels || (u32)pixels == 0xffffffffu) continue;
                bda_memcpy(pixels, scratch,
                           count * sizeof(road_pixel_t));
                g_runtime_static_scale_allocations[i][bucket] = pixels;
                g_runtime_static_scale_frames[i][bucket].width =
                    image.width;
                g_runtime_static_scale_frames[i][bucket].height =
                    image.height;
                g_runtime_static_scale_frames[i][bucket].pixels = pixels;
            }
        }
        if (record) {
            unsigned int bucket;
            for (bucket = 0u; bucket < 3u; ++bucket) {
                rr_cel_anchor_t anchor;
                if (rr_family_decode_static_anchor_bucket(record,
                        loaded_bytes, g_runtime_hazard_frame_ids[i],
                        bucket, &anchor)) {
                    g_runtime_static_frame_anchors[i][bucket].x =
                        anchor.x;
                    g_runtime_static_frame_anchors[i][bucket].y =
                        anchor.y;
                    g_runtime_static_frame_anchors[i][bucket].valid = 1u;
                }
            }
        }
        box_count = record ? rr_family_decode_static_hotspots(record,
            loaded_bytes, id, g_runtime_hazard_frame_ids[i], boxes) : 0u;
        g_runtime_static_collisions[i].count = (unsigned char)box_count;
        for (box_index = 0u; box_index < box_count; ++box_index) {
            g_runtime_static_collisions[i].box[box_index].left =
                boxes[box_index].left;
            g_runtime_static_collisions[i].box[box_index].top =
                boxes[box_index].top;
            g_runtime_static_collisions[i].box[box_index].right =
                boxes[box_index].right;
            g_runtime_static_collisions[i].box[box_index].bottom =
                boxes[box_index].bottom;
        }
    }
    g_runtime_static_loaded = 1;
    ok = 1;
done:
    if (scratch) bda_free(scratch);
    if (record) bda_free(record);
    (void)bda_fs_close_raw(file);
    return ok;
}

static void release_disc_effect_sprites(void)
{
    unsigned int i, frame;
    road_game_set_effect_sprites(0, 0u);
    for (i = 0u; i < g_runtime_effect_sprite_count; ++i) {
        for (frame = 0u; frame < ROAD_EFFECT_SPRITE_FRAMES; ++frame) {
            if (g_runtime_effect_allocations[i][frame])
                bda_free(g_runtime_effect_allocations[i][frame]);
            g_runtime_effect_allocations[i][frame] = 0;
            g_runtime_effect_sprites[i][frame].pixels = 0;
        }
    }
    g_runtime_effect_sprite_count = 0u;
}

static int load_disc_effect_sprites(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    static const rr_u32 state_keys[ROAD_EFFECT_SPRITE_FRAMES] = {
        RR_RSRC_TAG('S','t','n','d'),
        RR_RSRC_TAG('M','o','v','1'),
        RR_RSRC_TAG('M','o','v','2'),
        RR_RSRC_TAG('M','o','v','3'),
        RR_RSRC_TAG('M','o','v','4'),
        RR_RSRC_TAG('M','o','v','5'),
        RR_RSRC_TAG('M','o','v','6'),
        RR_RSRC_TAG('S','h','k','1'),
        RR_RSRC_TAG('S','h','k','2'),
        RR_RSRC_TAG('F','a','l','1'),
        RR_RSRC_TAG('F','a','l','2'),
        RR_RSRC_TAG('F','a','l','3'),
        RR_RSRC_TAG('F','a','l','4'),
        RR_RSRC_TAG('F','a','l','5'),
        RR_RSRC_TAG('F','a','l','6'),
        RR_RSRC_TAG('A','t','k','1'),
        RR_RSRC_TAG('A','t','k','2'),
        RR_RSRC_TAG('F','s','t','1'),
        RR_RSRC_TAG('F','s','t','2'),
        RR_RSRC_TAG('F','s','t','3'),
        RR_RSRC_TAG('F','s','t','4'),
        RR_RSRC_TAG('F','s','t','5'),
        RR_RSRC_TAG('F','s','t','6'),
        RR_RSRC_TAG('F','l','g','1'),
        RR_RSRC_TAG('F','l','g','2'),
        RR_RSRC_TAG('F','l','g','3'),
        RR_RSRC_TAG('F','l','g','4'),
        RR_RSRC_TAG('F','l','g','5'),
        RR_RSRC_TAG('F','l','g','6')
    };
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t family_record;
    rr_cel_image_t image;
    rr_u8 *record = 0;
    road_pixel_t *temporary = 0;
    unsigned int i;
    int file = road_disc_open(path, "Families.RSRC");
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto done;
    temporary = (road_pixel_t *)bda_alloc(16384u * 2u);
    if (!temporary || (u32)temporary == 0xffffffffu) {
        temporary = 0;
        goto done;
    }
    image.pixels = temporary;
    for (i = 0u; i < g_runtime_effect_count; ++i) {
        road_effect_placement_t *effect = &g_runtime_effects[i];
        unsigned int slot, frame;
        rr_u32 id = effect->family_id;
        rr_u32 variant = effect->family_inventory & 63u;
        effect->sprite_index = 255u;
        if (effect->travel_mode >= 11u || !id) continue;
        for (slot = 0u; slot < g_runtime_effect_sprite_count; ++slot)
            if (g_runtime_effect_family_ids[slot] == id &&
                g_runtime_effect_variants[slot] == variant) break;
        if (slot == g_runtime_effect_sprite_count) {
            if (slot >= 64u) continue;
            g_runtime_effect_family_ids[slot] = (unsigned short)id;
            g_runtime_effect_variants[slot] = (unsigned char)variant;
            g_runtime_effect_sprite_count++;
            if (!rr_rsrc_find(&catalog, RR_RSRC_TAG('F','A','M',' '),
                              id, &family_record)) continue;
            record = (rr_u8 *)bda_alloc(family_record.size);
            if (!record || (u32)record == 0xffffffffu) {
                record = 0;
                goto done;
            }
            if (!rr_rsrc_read(&catalog, &family_record, 0u,
                              record, family_record.size)) goto done;
            for (frame = 0u; frame < ROAD_EFFECT_SPRITE_FRAMES;
                 ++frame) {
                rr_u32 bytes, ticks = 6u;
                road_pixel_t *pixels;
                g_runtime_effect_ticks[slot][frame] = 6u;
                if (!rr_family_decode_rider_named_frame(record,
                    family_record.size, variant, state_keys[frame],
                    &image, 16384u, &ticks)) continue;
                g_runtime_effect_ticks[slot][frame] =
                    (unsigned char)(ticks > 255u ? 255u : ticks);
                bytes = image.width * image.height * 2u;
                pixels = (road_pixel_t *)bda_alloc(bytes);
                if (!pixels || (u32)pixels == 0xffffffffu) continue;
                bda_memcpy(pixels, temporary, bytes);
                g_runtime_effect_allocations[slot][frame] = pixels;
                g_runtime_effect_sprites[slot][frame].width = image.width;
                g_runtime_effect_sprites[slot][frame].height = image.height;
                g_runtime_effect_sprites[slot][frame].pixels = pixels;
            }
            if (!g_runtime_effect_sprites[slot][0].pixels &&
                rr_family_decode_rider_frame(record, family_record.size,
                    variant, 0u, &image, 16384u)) {
                rr_u32 bytes = image.width * image.height * 2u;
                road_pixel_t *pixels = (road_pixel_t *)bda_alloc(bytes);
                if (pixels && (u32)pixels != 0xffffffffu) {
                    bda_memcpy(pixels, temporary, bytes);
                    g_runtime_effect_allocations[slot][0] = pixels;
                    g_runtime_effect_sprites[slot][0].width = image.width;
                    g_runtime_effect_sprites[slot][0].height = image.height;
                    g_runtime_effect_sprites[slot][0].pixels = pixels;
                }
            }
            if (!g_runtime_effect_sprites[slot][1].pixels &&
                rr_family_decode_rider_frame(record, family_record.size,
                    variant, 1u, &image, 16384u)) {
                rr_u32 bytes = image.width * image.height * 2u;
                road_pixel_t *pixels = (road_pixel_t *)bda_alloc(bytes);
                if (pixels && (u32)pixels != 0xffffffffu) {
                    bda_memcpy(pixels, temporary, bytes);
                    g_runtime_effect_allocations[slot][1] = pixels;
                    g_runtime_effect_sprites[slot][1].width = image.width;
                    g_runtime_effect_sprites[slot][1].height = image.height;
                    g_runtime_effect_sprites[slot][1].pixels = pixels;
                }
            }
            for (frame = 1u; frame < ROAD_EFFECT_SPRITE_FRAMES;
                 ++frame)
                if (!g_runtime_effect_sprites[slot][frame].pixels) {
                    unsigned int first = frame >= ROAD_EFFECT_FLAG_FIRST ?
                        ROAD_EFFECT_FLAG_FIRST :
                        frame >= ROAD_EFFECT_FAST_FIRST ?
                        ROAD_EFFECT_FAST_FIRST :
                        frame >= ROAD_EFFECT_ATTACK_FIRST ?
                        ROAD_EFFECT_ATTACK_FIRST :
                        frame >= ROAD_EFFECT_FALL_FIRST ?
                        ROAD_EFFECT_FALL_FIRST :
                        frame >= ROAD_EFFECT_SHAKE_FIRST ?
                        ROAD_EFFECT_SHAKE_FIRST :
                        ROAD_EFFECT_MOTION_FIRST;
                    g_runtime_effect_sprites[slot][frame] =
                        g_runtime_effect_sprites[slot][frame > first ?
                            frame - 1u : ROAD_EFFECT_STANDING_FRAME];
                }
            bda_free(record);
            record = 0;
        }
        if (g_runtime_effect_sprites[slot][0].pixels)
            effect->sprite_index = (unsigned char)slot;
    }
done:
    if (record) bda_free(record);
    if (temporary) bda_free(temporary);
    (void)bda_fs_close_raw(file);
    return g_runtime_effect_sprite_count > 0u;
}

static void release_disc_bikes(void)
{
    unsigned int i;
    for (i = 0u; i < ROAD_BIKE_FRAME_COUNT; ++i) {
        if (g_runtime_bike_allocations[i]) {
            bda_free(g_runtime_bike_allocations[i]);
            g_runtime_bike_allocations[i] = 0;
        }
    }
}

static void release_disc_cars(void)
{
    unsigned int i, frame;
    road_game_set_car_animations(0, 0u);
    for (i = 0u; i < 16u; ++i) {
        for (frame = 0u; frame < ROAD_CAR_MAX_FRAMES; ++frame) {
            if (g_runtime_car_allocations[i][frame]) {
                bda_free(g_runtime_car_allocations[i][frame]);
                g_runtime_car_allocations[i][frame] = 0;
            }
        }
        g_runtime_cars[i].frames = 0;
        g_runtime_cars[i].frame_count = 0u;
        g_runtime_cars[i].frame_map = 0;
        g_runtime_cars[i].anchors = 0;
    }
    g_runtime_car_count = 0u;
}

static int load_disc_cars(unsigned int index)
{
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t anim;
    rr_rsrc_record_t cans;
    rr_cel_image_t image;
    road_pixel_t *temporary = 0;
    unsigned int i;
    unsigned int cans_id = 1u;
    char path[ROAD_RESOURCE_PATH_CAP];
    int file;
    int ok = 0;
    release_disc_cars();
    if (index >= RR_COURSE_COUNT) return 0;
    file = road_disc_open(path, rr_car_paths[index]);
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto done;
    temporary = (road_pixel_t *)bda_alloc(16384u * 2u);
    if (!temporary || (u32)temporary == 0xffffffffu) {
        temporary = 0;
        goto done;
    }
    for (i = 0u; i < 16u; ++i) {
        rr_u8 *record;
        rr_u32 frame, frame_count;
        if (!rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                          i + 1u, &anim)) break;
        record = (rr_u8 *)bda_alloc(anim.size);
        if (!record || (u32)record == 0xffffffffu) goto done;
        if (!rr_rsrc_read(&catalog, &anim, 0u, record, anim.size)) {
            bda_free(record);
            goto done;
        }
        frame_count = rr_animation_frame_count(record, anim.size);
        if (!frame_count || frame_count > ROAD_CAR_MAX_FRAMES) {
            bda_free(record);
            goto done;
        }
        image.pixels = temporary;
        for (frame = 0u; frame < frame_count; ++frame) {
            rr_u32 bytes;
            road_pixel_t *pixels;
            rr_cel_anchor_t anchor;
            if (!rr_animation_decode_frame(record, anim.size, frame, 0,
                                           &image, 16384u)) {
                bda_free(record);
                goto done;
            }
            bytes = image.width * image.height * 2u;
            pixels = (road_pixel_t *)bda_alloc(bytes);
            if (!pixels || (u32)pixels == 0xffffffffu) {
                bda_free(record);
                goto done;
            }
            bda_memcpy(pixels, temporary, bytes);
            g_runtime_car_allocations[i][frame] = pixels;
            g_runtime_car_frames[i][frame].width = image.width;
            g_runtime_car_frames[i][frame].height = image.height;
            g_runtime_car_frames[i][frame].pixels = pixels;
            g_runtime_car_anchors[i][frame].valid =
                (unsigned char)rr_animation_decode_frame_anchor(
                    record, anim.size, frame, &anchor);
            if (g_runtime_car_anchors[i][frame].valid) {
                g_runtime_car_anchors[i][frame].x = anchor.x;
                g_runtime_car_anchors[i][frame].y = anchor.y;
            }
        }
        bda_free(record);
        if (rr_rsrc_find(&catalog, RR_RSRC_TAG('C','A','N','S'),
                         cans_id, &cans)) {
            if (cans.size > sizeof(g_runtime_cans_bytes) ||
                !rr_rsrc_read(&catalog, &cans, 0u,
                              g_runtime_cans_bytes, cans.size) ||
                !rr_cans_parse_car_frames(g_runtime_cans_bytes, cans.size,
                                          frame_count,
                                          &g_runtime_car_maps[i]))
                goto done;
            ++cans_id;
        } else {
            if (!i) goto done;
            bda_memcpy(&g_runtime_car_maps[i], &g_runtime_car_maps[i - 1u],
                       sizeof(g_runtime_car_maps[i]));
        }
        g_runtime_cars[i].frames = g_runtime_car_frames[i];
        g_runtime_cars[i].frame_count = frame_count;
        g_runtime_cars[i].frame_map = &g_runtime_car_maps[i];
        g_runtime_cars[i].anchors = g_runtime_car_anchors[i];
        g_runtime_car_count++;
    }
    if (g_runtime_car_count) {
        road_game_set_car_animations(g_runtime_cars, g_runtime_car_count);
        ok = 1;
    }
done:
    if (temporary) bda_free(temporary);
    (void)bda_fs_close_raw(file);
    if (!ok) release_disc_cars();
    return ok;
}

static int load_disc_bikes(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    static const unsigned short extra_frames[] = {
        1u, 2u, 5u, 6u, 13u, 14u, 18u, 19u, 319u,
        241u, 242u, 243u, 244u, 245u, 246u, 247u,
        282u, 283u, 284u, 285u, 286u, 289u,
        303u, 304u, 305u, 306u, 307u, 308u,
        309u, 310u, 311u, 312u, 313u,
        213u, 214u, 215u, 216u, 217u, 219u,
        225u, 226u, 227u, 228u, 229u, 230u,
        1u, 31u, 32u
    };
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_rsrc_record_t anim;
    rr_cel_image_t image;
    rr_u8 *record = 0;
    road_pixel_t *temporary = 0;
    unsigned int slot;
    int file = road_disc_open(path, "rashOpt.rsrc");
    int ok = 0;
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto done;
    temporary = (road_pixel_t *)bda_alloc(128u * 128u * 2u);
    if (!temporary || (u32)temporary == 0xffffffffu) {
        temporary = 0;
        goto done;
    }
    for (slot = 0u; slot < ROAD_BIKE_FRAME_COUNT; ++slot) {
        unsigned int anim_id = slot == 21u ||
            slot >= ROAD_BIKE_OPPONENT_HIT_FIRST ? 3u : 2u;
        unsigned int frame = slot == 0u ? 3u :
            slot == 1u ? 20u : slot == 2u ? 0u :
            slot == 21u ? 0u : slot >= ROAD_BIKE_TURN_FIRST ?
            extra_frames[slot - ROAD_BIKE_TURN_FIRST] :
            100u + slot - 3u;
        unsigned int bytes;
        road_pixel_t *pixels;
        /* Keep ANIM 2 resident while decoding its selected frame set. */
        if (slot == 0u || slot == 21u ||
            slot == ROAD_BIKE_TURN_FIRST ||
            slot == ROAD_BIKE_OPPONENT_HIT_FIRST) {
            if (record) bda_free(record);
            record = 0;
            if (!rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                              anim_id, &anim)) goto done;
            record = (rr_u8 *)bda_alloc(anim.size);
            if (!record || (u32)record == 0xffffffffu) {
                record = 0;
                goto done;
            }
            if (!rr_rsrc_read(&catalog, &anim, 0u, record, anim.size))
                goto done;
        }
        image.pixels = temporary;
        if (!rr_animation_decode_frame(record, anim.size, frame,
                                       anim_id == 3u, &image, 16384u))
            goto done;
        bytes = image.width * image.height * 2u;
        pixels = (road_pixel_t *)bda_alloc(bytes);
        if (!pixels || (u32)pixels == 0xffffffffu) goto done;
        bda_memcpy(pixels, temporary, bytes);
        g_runtime_bike_allocations[slot] = pixels;
        g_runtime_bikes[slot].width = image.width;
        g_runtime_bikes[slot].height = image.height;
        g_runtime_bikes[slot].pixels = pixels;
    }
    road_game_set_bikes(g_runtime_bikes, ROAD_BIKE_FRAME_COUNT);
    ok = 1;
done:
    if (record) bda_free(record);
    if (temporary) bda_free(temporary);
    (void)bda_fs_close_raw(file);
    if (!ok) release_disc_bikes();
    return ok;
}

static int load_disc_road_textures(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    rr_u8 *scratch;
    rr_u8 *gauge_record;
    rr_rsrc_record_t gauge;
    unsigned int variant, style;
    int file = road_disc_open(path, "rashOpt.rsrc");
    road_game_set_hud(0);
    road_game_set_hud_needles(0, 0);
    road_game_set_alternate_hud(0, 0u, 0);
    road_game_set_edge_fill(0, 0);
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto fail;
    for (variant = 0u; variant < 3u; ++variant) {
        unsigned int used = 0u;
        for (style = 0u; style < 9u; ++style) {
            unsigned int slot = variant * 9u + style;
            image.pixels = &g_runtime_road_pixels[variant][used];
            if (!rr_cel_load_road_surface(&catalog, slot + 1u, &image,
                5656u - used)) goto fail;
            g_runtime_road_textures[slot].width = image.width;
            g_runtime_road_textures[slot].height = image.height;
            g_runtime_road_textures[slot].pixels = image.pixels;
            used += image.width * image.height;
        }
        if (used != 5656u) goto fail;
    }
    road_game_set_surface_textures(g_runtime_road_textures, 27u);
    for (variant = 0u; variant < 2u; ++variant) {
        image.pixels = g_runtime_edge_fill_pixels[variant];
        if (!rr_cel_load_road_surface(&catalog, 73u + variant,
            &image, 256u * 32u) || image.width != 256u ||
            image.height != 32u) goto fail;
        g_runtime_edge_fill[variant].width = image.width;
        g_runtime_edge_fill[variant].height = image.height;
        g_runtime_edge_fill[variant].pixels = image.pixels;
    }
    road_game_set_edge_fill(&g_runtime_edge_fill[0],
                             &g_runtime_edge_fill[1]);
    image.pixels = g_runtime_hud_pixels;
    scratch = (rr_u8 *)bda_alloc(12000u);
    if (scratch) {
        if (rr_cel_load_packed_image(&catalog, 55u, &image,
                                     300u * 58u, scratch, 12000u)) {
            g_runtime_hud.width = image.width;
            g_runtime_hud.height = image.height;
            g_runtime_hud.pixels = image.pixels;
            road_game_set_hud(&g_runtime_hud);
        }
        image.pixels = g_runtime_hud_speed_needle_pixels;
        if (rr_cel_load_uncoded6(&catalog, 57u, &image,
                                 48u * 6u)) {
            g_runtime_hud_speed_needle.width = image.width;
            g_runtime_hud_speed_needle.height = image.height;
            g_runtime_hud_speed_needle.pixels = image.pixels;
            image.pixels = g_runtime_hud_health_needle_pixels;
            if (rr_cel_load_uncoded1(&catalog, 58u, &image,
                                     10u * 2u)) {
                g_runtime_hud_health_needle.width = image.width;
                g_runtime_hud_health_needle.height = image.height;
                g_runtime_hud_health_needle.pixels = image.pixels;
                road_game_set_hud_needles(
                    &g_runtime_hud_speed_needle,
                    &g_runtime_hud_health_needle);
            } else {
                road_game_set_hud_needles(&g_runtime_hud_speed_needle, 0);
            }
        }
        if (rr_rsrc_find(&catalog, RR_RSRC_TAG('A','N','I','M'),
                          1u, &gauge) && gauge.size <= 4096u) {
            gauge_record = (rr_u8 *)bda_alloc(gauge.size);
            if (gauge_record) {
                unsigned int frame;
                int frames_ok = rr_rsrc_read(&catalog, &gauge, 0u,
                                              gauge_record, gauge.size);
                for (frame = 0u; frames_ok && frame < 32u; ++frame) {
                    image.pixels = g_runtime_hud_health_pixels[frame];
                    frames_ok = rr_animation_decode_rect_frame(
                        gauge_record, gauge.size, frame, &image,
                        32u * 8u);
                    if (frames_ok) {
                        g_runtime_hud_health_frames[frame].width = image.width;
                        g_runtime_hud_health_frames[frame].height = image.height;
                        g_runtime_hud_health_frames[frame].pixels =
                            image.pixels;
                    }
                }
                image.pixels = g_runtime_hud_portrait_pixels;
                if (frames_ok && rr_cel_load_packed_image(&catalog,
                    59u, &image, 43u * 14u, scratch, 12000u)) {
                    g_runtime_hud_portrait.width = image.width;
                    g_runtime_hud_portrait.height = image.height;
                    g_runtime_hud_portrait.pixels = image.pixels;
                    road_game_set_alternate_hud(
                        g_runtime_hud_health_frames, 32u,
                        &g_runtime_hud_portrait);
                }
                bda_free(gauge_record);
            }
        }
        bda_free(scratch);
    }
    (void)bda_fs_close_raw(file);
    return 1;
fail:
    road_game_set_surface_textures(0, 0u);
    road_game_set_edge_fill(0, 0);
    (void)bda_fs_close_raw(file);
    return 0;
}

static void init_vx(void)
{
    int i;
    bda_memset(g_vx, 0, sizeof(g_vx));
    g_vx[0] = 'V';
    g_vx[1] = 'X';
    for (i = 2; i < 6; ++i) g_vx[i] = 204u;
    g_vx[6] = PORTRAIT_WIDTH;
    g_vx[10] = (u8)PORTRAIT_HEIGHT;
    g_vx[11] = (u8)(PORTRAIT_HEIGHT >> 8);
    for (i = 14; i < 20; ++i) g_vx[i] = 204u;
    for (i = 20; i < 24; ++i) g_vx[i] = 255u;
}

static void release_draw(void)
{
    if (g_draw && (s32)g_draw != -1)
        bda_gui_end_draw(g_draw);
    g_draw = 0;
    g_draw_owner = 0;
}

static int acquire_draw(bda_handle_t owner)
{
    if (g_draw && g_draw_owner == owner) return 1;
    release_draw();
    g_draw = bda_gui_current_draw(owner);
    if (!g_draw || (s32)g_draw == -1) {
        g_draw = 0;
        return 0;
    }
    g_draw_owner = owner;
    return 1;
}

static int window_proc(bda_handle_t handle, u32 message,
                       u32 wparam, u32 lparam)
{
    if (message == 0x21u) {
        /* Firmware touch prefix may also appear as packet Escape. */
        u16 px = 0u;
        u16 py = 0u;
        bda_gui_touch_position(&px, &py);
        road_diag_live("TOUCH_PREFIX", px, py,
            g_frontend.screen);
        g_touch_escape_suppressed = 1;
        g_touch_escape_until_ms = bda_gui_millisecond_count() + 5000u;
        g_escape_pending = 0;
    }
    if (message == BDA_MSG_TOUCH_COORDINATE) {
        /* lparam belongs to this event; the global touch getter may already
         * contain the next tap when queued events are processed. */
        int portrait_x;
        int portrait_y;
        int x;
        int y;
        int button;
        int valid = road_virtual_touch_unpack(lparam, &portrait_x,
                                              &portrait_y);
        road_diag_live("TOUCH_COORD", (unsigned int)portrait_x,
            (unsigned int)portrait_y,
            g_frontend.screen);
        g_touch_escape_suppressed = 1;
        g_touch_escape_until_ms = bda_gui_millisecond_count() + 5000u;
        g_escape_pending = 0;
        x = PORTRAIT_HEIGHT - 1 - portrait_y;
        y = portrait_x;
        if (valid && !g_loading_active &&
            g_frontend.screen == ROAD_SCREEN_RACE) {
            button = road_virtual_touch_coordinate(&g_touch, x, y);
            if (button == ROAD_TOUCH_GAS)
                road_diag_live("TOUCH_GAS", x, y,
                    (unsigned int)g_touch.throttle_on);
            else if (button == ROAD_TOUCH_HIT ||
                     button == ROAD_TOUCH_KICK) {
                if (button == ROAD_TOUCH_KICK)
                    g_touch_kick_visual_until_ms =
                        bda_gui_millisecond_count() + 180u;
                else g_touch_hit_visual_until_ms =
                        bda_gui_millisecond_count() + 180u;
                g_touch_attack_until_ms =
                    bda_gui_millisecond_count() + 600u;
                road_diag_live(button == ROAD_TOUCH_KICK ?
                    "TOUCH_KICK" : "TOUCH_HIT", x, y, 1u);
            }
        } else g_touch.down = 1;
        return 1;
    } else if (message == BDA_MSG_TOUCH_RELEASE) {
        int portrait_x;
        int portrait_y;
        int button = ROAD_TOUCH_NONE;
        int valid = road_virtual_touch_unpack(lparam, &portrait_x,
                                              &portrait_y);
        road_diag_live("TOUCH_UP", (unsigned int)portrait_x,
            (unsigned int)portrait_y,
            (unsigned int)g_frontend.screen);
        if (valid && !g_loading_active &&
            g_frontend.screen == ROAD_SCREEN_RACE)
            button = road_virtual_touch_release(&g_touch,
                PORTRAIT_HEIGHT - 1 - portrait_y, portrait_x);
        else {
            g_touch.down = 0;
            g_touch.handled = 0;
        }
        if (button == ROAD_TOUCH_GAS)
            road_diag_live("TOUCH_GAS_RELEASE", portrait_y,
                portrait_x, (unsigned int)g_touch.throttle_on);
        else if (button == ROAD_TOUCH_HIT ||
                 button == ROAD_TOUCH_KICK) {
            if (button == ROAD_TOUCH_KICK)
                g_touch_kick_visual_until_ms =
                    bda_gui_millisecond_count() + 180u;
            else g_touch_hit_visual_until_ms =
                    bda_gui_millisecond_count() + 180u;
            g_touch_attack_until_ms =
                bda_gui_millisecond_count() + 600u;
            road_diag_live(button == ROAD_TOUCH_KICK ?
                "TOUCH_KICK_RELEASE" : "TOUCH_HIT_RELEASE", portrait_y,
                portrait_x, 1u);
        }
        g_touch_escape_suppressed = 1;
        g_touch_escape_until_ms = bda_gui_millisecond_count() + 5000u;
        g_escape_pending = 0;
        return 1;
    } else if (message == BDA_MSG_DRAW_CONTEXT_ATTACH) {
        (void)acquire_draw(handle);
    } else if (message == BDA_MSG_DRAW_CONTEXT_DETACH) {
        release_draw();
        g_detached = 1;
    }
    return bda_gui_default_proc(handle, message, wparam, lparam);
}

static u32 read_controls(void)
{
    bda_gui_input_packet_t packet = {{0}};
    u32 input = 0u;
    unsigned int physical = 0u;
    int escape_pressed = 0;
    if (bda_gui_input_packet(&packet) >= 0) {
        if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_UP))
            physical |= ROAD_PHYSICAL_UP;
        if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_DOWN))
            physical |= ROAD_PHYSICAL_DOWN;
        if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_LEFT))
            physical |= ROAD_PHYSICAL_LEFT;
        if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_RIGHT))
            physical |= ROAD_PHYSICAL_RIGHT;
        input |= road_landscape_direction_input(physical,
            g_frontend.screen == ROAD_SCREEN_RACE);
        if (bda_gui_input_packet_key_pressed(&packet, BDA_KEY_ENTER))
            input |= g_frontend.screen == ROAD_SCREEN_RACE ?
                ROAD_BRAKE : ROAD_ATTACK;
        escape_pressed = bda_gui_input_packet_key_pressed(
            &packet, BDA_KEY_ESCAPE);
    }
    if (g_touch_escape_suppressed) {
        if (!g_touch.down && !escape_pressed &&
            (s32)(bda_gui_millisecond_count() -
                  g_touch_escape_until_ms) >= 0) {
            g_touch_escape_suppressed = 0;
            road_diag_live("TOUCH_ESC_END", 0u, 0u, 0u);
        }
        escape_pressed = 0;
        g_escape_pending = 0;
        g_escape_key_seen = 0;
    }
    if (escape_pressed && !g_escape_key_seen) {
        g_escape_key_seen = 1;
        g_escape_pending = 1;
        g_escape_due_ms = bda_gui_millisecond_count() + 100u;
        road_diag_live("ESC_PENDING", g_frontend.screen,
            (unsigned int)g_touch_escape_suppressed, 0u);
        road_diag_live("ESC_PACKET",
            (unsigned int)packet.bytes[0] << 24 |
            (unsigned int)packet.bytes[1] << 16 |
            (unsigned int)packet.bytes[2] << 8 |
            (unsigned int)packet.bytes[3],
            (unsigned int)packet.bytes[4] << 8 |
            (unsigned int)packet.bytes[5],
            (unsigned int)g_touch.down);
    } else if (!escape_pressed) {
        /* A touchscreen-generated Escape is often only one packet long.
         * Do not turn that short pulse into a delayed pause. */
        if (g_escape_pending)
            road_diag_live("ESC_CANCEL", g_frontend.screen,
                0u, 0u);
        g_escape_key_seen = 0;
        g_escape_pending = 0;
    }
    if (g_escape_pending &&
        (s32)(bda_gui_millisecond_count() - g_escape_due_ms) >= 0) {
        input |= 0x80000000u;
        g_escape_pending = 0;
        road_diag_live("ESC_FIRE", g_frontend.screen,
            (unsigned int)g_touch_escape_suppressed, 0u);
    }
    if (g_frontend.screen == ROAD_SCREEN_RACE) {
        if (g_touch.throttle_on) input |= ROAD_ACCEL;
        if (g_touch.attack_pending) {
            if ((s32)(bda_gui_millisecond_count() -
                      g_touch_attack_until_ms) >= 0)
                g_touch.attack_pending = g_touch.kick_pending = 0;
            else if (!g_game.attack_ms && !g_game.recovery_ms &&
                     !(g_game.input & ROAD_ATTACK)) {
                input |= ROAD_ATTACK;
                if (g_touch.kick_pending ||
                    (physical & ROAD_PHYSICAL_RIGHT))
                    input |= ROAD_KICK;
            }
        }
    }
    return input;
}

static const unsigned char g_menu_font[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17},
    {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
};

static int load_disc_menu_background(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    rr_cel_image_t image;
    rr_u8 *scratch;
    unsigned int i;
    int file = road_disc_open(path, "Streams\\bgaudio\\RashIF.RSRC");
    int ok = 0;
    if (!bda_fs_file_is_valid(file)) return 0;
    g_menu_background = (road_pixel_t *)bda_alloc(
        ROAD_PIXELS * sizeof(road_pixel_t));
    if (!g_menu_background ||
        (u32)g_menu_background == 0xffffffffu) {
        g_menu_background = 0;
        (void)bda_fs_close_raw(file);
        return 0;
    }
    scratch = (rr_u8 *)bda_alloc(160000u);
    if (!scratch || (u32)scratch == 0xffffffffu) {
        bda_free(g_menu_background);
        g_menu_background = 0;
        (void)bda_fs_close_raw(file);
        return 0;
    }
    bda_fs_path_info_init(&info);
    image.pixels = g_menu_background;
    if (bda_fs_path_info(path, &info) == 0 &&
        rr_rsrc_open(&catalog, course_read_at, &file, info.size) &&
        rr_cel_load_rgb16(&catalog, 16u, &image, ROAD_PIXELS,
                           scratch, 160000u) &&
        image.width == ROAD_WIDTH && image.height == ROAD_HEIGHT) {
        /* Black is the presentation color key in this VX backend. */
        for (i = 0u; i < ROAD_PIXELS; ++i)
            if (g_menu_background[i] == 0u ||
                g_menu_background[i] == RR_CEL_TRANSPARENT)
                g_menu_background[i] = 1u;
        g_menu_background_ready = 1;
        ok = 1;
    }
    bda_free(scratch);
    (void)bda_fs_close_raw(file);
    if (!ok) {
        bda_free(g_menu_background);
        g_menu_background = 0;
    }
    return ok;
}

static int load_disc_bike_specs(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    bda_fs_path_info_t info;
    rr_rsrc_file_t catalog;
    unsigned int i;
    int file = road_disc_open(path, "BikeSpecs.rsrc");
    if (!bda_fs_file_is_valid(file)) return 0;
    bda_fs_path_info_init(&info);
    if (bda_fs_path_info(path, &info) != 0 ||
        !rr_rsrc_open(&catalog, course_read_at, &file, info.size))
        goto fail;
    for (i = 0u; i < 15u; ++i)
        if (!rr_bike_spec_load(&catalog, i + 1u, &g_bike_specs[i]))
            goto fail;
    (void)bda_fs_close_raw(file);
    return 1;
fail:
    (void)bda_fs_close_raw(file);
    return 0;
}

static void menu_text(road_pixel_t *pixels, int x, int y,
                      const char *label, int scale, road_pixel_t color)
{
    while (*label) {
        char letter = *label++;
        if (letter >= 'A' && letter <= 'Z') {
            unsigned int row;
            for (row = 0u; row < 7u; ++row) {
                unsigned int col;
                unsigned char bits = g_menu_font[letter - 'A'][row];
                for (col = 0u; col < 5u; ++col)
                    if (bits & (1u << (4u - col)))
                        box(pixels, x + (int)col * scale,
                            y + (int)row * scale, scale, scale, color);
            }
        }
        x += 6 * scale;
    }
}

static unsigned int road_ui_next_codepoint(const unsigned char **cursor)
{
    unsigned int c = *(*cursor)++;
    if (c < 0x80u) return c;
    if ((c & 0xe0u) == 0xc0u) {
        unsigned int next = *(*cursor)++;
        return ((c & 0x1fu) << 6) | (next & 0x3fu);
    }
    if ((c & 0xf0u) == 0xe0u) {
        unsigned int next = *(*cursor)++;
        unsigned int last = *(*cursor)++;
        return ((c & 0x0fu) << 12) | ((next & 0x3fu) << 6) |
               (last & 0x3fu);
    }
    return '?';
}

static const road_ui_glyph_t *road_ui_find_glyph(unsigned int codepoint)
{
    unsigned int low = 0u, high = ROAD_UI_GLYPH_COUNT;
    while (low < high) {
        unsigned int middle = low + (high - low) / 2u;
        if (g_road_ui_glyphs[middle].codepoint < codepoint)
            low = middle + 1u;
        else high = middle;
    }
    if (low < ROAD_UI_GLYPH_COUNT &&
        g_road_ui_glyphs[low].codepoint == codepoint)
        return &g_road_ui_glyphs[low];
    return road_ui_find_glyph('?');
}

static const road_ui_large_glyph_t *road_ui_find_large_glyph(
    unsigned int codepoint)
{
    unsigned int i;
    for (i = 0u; i < ROAD_UI_LARGE_GLYPH_COUNT; ++i)
        if (g_road_ui_large_glyphs[i].codepoint == codepoint)
            return &g_road_ui_large_glyphs[i];
    return 0;
}

static void road_ui_blend_glyph(road_pixel_t *pixels, int x, int y,
                                const unsigned char *packed,
                                unsigned int width, unsigned int height,
                                road_pixel_t color)
{
    unsigned int row, col;
    for (row = 0u; row < height; ++row) {
        int yy = y + (int)row;
        if (yy < 0 || yy >= ROAD_HEIGHT) continue;
        for (col = 0u; col < width; ++col) {
            int xx = x + (int)col;
            unsigned int pixel_index = row * width + col;
            unsigned int shade = packed[pixel_index >> 1u];
            unsigned int alpha;
            road_pixel_t old;
            unsigned int red, green, blue;
            if (xx < 0 || xx >= ROAD_WIDTH) continue;
            shade = (pixel_index & 1u) ? shade & 15u : shade >> 4u;
            if (!shade) continue;
            if (shade == 15u) {
                pixels[yy * ROAD_WIDTH + xx] = color;
                continue;
            }
            alpha = shade;
            old = pixels[yy * ROAD_WIDTH + xx];
            red = (((color >> 11) & 31u) * alpha +
                   ((old >> 11) & 31u) * (16u - alpha)) >> 4u;
            green = (((color >> 5) & 63u) * alpha +
                     ((old >> 5) & 63u) * (16u - alpha)) >> 4u;
            blue = ((color & 31u) * alpha +
                    (old & 31u) * (16u - alpha)) >> 4u;
            pixels[yy * ROAD_WIDTH + xx] =
                (road_pixel_t)((red << 11) | (green << 5) | blue);
        }
    }
}

static void road_ui_text(road_pixel_t *pixels, int x, int y,
                          const char *label, int large, road_pixel_t color)
{
    const unsigned char *cursor = (const unsigned char *)label;
    while (*cursor) {
        unsigned int codepoint = road_ui_next_codepoint(&cursor);
        const road_ui_large_glyph_t *big = large == 2 ?
            road_ui_find_large_glyph(codepoint) : 0;
        if (big) {
            road_ui_blend_glyph(pixels, x, y, big->alpha,
                ROAD_UI_LARGE_WIDTH, ROAD_UI_LARGE_HEIGHT, color);
            x += (int)big->advance;
        } else {
            const road_ui_glyph_t *glyph = road_ui_find_glyph(codepoint);
            road_ui_blend_glyph(pixels, x, y, glyph->alpha,
                ROAD_UI_SMALL_WIDTH, ROAD_UI_SMALL_HEIGHT, color);
            x += (int)glyph->advance;
        }
    }
}

static void draw_loading_page(road_pixel_t *pixels)
{
    road_pixel_t ink = rgb(235u, 232u, 208u);
    road_pixel_t accent = rgb(255u, 188u, 50u);
    unsigned int percent = g_loading_percent <= 100u ?
                           g_loading_percent : 100u;
    if (g_menu_background_ready)
        bda_memcpy(pixels, g_menu_background,
                   ROAD_PIXELS * sizeof(road_pixel_t));
    else box(pixels, 0, 0, ROAD_WIDTH, ROAD_HEIGHT,
             rgb(10u, 19u, 34u));
    box(pixels, 24, 62, 272, 121, rgb(17u, 24u, 35u));
    menu_text(pixels, 82, 77, "ROAD RASH", 2, ink);
    menu_text(pixels, 107, 108, "LOADING", 2, accent);
    menu_text(pixels, 53, 139, rr_course_names[g_frontend.course], 1, ink);
    if (g_loading_stage)
        menu_text(pixels, 53, 151, g_loading_stage, 1, ink);
    box(pixels, 52, 165, 216, 8, rgb(73u, 80u, 89u));
    box(pixels, 54, 167, (int)(percent * 212u / 100u), 4, accent);
    draw_number(pixels, 272, 165, percent, ink);
}

static void draw_virtual_button(road_pixel_t *pixels, int cx, int cy,
                                int radius, int active, const char *label)
{
    int dy;
    road_pixel_t rim = active ? rgb(255u, 206u, 66u) :
                     rgb(205u, 212u, 216u);
    road_pixel_t fill = active ? rgb(127u, 77u, 25u) :
                      rgb(37u, 49u, 59u);
    for (dy = -radius; dy <= radius; ++dy) {
        int dx;
        int yy = cy + dy;
        if (yy < 0 || yy >= ROAD_HEIGHT) continue;
        for (dx = -radius; dx <= radius; ++dx) {
            int xx = cx + dx;
            int distance = dx * dx + dy * dy;
            road_pixel_t color, old;
            unsigned int alpha, red, green, blue;
            if (xx < 0 || xx >= ROAD_WIDTH ||
                distance > radius * radius) continue;
            color = distance >= (radius - 3) * (radius - 3) ?
                rim : fill;
            alpha = distance >= (radius - 3) * (radius - 3) ?
                150u : active ? 112u : 92u;
            old = pixels[yy * ROAD_WIDTH + xx];
            red = (((color >> 11) & 31u) * alpha +
                   ((old >> 11) & 31u) * (256u - alpha)) >> 8u;
            green = (((color >> 5) & 63u) * alpha +
                     ((old >> 5) & 63u) * (256u - alpha)) >> 8u;
            blue = ((color & 31u) * alpha +
                    (old & 31u) * (256u - alpha)) >> 8u;
            pixels[yy * ROAD_WIDTH + xx] =
                (road_pixel_t)((red << 11) | (green << 5) | blue);
        }
    }
    menu_text(pixels, cx - (label[3] ? 12 : 9), cy - 4, label, 1,
              rgb(255u, 250u, 228u));
}

static void draw_virtual_controls(road_pixel_t *pixels)
{
    unsigned int place = road_game_finish_place(&g_game) + 1u;
    int separator_x;
    if (place > ROAD_OPPONENT_COUNT + 1u)
        place = ROAD_OPPONENT_COUNT + 1u;
    /* The original rank is inside the right gauge; keep it visible at 240p. */
    box(pixels, 8, 6, 90, 18, rgb(20u, 28u, 36u));
    box(pixels, 8, 6, 90, 1, rgb(225u, 181u, 69u));
    menu_text(pixels, 14, 11, "POS", 1, rgb(235u, 232u, 208u));
    draw_number(pixels, 42, 9, place, rgb(255u, 205u, 80u));
    separator_x = place >= 10u ? 58 : 50;
    box(pixels, separator_x + 3, 9, 2, 2, rgb(210u, 217u, 219u));
    box(pixels, separator_x + 2, 11, 2, 2, rgb(210u, 217u, 219u));
    box(pixels, separator_x + 1, 13, 2, 2, rgb(210u, 217u, 219u));
    box(pixels, separator_x, 15, 2, 2, rgb(210u, 217u, 219u));
    draw_number(pixels, separator_x + 10, 9,
                ROAD_OPPONENT_COUNT + 1u, rgb(235u, 232u, 208u));
    draw_virtual_button(pixels, ROAD_HIT_X, ROAD_HIT_Y,
                        ROAD_HIT_RADIUS,
                        (s32)(bda_gui_millisecond_count() -
                              g_touch_hit_visual_until_ms) < 0,
                        "HIT");
    draw_virtual_button(pixels, ROAD_KICK_X, ROAD_KICK_Y,
                        ROAD_KICK_RADIUS,
                        (s32)(bda_gui_millisecond_count() -
                              g_touch_kick_visual_until_ms) < 0,
                        "KICK");
    draw_virtual_button(pixels, ROAD_GAS_X, ROAD_GAS_Y,
                        ROAD_GAS_RADIUS, g_touch.throttle_on, "GAS");
    menu_text(pixels, ROAD_GAS_X - 6, ROAD_GAS_Y + 9,
              g_touch.throttle_on ? "ON" : "OFF", 1,
              rgb(255u, 250u, 228u));
}

static void draw_course_menu(road_pixel_t *pixels)
{
    static const char *const course_names_zh[RR_COURSE_COUNT] = {
        "公路", "峡谷", "城市", "纳帕", "综合"
    };
    unsigned int i;
    road_pixel_t ink = rgb(235u, 232u, 208u);
    road_pixel_t accent = rgb(255u, 188u, 50u);
    if (g_menu_background_ready) {
        bda_memcpy(pixels, g_menu_background,
                   ROAD_PIXELS * sizeof(road_pixel_t));
        box(pixels, 15, 13, 135, 222, rgb(13u, 17u, 29u));
        road_ui_text(pixels, 24, 25, "暴力摩托", 2, ink);
        road_ui_text(pixels, 27, 60, "关卡选择", 1, accent);
        road_ui_text(pixels, 25, 79, "等级", 1, ink);
        draw_number(pixels, 53, 80, g_profile.career.level + 1u, accent);
        road_ui_text(pixels, 77, 79, "金币", 1, ink);
        draw_number(pixels, 103, 80,
                    (unsigned int)g_profile.career.balance, accent);
        for (i = 0u; i < RR_COURSE_COUNT; ++i) {
            int y = 98 + (int)i * 17;
            if (i == g_frontend.course)
                box(pixels, 23, y - 2, 119, 16, rgb(90u, 46u, 25u));
            road_ui_text(pixels, 32, y, course_names_zh[i], 1,
                      i == g_frontend.course ? accent : ink);
        }
        road_ui_text(pixels, 25, 183, "车辆", 1, ink);
        draw_number(pixels, 57, 184, g_frontend.bike + 1u, accent);
        road_ui_text(pixels, 25, 199,
                  g_course_load_failed ? "加载失败" :
                  g_profile_save_failed ? "存档失败" :
                  g_frontend.bike != g_profile.career.bike ?
                  (road_career_can_buy(&g_profile.career,
                        g_frontend.bike) ? "购买车辆" : "余额不足") :
                  "开始比赛",
                  1, (g_course_load_failed || g_profile_save_failed) ?
                  rgb(255u, 90u, 70u) : ink);
        if (g_frontend.bike != g_profile.career.bike) {
            road_ui_text(pixels, 25, 215, "价格", 1, ink);
            draw_number(pixels, 57, 216,
                        road_career_price(g_frontend.bike), accent);
        } else {
            road_ui_text(pixels, 25, 215, "场次", 1, ink);
            draw_number(pixels, 57, 216, g_profile.race_count, accent);
        }
        if (g_frontend.bike == g_profile.career.bike &&
            g_profile.best_ms[g_frontend.course]) {
            road_ui_text(pixels, 82, 215, "最佳", 1, ink);
            draw_number(pixels, 110, 216,
                        g_profile.best_ms[g_frontend.course] / 1000u,
                        accent);
        }
        return;
    }
    box(pixels, 0, 0, ROAD_WIDTH, ROAD_HEIGHT, rgb(10u, 19u, 34u));
    box(pixels, 0, 0, ROAD_WIDTH, 5, accent);
    box(pixels, 26, 10, 268, 43, rgb(27u, 42u, 61u));
    road_ui_text(pixels, 120, 17, "暴力摩托", 2, ink);
    road_ui_text(pixels, 136, 53, "关卡选择", 1, accent);
    road_ui_text(pixels, 48, 70, "等级", 1, ink);
    draw_number(pixels, 80, 71, g_profile.career.level + 1u, accent);
    road_ui_text(pixels, 138, 70, "金币", 1, ink);
    draw_number(pixels, 170, 71,
                (unsigned int)g_profile.career.balance, accent);
    for (i = 0u; i < RR_COURSE_COUNT; ++i) {
        int y = 89 + (int)i * 18;
        if (i == g_frontend.course) {
            box(pixels, 48, y - 2, 224, 16, rgb(90u, 46u, 25u));
            menu_text(pixels, 57, y, "A", 1, accent);
        }
        road_ui_text(pixels, 81, y, course_names_zh[i], 1,
                  i == g_frontend.course ? accent : ink);
    }
    road_ui_text(pixels, 48, 181, "车辆", 1, accent);
    draw_number(pixels, 80, 182, g_frontend.bike + 1u, ink);
    if (g_course_load_failed)
        road_ui_text(pixels, 77, 198, "加载失败", 1,
                  rgb(255u, 90u, 70u));
    else if (g_profile_save_failed)
        road_ui_text(pixels, 77, 198, "存档失败", 1,
                  rgb(255u, 90u, 70u));
    else if (g_frontend.bike != g_profile.career.bike)
        road_ui_text(pixels, 77, 198,
                  road_career_can_buy(&g_profile.career, g_frontend.bike) ?
                  "购买车辆" : "余额不足", 1,
                  road_career_can_buy(&g_profile.career, g_frontend.bike) ?
                  ink : rgb(255u, 90u, 70u));
    else
        road_ui_text(pixels, 47, 198, "开始比赛", 1, ink);
    if (g_frontend.bike != g_profile.career.bike) {
        road_ui_text(pixels, 48, 215, "价格", 1, ink);
        draw_number(pixels, 80, 216,
                    road_career_price(g_frontend.bike), accent);
    } else {
        road_ui_text(pixels, 48, 215, "场次", 1, ink);
        draw_number(pixels, 80, 216, g_profile.race_count, accent);
    }
    if (g_frontend.bike == g_profile.career.bike &&
        g_profile.best_ms[g_frontend.course]) {
        road_ui_text(pixels, 151, 215, "最佳", 1, ink);
        draw_number(pixels, 183, 216,
                    g_profile.best_ms[g_frontend.course] / 1000u,
                    accent);
    }
    box(pixels, 0, ROAD_HEIGHT - 5, ROAD_WIDTH, 5, accent);
}

static void draw_title_menu(road_pixel_t *pixels)
{
    road_pixel_t ink = rgb(235u, 232u, 208u);
    road_pixel_t accent = rgb(255u, 188u, 50u);
    if (g_menu_background_ready)
        bda_memcpy(pixels, g_menu_background,
                   ROAD_PIXELS * sizeof(road_pixel_t));
    else
        box(pixels, 0, 0, ROAD_WIDTH, ROAD_HEIGHT,
            rgb(10u, 19u, 34u));
    box(pixels, 88, 12, 144, 35, rgb(13u, 19u, 31u));
    road_ui_text(pixels, 120, 18, "暴力摩托", 2, accent);
    box(pixels, 60, 105, 200, 131, rgb(13u, 19u, 31u));
    road_ui_text(pixels, 142, 111, "主菜单", 1, ink);
    box(pixels, 76, 128 + (int)g_frontend.title_selection * 30,
        168, 28, rgb(90u, 46u, 25u));
    road_ui_text(pixels, 120, 131, "关卡选择", 2,
        g_frontend.title_selection == 0u ? accent : ink);
    road_ui_text(pixels, 140, 161, "设置", 2,
        g_frontend.title_selection == 1u ? accent : ink);
    road_ui_text(pixels, 140, 191, "关于", 2,
        g_frontend.title_selection == 2u ? accent : ink);
    road_ui_text(pixels, 130, 219, "返回键退出", 1, ink);
}

static void draw_settings_page(road_pixel_t *pixels)
{
    road_pixel_t ink = rgb(235u, 232u, 208u);
    road_pixel_t accent = rgb(255u, 188u, 50u);
    if (g_menu_background_ready)
        bda_memcpy(pixels, g_menu_background,
                   ROAD_PIXELS * sizeof(road_pixel_t));
    else
        box(pixels, 0, 0, ROAD_WIDTH, ROAD_HEIGHT,
            rgb(10u, 19u, 34u));
    box(pixels, 38, 51, 244, 167, rgb(13u, 19u, 31u));
    road_ui_text(pixels, 140, 62, "设置", 2, accent);
    box(pixels, 62, 106, 196, 42, rgb(90u, 46u, 25u));
    road_ui_text(pixels, 105, 117, "音乐：", 2, ink);
    road_ui_text(pixels, 184, 117,
        g_frontend.music_enabled ? "开" : "关", 2, accent);
    road_ui_text(pixels, 108, 163, "确认键切换", 1, ink);
    if (g_profile_save_failed)
        road_ui_text(pixels, 132, 185, "存档失败", 1,
            rgb(255u, 90u, 70u));
    road_ui_text(pixels, 119, 199, "返回键返回", 1, ink);
}

static void draw_about_page(road_pixel_t *pixels)
{
    road_pixel_t ink = rgb(235u, 232u, 208u);
    road_pixel_t accent = rgb(255u, 188u, 50u);
    if (g_menu_background_ready)
        bda_memcpy(pixels, g_menu_background,
                   ROAD_PIXELS * sizeof(road_pixel_t));
    else
        box(pixels, 0, 0, ROAD_WIDTH, ROAD_HEIGHT,
            rgb(10u, 19u, 34u));
    box(pixels, 8, 8, 304, 224, rgb(13u, 19u, 31u));
    road_ui_text(pixels, 25, 17, "关于暴力摩托", 2, accent);
    road_ui_text(pixels, 239, 20, "返回键返回", 1, ink);
    box(pixels, 23, 52, 274, 1, rgb(105u, 77u, 43u));
    road_ui_text(pixels, 25, 58, "原版：ROAD RASH", 1, ink);
    road_ui_text(pixels, 25, 75, "平台：3DO", 1, ink);
    road_ui_text(pixels, 25, 92, "年份：1994", 1, ink);
    road_ui_text(pixels, 25, 109, "发行：Electronic Arts", 1, ink);
    road_ui_text(pixels, 25, 126, "类型：摩托竞速格斗", 1, ink);
    box(pixels, 23, 147, 274, 1, rgb(105u, 77u, 43u));
    road_ui_text(pixels, 25, 154, "移植：BBK 9588", 1, accent);
    road_ui_text(pixels, 25, 171, "移植作者：HelloClyde", 1, ink);
    road_ui_text(pixels, 25, 188, "赞助：唔识游水的鱼??", 1, ink);
    road_ui_text(pixels, 25, 205,
        "步步高电子词典游戏群（830340878）", 1, accent);
}

static int draw_frame(void)
{
    u32 render_start = bda_gui_millisecond_count();
    u32 present_start;
    void *previous_brush;
    int result;
    int trace = !g_loading_active &&
        g_frontend.screen == ROAD_SCREEN_RACE &&
        g_diag_render_frames != 0u;
    service_audio();
    if (trace) road_diag_checkpoint("FRAME_BEGIN", g_game.distance,
        g_game.speed, g_frontend.course);
    if (trace) road_diag_checkpoint("TEX_FRAME", g_surface_texture_count,
        (unsigned int)g_surface_textures,
        (unsigned int)g_runtime_road_textures[0].pixels);
    if (g_loading_active)
        draw_loading_page(g_landscape);
    else if (g_frontend.screen == ROAD_SCREEN_SETUP)
        draw_course_menu(g_landscape);
    else if (g_frontend.screen == ROAD_SCREEN_TITLE)
        draw_title_menu(g_landscape);
    else if (g_frontend.screen == ROAD_SCREEN_ABOUT)
        draw_about_page(g_landscape);
    else if (g_frontend.screen == ROAD_SCREEN_SETTINGS)
        draw_settings_page(g_landscape);
    else {
        service_audio();
        road_game_render(&g_game, g_landscape);
        if (g_frontend.screen == ROAD_SCREEN_RACE)
            draw_virtual_controls(g_landscape);
        if (trace) road_diag_checkpoint("FRAME_RENDER_DONE",
            g_game.distance, g_game.speed, 0u);
        if (g_frontend.screen == ROAD_SCREEN_PAUSE) {
            box(g_landscape, 90, 95, 142, 63, rgb(20u, 28u, 36u));
            menu_text(g_landscape, 120, 104, "PAUSED", 2,
                      rgb(255u, 188u, 50u));
            menu_text(g_landscape, 104, 132, "ENTER RESUME", 1,
                      rgb(235u, 232u, 208u));
            menu_text(g_landscape, 131, 145, "ESC MENU", 1,
                      rgb(235u, 232u, 208u));
        }
        if (g_frontend.screen == ROAD_SCREEN_RESULTS) {
            road_pixel_t ink = rgb(235u, 232u, 208u);
            box(g_landscape, 88, 105, 145, 128, rgb(20u, 28u, 36u));
            menu_text(g_landscape, 99, 114, "PLACE", 1, ink);
            draw_number(g_landscape, 176, 114,
                        g_profile.last_rank + 1u, ink);
            menu_text(g_landscape, 99, 131, "CASH", 1, ink);
            draw_number(g_landscape, 176, 131, g_last_award, ink);
            menu_text(g_landscape, 99, 148, "TIME", 1, ink);
            draw_number(g_landscape, 176, 148,
                        g_game.elapsed_ms / 1000u, ink);
            menu_text(g_landscape, 99, 165, "HITS", 1, ink);
            draw_number(g_landscape, 176, 165, g_game.hits, ink);
            menu_text(g_landscape, 99, 182, "HAZARDS", 1, ink);
            draw_number(g_landscape, 176, 182, g_game.hazard_hits, ink);
            menu_text(g_landscape, 99, 204, "ENTER", 1,
                      rgb(255u, 188u, 50u));
            if (g_profile_save_failed)
                menu_text(g_landscape, 99, 216, "SAVE FAILED", 1,
                          rgb(255u, 90u, 70u));
        }
    }
    if (trace) road_diag_checkpoint("ROTATE_BEGIN", g_game.distance,
        0u, 0u);
    service_audio();
    road_rotate_counterclockwise(g_landscape,
        (road_pixel_t *)(g_vx + 24));
    if (trace) road_diag_checkpoint("ROTATE_DONE", g_game.distance,
        0u, 0u);
    present_start = bda_gui_millisecond_count();
    g_game.last_render_ms = present_start - render_start;
    if (trace) road_diag_checkpoint("PRESENT_BEGIN", g_game.distance,
        g_game.last_render_ms, 0u);
    service_audio();
    result = bda_gui_draw_vx(g_back, 0, 0, g_vx);
    (void)bda_gui_draw_guard_begin();
    previous_brush = bda_gui_select_draw_object(g_draw, g_brush);
    result |= bda_gui_context_copy(
        g_back, 0, 0, PORTRAIT_WIDTH, PORTRAIT_HEIGHT,
        g_draw, 0, 0, BDA_GUI_COLOR_KEY_BLACK_RGB565
    );
    (void)bda_gui_select_draw_object(g_draw, previous_brush);
    (void)bda_gui_draw_guard_end();
    service_audio();
    g_game.last_present_ms =
        bda_gui_millisecond_count() - present_start;
    if (trace) {
        road_diag_checkpoint("PRESENT_DONE", g_game.distance,
            (unsigned int)result, g_game.last_present_ms);
        --g_diag_render_frames;
    }
    if (g_frontend.screen == ROAD_SCREEN_RACE) {
        ++g_diag_race_frames;
        if (g_diag_verbose && (g_diag_race_frames & 127u) == 0u)
            road_diag_checkpoint("RACE_HEARTBEAT", g_game.distance,
                g_game.last_render_ms, g_game.last_present_ms);
    }
    return result == 0;
}

static void loading_progress(unsigned int percent, const char *stage)
{
    if (!g_loading_active || !g_draw) return;
    g_loading_percent = percent;
    g_loading_stage = stage;
    road_diag_checkpoint("LOAD_PROGRESS", percent,
        g_frontend.course, 0u);
    (void)draw_frame();
    service_audio();
}

__attribute__((section(".text.bda_main")))
int bda_main(void)
{
    bda_frame_desc_t descriptor;
    bda_gui_message_t message;
    u32 frame_start;
    u32 previous_input = 0u;
    u32 close_wait = 0u;
    int closing = 0;
    int timer_started = 0;
    int result = 0;

    bda_memset(&descriptor, 0, sizeof(descriptor));
    bda_memset(&message, 0, sizeof(message));
    g_frame = 0;
    g_draw = 0;
    g_draw_owner = 0;
    g_back = 0;
    g_brush = 0;
    g_detached = 0;
    g_loading_active = 0;
    g_loading_percent = 0u;
    g_loading_stage = 0;
    g_touch.down = 0;
    g_touch.handled = 0;
    g_touch.throttle_on = 0;
    g_touch.attack_pending = 0;
    g_touch.kick_pending = 0;
    g_touch_hit_visual_until_ms = 0u;
    g_touch_kick_visual_until_ms = 0u;
    g_touch_attack_until_ms = 0u;
    g_touch_escape_suppressed = 0;
    g_touch_escape_until_ms = 0u;
    g_escape_key_seen = 0;
    g_escape_pending = 0;
    g_escape_due_ms = 0u;
    road_select_resource_root();
    road_diag_begin();
    if (!allocate_runtime_buffers()) {
        result = 7;
        goto cleanup;
    }
    road_frontend_init(&g_frontend, 1u);
    load_profile();
    g_frontend.music_enabled = g_profile.music_enabled;
    road_diag_checkpoint("PROFILE_DONE", g_profile.sequence,
        g_profile.career.level, g_profile.career.bike);
    g_course_load_failed = 0;
    g_profile_save_failed = 0;
    g_last_award = 0u;
    g_menu_background_ready = 0;
    g_menu_background = 0;
    road_game_init(&g_game);
    rr_audio_init(&g_audio_mixer);
    (void)load_disc_menu_background();
    road_diag_checkpoint("MENU_ASSET_DONE", g_menu_background_ready,
        0u, 0u);
    g_bike_specs_ready = load_disc_bike_specs();
    road_diag_checkpoint("BIKE_SPECS_DONE", g_bike_specs_ready,
        0u, 0u);
    g_frontend.bike_count = g_bike_specs_ready ? 15u : 1u;
    g_frontend.course = g_profile.course;
    g_frontend.bike = g_profile.career.bike < g_frontend.bike_count ?
                      g_profile.career.bike : 0u;
    if (!g_bike_specs_ready) g_profile.career.bike = 0u;
    (void)load_disc_audio();
    road_diag_checkpoint("SFX_DONE", g_audio_samples[0] != 0,
        g_sfx_source_drive, 0u);
    {
        int bike_frames_ready = load_disc_bikes();
        road_diag_checkpoint("BIKE_FRAMES_DONE", bike_frames_ready,
            bike_frames_ready ? ROAD_BIKE_FRAME_COUNT : 0u, 0u);
        if (!bike_frames_ready) {
#if ROAD_BIKE_COUNT > 0
            road_game_set_bikes(g_original_bikes, ROAD_BIKE_COUNT);
#endif
        }
    }
    (void)load_disc_road_textures();
    road_diag_checkpoint("ROAD_TEX_DONE", g_surface_texture_count,
        (unsigned int)g_surface_textures,
        (unsigned int)g_runtime_road_textures[0].pixels);
    init_vx();

    descriptor.title = "\xB1\xA9\xC1\xA6\xC4\xA6\xCD\xD0";
    descriptor.wndproc = window_proc;
    descriptor.height = PORTRAIT_WIDTH;
    descriptor.width = PORTRAIT_HEIGHT;
    g_frame = bda_gui_register_frame_desc(&descriptor);
    road_diag_checkpoint("GUI_REGISTER", (unsigned int)g_frame,
        0u, 0u);
    if (!g_frame || (s32)g_frame == -1) {
        g_frame = 0;
        result = 1;
        goto cleanup;
    }
    (void)bda_gui_frame_activate(g_frame, 0x100u);
    if (!acquire_draw(g_frame)) {
        result = 2;
        goto cleanup;
    }
    g_brush = bda_gui_draw_object_create(7u);
    if (!g_brush || (s32)(u32)g_brush == -1) {
        result = 3;
        goto cleanup;
    }
    g_back = bda_gui_compatible_context_create(g_draw);
    road_diag_checkpoint("GUI_BACK", (unsigned int)g_back,
        0u, 0u);
    if (!g_back || (s32)g_back == -1) {
        g_back = 0;
        result = 4;
        goto cleanup;
    }
    bda_gui_millisecond_timer_start();
    timer_started = 1;
    g_music_random_state = bda_gui_millisecond_count() ^
        g_profile.career.balance ^ 0x9e3779b9u;
    (void)choose_disc_music(ROAD_MUSIC_COUNT);
    road_diag_checkpoint("MUSIC_DONE", g_music_ready,
        g_music_ready ? g_music_stream.file_size : 0u,
        g_music_source_drive);
    if (g_audio_samples[0] || g_music_ready) {
        g_audio_saved_attenuation = bda_audio_get_attenuation();
        if (g_audio_saved_attenuation < 0 ||
            g_audio_saved_attenuation > 96)
            g_audio_saved_attenuation = 0;
        bda_audio_open_pcm(BDA_AUDIO_SAMPLE_RATE_22050,
                           BDA_AUDIO_BITS_16, BDA_AUDIO_CHANNELS_MONO);
        bda_audio_set_attenuation(BDA_AUDIO_ATTENUATION_FULL_SCALE);
        g_audio_pending_bytes = 0u;
        g_audio_pending_offset = 0u;
        g_audio_written_blocks = 0u;
        g_audio_ready_misses = 0u;
        g_audio_zero_writes = 0u;
        g_audio_open = 1;
        road_diag_checkpoint("AUDIO_OPEN",
            (unsigned int)g_audio_saved_attenuation,
            g_audio_samples[0] != 0, g_music_ready);
    } else {
        road_diag_checkpoint("AUDIO_NO_ASSETS", 0u, 0u, 0u);
    }
    frame_start = bda_gui_millisecond_count();
    if (!draw_frame()) {
        result = 5;
        goto cleanup;
    }
    road_diag_checkpoint("TITLE_READY", 0u, 0u, 0u);

    while (!g_detached) {
        int pumped = bda_gui_event_pump_frame_once(&message, g_frame);
        if (!closing) {
            u32 now = bda_gui_millisecond_count();
            u32 elapsed = now - frame_start;
            u32 input = read_controls();
            u32 pressed = input & ~previous_input;
            if (g_frontend.screen == ROAD_SCREEN_RACE &&
                (pressed & (ROAD_LEFT | ROAD_RIGHT)))
                road_diag_live("STEER_KEY", input & 3u,
                    (unsigned int)g_game.lane, g_game.speed);
            unsigned int old_screen = g_frontend.screen;
            unsigned int old_course = g_frontend.course;
            unsigned int old_bike = g_frontend.bike;
            unsigned int old_title_selection = g_frontend.title_selection;
            unsigned int old_music_enabled = g_frontend.music_enabled;
            unsigned int action = road_frontend_step(&g_frontend, pressed,
                                                     g_game.finished);
            if (old_music_enabled != g_frontend.music_enabled) {
                g_profile.music_enabled = g_frontend.music_enabled;
                g_profile_save_failed = !save_profile();
            }
            if (old_screen != g_frontend.screen)
                road_diag_live("UI_SCREEN", old_screen,
                    g_frontend.screen, input);
            if (old_screen == ROAD_SCREEN_RACE &&
                g_frontend.screen == ROAD_SCREEN_RESULTS)
                capture_race_result();
            if (action == ROAD_UI_EXIT) {
                closing = 1;
                (void)bda_gui_frame_stop(g_frame);
                (void)bda_gui_frame_release(g_frame);
            } else {
                if (action == ROAD_UI_START) {
                    if (g_frontend.bike != g_profile.career.bike) {
                        if (road_career_buy(&g_profile.career,
                                            g_frontend.bike))
                            g_profile_save_failed = !save_profile();
                    } else {
                        int started;
                        g_loading_active = 1;
                        loading_progress(0u, "PREPARING");
                        bda_sys_delay(25u);
                        started = start_course(g_frontend.course);
                        g_loading_active = 0;
                        g_touch.throttle_on = 0;
                        g_touch.attack_pending = g_touch.kick_pending = 0;
                        g_touch.down = 0;
                        g_touch.handled = 0;
                        frame_start = bda_gui_millisecond_count();
                        g_touch_escape_suppressed = 1;
                        g_touch_escape_until_ms = frame_start + 5000u;
                        now = frame_start;
                        elapsed = 0u;
                        if (!started) {
                            g_course_load_failed = 1;
                            g_frontend.screen = ROAD_SCREEN_SETUP;
                        } else {
                            g_profile.course = g_frontend.course;
                            g_profile_save_failed = !save_profile();
                        }
                    }
                }
                if (g_frontend.screen != ROAD_SCREEN_RACE) {
                    g_audio_mixer.engine_pitch = 0u;
                    rr_audio_stop_effect(&g_audio_mixer, 16u);
                    g_touch.throttle_on = 0;
                    g_touch.attack_pending = g_touch.kick_pending = 0;
                }
                if (g_frontend.screen == ROAD_SCREEN_RACE &&
                    elapsed >= FRAME_INTERVAL_MS && g_draw) {
                    unsigned int dealt_before = g_game.hits;
                    unsigned int received_before = g_game.rider_hits;
                    unsigned int bounce_before = g_game.bounce_events;
                    unsigned int crash_before = g_game.crash_elapsed_ms;
                    unsigned int sound_event;
                    frame_start = now;
                    g_game.last_frame_ms = elapsed;
                    if (g_diag_verbose && g_diag_race_frames < 3u)
                        road_diag_checkpoint("STEP_BEGIN",
                            g_game.distance, input, elapsed);
                    road_game_step(&g_game, input & 0x3fu, elapsed);
                    if (g_game.hits > dealt_before)
                        road_diag_live("RIDER_HIT",
                            g_game.attack_style, g_game.hits,
                            g_game.distance);
                    if (g_touch.attack_pending && (input & ROAD_ATTACK))
                        road_diag_live("TOUCH_ATTACK",
                            g_game.distance, g_game.attack_ms, elapsed);
                    if (!crash_before && g_game.crash_elapsed_ms)
                        road_diag_live("PLAYER_CRASH",
                            g_game.distance, g_game.rider_health,
                            g_game.bike_health);
                    else if (crash_before && !g_game.crash_elapsed_ms)
                        road_diag_live("PLAYER_REMOUNT",
                            g_game.distance, g_game.rider_health,
                            g_game.bike_health);
                    if (input & ROAD_ATTACK)
                        g_touch.attack_pending = g_touch.kick_pending = 0;
                    if (g_diag_verbose && g_diag_race_frames < 3u)
                        road_diag_checkpoint("STEP_DONE",
                            g_game.distance, g_game.speed,
                            g_game.last_frame_ms);
                    if (!choose_pending_forks()) {
                        g_frontend.screen = ROAD_SCREEN_SETUP;
                        g_course_load_failed = 1;
                        g_audio_mixer.engine_pitch = 0u;
                        if (!draw_frame()) {
                            result = 6;
                            break;
                        }
                        previous_input = input;
                        continue;
                    }
                    if (g_runtime_graph_valid &&
                        !rr_graph_race_sync(&g_runtime_graph_race,
                            &g_game, g_active_branch_mask))
                        rr_graph_race_disable(&g_runtime_graph_race,
                                               &g_game);
                    (void)road_frontend_step(&g_frontend, 0u,
                                             g_game.finished);
                    if (g_frontend.screen == ROAD_SCREEN_RESULTS)
                        capture_race_result();
                    g_audio_mixer.engine_pitch =
                        g_frontend.screen == ROAD_SCREEN_RACE &&
                        g_game.engine_pitch > 0 ?
                        (rr_u32)g_game.engine_pitch : 0u;
                    /* 3DO direct event 14 is an unarmed contact, while
                     * bounce event 26 plays sample index 2. */
                    if (g_game.hits > dealt_before ||
                        g_game.rider_hits > received_before)
                        rr_audio_play_race_event(&g_audio_mixer, 14u);
                    if (g_game.bounce_events > bounce_before)
                        rr_audio_play_race_event(&g_audio_mixer, 26u);
                    for (sound_event = 5u; sound_event <= 7u;
                         ++sound_event)
                        if (g_game.sound_event_mask & (1u << sound_event))
                            rr_audio_play_race_event(&g_audio_mixer,
                                                     sound_event);
                    for (sound_event = 2u; sound_event <= 4u;
                         ++sound_event)
                        if (g_game.sound_event_mask & (1u << sound_event))
                            rr_audio_play_race_event(&g_audio_mixer,
                                                     sound_event);
                    if (road_game_road_skid_active(&g_game)) {
                        if (!rr_audio_effect_active(&g_audio_mixer, 16u))
                            rr_audio_play_race_event(&g_audio_mixer, 16u);
                    } else rr_audio_stop_effect(&g_audio_mixer, 16u);
                    if (!draw_frame()) {
                        result = 6;
                        break;
                    }
                } else if ((old_screen != g_frontend.screen ||
                            old_course != g_frontend.course ||
                            old_bike != g_frontend.bike ||
                            old_title_selection != g_frontend.title_selection ||
                            old_music_enabled != g_frontend.music_enabled ||
                            action == ROAD_UI_START) &&
                           g_draw && !draw_frame()) {
                    result = 6;
                    break;
                }
            }
            previous_input = input;
        } else if (!pumped || ++close_wait >= 128u) {
            break;
        }
        service_audio();
        bda_sys_delay(1u);
    }

cleanup:
    road_diag_checkpoint("APP_EXIT", (unsigned int)result,
        g_game.distance, g_diag_race_frames);
    release_disc_audio();
    road_pack_shutdown();
    if (g_menu_background) {
        bda_free(g_menu_background);
        g_menu_background = 0;
    }
    release_disc_bikes();
    release_disc_cars();
    release_disc_effect_sprites();
    if (timer_started) bda_gui_millisecond_timer_stop();
    if (g_frame && !closing) {
        (void)bda_gui_frame_stop(g_frame);
        (void)bda_gui_frame_release(g_frame);
    }
    if (g_back) {
        bda_gui_compatible_context_free(g_back);
        g_back = 0;
    }
    release_draw();
    if (g_frame) bda_gui_close_frame(g_frame);
    release_runtime_buffers();
    return result;
}

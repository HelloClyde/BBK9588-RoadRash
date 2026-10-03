#include <assert.h>
#include <stdio.h>

/* Exercise the renderer's projected masks, which are intentionally private
 * to road_core.c and consumed by both scanline and textured road passes. */
#include "road_core.c"

static signed char flat[512], fork_elevation[512];
static signed char near_clip_rise[512], near_clip_bend[512];
static unsigned short widths[512];
static unsigned short near_clip_widths[512], near_clip_families[512];
static unsigned char cross_section[512];
static unsigned char near_clip_profile[512], near_clip_selector[512];
static road_hill_profile_t center_band_profile;
static road_pixel_t frame[ROAD_PIXELS];
static const road_pixel_t marker_pixel = 0x07e0u;
static const road_sprite_t marker = {1u, 1u, &marker_pixel};
static const road_pixel_t far_pixel = 0x001fu;
static const road_sprite_t far_marker = {1u, 1u, &far_pixel};
static road_sprite_t overlap_cars[2];
static road_sprite_t effect_markers[ROAD_EFFECT_SPRITE_FRAMES];
static road_pixel_t tall_pixels[14u * 140u];
static const road_sprite_t tall_marker = {14u, 140u, tall_pixels};
static const road_static_placement_t near_static_placement = {
    10u, 200, 0u, 0u, 0u
};
static const road_scenery_placement_t near_scenery_placement = {
    10u, 0u, 0u, 8u, 16000u
};
static const road_pixel_t road_red[4] = {
    0xf800u, 0xf800u, 0xf800u, 0xf800u
};
static road_sprite_t red_road_textures[27];
static road_pixel_t tall_crest_pixels[14u * 280u];
static const road_sprite_t tall_crest_marker = {
    14u, 280u, tall_crest_pixels
};
static const road_static_placement_t far_crest_placement = {
    10u, 0, 0u, 0u, 0u
};
static const road_static_placement_t marker_placement = {
    9u, 0, 0u, 0u, 0u
};

int main(void)
{
    road_game_t game;
    unsigned int sample, channel;
    for (sample = 0u; sample < 512u; ++sample) {
        widths[sample] = 256u;
        fork_elevation[sample] = sample < 8u ? 40 :
            sample < 16u ? -40 : 0;
    }
    for (sample = 0u; sample < ROAD_EFFECT_SPRITE_FRAMES; ++sample)
        effect_markers[sample] = marker;
    for (sample = 0u; sample < 14u * 140u; ++sample)
        tall_pixels[sample] = marker_pixel;
    for (sample = 0u; sample < 14u * 280u; ++sample)
        tall_crest_pixels[sample] = marker_pixel;
    for (sample = 0u; sample < 27u; ++sample) {
        red_road_textures[sample].width = 1u;
        red_road_textures[sample].height = 4u;
        red_road_textures[sample].pixels = road_red;
    }
    overlap_cars[0] = marker;
    overlap_cars[1] = far_marker;
    road_game_set_course(flat, 512u);
    road_game_set_elevation(flat, 512u);
    road_game_set_widths(widths, widths, 512u);
    road_game_set_cross_section(cross_section, cross_section,
        cross_section, cross_section, cross_section, 512u);
    road_game_set_fork_preview(flat, fork_elevation, widths, widths,
        512u, 1u, 0u);
    road_game_set_fork_span(100u);
    road_game_init(&game);
    for (sample = 0u; sample < ROAD_OPPONENT_COUNT; ++sample)
        game.opponents[sample].distance = 0u;
    road_game_render(&game, frame);
    for (sample = 8u; sample < 16u; ++sample) {
        assert(g_road_surface_visible[0][sample - 1u]);
        assert(!g_road_surface_visible[1][sample - 1u]);
    }
    road_game_set_fork_preview(0, 0, 0, 0, 0u, 0u, 0u);
    road_game_render(&game, frame);
    for (channel = 0u; channel < 2u; ++channel)
        for (sample = 0u; sample < 32u; ++sample)
            assert(g_road_surface_visible[channel][sample]);
    road_game_set_elevation(fork_elevation, 512u);
    road_game_set_static_scenery(&marker, 1u, &marker_placement, 1u);
    road_game_seek(&game, 0u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(9u * 256u);
        int projected = road_project_y(&game, 9u * 256u, y);
        int x = road_center(&game, 9u * 256u, y - HORIZON);
        assert(g_projected_sort_depth[8] < projected - 1);
        assert(frame[(projected - 1) * ROAD_WIDTH + x] != marker_pixel);
        g_projected_sort_depth[8] = ROAD_VIEW_BOTTOM;
        draw_static_scenery(&game, frame, 9u);
        assert(frame[(projected - 1) * ROAD_WIDTH + x] == marker_pixel);
        g_projected_sort_depth[8] = ROAD_VIEW_BOTTOM + 10;
        assert(road_roadside_object_visible(9u, ROAD_VIEW_BOTTOM));
        assert(!road_roadside_object_visible(9u, ROAD_VIEW_BOTTOM + 1));
    }
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_cars(&marker, 1u);
    for (sample = 0u; sample < ROAD_TRAFFIC_COUNT; ++sample)
        game.traffic[sample].distance = 0u;
    for (sample = 0u; sample < ROAD_CROSSING_COUNT; ++sample)
        game.crossing[sample].distance = 0u;
    game.traffic[0].distance = 10u * 256u;
    game.traffic[0].lane = 200;
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int projected = road_project_y(&game, 10u * 256u, y);
        int height = 4 + (y - HORIZON) * 2 / 5;
        int x = road_center(&game, 10u * 256u, y - HORIZON) +
            game.traffic[0].lane * road_projection_unit(10u * 256u) / 111;
        assert(g_projected_sort_depth[9] <= projected - height);
        assert(road_projected_secondary_visible(9u * 256u + 128u,
            g_projected_sort_depth[9] - 1));
        assert(!road_projected_secondary_visible(9u * 256u + 128u,
            g_projected_sort_depth[9]));
        assert(!road_projected_secondary_visible(10u * 256u,
            projected - height));
        assert(frame[(projected - 1) * ROAD_WIDTH + x] !=
            marker_pixel);
    }
    road_game_set_elevation(flat, 512u);
    road_game_seek(&game, 0u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int projected = road_project_y(&game, 10u * 256u, y);
        assert(road_projected_secondary_visible(10u * 256u,
            projected - 10));
        int x = road_center(&game, 10u * 256u, y - HORIZON) +
            game.traffic[0].lane * road_projection_unit(10u * 256u) / 111;
        assert(frame[(projected - 1) * ROAD_WIDTH + x] ==
            marker_pixel);
    }
    road_game_set_cars(0, 0u);
    road_game_set_effect_sprites(effect_markers, 1u);
    road_game_set_elevation(fork_elevation, 512u);
    game.effects[0].active = 1u;
    game.effects[0].distance = 14u * 256u;
    game.effects[0].lane = 200;
    game.effects[0].travel_mode = 0u;
    game.effects[0].sprite_index = 0u;
    road_game_seek(&game, 0u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(14u * 256u);
        int projected = road_project_y(&game, 14u * 256u, y);
        int size = 7 + (y - HORIZON) / 3;
        int x = road_center(&game, 14u * 256u, y - HORIZON) +
            game.effects[0].lane * road_projection_unit(14u * 256u) / 111;
        assert(!road_projected_secondary_visible(14u * 256u,
            projected - size * 2));
        assert(frame[(projected - 1) * ROAD_WIDTH + x] != marker_pixel);
    }
    road_game_set_elevation(flat, 512u);
    road_game_seek(&game, 0u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(14u * 256u);
        int projected = road_project_y(&game, 14u * 256u, y);
        int x = road_center(&game, 14u * 256u, y - HORIZON) +
            game.effects[0].lane * road_projection_unit(14u * 256u) / 111;
        assert(frame[(projected - 1) * ROAD_WIDTH + x] == marker_pixel);
    }
    game.effects[0].active = 0u;
    road_game_set_cars(overlap_cars, 2u);
    game.traffic[0].distance = 10u * 256u;
    game.traffic[0].lane = 200;
    game.traffic[1].distance = 11u * 256u;
    game.traffic[1].lane = 200;
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int near_y = road_project_y(&game, 10u * 256u, y);
        int x = road_center(&game, 10u * 256u, y - HORIZON) +
            game.traffic[0].lane * road_projection_unit(10u * 256u) / 111;
        assert(frame[(near_y - 3) * ROAD_WIDTH + x] == marker_pixel);
    }
    game.traffic[1].distance = 0u;
    for (sample = 0u; sample < ROAD_EFFECT_SPRITE_FRAMES; ++sample)
        effect_markers[sample] = far_marker;
    road_game_set_effect_sprites(effect_markers, 1u);
    game.effects[0].active = 1u;
    game.effects[0].distance = 11u * 256u;
    game.effects[0].lane = 200;
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int near_y = road_project_y(&game, 10u * 256u, y);
        int x = road_center(&game, 10u * 256u, y - HORIZON) +
            game.traffic[0].lane * road_projection_unit(10u * 256u) / 111;
        assert(frame[(near_y - 3) * ROAD_WIDTH + x] == marker_pixel);
    }
    game.effects[0].active = 0u;
    game.traffic[0].distance = 0u;
    game.traffic[1].distance = 11u * 256u;
    game.traffic[1].lane = 220;
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int near_y = road_project_y(&game, 10u * 256u, y);
        int x = road_center(&game, 10u * 256u, y - HORIZON) +
            200 * road_projection_unit(10u * 256u) / 111;
        assert(frame[(near_y - 5) * ROAD_WIDTH + x] == far_pixel);
        road_game_set_static_scenery(&tall_marker, 1u,
            &near_static_placement, 1u);
        road_game_render(&game, frame);
        assert(frame[(near_y - 5) * ROAD_WIDTH + x] == marker_pixel);
    }
    road_game_set_static_scenery(0, 0u, 0, 0u);
    road_game_set_scenery(&marker, 1u);
    road_game_set_scenery_placements(&near_scenery_placement, 1u);
    road_game_render(&game, frame);
    {
        int y = HORIZON + road_projection_dy(10u * 256u);
        int near_y = road_project_y(&game, 10u * 256u, y);
        int dy = y - HORIZON;
        int edge = road_projection_unit(10u * 256u) * 256 / 512;
        int size = 2 + dy / 3;
        int x = road_center(&game, 10u * 256u, dy) + edge + size +
            (int)near_scenery_placement.lateral * dy / 4096;
        assert(frame[(near_y - 5) * ROAD_WIDTH + x] == marker_pixel);
    }
    road_game_set_scenery_placements(0, 0u);
    road_game_set_cars(0, 0u);
    game.traffic[1].distance = 0u;
    road_game_set_elevation(fork_elevation, 512u);
    road_game_set_surface_textures(red_road_textures, 27u);
    road_game_set_static_scenery(&tall_crest_marker, 1u,
        &far_crest_placement, 1u);
    road_game_seek(&game, 0u);
    road_game_render(&game, frame);
    {
        int yy = HORIZON + road_projection_dy(10u * 256u);
        int projected = road_project_y(&game, 10u * 256u, yy);
        int height = 280 * (yy - HORIZON) / 140;
        assert(projected - height < g_projected_sort_depth[9]);
        assert(frame[60u * ROAD_WIDTH + ROAD_VIEW_WIDTH / 2] == marker_pixel);
        assert(frame[69u * ROAD_WIDTH + ROAD_VIEW_WIDTH / 2] == road_red[0]);
        draw_static_scenery(&game, frame, 10u);
        assert(frame[69u * ROAD_WIDTH + ROAD_VIEW_WIDTH / 2] == marker_pixel);
    }
    /* A rising, narrow dual road brings the 0xd2 plane into the active
     * viewport. Its closest center segment starts behind that plane and
     * must retain both bridge colors after clipping. */
    for (sample = 0u; sample < 512u; ++sample) {
        near_clip_widths[sample] = 16u;
        near_clip_profile[sample] = 4u;
    }
    near_clip_rise[1] = 100;
    near_clip_bend[1] = 80;
    road_game_set_course(flat, 512u);
    road_game_set_elevation(near_clip_rise, 512u);
    road_game_set_widths(near_clip_widths, near_clip_widths, 512u);
    road_game_set_cross_section(cross_section, cross_section,
        cross_section, cross_section, near_clip_profile, 512u);
    road_game_set_margin_resources(1u, near_clip_selector,
        near_clip_selector, near_clip_families, near_clip_families,
        cross_section, near_clip_profile, 512u);
    road_game_set_fork_preview(near_clip_bend, near_clip_rise,
        near_clip_widths, near_clip_widths, 512u, 1u, 0u);
    road_game_set_fork_span(82u);
    road_game_seek(&game, 256u);
    road_game_render(&game, frame);
    for (sample = 0u; sample < ROAD_PIXELS; ++sample) frame[sample] = 0u;
    draw_fork_profile_center_sample(&game, frame, 2u);
    {
        unsigned int pale = 0u, bridge = 0u;
        for (sample = 0u; sample < ROAD_PIXELS; ++sample) {
            pale += frame[sample] == 0xa534u;
            bridge += frame[sample] == 0x3188u;
        }
        assert(pale > 0u && bridge > 0u);
    }
    center_band_profile.start_sample = 0u;
    center_band_profile.end_sample = 512u;
    road_game_set_hill_profiles(&center_band_profile, 1u);
    road_game_set_fork_hill_profiles(&center_band_profile, 1u);
    road_game_set_hill_tiles(&marker, 0, 0, 1u);
    /* The source changes from solid fill to the left inner RHIL child once
     * dual-lane separation clears 0x100 plus the inner margins. */
    g_fork_forward_left_step[1u] = 0x100;
    recalculate_fork_clearance();
    assert(g_fork_forward_clearance_index == 1u);
    for (sample = 0u; sample < ROAD_PIXELS; ++sample) frame[sample] = 0u;
    draw_fork_profile_center_sample(&game, frame, 2u);
    {
        unsigned int textured = 0u, solid = 0u;
        for (sample = 0u; sample < ROAD_PIXELS; ++sample) {
            textured += frame[sample] == marker_pixel;
            solid += frame[sample] == 0xa534u ||
                frame[sample] == 0x3188u;
        }
        assert(textured > 0u && solid == 0u);
    }
    /* A later narrower node must not close the transition again. */
    g_fork_forward_left_step[2u] = 0;
    recalculate_fork_clearance();
    assert(fork_profile_center_has_clearance(3u));
    /* The original transition shrinks only the two facing shoulders and
     * flattens their profile until the latched clearance node. */
    cross_section[1u] = 30u;
    cross_section[2u] = 30u;
    cross_section[3u] = 30u;
    cross_section[4u] = 30u;
    road_game_set_fork_slopes(cross_section, cross_section, 512u);
    g_fork_forward_left_step[1u] = 40;
    g_fork_forward_left_step[2u] = 120;
    g_fork_forward_left_step[3u] = 400;
    recalculate_fork_clearance();
    assert(g_fork_forward_clearance_index == 3u);
    {
        int flatten = 0;
        road_hill_section_t transition_section, clear_section;
        assert(fork_transition_inner_margin(2u, 2u, 0u, 1u,
            30, &flatten) == 10 && flatten);
        assert(fork_transition_inner_margin(3u, 3u, 0u, 1u,
            30, &flatten) == 19 && flatten);
        assert(fork_transition_inner_margin(3u, 3u, 0u, 0u,
            30, &flatten) == 30 && !flatten);
        assert(fork_transition_inner_margin(4u, 4u, 0u, 1u,
            30, &flatten) == 30 && !flatten);
        center_band_profile.y[1][0] = 50;
        hill_screen_section(&game, 3u * 256u, 0u,
            &transition_section);
        hill_screen_section(&game, 4u * 256u, 0u,
            &clear_section);
        assert(transition_section.valid && clear_section.valid);
        assert(transition_section.y[1][1] ==
            transition_section.y[1][0]);
        assert(clear_section.y[1][1] < clear_section.y[1][0]);
        center_band_profile.y[1][0] = 0;
    }
    g_fork_reverse_mode = 1;
    g_fork_reverse_step_start = 100u;
    g_fork_reverse_step_count = 81u;
    g_fork_sample_shift = 0;
    g_fork_reverse_left_step[80u] = 0;
    g_fork_reverse_left_step[79u] = 0x100;
    g_fork_reverse_left_step[78u] = 0;
    recalculate_fork_clearance();
    assert(g_fork_reverse_clearance_offset == 1u);
    assert(fork_profile_center_has_clearance(178u));
    cross_section[177u] = cross_section[178u] =
        cross_section[179u] = cross_section[180u] = 30u;
    g_fork_reverse_left_step[179u - g_fork_reverse_step_start] = 40;
    g_fork_reverse_left_step[178u - g_fork_reverse_step_start] = 120;
    g_fork_reverse_left_step[177u - g_fork_reverse_step_start] = 400;
    recalculate_fork_clearance();
    assert(g_fork_reverse_clearance_offset == 3u);
    {
        int flatten = 0;
        assert(fork_transition_inner_margin(179u, 179u, 0u, 1u,
            30, &flatten) == 10 && flatten);
        assert(fork_transition_inner_margin(178u, 178u, 0u, 1u,
            30, &flatten) == 19 && flatten);
        assert(fork_transition_inner_margin(177u, 177u, 0u, 1u,
            30, &flatten) == 30 && !flatten);
        g_fork_main_channel = 1u;
        assert(fork_transition_inner_margin(179u, 179u, 0u, 0u,
            30, &flatten) == 10 && flatten);
        assert(fork_transition_inner_margin(179u, 179u, 1u, 1u,
            30, &flatten) == 10 && flatten);
        assert(fork_transition_inner_margin(179u, 179u, 0u, 1u,
            30, &flatten) == 30 && !flatten);
    }
    {
        road_hill_section_t left = {0}, right = {0};
        unsigned int point;
        g_fork_main_channel = 0u;
        g_fork_reverse_mode = 0;
        g_fork_forward_clearance_index = 20u;
        left.profile_count[1] = right.profile_count[0] = 4u;
        for (point = 0u; point < 4u; ++point) {
            left.x[1][point] = (int)point * 20;
            right.x[0][point] = 100 - (int)point * 20;
            left.y[1][point] = right.y[0][point] =
                100 - (int)point * 10;
        }
        reconcile_fork_hill_sections(&left, &right, 10u);
        assert(left.profile_count[1] == 3u);
        assert(right.profile_count[0] == 2u);
        assert(left.x[1][3] == 60 && right.x[0][2] == 60);
        assert(left.y[1][3] == 70 && right.y[0][2] == 70);
        right.x[0][0] = 200;
        right.x[0][1] = 180;
        right.x[0][2] = 160;
        right.x[0][3] = 140;
        left.profile_count[1] = right.profile_count[0] = 4u;
        reconcile_fork_hill_sections(&left, &right, 10u);
        assert(left.profile_count[1] == 4u &&
            right.profile_count[0] == 4u);
    }
    puts("Fork, crest clipping and far-to-near scene draw order passed.");
    return 0;
}

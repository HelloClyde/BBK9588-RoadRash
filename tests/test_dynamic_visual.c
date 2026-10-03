#include <assert.h>
#include <stdio.h>
#include "road_core.h"

static road_pixel_t frame[ROAD_PIXELS];

static unsigned int color_height(road_pixel_t color)
{
    unsigned int x, y, first = ROAD_HEIGHT, last = 0u, found = 0u;
    for (y = 0u; y < ROAD_HEIGHT; ++y)
        for (x = 0u; x < ROAD_WIDTH; ++x)
            if (frame[y * ROAD_WIDTH + x] == color) {
                if (y < first) first = y;
                if (y > last) last = y;
                found = 1u;
            }
    return found ? last - first + 1u : 0u;
}

int main(void)
{
    static road_pixel_t player_color = 0x0f55u;
    static road_pixel_t opponent_color = 0xf54au;
    static road_pixel_t kick_right_color = 0x9d75u;
    static road_pixel_t kick_left_color = 0xb3abu;
    static road_pixel_t reaction_color = 0xa48du;
    static road_pixel_t pedestrian_color = 0x19fbu;
    road_sprite_t bikes[ROAD_BIKE_FRAME_COUNT];
    road_sprite_t pedestrian[ROAD_EFFECT_SPRITE_FRAMES];
    road_game_t game;
    unsigned int i, near_opponent, near_pedestrian, far_opponent;
    for (i = 0u; i < ROAD_BIKE_FRAME_COUNT; ++i) {
        bikes[i].width = 1u;
        bikes[i].height = 1u;
        bikes[i].pixels = &player_color;
    }
    bikes[ROAD_BIKE_OPPONENT_INDEX].pixels = &opponent_color;
    for (i = 0u; i < ROAD_BIKE_KICK_FRAME_COUNT; ++i) {
        bikes[ROAD_BIKE_KICK_RIGHT_FIRST + i].pixels = &kick_right_color;
        bikes[ROAD_BIKE_KICK_LEFT_FIRST + i].pixels = &kick_left_color;
    }
    for (i = 0u; i < ROAD_BIKE_OPPONENT_HIT_COUNT; ++i)
        bikes[ROAD_BIKE_OPPONENT_HIT_FIRST + i].pixels = &reaction_color;
    for (i = 0u; i < ROAD_EFFECT_SPRITE_FRAMES; ++i) {
        pedestrian[i].width = 1u;
        pedestrian[i].height = 1u;
        pedestrian[i].pixels = &pedestrian_color;
    }
    road_game_set_bikes(bikes, ROAD_BIKE_FRAME_COUNT);
    road_game_set_effect_sprites(pedestrian, 1u);
    road_game_init(&game);
    for (i = 0u; i < ROAD_OPPONENT_COUNT; ++i)
        game.opponents[i].distance = 0u;
    game.opponents[2].distance = 480u;
    game.opponents[2].lane = -30;
    game.effects[0].active = 1u;
    game.effects[0].distance = 480u;
    game.effects[0].lane = 30;
    game.effects[0].sprite_index = 0u;
    game.effects[0].travel_mode = 4u;
    game.effects[0].animation = ROAD_EFFECT_ANIM_STANDING;
    road_game_render(&game, frame);
    near_opponent = color_height(opponent_color);
    near_pedestrian = color_height(pedestrian_color);
    assert(near_opponent >= 55u && near_opponent <= 70u);
    assert(near_pedestrian >= 48u && near_pedestrian <= 62u);
    assert(near_opponent > near_pedestrian);
    game.opponents[2].distance = 960u;
    game.effects[0].active = 0u;
    road_game_render(&game, frame);
    far_opponent = color_height(opponent_color);
    assert(far_opponent > 0u && far_opponent < near_opponent);
    game.attack_ms = ROAD_ATTACK_DURATION_MS;
    game.attack_style = 1u;
    game.attack_side = 1u;
    road_game_render(&game, frame);
    assert(color_height(kick_right_color) > 0u);
    game.attack_side = 0u;
    road_game_render(&game, frame);
    assert(color_height(kick_left_color) > 0u);
    game.attack_ms = 0u;
    game.opponents[2].hit_reaction_ms = 360u;
    road_game_render(&game, frame);
    assert(color_height(reaction_color) > 0u);
    road_game_set_bikes(0, 0u);
    road_game_set_effect_sprites(0, 0u);
    puts("Opponent/pedestrian scale, kick sides, and hit reaction: PASS");
    return 0;
}

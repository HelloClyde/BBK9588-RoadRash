#include <assert.h>
#include <stdio.h>
#include "road_frontend.h"

int main(void)
{
    road_frontend_t ui;
    road_frontend_init(&ui, 15u);
    assert(ui.screen == ROAD_SCREEN_TITLE);
    road_frontend_step(&ui, ROAD_UI_COURSE_NEXT, 0);
    assert(ui.title_selection == 1u);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.screen == ROAD_SCREEN_SETTINGS);
    assert(ui.music_enabled == 1u);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.music_enabled == 0u);
    road_frontend_step(&ui, ROAD_UI_COURSE_NEXT, 0);
    assert(ui.music_enabled == 1u);
    road_frontend_step(&ui, ROAD_UI_BACK, 0);
    assert(ui.screen == ROAD_SCREEN_TITLE);
    road_frontend_step(&ui, ROAD_UI_COURSE_NEXT, 0);
    assert(ui.title_selection == 2u);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.screen == ROAD_SCREEN_ABOUT);
    road_frontend_step(&ui, ROAD_UI_BACK, 0);
    assert(ui.screen == ROAD_SCREEN_TITLE);
    road_frontend_step(&ui, ROAD_UI_COURSE_NEXT, 0);
    assert(ui.title_selection == 0u);
    assert(road_frontend_step(&ui, ROAD_UI_CONFIRM, 0) == ROAD_UI_NONE);
    assert(ui.screen == ROAD_SCREEN_SETUP);
    road_frontend_step(&ui, ROAD_UI_COURSE_PREV | ROAD_UI_BIKE_PREV, 0);
    assert(ui.course == 4u && ui.bike == 14u);
    assert(road_frontend_step(&ui, ROAD_UI_CONFIRM, 0) == ROAD_UI_START);
    assert(ui.screen == ROAD_SCREEN_SETUP);
    road_frontend_race_started(&ui);
    assert(ui.screen == ROAD_SCREEN_RACE);
    road_frontend_step(&ui, ROAD_UI_BACK, 0);
    assert(ui.screen == ROAD_SCREEN_PAUSE);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.screen == ROAD_SCREEN_RACE);
    road_frontend_step(&ui, 0u, 1);
    assert(ui.screen == ROAD_SCREEN_RESULTS);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 1);
    assert(ui.screen == ROAD_SCREEN_SETUP);
    road_frontend_step(&ui, ROAD_UI_BACK, 0);
    assert(ui.screen == ROAD_SCREEN_TITLE);
    assert(road_frontend_step(&ui, ROAD_UI_BACK, 0) == ROAD_UI_EXIT);
    puts("Front-end title/setup/pause/results flow: PASS");
    return 0;
}

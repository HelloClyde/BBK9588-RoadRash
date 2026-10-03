#include <assert.h>
#include <stdio.h>
#include "road_landscape_input.h"
#include "road_frontend.h"

int main(void)
{
    road_frontend_t ui;
    road_frontend_init(&ui, 15u);
    assert(road_landscape_direction_input(ROAD_PHYSICAL_RIGHT, 0) ==
           ROAD_UI_COURSE_NEXT);
    road_frontend_step(&ui,
        road_landscape_direction_input(ROAD_PHYSICAL_RIGHT, 0), 0);
    assert(ui.title_selection == 1u);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.screen == ROAD_SCREEN_SETTINGS);
    road_frontend_step(&ui, ROAD_UI_BACK, 0);
    road_frontend_step(&ui,
        road_landscape_direction_input(ROAD_PHYSICAL_LEFT, 0), 0);
    road_frontend_step(&ui, ROAD_UI_CONFIRM, 0);
    assert(ui.screen == ROAD_SCREEN_SETUP);
    road_frontend_step(&ui,
        road_landscape_direction_input(ROAD_PHYSICAL_RIGHT, 0), 0);
    assert(ui.course == 1u);
    road_frontend_step(&ui,
        road_landscape_direction_input(ROAD_PHYSICAL_UP, 0), 0);
    assert(ui.bike == 1u);
    road_frontend_step(&ui,
        road_landscape_direction_input(ROAD_PHYSICAL_DOWN, 0), 0);
    assert(ui.bike == 0u);
    assert(road_landscape_direction_input(ROAD_PHYSICAL_UP, 1) == ROAD_RIGHT);
    assert(road_landscape_direction_input(ROAD_PHYSICAL_DOWN, 1) == ROAD_LEFT);
    assert(road_landscape_direction_input(ROAD_PHYSICAL_LEFT |
           ROAD_PHYSICAL_RIGHT, 1) == 0u);
    puts("Clockwise landscape direction mapping: PASS");
    return 0;
}

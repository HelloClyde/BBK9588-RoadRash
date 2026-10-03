#include "road_frontend.h"
#include "course_catalog.h"

void road_frontend_init(road_frontend_t *ui, unsigned int bike_count)
{
    ui->screen = ROAD_SCREEN_TITLE;
    ui->course = 0u;
    ui->bike = 0u;
    ui->bike_count = bike_count ? bike_count : 1u;
    ui->title_selection = 0u;
    ui->music_enabled = 1u;
}

void road_frontend_race_started(road_frontend_t *ui)
{
    ui->screen = ROAD_SCREEN_RACE;
}

unsigned int road_frontend_step(road_frontend_t *ui,
                                unsigned int pressed,
                                int race_finished)
{
    if (ui->screen == ROAD_SCREEN_RACE && race_finished) {
        ui->screen = ROAD_SCREEN_RESULTS;
        return ROAD_UI_NONE;
    }
    if (pressed & ROAD_UI_BACK) {
        if (ui->screen == ROAD_SCREEN_TITLE) return ROAD_UI_EXIT;
        if (ui->screen == ROAD_SCREEN_RACE)
            ui->screen = ROAD_SCREEN_PAUSE;
        else ui->screen = (ROAD_SCREEN_SETUP == ui->screen ||
                          ROAD_SCREEN_ABOUT == ui->screen ||
                          ROAD_SCREEN_SETTINGS == ui->screen) ?
                          ROAD_SCREEN_TITLE : ROAD_SCREEN_SETUP;
        return ROAD_UI_NONE;
    }
    if (ui->screen == ROAD_SCREEN_TITLE) {
        if (pressed & ROAD_UI_COURSE_PREV)
            ui->title_selection = (ui->title_selection + 2u) % 3u;
        if (pressed & ROAD_UI_COURSE_NEXT)
            ui->title_selection = (ui->title_selection + 1u) % 3u;
        if (pressed & ROAD_UI_CONFIRM)
            ui->screen = ui->title_selection == 0u ? ROAD_SCREEN_SETUP :
                ui->title_selection == 1u ? ROAD_SCREEN_SETTINGS :
                ROAD_SCREEN_ABOUT;
        return ROAD_UI_NONE;
    }
    if (ui->screen == ROAD_SCREEN_SETTINGS) {
        if (pressed & (ROAD_UI_COURSE_PREV | ROAD_UI_COURSE_NEXT |
                       ROAD_UI_CONFIRM))
            ui->music_enabled ^= 1u;
        return ROAD_UI_NONE;
    }
    if (ui->screen == ROAD_SCREEN_ABOUT) {
        if (pressed & ROAD_UI_CONFIRM)
            ui->screen = ROAD_SCREEN_TITLE;
        return ROAD_UI_NONE;
    }
    if (ui->screen == ROAD_SCREEN_SETUP) {
        if (pressed & ROAD_UI_COURSE_PREV)
            ui->course = rr_course_move(ui->course, -1);
        if (pressed & ROAD_UI_COURSE_NEXT)
            ui->course = rr_course_move(ui->course, 1);
        if (pressed & ROAD_UI_BIKE_PREV)
            ui->bike = (ui->bike + ui->bike_count - 1u) % ui->bike_count;
        if (pressed & ROAD_UI_BIKE_NEXT)
            ui->bike = (ui->bike + 1u) % ui->bike_count;
        if (pressed & ROAD_UI_CONFIRM) return ROAD_UI_START;
    } else if (pressed & ROAD_UI_CONFIRM) {
        if (ui->screen == ROAD_SCREEN_PAUSE)
            ui->screen = ROAD_SCREEN_RACE;
        else if (ui->screen == ROAD_SCREEN_RESULTS)
            ui->screen = ROAD_SCREEN_SETUP;
    }
    return ROAD_UI_NONE;
}

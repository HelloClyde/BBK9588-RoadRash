#ifndef ROAD_FRONTEND_H
#define ROAD_FRONTEND_H

#define ROAD_UI_COURSE_PREV 1u
#define ROAD_UI_COURSE_NEXT 2u
#define ROAD_UI_BIKE_NEXT 4u
#define ROAD_UI_BIKE_PREV 8u
#define ROAD_UI_CONFIRM 16u
#define ROAD_UI_BACK 0x80000000u

enum road_frontend_screen {
    ROAD_SCREEN_TITLE,
    ROAD_SCREEN_SETUP,
    ROAD_SCREEN_RACE,
    ROAD_SCREEN_PAUSE,
    ROAD_SCREEN_RESULTS,
    ROAD_SCREEN_ABOUT,
    ROAD_SCREEN_SETTINGS
};

enum road_frontend_action {
    ROAD_UI_NONE,
    ROAD_UI_START,
    ROAD_UI_EXIT
};

typedef struct road_frontend {
    unsigned int screen;
    unsigned int course;
    unsigned int bike;
    unsigned int bike_count;
    unsigned int title_selection;
    unsigned int music_enabled;
} road_frontend_t;

void road_frontend_init(road_frontend_t *ui, unsigned int bike_count);
unsigned int road_frontend_step(road_frontend_t *ui,
                                unsigned int pressed,
                                int race_finished);
void road_frontend_race_started(road_frontend_t *ui);
#endif

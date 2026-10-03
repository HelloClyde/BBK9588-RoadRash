#ifndef ROAD_CAREER_H
#define ROAD_CAREER_H

/* Values and transitions from the pinned 3DO source's
 * schedule_race_outcome_event / handle_front_end_command. */
#define ROAD_CAREER_BIKES 15u
#define ROAD_CAREER_COURSES 5u
#define ROAD_CAREER_LEVELS 5u

typedef struct road_career {
    unsigned int level;
    unsigned int bike;
    unsigned int completed_courses;
    int balance;
    unsigned int champion;
} road_career_t;

void road_career_init(road_career_t *career);
int road_career_can_buy(const road_career_t *career, unsigned int bike);
unsigned int road_career_price(unsigned int bike);
int road_career_buy(road_career_t *career, unsigned int bike);
unsigned int road_career_finish(road_career_t *career,
                                unsigned int course, unsigned int place);

#endif

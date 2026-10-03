#ifndef RR_COURSE_CATALOG_H
#define RR_COURSE_CATALOG_H

#define RR_COURSE_COUNT 5u

static const char *const rr_course_names[RR_COURSE_COUNT] = {
    "HIGHWAY", "CANYON", "CITY", "NAPA", "MEDLEY"
};

static const char *const rr_course_paths[RR_COURSE_COUNT] = {
    "Highway\\Highwayopt.rsrc",
    "Canyon\\Canyonopt.rsrc",
    "City\\Cityopt.rsrc",
    "Napa\\Napaopt.rsrc",
    "Medley\\Medleyopt.rsrc"
};

static const char *const rr_car_paths[RR_COURSE_COUNT] = {
    "Highway\\Highway.Cars.RSRC",
    "Canyon\\Canyon.Cars.RSRC",
    "City\\City.Cars.RSRC",
    "Napa\\Napa.Cars.RSRC",
    "Medley\\Medley.Cars.RSRC"
};

static unsigned int rr_course_move(unsigned int selected, int direction)
{
    if (selected >= RR_COURSE_COUNT) selected = 0u;
    if (direction < 0)
        return selected ? selected - 1u : RR_COURSE_COUNT - 1u;
    if (direction > 0)
        return (selected + 1u) % RR_COURSE_COUNT;
    return selected;
}

#endif

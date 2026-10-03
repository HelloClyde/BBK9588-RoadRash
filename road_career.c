#include "road_career.h"

static const int purchase_prices[ROAD_CAREER_BIKES] = {
    4495, 3249, 3497, 5489, 2999,
    29998, 18999, 40000, 21789, 34888,
    13796, 16875, 11988, 9199, 6994
};
static const int resale_values[ROAD_CAREER_BIKES] = {
    2247, 1624, 1748, 2744, 1499,
    14999, 9499, 20000, 10894, 17444,
    6898, 8437, 5994, 4599, 3497
};
static const unsigned int cash_awards[15] = {
    100u, 75u, 50u, 40u, 30u, 25u, 20u, 16u,
    13u, 10u, 7u, 5u, 3u, 2u, 1u
};

void road_career_init(road_career_t *career)
{
    career->level = 0u;
    career->bike = 1u;
    career->completed_courses = 0u;
    career->balance = 500;
    career->champion = 0u;
}

unsigned int road_career_price(unsigned int bike)
{
    return bike < ROAD_CAREER_BIKES ?
        (unsigned int)purchase_prices[bike] : 0u;
}

int road_career_can_buy(const road_career_t *career, unsigned int bike)
{
    if (!career || bike >= ROAD_CAREER_BIKES ||
        career->bike >= ROAD_CAREER_BIKES) return 0;
    if (bike == career->bike) return 1;
    return career->balance + resale_values[career->bike] >=
           purchase_prices[bike];
}

int road_career_buy(road_career_t *career, unsigned int bike)
{
    if (!road_career_can_buy(career, bike)) return 0;
    if (bike != career->bike) {
        career->balance -= purchase_prices[bike] -
                           resale_values[career->bike];
        career->bike = bike;
    }
    return 1;
}

unsigned int road_career_finish(road_career_t *career,
                                unsigned int course, unsigned int place)
{
    unsigned int award;
    if (!career || course >= ROAD_CAREER_COURSES || place >= 15u ||
        career->level >= ROAD_CAREER_LEVELS) return 0u;
    award = (career->level + 1u) * cash_awards[place] * 10u;
    career->balance += (int)award;
    if (place < 3u) {
        career->completed_courses |= 1u << course;
        if (career->completed_courses == 31u) {
            career->completed_courses = 0u;
            if (career->level + 1u < ROAD_CAREER_LEVELS)
                career->level++;
            else career->champion = 1u;
        }
    }
    return award;
}

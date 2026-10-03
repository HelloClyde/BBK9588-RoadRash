#include <assert.h>
#include <stdio.h>
#include "road_career.h"

int main(void)
{
    road_career_t career;
    unsigned int i;
    road_career_init(&career);
    assert(career.bike == 1u && career.balance == 500);
    assert(road_career_price(4u) == 2999u);
    assert(!road_career_can_buy(&career, 5u));
    assert(!road_career_can_buy(&career, 4u));
    assert(road_career_finish(&career, 0u, 0u) == 1000u);
    assert(road_career_can_buy(&career, 4u));
    assert(road_career_buy(&career, 4u));
    assert(career.bike == 4u && career.balance == 125);
    assert(road_career_finish(&career, 0u, 3u) == 400u);
    assert(career.completed_courses == 1u && career.balance == 525);
    for (i = 1u; i < 5u; ++i)
        assert(road_career_finish(&career, i, 2u) == 500u);
    assert(career.level == 1u && career.completed_courses == 0u);
    assert(road_career_finish(&career, 0u, 0u) == 2000u);
    assert(!road_career_finish(&career, 5u, 0u));
    assert(!road_career_finish(&career, 0u, 15u));
    road_career_init(&career);
    for (i = 0u; i < 25u; ++i)
        assert(road_career_finish(&career, i % 5u, 0u) ==
               (i / 5u + 1u) * 1000u);
    assert(career.level == 4u && career.champion == 1u);
    puts("Original 3DO prices, awards and podium progression: PASS");
    return 0;
}

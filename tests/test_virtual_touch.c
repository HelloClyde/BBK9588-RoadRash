#include <assert.h>
#include <stdio.h>
#include "road_virtual_touch.h"

int main(void)
{
    road_virtual_touch_t touch = {0};
    int x, y;
    int i;

    assert(road_virtual_touch_unpack(((319u - ROAD_GAS_X) << 16) |
                                     ROAD_GAS_Y, &x, &y));
    assert(x == ROAD_GAS_Y && y == 319 - ROAD_GAS_X);
    assert(road_virtual_touch_button(319 - y, x) == ROAD_TOUCH_GAS);
    assert(road_virtual_touch_unpack(((319u - ROAD_HIT_X) << 16) |
                                     ROAD_HIT_Y, &x, &y));
    assert(road_virtual_touch_button(319 - y, x) == ROAD_TOUCH_HIT);
    assert(!road_virtual_touch_unpack((320u << 16) | ROAD_HIT_Y, &x, &y));

    /* The visible rim and a small margin must respond. */
    assert(road_virtual_touch_button(ROAD_GAS_X + 25, ROAD_GAS_Y) ==
           ROAD_TOUCH_GAS);
    assert(road_virtual_touch_button(ROAD_HIT_X, ROAD_HIT_Y - 25) ==
           ROAD_TOUCH_HIT);
    assert(road_virtual_touch_button(ROAD_KICK_X, ROAD_KICK_Y) ==
           ROAD_TOUCH_KICK);
    assert(road_virtual_touch_button(ROAD_GAS_X + 27,
                                     ROAD_GAS_Y) == ROAD_TOUCH_NONE);

    /* Repeated coordinate messages from one contact are one tap. */
    assert(road_virtual_touch_coordinate(&touch, ROAD_GAS_X,
                                         ROAD_GAS_Y) == ROAD_TOUCH_GAS);
    assert(touch.throttle_on);
    for (i = 0; i < 8; ++i)
        assert(road_virtual_touch_coordinate(&touch, ROAD_GAS_X + i,
                                             ROAD_GAS_Y) == ROAD_TOUCH_NONE);
    assert(road_virtual_touch_release(&touch, ROAD_GAS_X,
                                      ROAD_GAS_Y) == ROAD_TOUCH_NONE);
    assert(touch.throttle_on);

    /* Next contact toggles it back, even when it lands at the same point. */
    assert(road_virtual_touch_coordinate(&touch, ROAD_GAS_X,
                                         ROAD_GAS_Y) == ROAD_TOUCH_GAS);
    assert(!touch.throttle_on);
    (void)road_virtual_touch_release(&touch, ROAD_GAS_X, ROAD_GAS_Y);

    /* Coordinate filtering and release fallback both accept a quick tap. */
    assert(road_virtual_touch_coordinate(&touch, 180, 180) ==
           ROAD_TOUCH_NONE);
    assert(road_virtual_touch_coordinate(&touch, ROAD_HIT_X,
                                         ROAD_HIT_Y) == ROAD_TOUCH_HIT);
    assert(touch.attack_pending);
    assert(!touch.kick_pending);
    (void)road_virtual_touch_release(&touch, ROAD_HIT_X, ROAD_HIT_Y);
    touch.attack_pending = 0;
    assert(road_virtual_touch_release(&touch, ROAD_HIT_X,
                                      ROAD_HIT_Y) == ROAD_TOUCH_HIT);
    assert(touch.attack_pending);

    /* The next finger may update the global cache before a queued release
     * is handled. Its own lparam must still select GAS, never HIT. */
    touch.attack_pending = 0;
    assert(road_virtual_touch_unpack(((319u - ROAD_GAS_X) << 16) |
                                     ROAD_GAS_Y, &x, &y));
    assert(road_virtual_touch_release(&touch, 319 - y, x) == ROAD_TOUCH_GAS);
    assert(touch.throttle_on);
    assert(!touch.attack_pending);
    assert(road_virtual_touch_unpack(((319u - ROAD_HIT_X) << 16) |
                                     ROAD_HIT_Y, &x, &y));
    assert(road_virtual_touch_coordinate(&touch, 319 - y, x) ==
           ROAD_TOUCH_HIT);
    (void)road_virtual_touch_release(&touch, 319 - y, x);
    assert(touch.attack_pending);

    touch.attack_pending = 0;
    assert(road_virtual_touch_coordinate(&touch, ROAD_KICK_X,
                                         ROAD_KICK_Y) == ROAD_TOUCH_KICK);
    assert(touch.attack_pending && touch.kick_pending);
    (void)road_virtual_touch_release(&touch, ROAD_KICK_X, ROAD_KICK_Y);
    touch.attack_pending = 0;
    assert(road_virtual_touch_coordinate(&touch, ROAD_HIT_X,
                                         ROAD_HIT_Y) == ROAD_TOUCH_HIT);
    assert(touch.attack_pending && !touch.kick_pending);

    puts("virtual touch input OK");
    return 0;
}

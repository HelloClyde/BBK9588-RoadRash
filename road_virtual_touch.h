#ifndef ROAD_VIRTUAL_TOUCH_H
#define ROAD_VIRTUAL_TOUCH_H

/* Compact right-edge column; each target remains separate. */
#define ROAD_GAS_X 278
#define ROAD_GAS_Y 150
#define ROAD_GAS_RADIUS 23
#define ROAD_HIT_X 278
#define ROAD_HIT_Y 92
#define ROAD_HIT_RADIUS 23
#define ROAD_KICK_X 278
#define ROAD_KICK_Y 34
#define ROAD_KICK_RADIUS 23
#define ROAD_GAS_TOUCH_RADIUS 26
#define ROAD_HIT_TOUCH_RADIUS 26
#define ROAD_KICK_TOUCH_RADIUS 26

#define ROAD_TOUCH_NONE 0
#define ROAD_TOUCH_GAS 1
#define ROAD_TOUCH_HIT 2
#define ROAD_TOUCH_KICK 3

typedef struct road_virtual_touch {
    int down;
    int handled;
    int throttle_on;
    int attack_pending;
    int kick_pending;
} road_virtual_touch_t;

static int road_virtual_touch_unpack(unsigned int event_lparam,
                                     int *portrait_x, int *portrait_y)
{
    int x = (int)(short)(event_lparam & 0xffffu);
    int y = (int)(short)(event_lparam >> 16);
    *portrait_x = x;
    *portrait_y = y;
    return x >= 0 && x < 240 && y >= 0 && y < 320;
}

static int road_virtual_touch_button(int x, int y)
{
    int dx = x - ROAD_GAS_X;
    int dy = y - ROAD_GAS_Y;
    if (dx * dx + dy * dy <=
        ROAD_GAS_TOUCH_RADIUS * ROAD_GAS_TOUCH_RADIUS)
        return ROAD_TOUCH_GAS;
    dx = x - ROAD_HIT_X;
    dy = y - ROAD_HIT_Y;
    if (dx * dx + dy * dy <=
        ROAD_HIT_TOUCH_RADIUS * ROAD_HIT_TOUCH_RADIUS)
        return ROAD_TOUCH_HIT;
    dx = x - ROAD_KICK_X;
    dy = y - ROAD_KICK_Y;
    if (dx * dx + dy * dy <=
        ROAD_KICK_TOUCH_RADIUS * ROAD_KICK_TOUCH_RADIUS)
        return ROAD_TOUCH_KICK;
    return ROAD_TOUCH_NONE;
}

static int road_virtual_touch_coordinate(road_virtual_touch_t *touch,
                                         int x, int y)
{
    int button;
    if (!touch->down) touch->handled = 0;
    touch->down = 1;
    if (touch->handled) return ROAD_TOUCH_NONE;
    button = road_virtual_touch_button(x, y);
    if (button == ROAD_TOUCH_NONE) return ROAD_TOUCH_NONE;
    touch->handled = 1;
    if (button == ROAD_TOUCH_GAS)
        touch->throttle_on = !touch->throttle_on;
    else {
        touch->attack_pending = 1;
        touch->kick_pending = button == ROAD_TOUCH_KICK;
    }
    return button;
}

static int road_virtual_touch_release(road_virtual_touch_t *touch,
                                      int x, int y)
{
    int button = ROAD_TOUCH_NONE;
    /* A quick tap can deliver only a release with a cached position. */
    if (!touch->handled)
        button = road_virtual_touch_coordinate(touch, x, y);
    touch->down = 0;
    touch->handled = 0;
    return button;
}

#endif

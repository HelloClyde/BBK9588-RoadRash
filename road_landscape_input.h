#ifndef ROAD_LANDSCAPE_INPUT_H
#define ROAD_LANDSCAPE_INPUT_H

#include "road_core.h"

#define ROAD_PHYSICAL_UP    1u
#define ROAD_PHYSICAL_DOWN  2u
#define ROAD_PHYSICAL_LEFT  4u
#define ROAD_PHYSICAL_RIGHT 8u

/* The landscape frame is rotated counterclockwise into the portrait LCD.
 * With the device held clockwise, physical Up/Down point screen Right/Left,
 * while physical Left/Right point screen Up/Down. */
static unsigned int road_landscape_direction_input(unsigned int physical,
                                                   int racing)
{
    unsigned int input = 0u;
    if (racing) {
        if (physical & ROAD_PHYSICAL_DOWN) input |= ROAD_LEFT;
        if (physical & ROAD_PHYSICAL_UP) input |= ROAD_RIGHT;
    } else {
        if (physical & ROAD_PHYSICAL_LEFT) input |= ROAD_LEFT;
        if (physical & ROAD_PHYSICAL_RIGHT) input |= ROAD_RIGHT;
        if (physical & ROAD_PHYSICAL_DOWN) input |= ROAD_BRAKE;
        if (physical & ROAD_PHYSICAL_UP) input |= ROAD_ACCEL;
    }
    return input;
}

#endif

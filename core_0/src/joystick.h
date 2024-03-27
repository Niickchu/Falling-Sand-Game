#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdio.h>
#include "common.h"

#define UPPER_THRESH 40000
#define LOWER_THRESH 20000

void printDirection(enum direction dir) {
    switch (dir) {
        case N:
            printf("North\n");
            break;
        case NE:
            printf("North East\n");
            break;
        case E:
            // Assuming the intended unique value for East
            printf("East\n");
            break;
        case SE:
            printf("South East\n");
            break;
        case S:
            printf("South\n");
            break;
        case SW:
            printf("South West\n");
            break;
        case W:
            printf("West\n");
            break;
        case NW:
            printf("North West\n");
            break;
        case C:
        	printf("Center\n");
        	break;
        default:
            printf("Unknown direction\n");
    }
}

enum direction parse_dir(u16 VpVn_channel, u16 VAux_channel) {
	u8 dir = 0;

	if (VAux_channel > UPPER_THRESH) {
		dir = N;
	} else if (VAux_channel < LOWER_THRESH) {
		dir = S;
	}

	if (VpVn_channel > UPPER_THRESH) {
		dir += W;
	} else if (VpVn_channel < LOWER_THRESH) {
		dir += E;
	}

	return (direction)dir;
}

#endif

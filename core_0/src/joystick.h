#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdio.h>
#include "common.h"
#include <math.h>

#define BASE_VAL 32000

// since we only move by pixels, maybe just having discrete speed steps
// may be better than having some sort of analog movement
#define SPEED_STEP 6500
#define DEADZONE 4000

// mostly just for debugging
void printDirection(enum direction dir) {
    switch (dir) {
        case N:
            printf("North\n");
            break;
        case NE:
            printf("North East\n");
            break;
        case E:
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

movement_t parse_dir(u16 VpVn_channel, u16 VAux_channel) {
	movement_t movement;
	int y_diff, x_diff = 0;

	u8 dir_int = 0;

	// use the DEADZONE to try and help eliminate drifting due
	// to poor potentiometers
	if (VAux_channel > (BASE_VAL + DEADZONE)) {
		dir_int = N;
		y_diff = abs((VAux_channel - DEADZONE) - BASE_VAL);
	} else if (VAux_channel < (BASE_VAL - DEADZONE)) {
		dir_int = S;
		y_diff = abs((VAux_channel + DEADZONE) - BASE_VAL);
	}

	// since we use one hot encoding for NSWE, can just add them
	// together to make intermediate directions
	if (VpVn_channel > (BASE_VAL + DEADZONE)) {
		dir_int += W;
		x_diff = abs((VpVn_channel - DEADZONE) - BASE_VAL);
	} else if (VpVn_channel < (BASE_VAL - DEADZONE)) {
		dir_int += E;
		x_diff = abs((VpVn_channel + DEADZONE) - BASE_VAL);
	}

	direction dir = (direction)dir_int;

	movement.dir = dir;

	// we just consider the speed how many multiples of SPEED_STEP
	// the applied directions deviate from their base values
	movement.y_mult = (unsigned int)(y_diff / SPEED_STEP) + 1;
	movement.x_mult = (unsigned int)(x_diff / SPEED_STEP) + 1;

	return movement;
}

#endif

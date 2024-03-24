#include <stdio.h>

enum direction{N=0x4, NE=0x5, E=0x1, SE=0x9, S=0x8, SW=0xA, W=0x2, NW=0x6, C=0x0};

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

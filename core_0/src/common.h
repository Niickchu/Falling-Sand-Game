#ifndef COMMON_H
#define COMMON_H

#define FRAME_WIDTH         (640)
#define FRAME_HEIGHT        (480)
#define GRID_WIDTH          FRAME_WIDTH
#define GRID_HEIGHT         FRAME_HEIGHT
#define NUM_BYTES_BUFFER    (FRAME_HEIGHT * FRAME_WIDTH * 4) //1280 x 1024 x 4 bytes
#define NUM_INTS_BUFFER     (FRAME_HEIGHT * FRAME_WIDTH) //1280 x 1024 x 4 bytes / 4 bytes per int
#define CURSOR_LENGTH       (5)	//so that cursor has a centre


enum direction{N=0x4, NE=0x5, E=0x1, SE=0x9, S=0x8, SW=0xA, W=0x2, NW=0x6, C=0x0};

typedef struct {
	short x;
	short y;
	int colour;

	//short previousX;		//used for when the cursor moves multiple pixels per update
	//short previousY;
} cursor_t;

typedef struct {
	short moveCursorVertically; // : 8;
	short moveCursorHorizontally; // : 8;
	short switchValues; // : 8;
	bool placeElement; // : 1;
	bool resetGrid; // : 1;
	direction joystick_dir;
}  userInput_t;

extern userInput_t userInput;
extern volatile bool RESET_BUTTON_PRESSED_FLAG;
extern volatile bool STOP_TIME_FLAG;
extern volatile bool ENABLE_CHUNKS_FLAG;

#endif

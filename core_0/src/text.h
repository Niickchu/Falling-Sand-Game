#include "font8x8_basic.h"
#include <string.h>

void draw_text_at_location(int x, int y, const char * text, int * memory_location, int color) {
	unsigned int text_length = strlen(text);
	char char_slice;
	int pixel_val;

	// 1 int per pixel, 8 pixels per character
	int *text_row = (int *)malloc(text_length * sizeof(int) * 8);

	// idea is to get the entire string row by row, then copy that entire row the the intermediate buffer
	for (int row = 0; row < 8; row++) {
	    memset(text_row, 0, text_length * 4 * 8);

	    for (unsigned int char_number = 0; char_number < text_length; char_number++) {
	    	// the font8x8_basic uses the unicode values for array indexing so we can just pass in the
	    	// actual char value
	        char_slice = font8x8_basic[text[char_number]][row];

	        for (int column = 0; column < 8; column++) {
	        	// 1 if we should draw at this location, else 0
	            pixel_val = !!(char_slice & (1 << (column)));
	            text_row[char_number * 8 + column] = pixel_val * color;
	        }
	    }
	    memcpy(memory_location + (x + FRAME_WIDTH * (row + y)), text_row, text_length * 4 * 8);
	}
	free(text_row);
}

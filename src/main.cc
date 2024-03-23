#include "common.h"
#include "interrupts.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "game.h"
#include "memoryAddresses.h"

volatile bool MIDDLE_BUTTON_PRESSED_FLAG = false;
volatile int global_button_values = 0;

userInput_t userInput = {0, 0, 0, 0, 0};

void setUserInput(userInput_t input);

int main(){
	int status = setupAllInterrupts();
	if (status != XST_SUCCESS) {
		print("Interrupt setup failed\n\r");
		return 1;
	}

	int * image_buffer_pointer = (int *)IMAGE_BUFFER_BASE_ADDR;
	int * grid_buffer = (int *)GRID_BUFFER_BASE_ADDR;

	memset(image_buffer_pointer, 0, NUM_BYTES_BUFFER);
	Xil_DCacheFlushRange((u32)image_buffer_pointer, NUM_BYTES_BUFFER);

	//initialize game
	FallingSandGame game(grid_buffer);

    while(1){

		setUserInput(userInput);
		game.handleInput(userInput);
		
		game.update();

		//render game state		//we can avoid a memcpy by using the in between bits for RGB
		memcpy(image_buffer_pointer, grid_buffer, NUM_BYTES_BUFFER);

		//draw the cursor on top of the grid
		game.drawCursor(image_buffer_pointer);

		if (userInput.switchValues & INCREASE_SPEED){
			print("increasingSpeed\n\r");
		}

		Xil_DCacheFlushRange((u32)image_buffer_pointer, NUM_BYTES_BUFFER);

	}

    return 0;
}

void setUserInput(userInput_t input){
	//Because we are using interrupts, I think we need to do this weird flag thing
	//maybe better because we don't want to change the userInput while game.update() is running
	if (MIDDLE_BUTTON_PRESSED_FLAG) {
		userInput.placeElement = 1;
	}
	else{
		userInput.placeElement = 0;
	}

	short switch_values = getSwitchValues();

	userInput.switchValues = switch_values;
	
}

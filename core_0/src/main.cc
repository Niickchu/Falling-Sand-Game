#include "common.h"
#include "interrupts.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "game.h"
#include "memoryAddresses.h"

#include <stdio.h>
#include "sleep.h"
#include "xil_io.h"
#include "xil_mmu.h"
#include "xil_printf.h"
#include "xpseudo_asm.h"
#include "platform.h"
#include "joystick.h"		//included in common.h
#include <xsysmon.h>
#include "time.h"
#include "text.h"

#define sev()           __asm__("sev")
#define CPU1STARTADR    0xfffffff0
#define COMM_VAL        (*(volatile unsigned long *)(0xFFFF0000))


XSysMon SysMonInst;
XSysMon_Config *ConfigPtr;
XSysMon *SysMonInstPtr = &SysMonInst;
int VpVnData, VAux0Data;

volatile bool RESET_BUTTON_PRESSED_FLAG = false;
volatile bool STOP_TIME_FLAG = false;
volatile bool ENABLE_CHUNKS_FLAG = false;
volatile bool INCREASE_CURSOR_FLAG = false;

userInput_t userInput = {0, 0, 0, 0, 0, C, AIR_ID};

void setUserInput(userInput_t input);
void draw_current_element(int *buffer);

int main(){
    COMM_VAL = 0;

    xil_printf("CPU0: writing startaddress for cpu1\n\r");
    Xil_Out32(CPU1STARTADR, 0x10080000);
    dmb();  //waits until write has finished

    xil_printf("CPU0: sending the SEV to wake up CPU1\n\r");
    sev();
    sleep(1);
    COMM_VAL = 1;

	// -----------

	int status = setupAllInterrupts();
	if (status != XST_SUCCESS) {
		print("Interrupt setup failed\n\r");
		return 1;
	}

	ConfigPtr = XSysMon_LookupConfig(XPAR_SYSMON_0_DEVICE_ID);

	XSysMon_CfgInitialize(SysMonInstPtr, ConfigPtr, ConfigPtr->BaseAddress);

	// Wait for the end of the ADC conversion
	while ((XSysMon_GetStatus(SysMonInstPtr) & XSM_SR_EOS_MASK) != XSM_SR_EOS_MASK);


	int * image_buffer_pointer = (int *)IMAGE_BUFFER_BASE_ADDR;
	int * intermediate_buffer = (int *)INTERMEDIATE_BUFFER_BASE_ADDR;
	int * grid_buffer = (int *)GRID_BUFFER_BASE_ADDR;

	memset(image_buffer_pointer, 0, NUM_BYTES_BUFFER);
	Xil_DCacheFlushRange((u32)image_buffer_pointer, NUM_BYTES_BUFFER);

	//initialize game
	FallingSandGame game(grid_buffer);

    while(1){

		setUserInput(userInput);
		game.handleInput(&userInput);

		if(!STOP_TIME_FLAG){
			game.update();
		}


		//want to draw UI on the intermediate buffer
		memcpy(intermediate_buffer, grid_buffer, NUM_BYTES_BUFFER);


		//draw the active chunks
		if(ENABLE_CHUNKS_FLAG){
			game.drawActiveChunks(intermediate_buffer);
		}

		draw_current_element(intermediate_buffer);

		//draw the cursor on top of the grid and above text
		game.drawCursor(intermediate_buffer);

		//render game state
		memcpy(image_buffer_pointer, intermediate_buffer, NUM_BYTES_BUFFER);

		Xil_DCacheFlushRange((u32)image_buffer_pointer, NUM_BYTES_BUFFER);

	}

    return 0;
}

void draw_current_element(int *buffer) {

    switch (userInput.selected_element) {
    case SAND_ID:
    	draw_text_at_location(5, 5, "[", buffer, COLOUR_WHITE);
    	break;
    case WATER_ID:
    	draw_text_at_location(80, 5, "[", buffer, COLOUR_WHITE);
    	break;
    case STONE_ID:
    	draw_text_at_location(155, 5, "[", buffer, COLOUR_WHITE);
    	break;
    case SALT_ID:
    	draw_text_at_location(230, 5, "[", buffer, COLOUR_WHITE);
    	break;
    case LAVA_ID:
    	draw_text_at_location(305, 5, "[", buffer, COLOUR_WHITE);
    	break;
	default:
		break;
    }

	draw_text_at_location(14, 5, "SAND", buffer, COLOUR_SAND);
	draw_text_at_location(89, 5, "WATER", buffer, COLOUR_WATER);
	draw_text_at_location(164, 5, "STONE", buffer, COLOUR_STONE);
	draw_text_at_location(239, 5, "SALT", buffer, COLOUR_SALT);
	draw_text_at_location(314, 5, "LAVA", buffer, COLOUR_LAVA);
}

void setUserInput(userInput_t input){
	//Because we are using interrupts, I think we need to do this weird flag thing
	//maybe better because we don't want to change the userInput while game.update() is running
	if (getButtonValues() & MIDDLE_BUTTON_MASK) {
		userInput.placeElement = true;
	}
	else{
		userInput.placeElement = false;
	}

	if (RESET_BUTTON_PRESSED_FLAG) {
		userInput.resetGrid = 1;
		RESET_BUTTON_PRESSED_FLAG = false;
	}

	userInput.switchValues = getSwitchValues();

	VpVnData = XSysMon_GetAdcData(SysMonInstPtr, XSM_CH_VPVN);
	VAux0Data = XSysMon_GetAdcData(SysMonInstPtr, XSM_CH_AUX_MIN + 0);

	userInput.movement = parse_dir(VpVnData, VAux0Data);
}

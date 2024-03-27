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

#define sev()           __asm__("sev")
#define CPU1STARTADR    0xfffffff0
#define COMM_VAL        (*(volatile unsigned long *)(0xFFFF0000))

XSysMon SysMonInst;
XSysMon_Config *ConfigPtr;
XSysMon *SysMonInstPtr = &SysMonInst;
u16 VpVnData, VAux0Data;

volatile bool RESET_BUTTON_PRESSED_FLAG = false;
volatile bool STOP_TIME_FLAG = false;

userInput_t userInput = {0, 0, 0, 0, 0, C};

void setUserInput(userInput_t input);

int main(){

    sleep(2);

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

		//draw the cursor on top of the grid
		game.drawCursor(intermediate_buffer);

		//render game state
		memcpy(image_buffer_pointer, intermediate_buffer, NUM_BYTES_BUFFER);

		Xil_DCacheFlushRange((u32)image_buffer_pointer, NUM_BYTES_BUFFER);

	}

    return 0;
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

	userInput.joystick_dir = parse_dir(VpVnData, VAux0Data);
}

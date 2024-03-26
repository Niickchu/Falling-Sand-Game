#include "xparameters.h"
#include "interrupts.h"
#include "common.h" //to access cursor, REDRAW_FRAME, button_value, switch_value
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "game.h"

// Device IDs
#define BUTTON_GPIO_ADDRESS 		XPAR_AXI_GPIO_0_BASEADDR
#define BUTTON_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID
#define SWITCH_GPIO_ADDRESS 		XPAR_AXI_GPIO_1_BASEADDR
#define SWITCH_DEVICE_ID			XPAR_AXI_GPIO_1_DEVICE_ID

#define INTC_GPIO_INTERRUPT_ID      XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define INTC_DEVICE_ID 		        XPAR_PS7_SCUGIC_0_DEVICE_ID
#define BUTTON_MASK                 XGPIO_IR_CH1_MASK

#define DOWN_BUTTON_MASK			(1 << 1)
#define LEFT_BUTTON_MASK			(1 << 2)
#define RIGHT_BUTTON_MASK			(1 << 3)
#define UP_BUTTON_MASK				(1 << 4)


XScuGic InterruptController;
static XScuGic_Config *GicConfig;

XGpio gpioButtons;
XGpio gpioSwitch;

static void BTN_Handler(void *InstancePtr);

static int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr) {

	XGpio_InterruptEnable(&gpioButtons, BUTTON_MASK);
	XGpio_InterruptGlobalEnable(&gpioButtons);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
								(Xil_ExceptionHandler)XScuGic_InterruptHandler,
								XScuGicInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

static int interrupt_init() {
	int Status;

	GicConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (GicConfig == NULL) return XST_FAILURE;

	Status = XScuGic_CfgInitialize(&InterruptController, GicConfig, GicConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
        xil_printf("Error: Could not initialize interrupt controller\r\n");
        return XST_FAILURE;
    }

	Status = SetUpInterruptSystem(&InterruptController);
	if (Status != XST_SUCCESS) return XST_FAILURE;

	Status = XScuGic_Connect(&InterruptController, INTC_GPIO_INTERRUPT_ID, (Xil_ExceptionHandler)BTN_Handler, &gpioButtons);
    if (Status != XST_SUCCESS) {
		xil_printf("Error: Could not connect GPIO Button interrupt to interrupt controller\r\n");
		return XST_FAILURE;
	}

	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(&gpioButtons, 1);
	XGpio_InterruptGlobalEnable(&gpioButtons);

	// Enable GPIO and timer interrupts in the controller
	XScuGic_Enable(&InterruptController, INTC_GPIO_INTERRUPT_ID);

	return XST_SUCCESS;
}

int setupAllInterrupts(){
    
	// initialize the GPIO driver for the buttons
	int status = XGpio_Initialize(&gpioButtons, BUTTON_DEVICE_ID);
	if (status != XST_SUCCESS) {
        xil_printf("Error: Could not initialize GPIO Button driver\r\n");
        return XST_FAILURE;
    }

	// set the direction of the GPIO
	XGpio_SetDataDirection(&gpioButtons, 1, 0xFF);

	// initialize the GPIO driver for the switches
	status = XGpio_Initialize(&gpioSwitch, SWITCH_DEVICE_ID);
	if (status != XST_SUCCESS) {
        xil_printf("Error: Could not initialize GPIO Switch driver\r\n");
        return XST_FAILURE;
    }

	// set the direction of the GPIO
	XGpio_SetDataDirection(&gpioSwitch, 1, 0xFF);

    status = interrupt_init();
	if (status != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}

short getSwitchValues(){
	return (short)XGpio_DiscreteRead(&gpioSwitch, 1);
}

short getButtonValues(){
	return XGpio_DiscreteRead(&gpioButtons, 1);
}

static void BTN_Handler(void *InstancePtr){
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&gpioButtons, BUTTON_MASK);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&gpioButtons) & BUTTON_MASK) !=
			BUTTON_MASK) {
			return;
		}

	int button_value = XGpio_DiscreteRead(&gpioButtons, 1);

	switch(button_value){
		case 0: //Any Button Released
			break;

		case MIDDLE_BUTTON_MASK: //1
			break;

		case DOWN_BUTTON_MASK: //2
			RESET_BUTTON_PRESSED_FLAG = true;
			break;

		case LEFT_BUTTON_MASK: //4
			break;

		case RIGHT_BUTTON_MASK: //8
			break;

		case UP_BUTTON_MASK: //16
			break;

		default:
			break;
	}

	// Clear the interrupt such that it is no longer pending in the GPIO
	(void)XGpio_InterruptClear(&gpioButtons, BUTTON_MASK);

	// Enable GPIO interrupts
	XGpio_InterruptEnable(&gpioButtons, BUTTON_MASK);
}

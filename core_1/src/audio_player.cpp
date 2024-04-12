#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "xil_printf.h"

#include "xil_mmu.h"
#include "xil_types.h"
#include "xstatus.h"
#include "xscugic.h"
#include "xaxidma.h"
#include "ff.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xiicps.h"
#include "adau.h"

// spinlock related to starting the background music, set by CPU 0
volatile uint32_t* spinlock = (uint32_t*)0xFFFF0000;

int main()
{

	// maybe not the best practice but works, and sometimes gives undefined behaviour otherwise
	*spinlock = 0;

    // had to change settings in order to get the SD card to mount on core 1, as per:
    // https://support.xilinx.com/s/question/0D52E00006hpMMwSAM/xapp1079-20191-sd-access-on-processor-1?language=en_US
    // honestly don't know why it works though
    Xil_SetTlbAttributes(0xFFFF0000, 0x14de2);
    Xil_SetTlbAttributes(0x00200000, 0x14de6);
    Xil_SetTlbAttributes(0x00300000, 0x14de6);
    Xil_SetTlbAttributes(0x00400000, 0x14de6);

    // spin lock until we get the "signal" from core 0
    while (*spinlock != 1) {
    };

    adau1761_playMusic();

	while(*spinlock == 1);

    return 0;
}

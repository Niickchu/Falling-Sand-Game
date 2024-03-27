#ifndef MEMORYADDRESSES_H
#define MEMORYADDRESSES_H

//#include "xparameters.h"

#define DDR_BASE_ADDR				    (XPAR_PS7_DDR_0_S_AXI_BASEADDR) // 0010_0000 to 3FFF_FFFF
#define IMAGE_BUFFER_BASE_ADDR          (0x00900000)
#define INTERMEDIATE_BUFFER_BASE_ADDR   (0x020BB00C)
#define GRID_BUFFER_BASE_ADDR           (0x018D2008)

#endif
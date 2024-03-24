#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "platform.h"
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
#define fatalError(msg) throwFatalError(__PRETTY_FUNCTION__,msg)
#define COMM_VAL            (*(volatile unsigned long *)(0xFFFF0000))

#define R0_CLOCK_CONTROL 0x00

void throwFatalError(const char *func,const char *msg) {
	printf("%s() : %s\r\n",func,msg);
	for(;;);
}
/* Redefine audio controller base address from xparameters.h */
#define AUDIO_BASE				XPAR_ZED_AUDIO_CTRL_0_BASEADDR

/* Slave address for the ADAU audio controller 8 */
#define IIC_SLAVE_ADDR			0x76

/* I2C Serial Clock frequency in Hertz */
#define IIC_SCLK_RATE			400000
XIicPs Iic;
// Size of the buffer which holds the DMA Buffer Descriptors (BDs)
#define DMA_BDUFFERSIZE 4000

typedef struct
{
	u8 spiChipAddr;
	int spiFifoWordsize;

	XAxiDma dmaAxiController;
	XAxiDma_Bd dmaBdBuffer[1] __attribute__((aligned(XAXIDMA_BD_MINIMUM_ALIGNMENT)));
	int dmaWritten;
} adau1761_config;

int adau1761_init(adau1761_config *pDevice, u32 DeviceId, u16 dmaId, u16 interruptId);

int adau1761_spiCheckInit(adau1761_config *pDevice);
void adau1761_spiWrite(adau1761_config *pDevice,u16 addr, u8 wdata);
u8 adau1761_spiRead(adau1761_config *pDevice,u16 addr);

int adau1761_dmaSetup(adau1761_config *pDevice);
void adau1761_dmaFreeProcessedBDs(adau1761_config *pDevice);
int adau1761_dmaBusy(adau1761_config *pDevice);
void adau1761_dmaReset(adau1761_config *pDevice);

void adau1761_dmaTransmit(adau1761_config *pDevice, u32 *buffer, size_t buffer_len, int nRepeats);
void adau1761_dmaTransmitBLOB(adau1761_config *pDevice, u32 *buffer, size_t buffer_len);
void AudioWriteToReg(unsigned char u8RegAddr, unsigned char u8Data) {
    unsigned char u8TxData[3];
    u8TxData[0] = 0x40; // Device address
    u8TxData[1] = u8RegAddr; // Register address
    u8TxData[2] = u8Data; // Data to write

    XIicPs_MasterSendPolled(&Iic, u8TxData, 3, (IIC_SLAVE_ADDR >> 1));
    while(XIicPs_BusIsBusy(&Iic));
}
FATFS FS_instance;
adau1761_config codec;
unsigned char IicConfig(unsigned int DeviceIdPS)
{
	XIicPs_Config *Config;
	int Status;

	/* Initialise the IIC driver so that it's ready to use */

	// Look up the configuration in the config table
	Config = XIicPs_LookupConfig(DeviceIdPS);
	if(NULL == Config) {
		return XST_FAILURE;
	}

	// Initialise the IIC driver configuration
	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//Set the IIC serial clock rate.
	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	return XST_SUCCESS;
}

void AudioPllConfig() {

	unsigned char u8TxData[8], u8RxData[6];
	int Status;

	Status = IicConfig(XPAR_XIICPS_0_DEVICE_ID);
	if(Status != XST_SUCCESS) {
		xil_printf("\nError initializing IIC");

	}

	// Disable Core Clock
	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0E);

	/* 	MCLK = 10 MHz
		R = 0100 = 4, N = 0x02 0x3C = 572, M = 0x02 0x71 = 625

		PLL required output = 1024x48 KHz
		(PLLout)			= 49.152 MHz

		PLLout/MCLK			= 49.152 MHz/10 MHz
							= 4.9152 MHz
							= R + (N/M)
							= 4 + (572/625) */

	// Write 6 bytes to R1 @ register address 0x4002
	u8TxData[0] = 0x40; // Register write address [15:8]
	u8TxData[1] = 0x02; // Register write address [7:0]
	u8TxData[2] = 0x02; // byte 6 - M[15:8]
	u8TxData[3] = 0x71; // byte 5 - M[7:0]
	u8TxData[4] = 0x02; // byte 4 - N[15:8]
	u8TxData[5] = 0x3C; // byte 3 - N[7:0]
	u8TxData[6] = 0x21; // byte 2 - 7 = reserved, bits 6:3 = R[3:0], 2:1 = X[1:0], 0 = PLL operation mode
	u8TxData[7] = 0x01; // byte 1 - 7:2 = reserved, 1 = PLL Lock, 0 = Core clock enable

	// Write bytes to PLL Control register R1 @ 0x4002
	XIicPs_MasterSendPolled(&Iic, u8TxData, 8, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));

	// Register address set: 0x4002
	u8TxData[0] = 0x40;
	u8TxData[1] = 0x02;

	// Poll PLL Lock bit
	do {
		XIicPs_MasterSendPolled(&Iic, u8TxData, 2, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
		XIicPs_MasterRecvPolled(&Iic, u8RxData, 6, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
	}
	while((u8RxData[5] & 0x02) == 0); // while not locked

	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0F);	// 1111
												// bit 3:		CLKSRC = PLL Clock input
												// bits 2:1:	INFREQ = 1024 x fs
												// bit 0:		COREN = Core Clock enabled
}

// This holds the memory allocated for the wav file currently played.
u8 *theBuffer = NULL;
size_t theBufferSize = 0;
int theVolume = 2;

int adau1761_init(adau1761_config *pDevice, u16 dmaId, u16 interruptId) {
	pDevice->spiChipAddr = 0;
	pDevice->spiFifoWordsize = 4;

    int Status = IicConfig(XPAR_XIICPS_0_DEVICE_ID);
    if (Status != XST_SUCCESS) {
        fatalError("Could not initialize IIC");
    }

    // Now replace SPI write calls with AudioWriteToReg for I2C
    // Enable clock
    AudioWriteToReg(0x00, 0x01); // Adjusted the register address

    // Write other configurations using AudioWriteToReg as needed, for example:
    AudioWriteToReg(0xF9, 0x7F); // Adjusted register addresses
    AudioWriteToReg(0xFA, 0x01);
    AudioWriteToReg(0x15, 0x00);
    AudioWriteToReg(0x16, 0x00);
    AudioWriteToReg(0x17, 0x00);
    AudioWriteToReg(0x1C, 0x21);
    AudioWriteToReg(0x1D, 0x00);
    AudioWriteToReg(0x1E, 0x41);
    AudioWriteToReg(0x1F, 0x00);
    AudioWriteToReg(0x20, 0x05);
    AudioWriteToReg(0x21, 0x11);
    AudioWriteToReg(0x22, 0x00);
    AudioWriteToReg(0x25, 0xFE);
    AudioWriteToReg(0x26, 0xFE);
    AudioWriteToReg(0x29, 0x03);
    AudioWriteToReg(0x2A, 0x03);
    AudioWriteToReg(0xF2, 0x01);



	int xStatus = XAxiDma_CfgInitialize(&pDevice->dmaAxiController,XAxiDma_LookupConfig(dmaId));
	if(XST_SUCCESS != xStatus) {
		fatalError("Failed to initialize DMA");
	}

	if(!XAxiDma_HasSg(&pDevice->dmaAxiController)){
		fatalError("Device configured as simple mode");
	}

	xStatus = adau1761_dmaSetup(pDevice);
	if (xStatus!=0) {
		return xStatus;
	}

	return 0;
}



int adau1761_dmaSetup(adau1761_config *pDevice) {
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&pDevice->dmaAxiController);

	pDevice->dmaWritten = FALSE;

	// Disable all TX interrupts before TxBD space setup
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	// Setup TxBD space
	u32 BdCount = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,(u32) sizeof(pDevice->dmaBdBuffer));
	int Status = XAxiDma_BdRingCreate(TxRingPtr, (UINTPTR)&pDevice->dmaBdBuffer[0], (UINTPTR)&pDevice->dmaBdBuffer[0], XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (Status != XST_SUCCESS) {
		fatalError("Failed create BD ring");
	}

	// Like the RxBD space, we create a template and set all BDs to be the
	// same as the template. The sender has to set up the BDs as needed.
	XAxiDma_Bd BdTemplate;
	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		fatalError("Failed clone BDs");
	}

	// Start the TX channel
	XAxiDma_BdRingEnableCyclicDMA(TxRingPtr);
	XAxiDma_SelectCyclicMode(&pDevice->dmaAxiController, XAXIDMA_DMA_TO_DEVICE, 1);
	Status = XAxiDma_BdRingStart(TxRingPtr);
	//Status = XAxiDma_StartBdRingHw(TxRingPtr);
	if (Status != XST_SUCCESS) {
		fatalError("Failed bd start");
	}

	return 0;
}

int adau1761_dmaBusy(adau1761_config *pDevice) {
	if (pDevice->dmaWritten && XAxiDma_Busy(&pDevice->dmaAxiController,XAXIDMA_DMA_TO_DEVICE)!=0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void adau1761_dmaReset(adau1761_config *pDevice) {
	XAxiDma_Reset(&pDevice->dmaAxiController);
	for(int TimeOut = 2000000;TimeOut>0;--TimeOut) {
		if (XAxiDma_ResetIsDone(&pDevice->dmaAxiController)) {
			break;
		}
	}
}

void adau1761_dmaStop(adau1761_config *pDevice) {
	// That's a slightly crude way to stop the DMA
	adau1761_dmaReset(pDevice);

	// Now everything is messed up, we need to reinitialize the DMA controller.
	int xStatus = adau1761_dmaSetup(pDevice);
	if (xStatus!=0) {
		fatalError("adau1761_dmaSetup() failed");
	}
}

void adau1761_dmaFreeProcessedBDs(adau1761_config *pDevice) {
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&pDevice->dmaAxiController);

	// Get all processed BDs from hardware
	XAxiDma_Bd *BdPtr;
	int BdCount = XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	// Free all processed BDs for future transmission
	int Status = XAxiDma_BdRingFree(TxRingPtr, BdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		fatalError("Failed to free BDs");
	}
}

void adau1761_dmaTransmit_single(adau1761_config *pDevice, u32 *buffer, size_t buffer_len) {

	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&pDevice->dmaAxiController);

	// Free the processed BDs from previous run.
	adau1761_dmaFreeProcessedBDs(pDevice);

	// Flush the SrcBuffer before the DMA transfer, in case the Data Cache is enabled
	Xil_DCacheFlushRange((u32)buffer, buffer_len * sizeof(u32));

	XAxiDma_Bd *BdPtr = NULL;
	int Status = XAxiDma_BdRingAlloc(TxRingPtr, 1, &BdPtr);
	if (Status != XST_SUCCESS) {
		fatalError("Failed bd alloc");
	}

	XAxiDma_Bd *BdCurPtr = BdPtr;;
	Status = XAxiDma_BdSetBufAddr(BdCurPtr, (UINTPTR)buffer);
	if (Status != XST_SUCCESS) {
		fatalError("Tx set buffer addr failed");
	}

	Status = XAxiDma_BdSetLength(BdCurPtr, buffer_len*sizeof(u32),	TxRingPtr->MaxTransferLen);
	if (Status != XST_SUCCESS) {
		fatalError("Tx set length failed");
	}

	u32 CrBits = 0;
	CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK; // First BD
	CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK; // Last BD
	XAxiDma_BdSetCtrl(BdCurPtr, CrBits);

	XAxiDma_BdSetId(BdCurPtr, (UINTPTR)buffer);

	BdCurPtr = (XAxiDma_Bd *)XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);

	// Give the BD to hardware
	Status = XAxiDma_BdRingToHw(TxRingPtr, 1, BdPtr);
	if (Status != XST_SUCCESS) {
		fatalError("Failed to hw");
	}

	pDevice->dmaWritten = TRUE;
}

typedef struct {
	char riff[4];
	u32 riffSize;
	char wave[4];
} headerWave_t;

typedef struct {
	char ckId[4];
	u32 cksize;
} genericChunk_t;

typedef struct {
	u16 wFormatTag;
	u16 nChannels;
	u32 nSamplesPerSec;
	u32 nAvgBytesPerSec;
	u16 nBlockAlign;
	u16 wBitsPerSample;
	u16 cbSize;
	u16 wValidBitsPerSample;
	u32 dwChannelMask;
	u8 SubFormat[16];
} fmtChunk_t;

void stopWavFile() {
	// If there is already a WAV file playing, stop it
	if (adau1761_dmaBusy(&codec)) {
		adau1761_dmaStop(&codec);
	}
    // If there was already a WAV file, free the memory
    if (theBuffer){
    	free(theBuffer);
    	theBuffer = NULL;
    	theBufferSize = 0;
    }
}

void playWavFile(const char *filename) {
    headerWave_t headerWave;
    fmtChunk_t fmtChunk;
    FIL file;
    UINT nBytesRead=0;

    stopWavFile();

    FRESULT res = f_open(&file, filename, FA_READ);
    if (res!=0) {
    	fatalError("File not found");
    }
    printf("Loading %s\r\n",filename);

    // Read the RIFF header and do some basic sanity checks
    res = f_read(&file,(void*)&headerWave,sizeof(headerWave),&nBytesRead);
    if (res!=0) {
    	fatalError("Failed to read file");
    }
	if (memcmp(headerWave.riff,"RIFF",4)!=0) {
		fatalError("Illegal file format. RIFF not found");
	}
	if (memcmp(headerWave.wave,"WAVE",4)!=0) {
		fatalError("Illegal file format. WAVE not found");
	}

	// Walk through the chunks and interpret them
	for(;;) {
		// read chunk header
		genericChunk_t genericChunk;
		res = f_read(&file,(void*)&genericChunk,sizeof(genericChunk),&nBytesRead);
		if (res!=0) {
			fatalError("Failed to read file");
		}
		if (nBytesRead!=sizeof(genericChunk)) {
			break; // probably EOF
		}

		// The "fmt " is compulsory and contains information about the sample format
		if (memcmp(genericChunk.ckId,"fmt ",4)==0) {
			res = f_read(&file,(void*)&fmtChunk,genericChunk.cksize,&nBytesRead);
			if (res!=0) {
				fatalError("Failed to read file");
			}
			if (nBytesRead!=genericChunk.cksize) {
				fatalError("EOF reached");
			}
			if (fmtChunk.wFormatTag!=1) {
				fatalError("Unsupported format");
			}
			if (fmtChunk.nChannels!=2) {
				fatalError("Only stereo files supported");
			}
			if (fmtChunk.wBitsPerSample!=16) {
				fatalError("Only 16 bit per samples supported");
			}
		}
		// the "data" chunk contains the audio samples
		else if (memcmp(genericChunk.ckId,"data",4)==0) {
		    theBuffer = (u8*)malloc(genericChunk.cksize);
		    if (!theBuffer){
		    	fatalError("Could not allocate memory");
		    }
		    theBufferSize = genericChunk.cksize;

		    res = f_read(&file,(void*)theBuffer,theBufferSize,&nBytesRead);
		    if (res!=0) {
		    	fatalError("Failed to read file");
		    }
		    if (nBytesRead!=theBufferSize) {
		    	fatalError("Didn't read the complete file");
		    }
		}
		// Unknown chunk: Just skip it
		else {
			DWORD fp = f_tell(&file);
			f_lseek(&file,fp + genericChunk.cksize);
		}
	}

	// If we have data to play
    if (theBuffer) {
        printf("Playing %s\r\n",filename);

    	// Changing the volume and swap left/right channel and polarity
    	{
    		u32 *pSource = (u32*) theBuffer;
    		for(u32 i=0;i<theBufferSize/4;++i) {
    			short left  = (short) ((pSource[i]>>16) & 0xFFFF);
    			short right = (short) ((pSource[i]>> 0) & 0xFFFF);
    			int left_i  = -(int)left * theVolume / 4;
    			int right_i = -(int)right * theVolume / 4;
    			if (left>32767) left = 32767;
    			if (left<-32767) left = -32767;
    			if (right>32767) right = 32767;
    			if (right<-32767) right = -32767;
    			left = (short)left_i;
    			right = (short)right_i;
    			pSource[i] = ((u32)right<<16) + (u32)left;
    		}
    	}

    	adau1761_dmaTransmit_single(&codec, (u32*)theBuffer, theBufferSize/4);
    }

    f_close(&file);
}

int main()
{
    init_platform();

    Xil_SetTlbAttributes(0xFFFF0000, 0x14de2);  // S=b1 TEX=b100 AP=b11, Domain=b1111, C=b0, B=b0

    Xil_SetTlbAttributes(0x00200000, 0x14de6);
    Xil_SetTlbAttributes(0x00300000, 0x14de6);
    Xil_SetTlbAttributes(0x00400000, 0x14de6);

    IicConfig(XPAR_XIICPS_0_DEVICE_ID);
    AudioPllConfig();
    while (COMM_VAL == 0) {
    };

    setvbuf(stdin, NULL, _IONBF, 0);
    adau1761_init(&codec, XPAR_AXI_DMA_0_DEVICE_ID, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);

	FRESULT result = f_mount(&FS_instance,"0:/", 1);
	if (result != 0) {
		print("Couldn't mount SD Card. Press RETURN to try again\r\n");
		getchar();
		exit(-1);
	}

	playWavFile("BGC.WAV");

	while(1);

	adau1761_dmaReset(&codec);

    cleanup_platform();
    return 0;
}

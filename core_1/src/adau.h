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


/* Redefine audio controller base address from xparameters.h */
#define AUDIO_BASE				XPAR_ZED_AUDIO_CTRL_0_BASEADDR
/* Slave address for the ADAU audio controller 8 */
#define IIC_SLAVE_ADDR			0x76
/* I2C Serial Clock frequency in Hertz */
#define IIC_SCLK_RATE			400000
// Size of the buffer which holds the DMA Buffer Descriptors (BDs)
#define DMA_BDUFFERSIZE 4000
#define R0_CLOCK_CONTROL 0x00

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
void adau1761_dmaReset(adau1761_config *pDevice);

void adau1761_dmaTransmit(adau1761_config *pDevice, u32 *buffer, size_t buffer_len, int nRepeats);
void adau1761_dmaTransmitBLOB(adau1761_config *pDevice, u32 *buffer, size_t buffer_len);
void adau1761_playMusic();

void AudioWriteToReg(unsigned char u8RegAddr, unsigned char u8Data);

void playWavFile(const char *filename);

void fatalError(const char *msg);


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

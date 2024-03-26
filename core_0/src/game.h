#ifndef GAME_H
#define GAME_H

#include "common.h"
#include <xsysmon.h>

// Colours
// 0x000000F0 Red
// 0x0000F000 Green
// 0x00F00000 Blue
#define COLOUR_RED      0x00000010
#define COLOUR_GREEN    0x00001000
#define COLOUR_BLUE     0x00100000

#define COLOUR_AIR      0x00000000
#define COLOUR_SAND     0x0000F0F0
#define COLOUR_WATER    0x00F04000
#define COLOUR_STONE    0x00B0A0A0
#define COLOUR_SALT     0x00F0F0F0
#define COLOUR_LAVA     0x000000F0
#define COLOUR_OIL      0x00F0F000
#define COLOUR_FIRE     0x00F000F0

#define CURSOR_COLOUR   0x00F0F0F8 //needs the 8 to not be counted as air

#define AIR_ID          (0x00000000)
#define SAND_ID         (0x00000001)
#define WATER_ID        (0x00000002)
#define STONE_ID        (0x00000003)
#define SALT_ID         (0x00000004)
#define LAVA_ID         (0x00000005)
#define OIL_ID          (0x00000006)        //beyond the scope of this project
#define FIRE_ID         (0x00000007)        //beyond the scope of this project, might do anyway

#define ID_MASK         (0x0000000F)

#define MAX_NUM_PARTICLES (20000)

#define rng() Xil_In32(XPAR_AXI_RNG_0_S00_AXI_BASEADDR)

//USER INPUT DEFINES
#define INCREASE_SPEED (1 << 7) //not used in game class but in main.cc

// enum class ElementType {
//     AIR,
//     SAND,
//     WATER,
//     STONE,
//     SALT,
//     LAVA,
//     TEMP1,
//     TEMP2
// };

//game class
class FallingSandGame{
    public:
        //want game to have pointer to the image buffer used for rendering. add it to the constructor inputs
        FallingSandGame(int* gridPtr);
        void handleInput(userInput_t* input);
        void update();
        //void render(int* imageBuffer);  //no longer needed

        void drawCursor(int* image_buffer_pointer);

    private:
    
        bool isInbounds(int x, int y);
        bool xInbounds(int x);
        bool yInbounds(int y);

        int* grid;
        
        cursor_t cursor;

        int numParticles;

        int particleColours[8] = {COLOUR_AIR, COLOUR_SAND, COLOUR_WATER, COLOUR_STONE, COLOUR_SALT, COLOUR_LAVA, COLOUR_OIL, COLOUR_FIRE};
        int particleIDs[8] = {AIR_ID, SAND_ID, WATER_ID, STONE_ID, SALT_ID, LAVA_ID, OIL_ID, FIRE_ID};


        void updateSand(int x, int y);
        void updateWater(int x, int y);
        void updateStone(int x, int y);

        void placeElementsAtCursor(int element);

        void swap(int x1, int y1, int x2, int y2);
        int getElement(short input);
        int getColourModifier(int element);

        XSysMon SysMonInst;
        XSysMon_Config *ConfigPtr;
        XSysMon *SysMonInstPtr = &SysMonInst;
        u16 VpVnData, VAux0Data;

};

#endif

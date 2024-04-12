#ifndef GAME_H
#define GAME_H

#include "common.h"

// Colours
// 0x000000F0 Red
// 0x0000F000 Green
// 0x00F00000 Blue
#define COLOUR_RED      0x00000010
#define COLOUR_GREEN    0x00001000
#define COLOUR_BLUE     0x00100000
#define COLOUR_WHITE	0x00F0F0F0

#define COLOUR_AIR      0x00000000
#define COLOUR_SAND     0x0000F0F0
#define COLOUR_WATER    0x00F04000
#define COLOUR_STONE    0x00B0A0A0
#define COLOUR_SALT     0x00E0E0E0
#define COLOUR_LAVA     0x000040F0
#define COLOUR_SALT_WATER 0x00F0C050
#define COLOUR_BORDER   0x00505050
#define CURSOR_COLOUR   0x00F0F0F8

//placeable elements
#define AIR_ID          (0x00000000)
#define SAND_ID         (0x00000001)
#define WATER_ID        (0x00000002)
#define STONE_ID        (0x00000003)
#define SALT_ID         (0x00000004)
#define LAVA_ID         (0x00000005)
#define BORDER_ID       (0x00000008)

#define IN_ALT_STATE    (0x01000000)
#define LIFESPAN_MASK   (0x000F0000)
#define LIFEBIT_MASK    (0x00010000)

#define BASE_STONE_LIFE (0x00040000)

#define CHUNK_SIZE     (20)

#define ID_MASK         (0x0000000F)
#define ID_MASK_ALT     (0x0100000F)

#define MAX_NUM_PARTICLES (40000)

#define rng() Xil_In32(XPAR_AXI_RNG_0_S00_AXI_BASEADDR)

typedef struct {
    bool computeOnCurrentFrame;
    bool computeOnNextFrame;
} chunkBools_t;

//game class
class FallingSandGame{
    public:
        //want game to have pointer to the image buffer used for rendering. add it to the constructor inputs
        FallingSandGame(int* gridPtr);
        int handleInput(userInput_t* input);
        void update();

        void drawCursor(int* image_buffer_pointer);
        void drawActiveChunks(int* image_buffer_pointer);
        int returnCursorSize();
        int returnNumParticles();
    private:
    
        bool xInbounds(int x);

        int waterSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace);
        int lavaSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace);
        void drawBorder();


        chunkBools_t chunks[(GRID_WIDTH / CHUNK_SIZE)][(GRID_HEIGHT / CHUNK_SIZE)];     //x and y swapped when compared to row major format for the grid, bcuz standard
        void hitChunk(int absoluteX, int absoluteY);
        void updateChunkBools();
        void updateRowChunk(int chunkX, int chunkY, int row);

        int* grid;
        
        cursor_t cursor;

        int numParticles;

        int particleColours[6] = {COLOUR_AIR, COLOUR_SAND, COLOUR_WATER, COLOUR_STONE, COLOUR_SALT, COLOUR_LAVA};
        int particleIDs[6] = {AIR_ID, SAND_ID, WATER_ID, STONE_ID, SALT_ID, LAVA_ID};

        void updateSand(int x, int y);
        void updateWater(int x, int y);
        void updateStone(int x, int y);
        void updateSalt(int x, int y);
        void updateLava(int x, int y);

        void placeElementsAtLocation(int x, int y, int element);

        void waterSaltInteraction(int sourceX, int sourceY, int targetX, int targetY);
        void lavaSandInteraction(int sandX, int sandY);
        void lavaStoneInteraction(int stoneX, int stoneY);
        void lavaWaterInteraction(int sourceX, int sourceY, int targetX, int targetY);

        bool isFreshWater(int element);
        void placeElementsAtCursor(int element);
        int getRandomDirection();

        void swap(int x1, int y1, int x2, int y2);
        int getElement(short input);
        int getModifiers(int element);
};

#endif

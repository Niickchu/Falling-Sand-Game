#include "game.h"
#include "common.h"
#include "xil_io.h"

// LIST OF PERFORMANCE IMPROVEMENTS
// MAKE GRID COLUMN MAJOR BECAUSE ELEMENTS CHECK VERTICALLY MORE OFTEN THAN HORIZONTALLY

FallingSandGame::FallingSandGame(int* gridPtr){

    grid = gridPtr;
    //initialize elements
    memset(grid, 0, NUM_BYTES_BUFFER);

    numParticles = 0;
    circularCounter = 0;

    cursor = {GRID_WIDTH/2, GRID_WIDTH/2, CURSOR_COLOUR};  //cursor needs the 1 to not be counted as air
}

void FallingSandGame::handleInput(userInput_t input){

   //TODO: want bound checks here to make sure cursor does not go off the screen
   //TODO: interface with joystick to move cursor
     cursor.y += input.moveCursorVertically;
     cursor.x += input.moveCursorHorizontally;

    if (cursor.y > FRAME_HEIGHT - CURSOR_LENGTH - 1) cursor.y = FRAME_HEIGHT - CURSOR_LENGTH - 1;
    if (cursor.x < 0) cursor.x = 0;
    if (cursor.x > FRAME_WIDTH -  CURSOR_LENGTH - 1) cursor.x = FRAME_WIDTH -  CURSOR_LENGTH - 1;
    if (cursor.y < 0) cursor.y = 0;

    //place an element
    if(input.placeElement){
        if (numParticles < MAX_NUM_PARTICLES) {
            
            //protection in case of multiple elements selected. Note, could just use binary inputs for the switches instead
            int element = getElement(input.switchValues);

            placeElementsAtCursor(element);
        }

    }

   if(input.resetGrid){
        memset(grid, 0, NUM_BYTES_BUFFER);
        numParticles = 0;
   }

    userInput.moveCursorHorizontally = 0;	//temporary because we don't have a joystick
    userInput.moveCursorVertically = 0;
}


//TODO: change numparticles logic when we have to implement the eraser
void FallingSandGame::placeElementsAtCursor(int element){
    //since cursor movement should be limited to the grid, we don't need to check for bounds
    for(int i = 0; i < CURSOR_LENGTH; i++){
        for(int j = 0; j < CURSOR_LENGTH; j++){
            if(grid[(cursor.y + i) * GRID_WIDTH + cursor.x + j] == AIR_ID){ //&& randomNum > threshold for randomness

            	grid[(cursor.y + i) * GRID_WIDTH + cursor.x + j] = element + getColourModifier(element);

                numParticles++;
                if (numParticles >= MAX_NUM_PARTICLES) {
                    return;
                }

            }
        }
    }
}

int FallingSandGame::getColourModifier(int element){

    circularCounter = (circularCounter + 1) % 6;    //just experimenting with different colours, will look better with randomness

    switch(element & ID_MASK){
        case SAND_ID:
            return - COLOUR_GREEN*(circularCounter) - COLOUR_RED*(circularCounter);
        case WATER_ID:
            return 0;
        case STONE_ID:
            return 0;
        case SALT_ID:
            return 0;
        case LAVA_ID:
            return 0;
        case OIL_ID:
            return 0;
        case FIRE_ID:
            return 0;
        default:
            return 0;
    }
}

//returns colour + ID of the element selected
int FallingSandGame::getElement(short input){   //protection in case of multiple elements selected

    for (int i = 0; i < 7; i++){
        if (input & (1 << i)){
            return (particleColours[i+1] + particleIDs[i+1]);
        }
    }
    return particleColours[0];
}

void FallingSandGame::updateSand(int x, int y) {
    // if the sand is at the bottom of the grid, then it does not move
    if(y == GRID_HEIGHT - 1){
        return;
    }

    // if the sand is not at the bottom of the grid, then it looks down
    int targetElementId = grid[(y + 1) * GRID_WIDTH + x] & ID_MASK;

    if(targetElementId == AIR_ID || targetElementId == WATER_ID) {
        swap(x, y, x, y + 1);
    } 
    else {  
        
        //TODO: add randomness to which direction is checked first
        //int direction = 1 if randomNum > threshold else -1

        //then targetElementId = grid[(y + 1) * GRID_WIDTH + x + direction] & ID_MASK;

        //and the other is grid[(y + 1) * GRID_WIDTH + x - direction] & ID_MASK;

        // if the sand is not at the bottom of the grid and there is no air below it, then it looks down left and down right
        targetElementId = grid[(y + 1) * GRID_WIDTH + x - 1] & ID_MASK;
        if (x > 0 && (targetElementId == AIR_ID || targetElementId == WATER_ID)) {
            swap(x, y, x - 1, y + 1);
        } 
        
        else if (x < GRID_WIDTH - 1) 
        {
            targetElementId = grid[(y + 1) * GRID_WIDTH + x + 1] & ID_MASK;
            if (targetElementId == AIR_ID || targetElementId == WATER_ID) {
                swap(x, y, x + 1, y + 1);
            }
        }
    }
}


void FallingSandGame::updateWater(int x, int y){
    //water falls first, when it hits the ground it moves randomly left or right by x distance
    //TODO: implement randomness in the direction of water flow

    if((grid[(y + 1) * GRID_WIDTH + x] & ID_MASK) == AIR_ID) {
        swap(x, y, x, y + 1);
    } 

    //else something is below the water, so it moves left or right
    else {
        if (x > 0 && (grid[y * GRID_WIDTH + x - 1] & ID_MASK) == AIR_ID) {
            swap(x, y, x - 1, y);
        } 
        else if (x < GRID_WIDTH - 1) {
            if ((grid[y * GRID_WIDTH + x + 1] & ID_MASK) == AIR_ID) {
                swap(x, y, x + 1, y);
            }
        }
    }

}

void FallingSandGame::update(){

    //want to loop over the grid from bottom to top, left to right
    for(int y = GRID_HEIGHT - 1; y >= 0; y--){
        for(int x = 0; x < GRID_WIDTH; x++){
            switch(grid[y*GRID_WIDTH + x] & ID_MASK){
                case SAND_ID:
                    updateSand(x, y);
                    break;
                case WATER_ID:
                    updateWater(x, y);
                    break;
                case STONE_ID:  //stone doesn't do anything
                    //updateStone(x, y);
                    break;
                default:
                    break;
            }
        }
    }
}

void FallingSandGame::swap(int x1, int y1, int x2, int y2){     //classic swap function 
    int temp = grid[y1*GRID_WIDTH + x1]; //grid[y1][x1]
    grid[y1*GRID_WIDTH + x1] = grid[y2*GRID_WIDTH + x2]; //grid[y1][x1] = grid[y2][x2]
    grid[y2*GRID_WIDTH + x2] = temp; //grid[y2][x2] = temp
}

void FallingSandGame::drawCursor(int* image_buffer_pointer){
    for(int i = 0; i < CURSOR_LENGTH; i++){
        for(int j = 0; j < CURSOR_LENGTH; j++){
            image_buffer_pointer[(cursor.y + i) * GRID_WIDTH + cursor.x + j] = cursor.colour;
        }
    }
}


// void FallingSandGame::bresenham(int x1, int y1, int x2, int y2)
// {
//    int m_new = 2 * (y2 - y1);   //overflow if vertical line?
//    int slope_error_new = m_new - (x2 - x1);
//    for (int x = x1, y = y1; x <= x2; x++) {

//        // Add slope to increment angle formed
//        slope_error_new += m_new;

//        // Slope error reached limit, time to
//        // increment y and update slope error.
//        if (slope_error_new >= 0) {
//            y++;
//            slope_error_new -= 2 * (x2 - x1);
//        }
//    }
// }


// void FallingSandGame::replaceElement(int x, int y, int element){
//     grid[y * GRID_WIDTH + x] = element;
// }

#include "game.h"
#include "common.h"
#include "xil_io.h"
#include <math.h>

// #include <xsysmon.h>
// #include "platform.h"

// LIST OF PERFORMANCE IMPROVEMENTS
// MAKE GRID COLUMN MAJOR BECAUSE ELEMENTS CHECK VERTICALLY MORE OFTEN THAN HORIZONTALLY

FallingSandGame::FallingSandGame(int* gridPtr){

    grid = gridPtr;
    memset(grid, 0, NUM_BYTES_BUFFER);

    numParticles = 0;

    cursor = {GRID_WIDTH/2, GRID_HEIGHT/2,
    		GRID_WIDTH/2, GRID_HEIGHT/2,
			CURSOR_COLOUR, CURSOR_LENGTH};  //cursor needs the 1 to not be counted as air

    //initialise the chunkBools_t array to false
    for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {
        for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
            chunks[i][j] = {false, false};
        }
    }

}

void FallingSandGame::handleInput(userInput_t* input){

   //TODO: want bound checks here to make sure cursor does not go off the screen
   //TODO: interface with joystick to move cursor

	cursor.prev_x = cursor.x;
	cursor.prev_y = cursor.y;

	switch (input->movement.dir) {
	    case N:
	        cursor.y -= input->movement.y_mult;
	        break;
	    case NE:
	        cursor.y -= input->movement.y_mult;
	        cursor.x += input->movement.x_mult;
	        break;
	    case E:
	        cursor.x += input->movement.x_mult;
	        break;
	    case SE:
	        cursor.y += input->movement.y_mult;
	        cursor.x += input->movement.x_mult;
	        break;
	    case S:
	        cursor.y += input->movement.y_mult;
	        break;
	    case SW:
	        cursor.y += input->movement.y_mult;
	        cursor.x -= input->movement.x_mult;
	        break;
	    case W:
	        cursor.x -= input->movement.x_mult;
	        break;
	    case NW:
	        cursor.y -= input->movement.y_mult;
	        cursor.x -= input->movement.x_mult;
	        break;
	    case C:
	        break;
	    default:
	        break;
	}

	if (INCREASE_CURSOR_FLAG) {
		cursor.cursorSize += 2;
		if(cursor.cursorSize > MAX_CURSOR_LENGTH) {
			cursor.cursorSize = MIN_CURSOR_LENGTH;
		}
		INCREASE_CURSOR_FLAG = false;
	}

    if (cursor.y > FRAME_HEIGHT - cursor.cursorSize) cursor.y = FRAME_HEIGHT - cursor.cursorSize;
    if (cursor.x < 0) cursor.x = 0;
    if (cursor.x > FRAME_WIDTH -  cursor.cursorSize) cursor.x = FRAME_WIDTH -  cursor.cursorSize;
    if (cursor.y < 0) cursor.y = 0;

    //place an element
    if(input->placeElement){
        if (numParticles < MAX_NUM_PARTICLES) {
            
            //protection in case of multiple elements selected. Note, could just use binary inputs for the switches instead
            int element = getElement(input->switchValues);

            placeElementsAtCursor(element);
        }

    }

   if(input->resetGrid){
        memset(grid, 0, NUM_BYTES_BUFFER);
        numParticles = 0;
        input->resetGrid = 0;
   }

}

void FallingSandGame::placeElementsAtLocation(int x, int y, int element){

    if (element == COLOUR_AIR){     //ERASER
        for(int i = 0; i < cursor.cursorSize; i++) {
            for(int j = 0; j < cursor.cursorSize; j++) {
                int index = (y + i) * GRID_WIDTH + x + j;
                if (grid[index] != AIR_ID) {
                    grid[index] = COLOUR_AIR;
                    numParticles--;

                    //cursor.x is the absolute x position of the cursor
                    //cursor.y is the absolute y position of the cursor
                    hitChunk(x + j, y + i);

                }
            }
        }

        //xil_printf("numParticles: %d\n\r", numParticles);
        return;
    }    



    for(int i = 0; i < cursor.cursorSize; i++) {
        for(int j = 0; j < cursor.cursorSize; j++) {

            int index = (y + i) * GRID_WIDTH + x + j;
            if (grid[index] == AIR_ID) {

                if ((element & ID_MASK) == STONE_ID) {  // No RNG for stone
                    grid[index] = element + getColourModifier(element);
                    numParticles++;

                    hitChunk(x + j, y + i);

                    if (numParticles >= MAX_NUM_PARTICLES) {
                        return;
                    }
                } 
                
                else {  // For other elements with RNG
                    if (rng() > 13) {
                        grid[index] = element + getColourModifier(element);
                        numParticles++;

                        hitChunk(x + j, y + i);

                        if (numParticles >= MAX_NUM_PARTICLES) {
                            return;
                        }
                    }
                }

            }
        }
    }
}

//TODO: change numparticles logic when we have to implement the eraser
void FallingSandGame::placeElementsAtCursor(int element){
	int x_diff, y_diff;

	x_diff = cursor.x - cursor.prev_x;
	y_diff = cursor.y - cursor.prev_y;

	// not quite equal to distance, but kinda close enough,
	// but the idea is that if the cursor moves too much
	// and leaves a gap, we should also spawn particles there.
	// this is mostly for improving stone placements at lower
	// cursor sizes
	if ((abs(x_diff) + abs(y_diff)) > cursor.cursorSize) {
		this->placeElementsAtLocation(cursor.prev_x + (int)(x_diff / 2),
				cursor.prev_y + (int)(y_diff / 2), element);
	}

	this->placeElementsAtLocation(cursor.x, cursor.y, element);

    //xil_printf("numParticles: %d\n\r", numParticles);
}

int FallingSandGame::getColourModifier(int element){

    u32 rng_val = rng();

    switch(element & ID_MASK){
        case SAND_ID:
            rng_val = rng_val % 6;
            return - COLOUR_GREEN*(rng_val) - COLOUR_RED*(rng_val);
        case WATER_ID:
            rng_val = rng_val % 3;
            return - COLOUR_BLUE*(rng_val);
        case STONE_ID:
            rng_val = rng_val % 2;
            return - COLOUR_GREEN*(rng_val) - COLOUR_RED*(rng_val) - COLOUR_BLUE*(rng_val);
        case SALT_ID:
            rng_val = rng_val % 3;
            return - COLOUR_GREEN*(rng_val) - COLOUR_RED*(rng_val) - COLOUR_BLUE*(rng_val);
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
int FallingSandGame::getElement(short input){

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
        hitChunk(x, y + 1);
        hitChunk(x, y);
    } 
    else {  
        u32 rng_val = rng();
        int direction = 1;
        if (rng_val % 2 == 1) {
            direction = -1;
        }

        targetElementId = grid[(y + 1) * GRID_WIDTH + (x + direction)] & ID_MASK;
        if (xInbounds(x + direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID)) {
            swap(x, y, x + direction, (y + 1));
            hitChunk(x + direction, y + 1);
            hitChunk(x, y); //because sand may move away from a chunk border, so need to check where it moves from as well
        } 
        else if (xInbounds(x - direction)) {
            targetElementId = grid[(y + 1) * GRID_WIDTH + (x - direction)] & ID_MASK;
            if (targetElementId == AIR_ID || targetElementId == WATER_ID) {
                swap(x, y, x - direction, (y + 1));
                hitChunk(x - direction, y + 1);
                hitChunk(x, y);
            }
        }
    }
}


void FallingSandGame::updateWater(int x, int y){
    //water falls first, when it hits the ground it moves randomly left or right by x distance

    if((grid[(y + 1) * GRID_WIDTH + x] & ID_MASK) == AIR_ID) {
        swap(x, y, x, y + 1);
        hitChunk(x, y + 1);
        hitChunk(x, y);
    } 

    //else something is below the water, so it moves left or right
    else {
        u32 rng_val = rng();
        int direction = 1;
    	if (rng_val % 2 == 1) {
            direction = -1;
        }

        int openSpace = searchHorizontallyForOpenSpace(x, y, direction, rng_val % 4 + 1);   //direction is now the number of spaces to move left or right
        //bound is already checked in searchHorizontallyForOpenSpace
        //first check rng direction

        if (openSpace != 0) {
            swap(x, y, x + openSpace, y);
            hitChunk(x + openSpace, y);
            hitChunk(x, y);
        }
        else{
            openSpace = searchHorizontallyForOpenSpace(x, y, -direction, rng_val % 4 + 1);
            if (openSpace != 0) {
                swap(x, y, x + openSpace, y);
                hitChunk(x + openSpace, y);
                hitChunk(x, y);
            }
        }
    }
}

/*

direction = -1 for left, 1 for right
NOTE: ONLY USE FOR WATER IN THE MEANTIME

*/ 
int FallingSandGame::searchHorizontallyForOpenSpace(int x, int y, int direction, int numSpaces) {
    
    int targetElementId = 0;
    for (int i = 1; i <= numSpaces; i++) {
        targetElementId = grid[y * GRID_WIDTH + (x + i * direction)] & ID_MASK;
        
        if (xInbounds(x + i * direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID)) {
            continue;
        } else {
            // If the previous cell is air, return the offset
            if ((grid[y * GRID_WIDTH + (x + (i - 1) * direction)] & ID_MASK) == AIR_ID) {
                return (i - 1) * direction;
            } else {
                return 0;
            }
        }
    }
    
    // If the last cell is air, return the offset
    if (targetElementId == AIR_ID) {
        return numSpaces * direction;
    }
    
    return 0;
}

bool FallingSandGame::isInbounds(int x, int y){
    return (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT);
}

bool FallingSandGame::xInbounds(int x){
    return (x >= 0 && x < GRID_WIDTH);
}

bool FallingSandGame::yInbounds(int y){
    return (y >= 0 && y < GRID_HEIGHT);
}

void FallingSandGame::hitChunk(int absoluteX, int absoluteY){
    

    int chunkX = absoluteX / CHUNK_SIZE;
    int chunkY = absoluteY / CHUNK_SIZE;

    //update chunk and neighbouring chunks
    chunks[chunkX][chunkY].computeOnNextFrame = true;

    //if absoluteX or absoluteY is on the edge of a chunk, then we need to compute the neighbouring chunk
    if(absoluteX % CHUNK_SIZE == 0 && chunkX > 0){
        chunks[chunkX - 1][chunkY].computeOnNextFrame = true;
    }
    if(absoluteX % CHUNK_SIZE == CHUNK_SIZE - 1 && chunkX < GRID_WIDTH / CHUNK_SIZE - 1){
        chunks[chunkX + 1][chunkY].computeOnNextFrame = true;
    }
    if(absoluteY % CHUNK_SIZE == 0 && chunkY > 0){
        chunks[chunkX][chunkY - 1].computeOnNextFrame = true;
    }
    if(absoluteY % CHUNK_SIZE == CHUNK_SIZE - 1 && chunkY < GRID_HEIGHT / CHUNK_SIZE - 1){
        chunks[chunkX][chunkY + 1].computeOnNextFrame = true;
    }

}

void FallingSandGame::updateChunkBools(){
    for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {
        for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
            chunks[i][j].computeOnCurrentFrame = chunks[i][j].computeOnNextFrame;
            chunks[i][j].computeOnNextFrame = false;
        }
    }
}

void FallingSandGame::update(){

    updateChunkBools();

    for(int y = GRID_HEIGHT/CHUNK_SIZE - 1; y >= 0; y--){
        for(int rowOfY = CHUNK_SIZE - 1; rowOfY >= 0; rowOfY--){
            for(int x = 0; x < GRID_WIDTH/CHUNK_SIZE; x++){
                if(chunks[x][y].computeOnCurrentFrame){
                    updateRowChunk(x, y, rowOfY);
                }
            }
        }
    }
}


void FallingSandGame::updateRowChunk(int chunkX, int chunkY, int row){


    u32 rng_val = rng();        //to prevent Left Water and right Sand, or atleast I think it should???

    if (rng_val % 2 == 0) {     //update left to right
        
        for(int i = 0; i < CHUNK_SIZE; i++){        
            int x = chunkX * CHUNK_SIZE + i;
            int y = chunkY * CHUNK_SIZE + row;

            int element = grid[y * GRID_WIDTH + x] & ID_MASK;

            switch(element){
                case SAND_ID:
                    updateSand(x, y);
                    break;
                case WATER_ID:
                    updateWater(x, y);
                    break;
                case STONE_ID:
                    //updateStone(x, y);
                    break;
                default:
                    break;
            }
        }

    return; 
    }
    else{                   //update right to left
        for(int i = CHUNK_SIZE - 1; i >= 0; i--){
            int x = chunkX * CHUNK_SIZE + i;
            int y = chunkY * CHUNK_SIZE + row;

            int element = grid[y * GRID_WIDTH + x] & ID_MASK;

            switch(element){
                case SAND_ID:
                    updateSand(x, y);
                    break;
                case WATER_ID:
                    updateWater(x, y);
                    break;
                case STONE_ID:
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
	for(int i = 0; i < cursor.cursorSize; i++) {
	    for(int j = 0; j < cursor.cursorSize; j++) {
	        if (i == 0 || i == cursor.cursorSize - 1 || j == 0 || j == cursor.cursorSize - 1) {
	            image_buffer_pointer[(cursor.y + i) * GRID_WIDTH + cursor.x + j] = cursor.colour;
	        }
	    }
	}

}

void FallingSandGame::drawActiveChunks(int* image_buffer_pointer){
    //want to draw a red border around the chunks that are active
    for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {
        for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
            if (chunks[i][j].computeOnCurrentFrame) {
                for (int k = 0; k < CHUNK_SIZE; k++) {
                    image_buffer_pointer[(j * CHUNK_SIZE + k) * GRID_WIDTH + i * CHUNK_SIZE] = COLOUR_RED;
                    image_buffer_pointer[(j * CHUNK_SIZE + k) * GRID_WIDTH + i * CHUNK_SIZE + CHUNK_SIZE - 1] = COLOUR_RED;
                }
                for (int k = 0; k < CHUNK_SIZE; k++) {
                    image_buffer_pointer[j * CHUNK_SIZE * GRID_WIDTH + i * CHUNK_SIZE + k] = COLOUR_RED;
                    image_buffer_pointer[(j + 1) * CHUNK_SIZE * GRID_WIDTH + i * CHUNK_SIZE + k] = COLOUR_RED;
                }
            }
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

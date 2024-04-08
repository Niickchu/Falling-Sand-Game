#include "game.h"
#include "common.h"
#include "xil_io.h"

#include <math.h>
#include "xil_cache.h"

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

    drawBorder();

    //initialise the chunkBools_t array to false
    for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {
        for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
            chunks[i][j] = {false, false};
        }
    }

}

int FallingSandGame::returnCursorSize(){
    return cursor.cursorSize;
}

int FallingSandGame::returnNumParticles(){
    return numParticles;
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

	//protection in case of multiple elements selected. Note, could just use binary inputs for the switches instead
    int element = getElement(input->switchValues);
    input->selected_element = element & 0xF;

    //place an element
    if(input->placeElement){
        placeElementsAtCursor(element);

    }

   if(input->resetGrid){
        memset(grid, 0, NUM_BYTES_BUFFER);
        numParticles = 0;
        input->resetGrid = 0;

        drawBorder();

        for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {
            for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
                chunks[i][j] = {false, false};
            }
        }
        
        Xil_DCacheFlushRange((u32)grid, NUM_BYTES_BUFFER);
   }

}

void FallingSandGame::drawBorder(){
    for(int i = 0; i < GRID_WIDTH; i++){
        grid[i] = COLOUR_BORDER + BORDER_ID;
        grid[(GRID_HEIGHT - 1) * GRID_WIDTH + i] = COLOUR_BORDER + BORDER_ID;
    }

    for(int i = 1; i < GRID_HEIGHT - 1; i++){
        grid[i * GRID_WIDTH] = COLOUR_BORDER + BORDER_ID;
        grid[i * GRID_WIDTH + GRID_WIDTH - 1] = COLOUR_BORDER + BORDER_ID;
    }

    for(int i = 1; i < GRID_WIDTH - 1; i++){
        grid[GRID_WIDTH + i] = COLOUR_BORDER + BORDER_ID;
        grid[(GRID_HEIGHT - 2) * GRID_WIDTH + i] = COLOUR_BORDER + BORDER_ID;
    }

    for(int i = 2; i < GRID_HEIGHT - 2; i++){
        grid[i * GRID_WIDTH + 1] = COLOUR_BORDER + BORDER_ID;
        grid[i * GRID_WIDTH + GRID_WIDTH - 2] = COLOUR_BORDER + BORDER_ID;
    }
}

void FallingSandGame::placeElementsAtLocation(int x, int y, int element){

    if (element == COLOUR_AIR) {
        for(int i = 0; i < cursor.cursorSize; i++) {
            for(int j = 0; j < cursor.cursorSize; j++) {
                int index = (y + i) * GRID_WIDTH + x + j;
                if ((grid[index] != AIR_ID) && ((grid[index] & ID_MASK) != BORDER_ID)) {
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

    if (numParticles >= MAX_NUM_PARTICLES) {
        return;
    }



    for(int i = 0; i < cursor.cursorSize; i++) {
        for(int j = 0; j < cursor.cursorSize; j++) {

            int index = (y + i) * GRID_WIDTH + x + j;
            if (grid[index] == AIR_ID) {

                if ((element & ID_MASK) == STONE_ID) {  // No RNG for stone
                    grid[index] = element + getModifiers(element);
                    numParticles++;

                    hitChunk(x + j, y + i);

                    if (numParticles >= MAX_NUM_PARTICLES) {
                        return;
                    }
                } 
                
                else {  // For other elements with RNG, want rng to be dependent on cursor size. smallest cursor has no rng

                    if (rng() >= 13) {
                        grid[index] = element + getModifiers(element);
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

int FallingSandGame::getModifiers(int element){

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
            return - COLOUR_GREEN*(rng_val) - COLOUR_RED*(rng_val) - COLOUR_BLUE*(rng_val) + BASE_STONE_LIFE;
        case SALT_ID:
            rng_val = rng_val % 3;
            return - COLOUR_GREEN*(rng_val) - COLOUR_RED*(rng_val) - COLOUR_BLUE*(rng_val);
        case LAVA_ID:
            rng_val = rng_val % 5;
            return - COLOUR_GREEN*(rng_val);
        case WOOD_ID:
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
    } 
    else if (targetElementId == LAVA_ID){
        lavaSandInteraction(x, y);
    }
    else {  
        int direction = getRandomDirection();

        targetElementId = grid[(y + 1) * GRID_WIDTH + (x + direction)] & ID_MASK;
        if (xInbounds(x + direction)) {
            targetElementId = grid[(y + 1) * GRID_WIDTH + (x + direction)] & ID_MASK;
            if (targetElementId == AIR_ID || targetElementId == WATER_ID){
                swap(x, y, x + direction, y + 1);
            }
            if (targetElementId == LAVA_ID) {
                lavaSandInteraction(x, y);
            }

        } 
        else if (xInbounds(x - direction)) {
            targetElementId = grid[(y + 1) * GRID_WIDTH + (x - direction)] & ID_MASK;
            if (targetElementId == AIR_ID || targetElementId == WATER_ID) {
                swap(x, y, x - direction, y + 1);
            }
            if (targetElementId == LAVA_ID) {
                lavaSandInteraction(x, y);
            }
        }
    }
}

void FallingSandGame::updateWater(int x, int y){
    //water falls first, when it hits the ground it moves randomly left or right by x distance
    //saltwater is heavier than freshwater, so it sinks to the bottom
    //give it the option to swap with water below, then do the below logic normally

    int targetElementId = grid[(y + 1) * GRID_WIDTH + x] & ID_MASK;
    bool isSaltWater = grid[y * GRID_WIDTH + x] & IN_ALT_STATE; //we already know its water, so we just need to check if its salt water

    if(isSaltWater && (targetElementId == WATER_ID)){       //saltwater is heavier than freshwater
        swap(x, y, x, y + 1);
    }
    if((targetElementId) == AIR_ID) {       //fall into air
        swap(x, y, x, y + 1);
    } 
    else if (targetElementId == SALT_ID && !isSaltWater) {   //fall into salt
        waterSaltInteraction(x, y, x, y + 1);
    }
    else if (targetElementId == LAVA_ID) {   //fall into lava
        lavaWaterInteraction(x, y, x, y + 1);
    }

    //else something is below the water, so it moves left or right
    else {
        int direction = getRandomDirection();
        int openSpace = 0;
        int elementIDInWay = waterSearchHorizontally(x, y, direction, rng() % 4 + 1, &openSpace);

        if (elementIDInWay == SALT_ID && !isSaltWater) {
            waterSaltInteraction(x, y, x + openSpace + direction, y);
        }
        else if (elementIDInWay == LAVA_ID) {
            lavaWaterInteraction(x, y, x + openSpace + direction, y);
        }
        else if (openSpace != 0) {      //either air, sand, or water was found in range
            swap(x, y, x + openSpace, y);
        }

        else {
            openSpace = 0;
            elementIDInWay = waterSearchHorizontally(x, y, -direction, rng() % 4 + 1, &openSpace);
            if (elementIDInWay == SALT_ID && !isSaltWater) {
                waterSaltInteraction(x, y, x + openSpace - direction, y);
            }
            else if (elementIDInWay == LAVA_ID) {
                lavaWaterInteraction(x, y, x + openSpace - direction, y);
            }
            else if (openSpace != 0) {      //either air, sand, or water was found in range
                swap(x, y, x + openSpace, y);
            }
        }   
    }
}

void FallingSandGame::updateSalt(int x, int y){
    //salt behaves like sand, but also dissolves in water to set water isSaltWater to true + change colour

    if(y == GRID_HEIGHT - 1) return;

    int targetElement = grid[(y + 1) * GRID_WIDTH + x];
    int targetElementId = targetElement & ID_MASK;

    if(targetElementId == AIR_ID || targetElementId == LAVA_ID) {
        swap(x, y, x, y + 1);
    }
    else if (isFreshWater(targetElement)) {   //fall into water
        waterSaltInteraction(x, y, x, y + 1);
    }
    else {  
        int direction = getRandomDirection();

        targetElement = grid[(y + 1) * GRID_WIDTH + (x + direction)];   //actually okay to do this because not at bottom of grid and bounds checked later
        targetElementId = targetElement & ID_MASK;
        if (xInbounds(x + direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID || targetElementId == LAVA_ID)) {

            if (isFreshWater(targetElement)) {
                waterSaltInteraction(x, y, x + direction, y + 1);
                return;
            }

            swap(x, y, x + direction, y + 1);
        } 
        else if (xInbounds(x - direction)) {

            targetElement = grid[(y + 1) * GRID_WIDTH + (x - direction)];
            targetElementId = targetElement & ID_MASK;

            if (targetElementId == AIR_ID || targetElementId == WATER_ID || targetElementId == LAVA_ID) {
                if (isFreshWater(targetElement)) {
                    waterSaltInteraction(x, y, x - direction, y + 1);
                    return;
                }

                swap(x, y, x - direction, (y + 1));
            }
        }
    }
}

bool FallingSandGame::isFreshWater(int element){
    return (((element & ID_MASK) == WATER_ID) && !(element & IN_ALT_STATE));
}

void FallingSandGame::updateLava(int x, int y){
    //lava falls first, when it hits the ground it moves randomly left or right by x distance

    int targetElementId = grid[(y + 1) * GRID_WIDTH + x] & ID_MASK;

    if(targetElementId == AIR_ID) {       //fall into air
        swap(x, y, x, y + 1);
    } 
    else if (targetElementId == WATER_ID) {   //fall into water
        lavaWaterInteraction(x, y, x, y + 1);
    }
    else if (targetElementId == STONE_ID) {   //fall into stone
        lavaStoneInteraction(x, y + 1);
    }
    else if (targetElementId == SAND_ID) {   //fall into sand
        lavaSandInteraction(x, y + 1);
    }

    else {  
        int direction = getRandomDirection();
        int openSpace = 0;
        int elementIDInWay = lavaSearchHorizontally(x, y, direction, rng() % 2 + 1, &openSpace);


        if (elementIDInWay == STONE_ID) {
            //swap(x, y, x + openSpace, y);
            lavaStoneInteraction(x + openSpace + direction, y);
        }
        else if (elementIDInWay == SAND_ID) {
            //swap(x, y, x + openSpace, y);
            lavaSandInteraction(x + openSpace + direction, y);
        }
        else if (elementIDInWay == WATER_ID) {
            //swap(x, y, x + openSpace, y);
            lavaWaterInteraction(x, y, x + openSpace + direction, y);   // could remove + direction i think
        }
        else if (openSpace != 0) {      //air space was found and not water, stone, sand, or salt in range
            swap(x, y, x + openSpace, y);
        }
        else {
            openSpace = 0;
            elementIDInWay = lavaSearchHorizontally(x, y, -direction, rng() % 2 + 1, &openSpace);

            if (elementIDInWay == STONE_ID) {
                //swap(x, y, x + openSpace, y);
                lavaStoneInteraction(x + openSpace - direction, y);
            }
            else if (elementIDInWay == SAND_ID) {
                //swap(x, y, x + openSpace, y);
                lavaSandInteraction(x + openSpace - direction, y);
            }
            else if (elementIDInWay == WATER_ID) {
                //swap(x, y, x + openSpace, y);
                lavaWaterInteraction(x, y, x + openSpace - direction, y);   //probably right
            }
            else if (openSpace != 0) {      //air space was found and not water, stone, sand, or salt in range
                swap(x, y, x + openSpace, y);
            }
        }
        
    }

}

void FallingSandGame::waterSaltInteraction(int sourceX, int sourceY, int targetX, int targetY){
    //target = where you want the salt to be
    //source = where the empty spot will be

    grid[targetY * GRID_WIDTH + targetX] = COLOUR_SALT_WATER + WATER_ID + IN_ALT_STATE;
    grid[sourceY * GRID_WIDTH + sourceX] = COLOUR_AIR;

    numParticles--;

    hitChunk(sourceX, sourceY);
    hitChunk(targetX, targetY);
}

void FallingSandGame::lavaWaterInteraction(int sourceX, int sourceY, int targetX, int targetY){
    
    //target = where you want the stone to be
    //source = where the empty spot will be

    //rules: water evaporates into air, lava becomes stone

    grid[targetY * GRID_WIDTH + targetX] = COLOUR_STONE + STONE_ID + BASE_STONE_LIFE + getModifiers(STONE_ID);

    //TODO: maybe place a steam particle somewhere
    grid[sourceY * GRID_WIDTH + sourceX] = COLOUR_AIR;

    numParticles--;

    hitChunk(sourceX, sourceY);
    hitChunk(targetX, targetY);
}

void FallingSandGame::lavaStoneInteraction(int stoneX, int stoneY){
    //lava removes life from stone, if life is 0, then stone becomes lava

    grid[stoneY * GRID_WIDTH + stoneX] -= LIFEBIT_MASK;

    int stoneLife = grid[stoneY * GRID_WIDTH + stoneX] & LIFESPAN_MASK;

    if(stoneLife <= 0){
        if (rng() % 2 == 0) {
            grid[stoneY * GRID_WIDTH + stoneX] = COLOUR_LAVA + LAVA_ID + getModifiers(LAVA_ID);
        }
        else {
            grid[stoneY * GRID_WIDTH + stoneX] = COLOUR_AIR;
            numParticles--;
        }
    }

    hitChunk(stoneX, stoneY);

}

void FallingSandGame::lavaSandInteraction(int sandX, int sandY){
    //lava removes sand instantly

    grid[sandY * GRID_WIDTH + sandX] = COLOUR_AIR;
    numParticles--;

    hitChunk(sandX, sandY);

}

int FallingSandGame::getRandomDirection(){
    u32 rng_val = rng();
    int direction = 1;
    if (rng_val % 2 == 0) {
        direction = -1;
    }
    return direction;
}

void FallingSandGame::updateWood(int x, int y){
    return;
}

void FallingSandGame::updateFire(int x, int y){
    return;
}


//TODO: refactor in the form of lavaSearchHorizontally
int FallingSandGame::waterSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace){
    
    for (int i = 1; i <= numSpaces; i++) {
        int targetElementId = grid[y * GRID_WIDTH + (x + i*direction)] & ID_MASK;
        if (xInbounds(x + i*direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID)) { //==Water because saltwater needs to disperse
            continue;
        }
        else {  //if the space is not open, then return the last open space

            *openSpace = (i - 1) * direction; // returns value between -numSpaces and numSpaces, including 0
            return targetElementId;
        }
    }
    *openSpace = numSpaces * direction;
    return AIR_ID;
}

int FallingSandGame::lavaSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace) {
    //want to return the element ID that is in the way of the lava, so that we can interact with it

    for (int i = 1; i <= numSpaces; i++) {
        int targetElementId = grid[y * GRID_WIDTH + (x + i*direction)] & ID_MASK;
        if (xInbounds(x + i*direction) && (targetElementId == AIR_ID)) {
            continue;
        }
        else {
            //lava will move beside an element and then interact with it
            *openSpace = (i - 1) * direction;     // returns value between -numSpaces and numSpaces, including 0
            return targetElementId;
        }
    }

    *openSpace = numSpaces * direction;
    return AIR_ID;

}

bool FallingSandGame::xInbounds(int x){
    return (x >= 0 && x < GRID_WIDTH);
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
                case SALT_ID:
                    updateSalt(x, y);
                    break;
                case LAVA_ID:
                    updateLava(x, y);
                    break;
                case WOOD_ID:
                    updateWood(x, y);
                    break;
                case FIRE_ID:
                    updateFire(x, y);
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
                case SALT_ID:
                    updateSalt(x, y);
                    break;
                case LAVA_ID:
                    updateLava(x, y);
                    break;
                case WOOD_ID:
                    updateWood(x, y);
                    break;
                case FIRE_ID:
                    updateFire(x, y);
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

    hitChunk(x1, y1);
    hitChunk(x2, y2);
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

#include "game.h"
#include "common.h"
#include "xil_io.h"
#include <math.h>
#include "xil_cache.h"


FallingSandGame::FallingSandGame(int* gridPtr){

    grid = gridPtr;
    memset(grid, 0, NUM_BYTES_BUFFER);

    numParticles = 0;

    cursor = {GRID_WIDTH/2, GRID_HEIGHT/2,
    		GRID_WIDTH/2, GRID_HEIGHT/2,
			CURSOR_COLOUR, CURSOR_LENGTH};

    drawBorder();

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

int FallingSandGame::handleInput(userInput_t* input){

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

	if (input->increaseCursor) {
		cursor.cursorSize += 2;
		if(cursor.cursorSize > MAX_CURSOR_LENGTH) {
			cursor.cursorSize = MIN_CURSOR_LENGTH;
		}
		input->increaseCursor = false;
	}

    //have to check bounds of cursor movement to ensure it stays within the grid
    if (cursor.y > FRAME_HEIGHT - cursor.cursorSize) cursor.y = FRAME_HEIGHT - cursor.cursorSize;
    if (cursor.x < 0) cursor.x = 0;
    if (cursor.x > FRAME_WIDTH -  cursor.cursorSize) cursor.x = FRAME_WIDTH -  cursor.cursorSize;
    if (cursor.y < 0) cursor.y = 0;

	//protection in case of multiple elements selected
    //switches have 1 hot encoding, so only one element can be selected at a time
    int element = getElement(input->selectedElement);

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

   return element & ID_MASK;

}

void FallingSandGame::drawBorder(){
    //efficient way to draw a border around the grid
    //border is 2 pixels wide
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

    if (element == COLOUR_AIR) {    //this is the equivalent of erasing elements
        for(int i = 0; i < cursor.cursorSize; i++) {
            for(int j = 0; j < cursor.cursorSize; j++) {
                int index = (y + i) * GRID_WIDTH + x + j;
                if ((grid[index] != AIR_ID) && ((grid[index] & ID_MASK) != BORDER_ID)) {
                    grid[index] = COLOUR_AIR;
                    numParticles--;

                    hitChunk(x + j, y + i);
                }
            }
        }
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
                
                else {  // For other elements with RNG

                    if (rng() >= 13) {  //rng threshold for placing elements, 3/16 chance of placing element
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
}

int FallingSandGame::getModifiers(int element){
    //function to vary the colour of the elements
    //to achieve a textured look
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
        default:
            return 0;
    }
}

//returns base colour + ID of the element selected
int FallingSandGame::getElement(short input){

    for (int i = 0; i < 6; i++){
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
    else {  //if there is something below the sand, then it looks down left and down right
        int direction = getRandomDirection();   //randomly choose left or right to look at first

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

    int targetElementId = grid[(y + 1) * GRID_WIDTH + x] & ID_MASK;
    bool isSaltWater = grid[y * GRID_WIDTH + x] & IN_ALT_STATE; //we already know its water, so we just need to check if its salt water

    if(isSaltWater && (targetElementId == WATER_ID)){       //saltwater is heavier than freshwater
        swap(x, y, x, y + 1);
    }
    if((targetElementId) == AIR_ID) {
        swap(x, y, x, y + 1);
    } 
    else if (targetElementId == SALT_ID && !isSaltWater) { 
        waterSaltInteraction(x, y, x, y + 1);
    }
    else if (targetElementId == LAVA_ID) { 
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
    else if (isFreshWater(targetElement)) {
        waterSaltInteraction(x, y, x, y + 1);
    }
    else {  
        int direction = getRandomDirection();

        targetElement = grid[(y + 1) * GRID_WIDTH + (x + direction)]; 
        targetElementId = targetElement & ID_MASK;
        if (xInbounds(x + direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID || targetElementId == LAVA_ID)) {
        //for air, water, and lava, salt can swap with them
            if (isFreshWater(targetElement)) {  //if water is fresh, then salt dissolves into it
                waterSaltInteraction(x, y, x + direction, y + 1);
                return;
            }

            swap(x, y, x + direction, y + 1);
        } 
        else if (xInbounds(x - direction)) {    //check the other direction

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
    int targetElementId = grid[(y + 1) * GRID_WIDTH + x] & ID_MASK;

    if(targetElementId == AIR_ID) { 
        swap(x, y, x, y + 1);
    } 
    else if (targetElementId == WATER_ID) {
        lavaWaterInteraction(x, y, x, y + 1);
    }
    else if (targetElementId == STONE_ID) { 
        lavaStoneInteraction(x, y + 1);
    }
    else if (targetElementId == SAND_ID) { 
        lavaSandInteraction(x, y + 1);
    }

    else {  //if there is something below the lava, then it looks left and right
        int direction = getRandomDirection();
        int openSpace = 0;
        int elementIDInWay = lavaSearchHorizontally(x, y, direction, rng() % 2 + 1, &openSpace);


        if (elementIDInWay == STONE_ID) {
            lavaStoneInteraction(x + openSpace + direction, y);
        }
        else if (elementIDInWay == SAND_ID) {
            lavaSandInteraction(x + openSpace + direction, y);
        }
        else if (elementIDInWay == WATER_ID) {
            lavaWaterInteraction(x, y, x + openSpace + direction, y);
        }
        else if (openSpace != 0) {      //air space was found and not water, stone, sand, or salt in range
            swap(x, y, x + openSpace, y);
        }
        else {
            openSpace = 0;
            elementIDInWay = lavaSearchHorizontally(x, y, -direction, rng() % 2 + 1, &openSpace);

            if (elementIDInWay == STONE_ID) {
                lavaStoneInteraction(x + openSpace - direction, y);
            }
            else if (elementIDInWay == SAND_ID) {
                lavaSandInteraction(x + openSpace - direction, y);
            }
            else if (elementIDInWay == WATER_ID) {
                lavaWaterInteraction(x, y, x + openSpace - direction, y);   //probably right
            }
            else if (openSpace != 0) {      //air space was found and not water, stone, sand, or salt in range
                swap(x, y, x + openSpace, y);
            }
        }
        
    }

}

void FallingSandGame::waterSaltInteraction(int sourceX, int sourceY, int targetX, int targetY){
    //Rules: when water and salt interact, they become salt water

    grid[targetY * GRID_WIDTH + targetX] = COLOUR_SALT_WATER + WATER_ID + IN_ALT_STATE;
    grid[sourceY * GRID_WIDTH + sourceX] = COLOUR_AIR;

    numParticles--; // because salt is diluted into water

    hitChunk(sourceX, sourceY);
    hitChunk(targetX, targetY);
}

void FallingSandGame::lavaWaterInteraction(int sourceX, int sourceY, int targetX, int targetY){
    //Rules: water evaporates into air, lava becomes stone  

    grid[targetY * GRID_WIDTH + targetX] = COLOUR_STONE + STONE_ID + BASE_STONE_LIFE + getModifiers(STONE_ID);
    grid[sourceY * GRID_WIDTH + sourceX] = COLOUR_AIR;

    numParticles--;

    hitChunk(sourceX, sourceY);
    hitChunk(targetX, targetY);
}

void FallingSandGame::lavaStoneInteraction(int stoneX, int stoneY){
    //Rules: lava reduces stone life, if life is 0, then stone has a 50% chance of becoming lava or air

    grid[stoneY * GRID_WIDTH + stoneX] -= LIFEBIT_MASK;

    int stoneLife = grid[stoneY * GRID_WIDTH + stoneX] & LIFESPAN_MASK;

    if(stoneLife <= 0){  //lava killed the stone
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
    //Rules: lava removes sand on contact

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

int FallingSandGame::waterSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace){
    //water may move multiple spaces left or right.
    //this function returns the first element it encounters in the direction it wants to move

    for (int i = 1; i <= numSpaces; i++) {
        int targetElementId = grid[y * GRID_WIDTH + (x + i*direction)] & ID_MASK;
        if (xInbounds(x + i*direction) && (targetElementId == AIR_ID || targetElementId == WATER_ID)) { //water can swap with water because saltwater needs to disperse
            continue;
        }
        else {  //if the space is not open, then return the last open space

            *openSpace = (i - 1) * direction; // returns value between -numSpaces and numSpaces, including 0
            return targetElementId;
        }
    }
    *openSpace = numSpaces * direction; // all spaces were open
    return AIR_ID;
}

int FallingSandGame::lavaSearchHorizontally(int x, int y, int direction, int numSpaces, int* openSpace) {
    //lava may move multiple spaces left or right.
    //this function returns the first element it encounters in the direction it wants to move

    for (int i = 1; i <= numSpaces; i++) {
        int targetElementId = grid[y * GRID_WIDTH + (x + i*direction)] & ID_MASK;
        if (xInbounds(x + i*direction) && (targetElementId == AIR_ID)) {
            continue;
        }
        else {
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
    //elements should call this function when they move to a new location

    int chunkX = absoluteX / CHUNK_SIZE;
    int chunkY = absoluteY / CHUNK_SIZE;

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
    for (int i = 0; i < GRID_WIDTH / CHUNK_SIZE; i++) {     //shift the current frame bools to the next frame
        for (int j = 0; j < GRID_HEIGHT / CHUNK_SIZE; j++) {
            chunks[i][j].computeOnCurrentFrame = chunks[i][j].computeOnNextFrame;
            chunks[i][j].computeOnNextFrame = false;
        }
    }
}

void FallingSandGame::update(){

    updateChunkBools();

    //iterate through the chunks and update the elements in the chunk
    //still need to iterate through the rows of the chunk, 
    //but this is a lot faster than iterating through the entire grid

    for(int y = GRID_HEIGHT/CHUNK_SIZE - 1; y >= 0; y--){
        for(int rowOfY = CHUNK_SIZE - 1; rowOfY >= 0; rowOfY--){    //still O(n^2) because rowOfY is a small constant
            for(int x = 0; x < GRID_WIDTH/CHUNK_SIZE; x++){
                if(chunks[x][y].computeOnCurrentFrame){
                    updateRowChunk(x, y, rowOfY);
                }
            }
        }
    }
}

void FallingSandGame::updateRowChunk(int chunkX, int chunkY, int row){


    u32 rng_val = rng();        //to prevent strange behaviour, randomize the direction of the update

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
                    break;
                case SALT_ID:
                    updateSalt(x, y);
                    break;
                case LAVA_ID:
                    updateLava(x, y);
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
                    break;
                case SALT_ID:
                    updateSalt(x, y);
                    break;
                case LAVA_ID:
                    updateLava(x, y);
                    break;
                default:
                    break;
            }
        }
    }
}

void FallingSandGame::swap(int x1, int y1, int x2, int y2){
    int temp = grid[y1*GRID_WIDTH + x1];
    grid[y1*GRID_WIDTH + x1] = grid[y2*GRID_WIDTH + x2];
    grid[y2*GRID_WIDTH + x2] = temp;

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

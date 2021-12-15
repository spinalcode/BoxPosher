// =APP.customSetVariables({maxLogLength:100000})
//#define EMULATION
#include <Pokitto.h>
#include "timer_11u6x.h" // for screen fade timer
#include "clock_11u6x.h" // for screen fade timer
#include "PokittoCookie.h" // for save data
#include <File> // for level loading
#include <LibAudio> // for music playing
#include "graphics.h"
#include "globals.h"
#include "fonts.h"
#include "buttonhandling.h"
#include "sounds.h"
#include "screen.h"

#define my_TIMER_16_0_IRQn 16          // for Timer setup

void setVol(uint32_t v){
    v = 255 - (192 - v) * (192 - v) * 255 / 36864;
    uint32_t hwVolume = v ? (v>>1) | 0xF : 0;
    uint32_t swVolume = v ? (v | 0xF) + 1 : 0;
    SoftwareI2C(P0_4, P0_5).write(0x5e, hwVolume);
    SoftwareI2C(P0_5, P0_4).write(0x5e, hwVolume); // fix for newer boards with i2C right way around
}

// print text
void guiPrint(char x, char y, const char* text, bool menuText=0) {
  uint8_t numChars = strlen(text);
  int charPos = x + 27 * y;
  for (int t = 0; t < numChars;) {
    guiLineHasText[charPos/27] = 1+menuText;
    guiBG[charPos++] = text[t++];
  }
}

void drawMenu(char x, char y, char x1, char y1) {

    for(int yy = y; yy <= y1; yy++){
        int yCount = 27*yy;
        for(int xx = x; xx <= x1; xx++){
            if(yy==y){
                guiBG[xx + yCount] = 2;
            }else if(yy==y1){
                guiBG[xx + yCount] = 5;
            }else if(yy==y+2){
                guiBG[xx + yCount] = 10;
            }else{
                guiBG[xx + yCount] = 12;
            }
            if(xx==x){
                guiBG[xx + yCount] = 7;
            }else if(xx==x1){
                guiBG[xx + yCount] = 8;
            }
            guiLineHasText[yy] = 2;
        }
    }

    guiBG[x +27*y ] = 1;
    guiBG[x +27*(y+2) ] = 9;
    guiBG[x1+27*(y+2) ] = 11;
    guiBG[x1+27*y ] = 3;
    guiBG[x1+27*y1] = 6;
    guiBG[x +27*y1] = 4;
}



void bigPrint(char x, char y, const char* text) {
  uint8_t numChars = strlen(text);
  for (int t = 0; t < numChars;) {
    drawSprite(x+=16, y, bigfont[text[t++]-32], bigfont_pal, 0, 2);
  }
}

// how manu digits in an int?
int numPlaces (int n) {
    if (n < 0) return 0;
    if (n < 10) return 1;
    if (n < 100) return 2;
    if (n < 1000) return 3;
    if (n < 10000) return 4;
    if (n < 100000) return 5;
    if (n < 1000000) return 6;
    if (n < 10000000) return 7;
    if (n < 100000000) return 8;
    if (n < 1000000000) return 9;
    /*      2147483647 is 2^31-1 - add more ifs as needed
       and adjust this final return as well. */
    return 10;
}

void render_level(){

    if(renderSprites==true){
        // Draw player
        drawSprite(player.x - xScroll, player.y - yScroll, hero[(player.direction * 4)+(player.steps/8)], hero_pal, 0, 8);
    
        // Draw Boxes
        for(int t=0; t<numBoxes; t++){
            drawSprite(boxes[t].x - xScroll, boxes[t].y - yScroll, box[boxes[t].frame], box_pal, 0, 8);
        }
        
        // walk icon
        drawSprite(2, 176-16, step_icon_sml[0], step_icon_sml_pal, 0, 4);
        drawSprite(34, 176-16, step_icon_sml[1], step_icon_sml_pal, 0, 4);
        sprintf(tempText,":%d",totalSteps);
        guiPrint(6,20, tempText, 0);
    
        sprintf(tempText,":%d.%d",totalSteps/2000,totalSteps/20);
        guiPrint(6,21, tempText, 0);
    
        drawSprite(168, 176-16, step_icon_sml[2], step_icon_sml_pal, 0, 4);
        drawSprite(201, 176-16, step_icon_sml[3], step_icon_sml_pal, 0, 4);
    
        sprintf(tempText,"%d:",moveNumber);
        guiPrint(20-numPlaces(moveNumber),20, tempText);
        sprintf(tempText,"%d:",levNum);
        guiPrint(20-numPlaces(levNum),21, tempText);
    }
}

void fadeOut(){
    for(int t=255; t>0; t--) {
        bright = t;
        long int time_now = Pokitto::Core::getTime();
        while(Pokitto::Core::getTime() < time_now + 3){
            render_level();
            Pokitto::Core::update(1);
        }
    }
    bright=0;
}

void fadeIn(){

    for(int t=0; t<255; t++) {
        bright = t;
        long int time_now = Pokitto::Core::getTime();
        while(Pokitto::Core::getTime() < time_now + 4){
            render_level();
            Pokitto::Core::update(1);
        }
    }
    bright=255;
}


using PC = Pokitto::Core;
using PD = Pokitto::Display;
using PB = Pokitto::Buttons;
using PS = Pokitto::Sound;

int countLevels(){
    
    char levelPath[64]={0};
    strcat(levelPath, "boxpusher/1.bin");

    char line[80]={0};
    levCount = 0;
    File fr;
    if(fr.openRO(levelPath)){

        bool finishedFile = false;
        int t=0;
        while(finishedFile == false){
            if(fr.read(&t,1)){
                if(t == ';'){
                    levCount++;
                    //printf("%d\n",levCount);
                }
            }else {
                finishedFile = true;
            }
        }
        fr.close();
    } // if file open

    return levCount;
}

void loadSokLev(int lev){

    int ldOffset = 10; // how far in to the level data to place the new level to avoid over-scrolling the screen
    
    //notYet=10; // delay level completion by 10 frames
    int x,y;
    for(y=0; y<levHeight; y++){
		for(x=0; x<levWidth; x++){
            curLevel[x+levWidth*y] = 0;
        }
    }
    printf("Cleaned level data\n");
    numBoxes = 0;
    numButtons = 0;

    char levelPath[64]={0};
    strcat(levelPath, "boxpusher/1.bin");

    char line[80]={0};
    File fr;
    if(fr.openRO(levelPath)){
        //printf("File did open\n");
        int levCount = 0;
        // find current level...
        while(levCount < lev){
            // read line
            int x = 0;
            bool completeLine = false;
            bool failed = false;
            while(completeLine == false){
                line[x+1]=0;
                if(fr.read(&line[x],1)){
                    if(line[x] == 0x0A){
                        completeLine = true;
                    }
                }else {
                    failed = true;
                }
                x++;
            }
            
            if(failed == false){
                if(line[0] == ';')levCount++;
                //printf("%s",line);
                // Reset level
                int nt = levHeight * levWidth;
                for(y=0; y<nt; y++){
                    curLevel[nt] = 0;
                }
                // remove boxes
                numBoxes=0;
            }
        }
        
        int spaceCount = 0;
        int boxNum=0;
        // read current level...
        while(spaceCount <2){
            int x=0;
            bool completeLine = false;
            bool failed = false;
            while(completeLine == false && failed == false){
                line[x+1]=0;
                if(fr.read(&line[x],1)){
                    if(line[x] == 0x0A){
                        completeLine = true;
                    }
                }else {
                    failed = true;
                }
                x++;
            }
            
            if(failed == false){
                if(strlen(line)<2){
                    spaceCount++; // surely blank line cant be more than 2 characters!
                    y = 0;
                }else{
                    //printf("length:%d\n", strlen(line));
                    for(x = 0; x< strlen(line); x++){
                        Pokitto::Core::update(1);
                        switch(line[x]){
                            case '#': // wall
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 1;
                                break;
                            case '.': // goal
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 2;
                                numButtons++;
                                break;
                            case '$': // box
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 3;
                                boxes[numBoxes].x = tileSize * (x+ldOffset);
                                boxes[numBoxes].y = tileSize * (y+ldOffset);
                                boxes[numBoxes].frame = random(8);//boxNum++; 
                                //if(boxNum > 4) boxNum=0;
                                //boxNum = boxNum &7;////random(5);
                                numBoxes++;
                                break;
                            case '*': // box on goal
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 4;
                                boxes[numBoxes].x = tileSize * (x+ldOffset);
                                boxes[numBoxes].y = tileSize * (y+ldOffset);
                                boxes[numBoxes].frame = random(8);//boxNum++; 
                                //if(boxNum > 4) boxNum=0;
                                //boxNum = boxNum &7;////random(5);
                                numBoxes++;
                                numButtons++;
                                break;
                            case '@': // player
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 5;
                                player.x = (ldOffset+x) * 32;
                                player.y = (ldOffset+y) * 32;
                                break;
                            case '+': // player on goal
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 6;
                                player.x = (ldOffset+x) * 32;
                                player.y = (ldOffset+y) * 32;
                                numButtons++;
                                break;
                        } // switch
                    } // x
                    y++;
                } // len line < 2
                printf("%s",line);
            } // if read file
        } // if space < 2
        fr.close();
        if(lev>cookie.level){
            cookie.level = lev;
            cookie.saveCookie();
        }

    } // if file open
    player.direction = 1; // face forward
    moveNumber=0; // reset the undo history
}

int checkTile(int x, int y){
    int ly = y/tileSize;
    int lx = x/tileSize;
    return curLevel[lx+levWidth*ly];
}

int checkBox(int x, int y){
    for(int t=0; t<numBoxes; t++){
        if(boxes[t].x == x && boxes[t].y == y){
            return 1;
        }
    }
    return 0;
}

void checkComplete(){
        boxesOnButtons = 0;
        if(player.walking==false){
            // check for level complete
            for(int t=0; t<numBoxes; t++){
                if(boxes[t].walking==false){
                    int bx = boxes[t].x/tileSize;
                    int by = boxes[t].y/tileSize;
                    // for some reason curLevel[bx+levWidth*by]&1 was always never 0
                    if(curLevel[bx+levWidth*by] == 2 ||
                        curLevel[bx+levWidth*by] == 4 ||
                        curLevel[bx+levWidth*by] == 6){
                        boxesOnButtons++;
                    }
                }
            }
        }

        if(numButtons == boxesOnButtons){
            // Level is complete
            levNum++;
            if(levNum > totalNumberOfLevels) levNum=0;
            
            yScroll = player.y - 80;
            xScroll = player.x - 94;

            render_level();
            PD::update();
            
            fadeOut();
            
            // clear the BG map
            int num = levWidth*levHeight;
            for(int t=0; t<num; t++){
                curLevel[t] = 7;
            }
            spriteCount=0;
            clearText();
            numBoxes = 0;
            numButtons = 0;
            updateButtons();
            renderSprites = false;
            guiPrint(6,8,"Level Complete!");
            sprintf(tempText,"Total Moves:%d",moveNumber);
            guiPrint(6,10,tempText);
            sprintf(tempText,"Next Level:%d",levNum);
            guiPrint(6,12,tempText);
            PD::update();

            renderSprites = false;
            fadeIn();

            long int time_now = Pokitto::Core::getTime();
            while(Pokitto::Core::getTime() < time_now + 1500){ // 1 second delay
//                render_level();
                Pokitto::Core::update(0);
            }
            fadeOut();
            clearText();

            bright = 0;

            loadSokLev(levNum);
            
            spriteCount=0;
            yScroll = player.y - 80;
            xScroll = player.x - 94;
            renderSprites = true;
            render_level();
            player.direction=1;//face forward
            PD::update();
            fadeIn();
        }    
    
}

int boxHere(int x, int y){

    int bx = -1;
    for(int t=0; t<numBoxes; t++){
        if(boxes[t].x == player.x +x && boxes[t].y == player.y + y) bx = t;
    }
    return bx;
}

void playLevel(){

        if(player.walking==0){
            if( _B_But[NEW] && moveNumber >0){
                moveNumber--;
                if((moveHistory[moveNumber]&3) == 0){
                    int bx = boxHere(0,-tileSize);
                    player.y += tileSize;
                    if((moveHistory[moveNumber]&4) != 0) boxes[bx].y += tileSize;
                    player.direction=moveHistory[moveNumber]&3;
                }
                if((moveHistory[moveNumber]&3) == 1){
                    int bx = boxHere(0,tileSize);
                    player.y -= tileSize;
                    if((moveHistory[moveNumber]&4) != 0) boxes[bx].y -= tileSize;
                    player.direction=moveHistory[moveNumber]&3;
                }
                if((moveHistory[moveNumber]&3) == 2){
                    int bx = boxHere(-tileSize,0);
                    player.x += tileSize;
                    if((moveHistory[moveNumber]&4) != 0) boxes[bx].x += tileSize;
                    player.direction=moveHistory[moveNumber]&3;
                }
                if((moveHistory[moveNumber]&3) == 3){
                    int bx = boxHere(tileSize,0);
                    player.x -= tileSize;
                    if((moveHistory[moveNumber]&4) != 0) boxes[bx].x -= tileSize;
                    player.direction=moveHistory[moveNumber]&3;
                }
            }


            if (   _Up_But[HELD]){ player.direction=0; player.steps=0; player.walking = checkTile(player.x, player.y-tileSize) == WALL ? false : true; } else
            if ( _Down_But[HELD]){ player.direction=1; player.steps=0; player.walking = checkTile(player.x, player.y+tileSize) == WALL ? false : true; } else
            if ( _Left_But[HELD]){ player.direction=2; player.steps=0; player.walking = checkTile(player.x-tileSize, player.y) == WALL ? false : true; } else
            if (_Right_But[HELD]){ player.direction=3; player.steps=0; player.walking = checkTile(player.x+tileSize, player.y) == WALL ? false : true; }
        }

        // look for a box in the walking direction
        if(player.walking){
            switch (player.direction){
                case 0: // up
                    for(int t=0; t<numBoxes; t++){
                        if(boxes[t].x == player.x && boxes[t].y == player.y - tileSize){
                            if(checkTile(boxes[t].x, boxes[t].y-tileSize) != WALL && checkBox(boxes[t].x, boxes[t].y-tileSize) == 0){
                                currentBox = t;
                                boxes[currentBox].walking = true;
                            }else {
                                player.walking = false;
                                boxes[currentBox].walking = false;
                            }
                        }
                    }
                    break;
                case 1: // down
                    for(int t=0; t<numBoxes; t++){
                        if(boxes[t].x == player.x && boxes[t].y == player.y + tileSize){
                            if(checkTile(boxes[t].x, boxes[t].y+tileSize) != WALL && checkBox(boxes[t].x, boxes[t].y+tileSize) == 0){
                                currentBox = t;
                                boxes[currentBox].walking = true;
                            }else {
                                player.walking = false;
                            }
                        }
                    }
                    break;
                case 2: // left
                    for(int t=0; t<numBoxes; t++){
                        if(boxes[t].x == player.x - tileSize && boxes[t].y == player.y){
                            if(checkTile(boxes[t].x-tileSize, boxes[t].y) != WALL && checkBox(boxes[t].x-tileSize, boxes[t].y) == 0){
                                currentBox = t;
                                boxes[currentBox].walking = true;
                            }else {
                                player.walking = false;
                            }
                        }
                    }
                    break;
                case 3: // right
                    for(int t=0; t<numBoxes; t++){
                        if(boxes[t].x == player.x + tileSize && boxes[t].y == player.y){
                            if(checkTile(boxes[t].x+tileSize, boxes[t].y) != WALL && checkBox(boxes[t].x+tileSize, boxes[t].y) == 0){
                                currentBox = t;
                                boxes[currentBox].walking = true;
                            }else {
                                player.walking = false;
                            }
                        }
                    }
                    break;
            }
            
        }

        if(player.steps==0){
            // if we start walking, the add to the move history        
            if(player.walking==1){
                moveHistory[moveNumber++] = player.direction;
            }
            if(boxes[currentBox].walking == true){
                moveHistory[moveNumber-1] |= 4;
                Audio::play(slide);
            }
        }


        // if a box is moving, move it
        if(boxes[currentBox].walking){
            switch (player.direction){
                case 0:
                    boxes[currentBox].y = player.y - tileSize;
                    break;
                case 1:
                    boxes[currentBox].y = player.y + tileSize;
                    break;
                case 2:
                    boxes[currentBox].x = player.x - tileSize;
                    break;
                case 3:
                    boxes[currentBox].x = player.x + tileSize;
                    break;
            }
        }

        // if still walking, do so
        if(player.walking){
            switch (player.direction){
                case 0:
                    player.y-=STEPSIZE;
                    player.steps+=STEPSIZE;
                    break;
                case 1:
                    player.y+=STEPSIZE;
                    player.steps+=STEPSIZE;
                    break;
                case 2:
                    player.x-=STEPSIZE;
                    player.steps+=STEPSIZE;
                    break;
                case 3:
                    player.x+=STEPSIZE;
                    player.steps+=STEPSIZE;
                    break;
            }
            if(player.steps%16==0){
                totalSteps++;
            }
            // if walked a full tile, stop
            if(player.steps==32){
                player.steps=0;
                player.walking=false;
                boxes[currentBox].walking=false;
                boxes[currentBox].y = (int)((boxes[currentBox].y+16)/tileSize) * tileSize;
                boxes[currentBox].x = (int)((boxes[currentBox].x+16)/tileSize) * tileSize;
    
                checkComplete();
            }
        }

        yScroll = player.y - 80;
        xScroll = player.x - 94;

        render_level();        

}

//int updated;
void screenTimer(void){
	if (Chip_TIMER_MatchPending(LPC_TIMER16_0, 1)) {
/*
        if(++updated == 2){
            updated = 0;
            writeDAC( myMixSound() );
            currentBuffer = ((++audioOffset)&=2047)/audioBufferSize;
        }
*/
        if(++lightCount==255)lightCount=0;
        if(lightCount < bright){
            backlight = 1;
        }else {
            backlight = 0;
        }

        // last thing we reset the time
    	Chip_TIMER_ClearMatch(LPC_TIMER16_0, 1);

    }

}

// timer init stolen directly from Pokittolib
void initTimer(uint32_t sampleRate){
    /* Initialize 32-bit timer 0 clock */
	Chip_TIMER_Init(LPC_TIMER16_0);
    /* Timer rate is system clock rate */
	uint32_t timerFreq = Chip_Clock_GetSystemClockRate();
	/* Timer setup for match and interrupt at TICKRATE_HZ */
	Chip_TIMER_Reset(LPC_TIMER16_0);
	/* Enable both timers to generate interrupts when time matches */
	Chip_TIMER_MatchEnableInt(LPC_TIMER16_0, 1);
    /* Setup 32-bit timer's duration (32-bit match time) */
	Chip_TIMER_SetMatch(LPC_TIMER16_0, 1, (timerFreq / sampleRate));
	/* Setup both timers to restart when match occurs */
	Chip_TIMER_ResetOnMatchEnable(LPC_TIMER16_0, 1);
	/* Start both timers */
	Chip_TIMER_Enable(LPC_TIMER16_0);
	/* Clear both timers of any pending interrupts */
	NVIC_ClearPendingIRQ((IRQn_Type)my_TIMER_16_0_IRQn);
    /* Redirect IRQ vector - Jonne*/
    NVIC_SetVector((IRQn_Type)my_TIMER_16_0_IRQn, (uint32_t)&screenTimer);
	/* Enable both timer interrupts */
	NVIC_EnableIRQ((IRQn_Type)my_TIMER_16_0_IRQn);
}

// t = time, b = start, c = end, d = duration
float easeDirect(float t, float b, float c, float d){
	return (((c-b)*t)/d)+b;
}

void titleScreen(){
/*
 ______  ____  ______  _        ___        _____   __  ____     ___    ___  ____  
|      ][    ]|      ]| ]      /  _]      / ___/  /  ]|    \   /  _]  /  _]|    \ 
|      | |  [ |      || |     /  [_      (   \_  /  / |  D  ) /  [_  /  [_ |  _  ]
[_]  [_] |  | [_]  [_]| [___ [    _]      \__  ]/  /  |    / [    _][    _]|  |  |
  |  |   |  |   |  |  |     ]|   [_       /  \ /   \_ |    \ |   [_ |   [_ |  |  |
  |  |   ]  [   |  |  |     ||     ]      \    \     ||  .  ]|     ]|     ]|  |  |
  [__]  [____]  [__]  [_____][_____]       \___]\____][__]\_][_____][_____][__]__]
*/

    int midScreen = 72; // 88-16
     // horizon images
    for(int t=0; t<7; t++){
        drawSprite(t*32, 72, horizon[t], horizon_pal, 0, 8);
    }
    guiPrint(7,4, "CURTIS NERDLY:");
    bigPrint(24, 44, "WAREHOUSE");
    bigPrint(40, 64, "MANAGER");


   if(intro_timer <= 80){
        guiPrint(5,19, "Game By : Spinal");
        guiPrint(2,20, "http://spinalcode.co.uk");
        ts.px = easeDirect(intro_timer, -64, 63, 80);
        ts.py=88;
        if(ts.dir==0){
            // left to right
            drawSprite(ts.px, ts.py, hero_stack[0], hero_stack_pal, 0, 8);   // player
            drawSprite(ts.px + 10, ts.py+21, stacker[0], stacker_pal, 0, 8); // truck
            drawSprite(ts.px + 42, ts.py+21, stacker[1], stacker_pal, 0, 8); // truck
            if(ts.movingBox){
                drawSprite(ts.px+32, ts.py+4, box[ts.boxNum], box_pal, 0, 8); // Box
            }
        }else{
            // right to left
            drawSprite(188-ts.px, ts.py, hero_stack[0], hero_stack_pal, 1, 8); // Player
            drawSprite(188-ts.px - 10, ts.py+21, stacker[0], stacker_pal, 1, 8); // truck
            drawSprite(188-ts.px - 42, ts.py+21, stacker[1], stacker_pal, 1, 8); // truck
            if(ts.movingBox){
                drawSprite(188-ts.px-32, ts.py+4, box[ts.boxNum], box_pal, 0, 8); // Box
            }
        }
    }

    bool standStill = 0;
    if(intro_timer >80 && intro_timer <100) standStill = 1;


    if(intro_timer >=100 && intro_timer <180){
        guiPrint(5,19, "Music By : Jayenkai");
        guiPrint(5,20, "https://socoder.net");
        ts.px = easeDirect(intro_timer - 100, -64, 63, 80);
        if(ts.dir==0){ // 62
            drawSprite(midScreen-74-ts.px, ts.py, hero_stack[2], hero_stack_pal, 0, 8); // Player
            drawSprite(midScreen-74-ts.px + 10, ts.py+21, stacker[0], stacker_pal, 0, 8); // truck
            drawSprite(midScreen-74-ts.px + 42, ts.py+21, stacker[1], stacker_pal, 0, 8); // truck
            if(ts.movingBox){
                drawSprite(midScreen-74-ts.px+32, ts.py+4, box[ts.boxNum], box_pal, 0, 8); // Box
            }
        }else{//126
            // right to left
            drawSprite(midScreen+117+ts.px, ts.py, hero_stack[2], hero_stack_pal, 1, 8); // Player
            drawSprite(midScreen+117+ts.px - 10, ts.py+21, stacker[0], stacker_pal, 1, 8); // truck
            drawSprite(midScreen+117+ts.px - 42, ts.py+21, stacker[1], stacker_pal, 1, 8); // truck
            if(ts.movingBox){
                drawSprite(midScreen+117+ts.px-32, ts.py+4, box[ts.boxNum], box_pal, 0, 8); // Box
            }
        }
    }

    if(standStill){
        if(ts.dir==0){
            drawSprite(ts.px, ts.py, hero_stack[1], hero_stack_pal, 0, 8);   // Player
            drawSprite(ts.px + 10, ts.py+21, stacker[0], stacker_pal, 0, 8); // truck
            drawSprite(ts.px + 42, ts.py+21, stacker[1], stacker_pal, 0, 8); // truck
        }else{
            // right to left
            drawSprite(188-ts.px, ts.py, hero_stack[1], hero_stack_pal, 1, 8); // Player
            drawSprite(188-ts.px - 10, ts.py+21, stacker[0], stacker_pal, 1, 8); // truck
            drawSprite(188-ts.px - 42, ts.py+21, stacker[1], stacker_pal, 1, 8); // truck
        }        
        drawSprite(94, 94, box[ts.boxNum], box_pal, 0, 8); // Box
    }

    if(ts.movingBox==0){
        drawSprite(94, 94, box[ts.boxNum], box_pal, 0, 8); // Box
    }


	if (intro_timer >=200){ // was 180
		intro_timer=0;
        //if(ts.title_item==4){
        //    ts.title_item=0;
        //    ts.holdItem = random(4);
        //}
        ts.dir = (random(100)<50 ? 0 : 1);
        if(ts.movingBox){
            ts.boxNum = random(7);
        }
	}

    if(_A_But[RELEASED]){
        gameMode=2;
        Pokitto::Display::lineFillers[0] = myBGFiller;
    }
    //if(++IT>=3){
        //IT=0;
        intro_timer += 0.33;//(.33 * (50/fpsCount)); // animation is based on 50fps.
        if(okToChangeBoxState){
            if((int)intro_timer == 100){
                ts.movingBox = 1- ts.movingBox;
                okToChangeBoxState = false;
            }
        }else{
            if((int)intro_timer == 1) okToChangeBoxState = true;
        }
    //}
}



void initTitleScreen(){

    Pokitto::Display::lineFillers[0] = myBGFiller2;
    Pokitto::Display::lineFillers[1] = spritesToLine;
    Pokitto::Display::lineFillers[2] = GUILine;

    // clear the levelmap data, need an empty map for the level select screen
    int num = levWidth*levHeight;
    for(int t=0; t<num; t++){
        curLevel[t] = 0;
    }

    ts.px=0;
    ts.py=80;
    ts.frm=0;
    gameMode=3;
    intro_timer = 0; // for timing animations etc.
    totalSteps = 0;

    sprintf(currentTune,"%s","/boxpusher/binky.pcm");
    PS::playMusicStream(currentTune); // main music

    // grab the current highest level unlocked
    levNum=1;
    cookie.loadCookie();
    levNum = cookie.level;
    if(levNum <1)levNum=1;
//    levNum = 141;
    worldOffset = 0;
    stLev = (levNum-1)/12;
}

void worldSelect(){

    //sprintf(tempText,"%02d",stLev);
    //guiPrint(13, 15, tempText);
    bigPrint(-2, 16, "SELECT CRATE");

    int numWorlds = levNum/12;
    for(int t=0; t<=numWorlds; t++){
        long int x = (78-(worldOffset>>8)) + t*220;
        if(x>-64 && x<220){
            drawSprite(x, 56, crate[0], crate_pal, 0, 8);
            drawSprite(x+32, 56, crate[1], crate_pal, 0, 8);
            drawSprite(x, 88, crate[2], crate_pal, 0, 8);
            drawSprite(x+32, 88, crate[3], crate_pal, 0, 8);
            sprintf(tempText,"%03d",t+1);
            bigPrint(x-8, 93, tempText);
        }
    }

    if(_Right_But[NEW]){
        stLev += 1;
        if(stLev > (levNum-1)/12) stLev = (levNum-1)/12;
    }

    if(_Left_But[NEW]){
        stLev -= 1;
        if(stLev < 0) stLev = 0;
    }

    if(_A_But[RELEASED]){
        gameMode=4;
    }

    worldOffset += ((((stLev<<8)*220)-worldOffset)/16);

}

void levelSelect(){
    
    //guiPrint(0,4, "Select Level");
//    sprintf(tempText,"%002d",levNum);
//    guiPrint(0, 2, tempText);

    int x=0,y=0;
    int levToLoad = 1;
    for(int t=0; t<12; t++){
        int crtFrm = 0;
        if((stLev*12)+t < levNum) crtFrm = 1;
        drawSprite((x*48)+20, (y*48)+16, smlcrate[crtFrm], smlcrate_pal, 0, 8);
        sprintf(tempText,"%03d",(stLev*12)+t+1);
        guiPrint(3+x*6, 6+y*6, tempText);
        if(x==levCursorX && y==levCursorY){
            levToLoad = (stLev*12)+t+1;
        }
        if(++x==4){y++; x=0;}
    }


    drawSprite(20+levCursorX * 48, 16+levCursorY*48, cursor, cursor_pal, 0, 8);

    if(_Up_But[NEW]){
        levCursorY--;
        if(levCursorY < 0) levCursorY = 0;
    }

    if(_Down_But[NEW]){
        levCursorY++;
        if(levCursorY > 2) levCursorY = 2;
    }

    if(_Right_But[NEW]){
        levCursorX++;
        if(levCursorX > 3) levCursorX = 3;
    }

    if(_Left_But[NEW]){
        levCursorX--;
        if(levCursorX < 0) levCursorX = 0;
    }


    if(_A_But[RELEASED]){
        if(levToLoad <= levNum){
            gameMode=1;
            sprintf(currentTune,"%s","/boxpusher/splat.pcm");
            PS::playMusicStream(currentTune); // main music
            levNum = levToLoad;
            loadSokLev(levNum);
        }
    }

    if(_B_But[RELEASED]){
        gameMode=2;
    }
    
}

int main() {

	PC::begin();
	PD::persistence=1;
    PD::adjustCharStep=0;
    PD::adjustLineStep=0;
    PD::fixedWidthFont = true;

    updateButtons();
    while(_A_But[HELD]){
        updateButtons();
    }

    Pokitto::Display::lineFillers[0] = myBGFiller2;
    Pokitto::Display::lineFillers[1] = spritesToLine;
    Pokitto::Display::lineFillers[2] = GUILine;

    cookie.begin("NERDLY", sizeof(cookie), (char*)&cookie);

    cookie.loadCookie();
    myVolume = cookie.volume;
    //if(myVolume < 5)
    //myVolume=3;

//    cookie.isEmu = 1;  // save this the first time using the emulator, but never on a real pokitto.

    cookie.saveCookie();
    #ifndef POK_SIM
        swvolpot.write(0x5e, myVolume);
    #endif
    //display.print(x-1,y-20,(int)sound.getVolume());


    uint32_t frameCount=0;
    uint8_t line[256];

    if(!cookie.isEmu){
        initTimer(32000); // for screen fades
    }



//    PS::playMusicStream("/boxpusher/binky.pcm"); // titlescreen
//    PS::playMusicStream("/boxpusher/splat.pcm"); // main music

    gameMode = 0;

    totalNumberOfLevels = countLevels();
    printf("%d Levels found.\n",totalNumberOfLevels);

    while (1) {
        
        if(!PC::update())continue;
        spriteCount=0;
        clearText();
        updateButtons();
 
        // clear the old text buffer
        for(int t=0; t<22; t++){
            guiLineHasText[t] = 0;
        }

 
        switch(gameMode){
            case 0:
                initTitleScreen();
            break;
            case 1:
                if(gamePaused == false){
                    playLevel();
                }
                if(_C_But[RELEASED]){
                    gamePaused = 1-gamePaused;
                }
            break;
            case 2:
                worldSelect();
            break;
            case 3:
                titleScreen();
            break;
            case 4:
                levelSelect();
            break;
        }

        if(gamePaused==true){
            // paused menu goes here.
            drawMenu(3, 3, 23, 18);
            guiPrint(10,4, "PAUSED",1);

            guiPrint(6,8,  "CONTINUE",1);
            guiPrint(6,10, "RESTART",1);
            sprintf(tempText,"VOLUME:%d ",myVolume);
            guiPrint(6,12, tempText,1);
            guiPrint(6,14, "QUIT",1);

            guiPrint(5,8+2*pm.item,">",1);

            if(_Up_But[NEW] && pm.item>0) pm.item--;
            if(_Down_But[NEW] && pm.item<3) pm.item++;

            if(_A_But[RELEASED]){
                switch(pm.item){
                    case 0: // return to game
                        cookie.volume = myVolume;
                        cookie.saveCookie();
                        gamePaused = 0;
                    break;
                    case 1:
                        loadSokLev(levNum);
                        gamePaused = 0;
                        break;
                    case 3: // return to titleScreen
                        cookie.volume = myVolume;
                        cookie.saveCookie();
                        gamePaused = 0;
                        gameMode = 0;
                    break;
                }
            }

            if(_Right_But[RELEASED]){
                switch(pm.item){
                    case 2: // volume
                        if(myVolume < 8) myVolume++;
                        setVol(myVolume);
                    break;
                }
            }

            if(_Left_But[RELEASED]){
                switch(pm.item){
                    case 2: // volume
                        if(myVolume > 0) myVolume--;
                        setVol(myVolume);
                    break;
                }
            }

/*
*/

        }

//        sprintf(tempText,"  FPS [%03d] ",fpsCount);
//        guiPrint(0,0, tempText);

        if(PC::getTime() >= lastMillis+1000){
            lastMillis = PC::getTime();
            fpsCount = fpsCounter;
            fpsCounter = 0;
        }

 //       PC::update();
 
    }

    return 0;
}
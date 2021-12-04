// =APP.customSetVariables({maxLogLength:100000})
#define EMULATION
#include <Pokitto.h>
#include "timer_11u6x.h" // for screen fade timer
#include "clock_11u6x.h" // for screen fade timer
#include "PokittoCookie.h" // for save data
#include <File> // for level loading
#include <LibAudio> // for music playing
#include "globals.h"
#include "graphics.h"
#include "fonts.h"
#include "buttonhandling.h"
#include "sounds.h"

#define my_TIMER_16_0_IRQn 16          // for Timer setup

// print text
void guiPrint(char x, char y, const char* text) {
//  guiLineHasText[y] = 1;
  uint8_t numChars = strlen(text);
  int charPos = x + 27 * y;
  for (int t = 0; t < numChars;) {
    guiLineHasText[charPos/27] = 1;
    guiBG[charPos++] = text[t++];
  }
}

#include "screen.h"

using PC = Pokitto::Core;
using PD = Pokitto::Display;
using PB = Pokitto::Buttons;
using PS = Pokitto::Sound;

int countLevels(){
    
    char levelPath[64]={0};
    strcat(levelPath, "boxpusher/1.txt");

    char line[80]={0};
    int levCount = 0;
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
    strcat(levelPath, "boxpusher/1.txt");

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
                                boxes[numBoxes].frame = boxNum++; 
                                if(boxNum > 4) boxNum=0;
                                //boxNum = boxNum &7;////random(5);
                                numBoxes++;
                                break;
                            case '*': // box on goal
                                curLevel[ldOffset+x+levWidth*(ldOffset+y)] = 4;
                                boxes[numBoxes].x = tileSize * (x+ldOffset);
                                boxes[numBoxes].y = tileSize * (y+ldOffset);
                                boxes[numBoxes].frame = boxNum++; 
                                if(boxNum > 4) boxNum=0;
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
        cookie.level = lev;
        cookie.saveCookie();

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
            yScroll = player.y - 80;
            xScroll = player.x - 47;
            // Draw player
            drawSprite(player.x - xScroll, player.y - yScroll, hero[(player.direction * 4)+(player.steps/WALKSPEED)], hero_pal, 0, 8);
            // Draw Boxes
            for(int t=0; t<numBoxes; t++){
                drawSprite(boxes[t].x - xScroll, boxes[t].y - yScroll, box[boxes[t].frame], box_pal, 0, 8);
            }
            PD::update();
            fadeOut();
            
            for(int t=0; t<100; t++){
                wait_ms(5);
                Pokitto::Core::update(1); // keep the sound playing during the delay
            }
            
            loadSokLev(levNum);
            
            spriteCount=0;
            yScroll = player.y - 80;
            xScroll = player.x - 47;
            // Draw player
            drawSprite(player.x - xScroll, player.y - yScroll, hero[(player.direction * 4)+(player.steps/WALKSPEED)], hero_pal, 0, 8);
            // Draw Boxes
            for(int t=0; t<numBoxes; t++){
                drawSprite(boxes[t].x - xScroll, boxes[t].y - yScroll, box[boxes[t].frame], box_pal, 0, 8);
            }
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
        xScroll = player.x - 47;

        // Draw player
        drawSprite(player.x - xScroll, player.y - yScroll, hero[(player.direction * 4)+(player.steps/8)], hero_pal, 0, 8);
        //if(player.walking==true && (player.steps&7)==0){player.stepFrame++; if(player.stepFrame>=4)player.stepFrame=0;}
        
        // Draw Boxes
        for(int t=0; t<numBoxes; t++){
            drawSprite(boxes[t].x - xScroll, boxes[t].y - yScroll, box[boxes[t].frame], box_pal, 0, 8);
        }

}

void audioTimer(void){
	if (Chip_TIMER_MatchPending(LPC_TIMER16_0, 1)) {

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
    NVIC_SetVector((IRQn_Type)my_TIMER_16_0_IRQn, (uint32_t)&audioTimer);
	/* Enable both timer interrupts */
	NVIC_EnableIRQ((IRQn_Type)my_TIMER_16_0_IRQn);
}

// t = time, b = start, c = end, d = duration
float easeDirect(float t, float b, float c, float d){
	return (((c-b)*t)/d)+b;
}

void titleScreen(){
/*
 ______  ____  ______  _             ___  _____   __  ____     ___    ___  ____  
|      ][    ]|      ]| ]           /  _]/ ___/  /  ]|    \   /  _]  /  _]|    \ 
|      | |  [ |      || |          /  [_(   \_  /  / |  D  ) /  [_  /  [_ |  _  ]
[_]  [_] |  | [_]  [_]| [___      [    _]\__  ]/  /  |    / [    _][    _]|  |  |
  |  |   |  |   |  |  |     ]     |   [_ /  \ /   \_ |    \ |   [_ |   [_ |  |  |
  |  |   ]  [   |  |  |     |     |     ]\    \     ||  .  ]|     ]|     ]|  |  |
  [__]  [____]  [__]  [_____]     [_____] \___]\____][__]\_][_____][_____][__]__]
*/

    int IT = (int)intro_timer;

    // horizon images
    for(int t=0; t<7; t++){
        drawSprite(t*32, 72, horizon[t], horizon_pal, 0, 8);
    }


    int midScreen = 72; // 88-16
    if(IT <= 80){
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
    if(IT >80 && IT <100) standStill = 1;
    if(IT == 100) ts.movingBox = 1- ts.movingBox;
    
    guiPrint(7,6, "Curtis Nerdly");
    guiPrint(5,8, "Warehouse Manager");

    if(IT >=100 && IT <180){
        ts.px = easeDirect(intro_timer - 100, -64, 63, 80);
        if(ts.dir==0){ // 62
            drawSprite(midScreen-72-ts.px, ts.py, hero_stack[2], hero_stack_pal, 0, 8); // Player
            drawSprite(midScreen-72-ts.px + 10, ts.py+21, stacker[0], stacker_pal, 0, 8); // truck
            drawSprite(midScreen-72-ts.px + 42, ts.py+21, stacker[1], stacker_pal, 0, 8); // truck
            if(ts.movingBox){
                drawSprite(midScreen-72-ts.px+32, ts.py+4, box[ts.boxNum], box_pal, 0, 8); // Box
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


	if (IT >=200){ // was 180
		intro_timer=0;
        //if(ts.title_item==4){
        //    ts.title_item=0;
        //    ts.holdItem = random(4);
        //}
        ts.dir = (random(100)<50 ? 0 : 1);
        if(ts.px < 0 || ts.px > 200){
            ts.boxNum = random(5);
        }
	}
/*
    drawSprite(78, 56, crate[0], crate_pal, 0, 8);
    drawSprite(110, 56, crate[1], crate_pal, 0, 8);
    drawSprite(78, 88, crate[2], crate_pal, 0, 8);
    drawSprite(110, 88, crate[3], crate_pal, 0, 8);
*/
    if(_A_But[RELEASED]){
        gameMode=2;
        Pokitto::Display::lineFillers[0] = myBGFiller;
    }
    intro_timer+=.33;//(.33 * (50/fpsCount)); // animation is based on 50fps.
}

void initTitleScreen(){
    ts.px=0;
    ts.py=80;
    ts.frm=0;
    gameMode=3;
    intro_timer = 0; // for timing animations etc.
}

void levelSelect(){
    
    guiPrint(0,4, "Select Level");
    int x=0,y=0;
    for(int t=0; t<12; t++){
        drawSprite((x*48)+22, (y*48)+16, smlcrate[0], smlcrate_pal, 0, 8);
        if(++x==4){y++; x=0;}
    }

    if(_A_But[RELEASED]){
        gameMode=1;
        levNum=1;
        cookie.loadCookie();
        levNum = cookie.level;
        if(levNum <1)levNum=1;
        loadSokLev(levNum);
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

    cookie.begin("LEVEL", sizeof(cookie), (char*)&cookie);

    int myVolume = 5;
    cookie.loadCookie();
    myVolume = cookie.volume;
    //if(myVolume < 5)
    myVolume=3;
    cookie.saveCookie();
    #ifndef POK_SIM
        swvolpot.write(0x5e, myVolume);
    #endif


    uint32_t frameCount=0;
    uint8_t line[256];

    #ifndef EMULATION
        initTimer(32000); // for screen fades
    #endif
    
    PS::playMusicStream("/boxpusher/splat.pcm");

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
                playLevel();
                sprintf(tempText,"Moves [%03d] ",moveNumber);
                guiPrint(0,1, tempText);
                sprintf(tempText,"Level [%03d] ",levNum);
                guiPrint(0,2, tempText);
                if(_C_But[RELEASED]){
                    gameMode=0;
                }
            break;
            case 2:
                levelSelect();
            break;
            case 3:
                titleScreen();
            break;
        }
        
        sprintf(tempText,"  FPS [%03d] ",fpsCount);
        guiPrint(0,0, tempText);

        if(PC::getTime() >= lastMillis+1000){
            lastMillis = PC::getTime();
            fpsCount = fpsCounter;
            fpsCounter = 0;
        }

 //       PC::update();
 
    }

    return 0;
}
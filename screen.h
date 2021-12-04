
void clearText(){
    for(int t=0; t<594; t++){
        guiBG[t]=0;
    }
}

void drawSprite(int x, int y, const uint8_t *imageData,const uint16_t *paletteData, bool hFlip, uint8_t bit){

    // if out of screen bounds, don't bother
    if(x<-32 || x>220) return;
    if(y<-32 || y>175) return;

    if(++spriteCount>NUMSPRITES-1)spriteCount=NUMSPRITES-1; // don't overflow the sprite max

    sprites[spriteCount].x = x;
    sprites[spriteCount].y = y;
    sprites[spriteCount].imageData = imageData;
    sprites[spriteCount].paletteData = paletteData;
    sprites[spriteCount].hFlip = hFlip;
    sprites[spriteCount].bit = bit;

    // make sure we know there are sprites on these lines
    int y2 = y+sprites[spriteCount].imageData[1];
    for(int t=y; t<y2; t++){
        if(t>=0 && t<176)
            spriteLine[t]=1;
    }
}

void fadeOut(){
    for(int t=255; t>0; t--) {
        bright = t;
        wait_ms(3);
        Pokitto::Core::update(1);
    }
    bright=0;
}

void fadeIn(){
    for(int t=0; t<256; t++) {
        bright = t;
        wait_ms(3);
        Pokitto::Core::update(1);
    }
    bright=255;
}

void spritesToLine(std::uint8_t* line, std::uint32_t y, bool skip){

    if(spriteLine[y]==0) return;

    auto scanLine = &Pokitto::Display::palette[32]; // start 32 pixels in because of underflow
    #define width 32
    #define height 32
    
    int y2 = y;

    spriteLine[y]=0;
    if(spriteCount>=0){
        for(int32_t spriteIndex = 1; spriteIndex<=spriteCount; spriteIndex++){
            auto &sprite = sprites[spriteIndex];
            if((int)y >= sprite.y && (int)y < sprite.y + 32){
                if(sprite.x>-width && sprite.x<PROJ_LCDWIDTH){
                    uint32_t so = 2+(width * (y2-sprite.y));
                    auto sl = &scanLine[sprite.x];
                    auto palette = sprite.paletteData;
                    if(sprite.hFlip){
                        auto si = &sprite.imageData[so+31];
                        #define midLoop()\
                            if(auto pixel = *si) *sl = palette[pixel];\
                            si--; sl++;
                        midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); 
                        midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); 
                        midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); 
                        midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); midLoop(); 
                    }else{
                        auto si = &sprite.imageData[so];
                        #define midLoop1()\
                            if(auto pixel = *si) *sl = palette[pixel];\
                            si++; sl++;
                        midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); 
                        midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); 
                        midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); 
                        midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); midLoop1(); 
                    }
                    
                } // if X
            } // if Y
        } // for spriteCount
    } // sprite count >1
}



inline void GUILine(std::uint8_t* line, std::uint32_t y, bool skip){

    uint8_t tl = y/8;
    if(guiLineHasText[tl]==0) return;

    uint32_t x = 0;
    uint32_t tileIndex = tl * 27;
    uint32_t lineOffset;
    uint32_t alpha;
    uint32_t temp;
    auto &tileRef = guiFont[0];
    auto scanLine = &Pokitto::Display::palette[32]; // start 32 pixels in because of underflow

    for(int d=0; d<27; d++){
        lineOffset = 2 + ((y&7)*4) + guiBG[tileIndex++]*34;
        for(int c=0; c<4; c++){
            temp = tileRef[lineOffset]>>4;
            if(temp) scanLine[x] = guiFont_pal[temp];
            x++;
            temp = tileRef[lineOffset++]&15;
            if(temp) scanLine[x] = guiFont_pal[temp];
            x++;
        }
    }
}


inline void myBGFiller(std::uint8_t* line, std::uint32_t y, bool skip){

    if(y==0){
        for(uint32_t x=0; x<220; ++x){
            line[x]=x+32;
        }        
        fpsCounter++;
    }

    #define TILEWIDTH 32
    #define TILEHEIGHT 32

    // how far off screen should we render the map
    int x = -(xScroll%TILEWIDTH)+TILEWIDTH;
    // find current tile in the map
    int tileIndex = (xScroll/TILEWIDTH) + ((y+yScroll)/TILEHEIGHT) * levWidth;
    // Find first pixel in current line of the tile
    int jStart = 2+((y+yScroll)%TILEHEIGHT)*TILEWIDTH;

    auto lineP = &Pokitto::Display::palette[x];
    auto tilesP = &tiles[0];

    #define bgTileLine()\
        lineP = &Pokitto::Display::palette[x];\
        tilesP = &tiles[(curLevel[tileIndex++]*1024) + jStart];\
        x+=32;\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];\
        *lineP++ = tiles_pal[*tilesP++]; *lineP++ = tiles_pal[*tilesP++];

    #define bgHalfTileLine()\
        lineP = &Pokitto::Display::palette[x];\
        tilesP = &tiles[(curLevel[tileIndex++]*1024) + jStart];\
        while(x++ < 252){\
            *lineP++ = tiles_pal[*tilesP++];\
        }

    bgTileLine(); bgTileLine(); bgTileLine(); bgTileLine();
    bgTileLine(); bgTileLine(); bgTileLine();
    bgHalfTileLine();

    //GUILine(line, y, 0);
    //spritesToLine(y);
    //flushLine(Pokitto::Display::palette, line);
}

int z_line = 0;
inline void myBGFiller2(std::uint8_t* line, std::uint32_t y, bool skip){

    if(y==0){
        for(uint32_t x=0; x<220; ++x){
            line[x]=x+32;
        }        
        fpsCounter++;
        z_line = -88;
    }

    #define FIXPOINT 10 // change this for better accuracy
    #define yScale 16 // same as texture size?
    #define xScale 16
    #define tileWidth 31
    #define tileHeight 31
    
    int z_=0;
    int texOff=0;

    if(y<88){
        z_ = -z_line;
        texOff = 1024;
    }else{
        z_ = z_line;
    }

    int y_ = (y*yScale << FIXPOINT) / z_;
    y_ = (y_ >> FIXPOINT) &tileHeight;
    int y2 = 2+32*y_;

    int xAdder = (xScale << FIXPOINT) / z_;
    int x2 = -110 * xAdder;
    
    auto lineP = &Pokitto::Display::palette[32];
    //auto tilesP = &tiles[0];

    #define pix()\
        *lineP++ = floor_pal[floor_tile[texOff + y2 + (((x2+=xAdder) >> FIXPOINT)&tileWidth)]];

    for(int32_t x=0; x<220; x+=44){
        pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix();
        pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix();
        pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix();
        pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix(); pix();

    }
    z_line++;
}


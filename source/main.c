#include <string.h>
#include "Pibe.h"
#include "grass.h"
#include "mapita.h"

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;

typedef volatile unsigned char      vu8;
typedef volatile unsigned short     vu16;
typedef volatile unsigned int       vu32;

#define MEM_IO		0x04000000	//!< I/O registers

#define REG_TM0D			*(vu16*)(MEM_IO+0x0100)	//!< Timer 0 data
#define REG_TM0CNT			*(vu16*)(MEM_IO+0x0102)	//!< Timer 0 control
#define REG_TM1D			*(vu16*)(MEM_IO+0x0104)	//!< Timer 1 data
#define REG_TM1CNT			*(vu16*)(MEM_IO+0x0106)	//!< Timer 1 control
#define REG_TM2D			*(vu16*)(MEM_IO+0x0108)	//!< Timer 2 data
#define REG_TM2CNT			*(vu16*)(MEM_IO+0x010A)	//!< Timer 2 control
#define REG_TM3D			*(vu16*)(MEM_IO+0x010C)	//!< Timer 3 data
#define REG_TM3CNT			*(vu16*)(MEM_IO+0x010E)	//!< Timer 3 control

typedef uint32 Tile[16];
typedef Tile   TileBlock[256];

#define VIDEOMODE_0    0x0000
#define ENABLE_OBJECTS 0x1000
#define MAPPINGMODE_1D 0x0040
#define BACKGROUND_0   0x0100
#define BACKGROUND_1   0x0200
#define BACKGROUND_3   0x0400
#define BACKGROUND_4   0x0800

#define REG_KEY_INPUT      (*((volatile uint32 *)(MEM_IO + 0x0130)))

#define KEY_A   0x0001
#define KEY_B   0x0002
#define KEY_SELECT   0x0004
#define KEY_START   0x0008
#define KEY_LTRG   0x0100
#define KEY_RTRG   0x0200

#define KEY_DOWN 0x0080
#define KEY_UP   0x0040
#define KEY_LEFT 0x0020
#define KEY_RGHT 0x0010
#define KEY_ANY  0x03FF


#define REG_VCOUNT              (*(volatile uint16*) 0x04000006)
#define REG_DISPLAYCONTROL      (*(volatile uint16*) 0x04000000)

#define MEM_VRAM      ((volatile uint16*)0x6000000)
#define MEM_TILE      ((TileBlock*)0x6000000 )
#define MEM_PALETTE   ((uint16*)(0x05000200))
#define SCREEN_W      240
#define SCREEN_H      160

typedef struct ObjectAttributes {
    uint16 attr0;
    uint16 attr1;
    uint16 attr2;
    uint16 pad;
} __attribute__((packed, aligned(4))) ObjectAttributes;

#define MEM_OAM       ((volatile ObjectAttributes *)0x07000000)


inline void vsync()
{
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

uint32  key_states = 0;

#define BACKGROUND_W 255
#define BACKGROUND_H 255

struct Background{
    uint16 posX;
    uint16 posY;
    uint8 haveToScroll;
    volatile short* scrollX;
    volatile short* scrollY;
};

struct Background background0;

volatile uint16* char_block(uint32 block){ //Tiles
    return (volatile uint16*) (0x6000000 + (block * 0x4000));
}

volatile uint16* screen_block(uint32 block){ //Background
    return (volatile uint16*) (0x6000000 + (block * 0x800));
}

volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;


void createBackground(){
    for (int i = 0; i < grassPalLen; i++) {
        bg_palette[i] = grassPal[i];
    }

    volatile unsigned short* dest = char_block(0);
    for (int i = 0; i < grassTilesLen; i++) {
        dest[i] = grassTiles[i];
    }

    dest = screen_block(16);
    for (int i = 0; i < (mapita_width * mapita_height); i++) {
        dest[i] = mapita[i];
    }

    REG_DISPLAYCONTROL =  VIDEOMODE_0 | BACKGROUND_0 | ENABLE_OBJECTS | MAPPINGMODE_1D;

    *bg0_control = 0    |   /* priority, 0 is highest, 3 is lowest */
                   (0 << 2)  |   /* the char block the image data is stored in */
                   (0 << 6)  |   /* the mosaic flag */
                   (0 << 7)  |   /* color mode, 0 is 16 colors, 1 is 256 colors */
                   (16 << 8) |   /* the screen block the tile data is stored in */
                   (1 << 13) |   /* wrapping flag */
                   (0 << 14);    /* bg size, 0 is 256x256 */

    background0.posX = 0;
    background0.posY = 0;
    background0.scrollX = (volatile short*) 0x4000010;
    background0.scrollY = (volatile short*) 0x4000012;
}

enum Keys {
    A,
    B,
    SL,
    ST,
    RT,
    LFT,
    UP,
    DWN,
    R,
    L,
    ANY
};

uint16 getKeyDown(enum Keys key){

    if (key == ANY){
        return key_states;
    }

    key_states = ~REG_KEY_INPUT & KEY_ANY;

    return key_states & (1 << key);
};

struct PlayerSprite{
    uint8 animationFrame;
    uint16 firstUpAnim;
    uint16 firstBotAnim;
    uint16 firstRightAnim;
    uint16 firstLeftAnim;
    uint16 numAnims;
};

struct PlayerInfo{
    struct PlayerSprite* sprites;
    volatile struct ObjectAttributes* attributes;
    short isMoving;
    short canMove;
    short direction;
    short moveSpeed;
};

struct PlayerInfo player;
struct PlayerSprite pibito;

void createPlayer(){
    memcpy(MEM_PALETTE, PibePal, PibePalLen);
    memcpy(&MEM_TILE[4][1], PibeTiles, PibeTilesLen);

    volatile ObjectAttributes *attrs = &MEM_OAM[0];
    attrs->attr0 = 0x0000;
    attrs->attr1 = 0x8000;
    attrs->attr2 = 2;

    pibito.animationFrame = 0;
    pibito.firstBotAnim = 2;
    pibito.firstUpAnim = 64+2;
    pibito.firstLeftAnim = 128+2;
    pibito.firstRightAnim = 192+2;
    pibito.numAnims = 4;

    player.attributes = attrs;
    player.sprites = &pibito;
    player.direction = 1;
    player.canMove = 1;
    player.isMoving = 0;
    player.moveSpeed = 1;

}

uint16 getPlayerX(){
    return player.attributes->attr1 & (uint16)0x1FF;
}

uint16 getPlayerY(){
    return player.attributes->attr0 & 0xFF;
}

void setPlayerY(uint8 newY){
    if (newY > 0 && newY < SCREEN_H)
        player.attributes->attr0 = (player.attributes->attr0 & ~0xFF) | newY;
}

void setPlayerX(uint16 newX){
    if (newX > 0 && newX < SCREEN_W)
        player.attributes->attr1 = (player.attributes->attr1 & ~0x1FF) | newX;
}

void movePlayerX(int x){
    int actX = (player.attributes->attr1 & 0x1FF);
    if (actX + x > 0 && actX + x <= (BACKGROUND_W-(40)))
        player.attributes->attr1 += x;
}

void movePlayerY(int y){
    int actY = (player.attributes->attr0 & 0xFF);
    if (actY + y > 0 && actY + y <= SCREEN_H)
        player.attributes->attr0 += y;
}

uint8 haveToXScroll = 0;
uint8 haveToYScroll = 0;

void tickAnimationFrame(){
    uint16 fAnim = 0;

    if (player.direction == 1){
        fAnim = player.sprites->firstBotAnim;
    }
    else if (player.direction == 0){
        fAnim = player.sprites->firstUpAnim;
    }
    else if (player.direction == 3){
        fAnim = player.sprites->firstLeftAnim;
    }
    else if (player.direction == 2){
        fAnim = player.sprites->firstRightAnim;
    }

    if (player.isMoving == 1)
        player.attributes->attr2 = (fAnim + player.sprites->animationFrame * 16);
    else
        player.attributes->attr2 = fAnim;

    if (player.sprites->animationFrame == 3){
        player.sprites->animationFrame = 0;
    }else {
        player.sprites->animationFrame++;
    }
}

int xScroll = 0;
int yScroll = 0;

int recolocateScrollPlayer(volatile ObjectAttributes *attrs, uint8 dir){
    if (dir == 0){
        if ((attrs->attr0 & 0xFF)-10 > 0)
            attrs->attr0 -=10;
        else
            attrs->attr0 = 0;
    }
    else if (dir == 1){
        attrs->attr0 +=10;
    }
    else if (dir == 2){
        if ((attrs->attr1 & 0xFF)-10 > 0)
            attrs->attr1 -=10;
    }
    else if (dir == 3){
        attrs->attr1 +=10;
    }
    return 0;
}

/*int backgroundScrolling(volatile ObjectAttributes *attrs){
    if (haveToXScroll == 1){
        if (xScroll < SCREEN_W - 32){
            recolocateScrollPlayer(attrs, 2);
            xScroll+=10;
            cant_move = 1;
            moving = 0;
        } else{
            cant_move = 0;
        }

    }
    if (haveToYScroll == 1){
        if (yScroll < SCREEN_H - 32){
            recolocateScrollPlayer(attrs, 0);
            yScroll+=10;
            cant_move = 1;
            moving = 0;
        } else{
            cant_move = 0;
        }

    }

    if (haveToYScroll == 2){
        if (yScroll > -(SCREEN_H - 32)){
            recolocateScrollPlayer(attrs, 1);
            yScroll-=10;
            cant_move = 1;
            moving = 0;
        } else{
            cant_move = 0;
        }

    }

    return 0;
}*/

void keyActions(){
    if (getKeyDown(RT)){
        movePlayerX(player.moveSpeed);
        player.isMoving = 1;
        player.direction = 2;
    }

    if (getKeyDown(LFT)){
        movePlayerX(-player.moveSpeed);
        player.isMoving = 1;
        player.direction = 3;
    }

    if (getKeyDown(UP)){
        movePlayerY(-player.moveSpeed);
        player.isMoving = 1;
        player.direction = 0;
    }

    if (getKeyDown(DWN)){
        movePlayerY(player.moveSpeed);
        player.isMoving = 1;
        player.direction = 1;
    }

    if (!getKeyDown(ANY)){
        player.isMoving = 0;
    }
}

void checkBGPosition(){

    if (background0.haveToScroll > 0)
        return;

    uint16 playerAbsX = background0.posX + getPlayerX();
    uint16 BGScrenPosX = SCREEN_W - 32; //ancho del personaje

    uint16 playerAbsY = background0.posY + getPlayerY();
    uint16 BGScrenPosY = SCREEN_H; //ancho del personaje


    if (playerAbsX >= BGScrenPosX){
        background0.haveToScroll = 3;
    }
    else if(getPlayerX() <= 32 && background0.posX > 0){
        background0.haveToScroll = 4;
    }
    else if(playerAbsY >= BGScrenPosY){
        background0.haveToScroll = 2;
    }
    else if(getPlayerY() <= 32 && background0.posY > 0){
        background0.haveToScroll = 1;
    }
}

void smoothScroll(){
    if (background0.haveToScroll == 3){
        if (background0.posX + SCREEN_W < BACKGROUND_W)
            background0.posX += 1;
        else
            background0.haveToScroll = 0;
    }

    if (background0.haveToScroll == 4){
        if (background0.posX > 0)
            background0.posX -= 1;
        else
            background0.haveToScroll = 0;
    }

    if (background0.haveToScroll == 2){
        if (background0.posY + SCREEN_H < BACKGROUND_H)
            background0.posY += 1;
        else
            background0.haveToScroll = 0;
    }

    if (background0.haveToScroll == 1){
        if (background0.posY > 0)
            background0.posY -= 1;
        else
            background0.haveToScroll = 0;
    }
}

void updateBG(){
    *background0.scrollX = background0.posX;
    *background0.scrollY = background0.posY;
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void) {

    uint32 time = 0;
    createBackground();
    createPlayer();

	while (1) {
        if (time >= 6){ 
            time = 0;
            tickAnimationFrame();
            checkBGPosition();
        }else
            time++;

        keyActions();
        smoothScroll();

        vsync();

        updateBG();
	}

    return 0;
}



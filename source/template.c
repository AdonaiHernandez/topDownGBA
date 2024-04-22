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

#define KEY_UP   0x0040
#define KEY_DOWN 0x0080
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

volatile short* bg0_x_scroll = (volatile short*) 0x4000010;
volatile short* bg0_y_scroll = (volatile short*) 0x4000012;

struct PibeSprite{
    uint8 animationFrame;
    uint16 firstUpAnim;
    uint16 firstBotAnim;
    uint16 firstRightAnim;
    uint16 firstLeftAnim;
    uint16 numAnims;
};

struct PibeSprite pibito;

uint8 haveToXScroll = 0;
uint8 haveToYScroll = 0;

int direction = 0;
uint8 moving = 0;
uint8 moveSpeed = 2;
uint8 cant_move = 0;

void tickAnimationFrame(volatile ObjectAttributes *attrs){
    uint16 fAnim = 0;

    if (direction == 0){
        fAnim = pibito.firstBotAnim;
    }
    else if (direction == 1){
        fAnim = pibito.firstUpAnim;
    }
    else if (direction == 2){
        fAnim = pibito.firstLeftAnim;
    }
    else if (direction == 3){
        fAnim = pibito.firstRightAnim;
    }

    if (moving == 1)
        attrs->attr2 = (fAnim + pibito.animationFrame * 16);
    else
        attrs->attr2 = fAnim;

    if (pibito.animationFrame == 3){
        pibito.animationFrame = 0;
    }else {
        pibito.animationFrame++;
    }

}

void getKeys(){
    if (cant_move == 1){
        return;
    }
    key_states = ~REG_KEY_INPUT & KEY_ANY;

    if ( key_states & KEY_DOWN ){
        direction = 0;
        moving = 1;
    }
    else if ( key_states & KEY_UP ){
        direction = 1;
        moving = 1;
    }
    else if ( key_states & KEY_LEFT ){
        direction = 2;
        moving = 1;
    }
    else if ( key_states & KEY_RGHT ){
        direction = 3;
        moving = 1;
    }
    else {
        moving = 0;
    }
}
uint16 xScroll = 0;
uint16 yScroll = 0;
void changePosition(volatile ObjectAttributes *attrs){

    if (moving == 0)
        return;

    if (direction == 0){
        attrs->attr0 += moveSpeed;
        if ((attrs->attr0 & 0xFF) >= (SCREEN_H - 32)){
            haveToYScroll = 1;
        }
    }
    else if (direction == 1){
        if ((attrs->attr0 & 0xFF) > 0)
            attrs->attr0 -= moveSpeed;
    }
    else if (direction == 2){
        if ((attrs->attr1 & 0xFF) > 0)
            attrs->attr1 -= moveSpeed;
    }
    else if (direction == 3){
        attrs->attr1 += moveSpeed;
        if ((attrs->attr1 & 0xFF) >= (SCREEN_W - 32)){
            haveToXScroll = 1;
        }
    }

}

volatile uint16* char_block(uint32 block){ //Tiles
    return (volatile uint16*) (0x6000000 + (block * 0x4000));
}

volatile uint16* screen_block(uint32 block){ //Background
    return (volatile uint16*) (0x6000000 + (block * 0x800));
}

volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;

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

int backgroundScrolling(volatile ObjectAttributes *attrs){
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

    return 0;
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void) {
    //REG_TM2D= 0;
    //REG_TM2CNT= 0b0000000010000011;
//---------------------------------------------------------------------------------
    memcpy(MEM_PALETTE, PibePal, PibePalLen);
    memcpy(&MEM_TILE[4][1], PibeTiles, PibeTilesLen);


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

    volatile ObjectAttributes *spriteAttribsb = &MEM_OAM[0];
    spriteAttribsb->attr0 = 0x0000;
    spriteAttribsb->attr1 = 0x8000;
    spriteAttribsb->attr2 = 2;

    uint32 time = 0;

    pibito.animationFrame = 0;
    pibito.firstBotAnim = 2;
    pibito.firstUpAnim = 64+2;
    pibito.firstLeftAnim = 128+2;
    pibito.firstRightAnim = 192+2;
    pibito.numAnims = 4;

	while (1) {
        if (time >= 6){ 
            time = 0;
            tickAnimationFrame(spriteAttribsb);
            changePosition(spriteAttribsb);
            backgroundScrolling(spriteAttribsb);

        }else
            time++;

        getKeys();

        vsync();

        *bg0_x_scroll = xScroll;
        *bg0_y_scroll = yScroll;
	}

    return 0;
}



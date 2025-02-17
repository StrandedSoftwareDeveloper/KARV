/*------------------------------------------------------------------------------*\
 | libkarv.c Copyright (c) 2025 StrandedSoftwareDeveloper under the MIT License |
 | The main file of libkarv, responsibilities include:                          |
 |  -Running the emulator                                                       |
 |  -Loading and running Linux                                                  |
 |  -Terminal emulating                                                         |
 |  -Bitmap font rendering                                                      |
 |  -Production of the final framebuffer                                        |
 |                                                                              |
 | To test: Run `$ tcc -g -lX11 -DKARV_TEST -run path/to/libkarv.c`             |
 | from a folder with `linux.bin` and `Codepage-437.png`                        |
\*------------------------------------------------------------------------------*/
//#define KARV_TEST //Uncomment this line for LSP in the test harness code

#ifdef KARV_TEST
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"
#include "os_generic.h"
#define STBI_NO_SIMD
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
    int statusCode;
    int kbBufferLen;
} stepRetVal;

static uint32_t HandleControlStore( uint32_t addy, uint32_t val );
static uint32_t HandleControlLoad( uint32_t addy );
static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value );
static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno );

uint32_t ram_amt = 64*1024*1024;

#define MINI_RV32_RAM_SIZE ram_amt
#define MINIRV32_IMPLEMENTATION
//#define MINIRV32_RAM_IMAGE_OFFSET 0x0000000
#define MINIRV32_HANDLE_MEM_STORE_CONTROL( addy, val ) if( HandleControlStore( addy, val ) ) return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL( addy, rval ) rval = HandleControlLoad( addy );
#define MINIRV32_OTHERCSR_WRITE( csrno, value ) HandleOtherCSRWrite( image, csrno, value );
#define MINIRV32_OTHERCSR_READ( csrno, value ) value = HandleOtherCSRRead( image, csrno );
#include "mini-rv32ima.h"

#include "default64mbdtc.h"

static struct MiniRV32IMAState *core;
static uint8_t *ram_image;//[MINI_RV32_RAM_SIZE];
const char * kernel_command_line = 0;
static FILE *logFile;

static char *keyboardBuffer = NULL;
static int32_t kbBufferLen = 0;

static uint8_t *localVram;
static uint16_t width = 200;
static uint16_t height = 200;

const uint16_t charWidth = 9;
const uint16_t charHeight = 16;
static int fontWidth = 0;
static int fontHeight = 0;
static uint8_t *font = NULL;
static bool first = true;

static uint16_t cursorX = 0;
static uint16_t cursorY = 0;
static uint16_t backupCursorX = 0;
static uint16_t backupCursorY = 0;


static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image );

void clearScreen();
void drawChar(uint16_t x, uint16_t y, uint8_t c);
void writeChar(char c);
void writeArray(const char *str, int len);
void writeString(const char *str);

void setup(uint16_t screenWidth, uint16_t screenHeight) {
    logFile = fopen("rvlog.txt", "w");

    width = screenWidth;
    height = screenHeight;

    int n = 0;
    font = stbi_load("Codepage-437.png", &fontWidth, &fontHeight, &n, 1);
    if (font == NULL) {
        fprintf(logFile, "Error: failed to load font\n");
    }

    ram_image = malloc(MINI_RV32_RAM_SIZE);
    memset(ram_image, 0, MINI_RV32_RAM_SIZE);

    FILE *rom = fopen("linux.bin", "r");
    if (rom == NULL) {
        fprintf(logFile, "Error: rom not found\n");
    } else {
        fseek(rom, 0, SEEK_END);
        size_t len = ftell(rom);
        printf("len:%lu\n", len);
        fseek(rom, 0, SEEK_SET);

        if (len <= MINI_RV32_RAM_SIZE) {
            fread(ram_image, sizeof(uint8_t), len, rom);
        } else {
            fprintf(logFile, "Error: rom too big\n");
        }

        fclose(rom);
    }

    int dtb_ptr = 0;

    // Load a default dtb.
    dtb_ptr = ram_amt - sizeof(default64mbdtb) - sizeof( struct MiniRV32IMAState );
    memcpy( ram_image + dtb_ptr, default64mbdtb, sizeof( default64mbdtb ) );
    if (kernel_command_line) {
        strncpy( (char*)( ram_image + dtb_ptr + 0xc0 ), kernel_command_line, 54 );
    }

    // The core lives at the end of RAM.
	core = (struct MiniRV32IMAState *)(ram_image + ram_amt - sizeof( struct MiniRV32IMAState ));
	core->pc = MINIRV32_RAM_IMAGE_OFFSET;
	core->regs[10] = 0x00; //hart ID
	core->regs[11] = dtb_ptr?(dtb_ptr+MINIRV32_RAM_IMAGE_OFFSET):0; //dtb_pa (Must be valid pointer) (Should be pointer to dtb)
	core->extraflags |= 3; // Machine-mode.

	if (1) {
		// Update system ram size in DTB (but if and only if we're using the default DTB)
		// Warning - this will need to be updated if the skeleton DTB is ever modified.
		uint32_t * dtb = (uint32_t*)(ram_image + dtb_ptr);
		if( dtb[0x13c/4] == 0x00c0ff03 )
		{
			uint32_t validram = dtb_ptr;
			dtb[0x13c/4] = (validram>>24) | ((( validram >> 16 ) & 0xff) << 8 ) | (((validram>>8) & 0xff ) << 16 ) | ( ( validram & 0xff) << 24 );
		}
	}

	uint8_t checksum = 0;
	for (int i=0; i<ram_amt; i++) {
		checksum += ram_image[i];
	}
	fprintf(logFile, "Checksum: 0x%x\n", checksum);

    fprintf(logFile, "Finished setup\n");
    fflush(logFile);
}

stepRetVal step(uint8_t *vram, char *kbBuffer, int32_t len) {
    static int numLoops = 0;
    //fprintf(logFile, "\nLoops:%d\n", numLoops);
    //fflush(logFile);

    keyboardBuffer = kbBuffer;
    kbBufferLen = len;

    localVram = vram;
    if (first) {
        clearScreen();
        first = false;
    }

    numLoops += 1;
    stepRetVal ret;
    int numRunTotal = 0;
    while (numRunTotal < 65536*5) {
        //printf("%d\n", numRunTotal);
        int numRun = 0;
        ret.statusCode = MiniRV32IMAStep(core, ram_image, 0, 1024, (65536*5)-numRunTotal, &numRun);
        ret.kbBufferLen = kbBufferLen;
        numRunTotal += numRun;
    }
    
    if (numLoops % 30 >= 15) {
        drawChar(cursorX, cursorY, 219);
    } else {
        drawChar(cursorX, cursorY, ' ');
    }
    return ret;
}

void cleanup() {
    fclose(logFile);
    free(ram_image);
    stbi_image_free(font);
}

static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image )
{
	uint32_t pc = core->pc;
	uint32_t pc_offset = pc - MINIRV32_RAM_IMAGE_OFFSET;
	uint32_t ir = 0;

	fprintf( logFile, "PC: %08x ", pc );
	if( pc_offset >= 0 && pc_offset < MINI_RV32_RAM_SIZE - 3 )
	{
		ir = *((uint32_t*)(&((uint8_t*)ram_image)[pc_offset]));
		fprintf( logFile, "[0x%08x] ", ir );
	}
	else
		fprintf( logFile, "[xxxxxxxxxx] " );
	uint32_t * regs = core->regs;
	fprintf( logFile, "Z:%08x ra:%08x sp:%08x gp:%08x tp:%08x t0:%08x t1:%08x t2:%08x s0:%08x s1:%08x a0:%08x a1:%08x a2:%08x a3:%08x a4:%08x a5:%08x ",
		regs[0], regs[1], regs[2], regs[3], regs[4], regs[5], regs[6], regs[7],
		regs[8], regs[9], regs[10], regs[11], regs[12], regs[13], regs[14], regs[15] );
	fprintf( logFile, "a6:%08x a7:%08x s2:%08x s3:%08x s4:%08x s5:%08x s6:%08x s7:%08x s8:%08x s9:%08x s10:%08x s11:%08x t3:%08x t4:%08x t5:%08x t6:%08x\n",
		regs[16], regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23],
		regs[24], regs[25], regs[26], regs[27], regs[28], regs[29], regs[30], regs[31] );

    fflush(logFile);
}

#ifdef KARV_TEST
char globalKBBuffer[1024];
uint32_t globalKBBufferLen = 0;
bool leftShift = false;
bool rightShift = false;
char shiftRemap[] = {
    '\"', 1, 1, 1, 1, '<', '_', '>', '?', ')', '!', '@', '#', '$', '%', '^', '&', '*', '(', 1, ':', 1, '+', 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    '{', '|', '}', 1, 1, '~'
};

void HandleKey(int keycode, int bDown) {
    if (bDown) {
        char c = keycode;
        
        if (keycode == CNFG_KEY_SHIFT) {
            leftShift = true;
            return;
        } else if (keycode == CNFG_KEY_SHIFT+1) {
            rightShift = true;
            return;
        } else if (leftShift || rightShift) {
            if (keycode >= '\'' && keycode <= '`') {
                c = shiftRemap[keycode-'\''];
            } else if (keycode >= 'a' && keycode <= 'z') {
                c = keycode - ('a' - 'A');
            }
        }
        
        if (globalKBBufferLen < 1023) {
            globalKBBuffer[globalKBBufferLen++] = c;
        }
    } else {
        if (keycode == CNFG_KEY_SHIFT) {
            leftShift = false;
        } else if (keycode == CNFG_KEY_SHIFT+1) {
            rightShift = false;
        }
    }
}

void HandleButton(int x, int y, int button, int bDown) {
    
}

void HandleMotion(int x, int y, int mask) {
    
}
void HandleDestroy() {}

int main() {
    setup(400, 400);
    CNFGSetup("KARV external test program", 400, 400);
    printf("start\n");
    uint32_t vram[400*400];
    
    int running = 1;
    for (int i=0; CNFGHandleInput() != 0 && running; i++) {
        double lastTime = OGGetAbsoluteTime();
        //DumpState(core, ram_image);
        //printf("\n");
        stepRetVal ret = step((uint8_t*)vram, globalKBBuffer, globalKBBufferLen);
        globalKBBufferLen = ret.kbBufferLen;
        switch( ret.statusCode )
		{
			case 0: break;
			case 1: break;//if( do_sleep ) MiniSleep(); *this_ccount += instrs_per_flip; break;
			case 3: break;//instct = 0; break;
			case 0x7777: printf("Tried to restart\n");	//syscon code for restart
			case 0x5555: printf( "POWEROFF@0x%08x%08x\n", core->cycleh, core->cyclel ); running = 0; break; //syscon code for power-off
			default: printf( "Unknown failure\n" ); break;
		}
		
		CNFGClearFrame();
		CNFGBlitImage(vram, 0, 0, 400, 400);
        CNFGSwapBuffers();
        
        double newTime = OGGetAbsoluteTime();
        //printf("%lf\n", newTime-lastTime);
        if (newTime-lastTime < 0.0166666) {
            OGUSleep((int)((0.0166666-(newTime-lastTime)) * 1000000.0));
        }
    }
    printf("stop\n");

    cleanup();
}
#endif

void clearScreen() {
    for (int i=0; i<width*height; i++) {
        localVram[i*4+0] = 0;
        localVram[i*4+1] = 0;
        localVram[i*4+2] = 0;
        localVram[i*4+3] = 255;
    }
}

void drawChar(uint16_t x, uint16_t y, uint8_t c) {
    const int charOffsetX = (c % (fontWidth / charWidth)) * charWidth;
    const int charOffsetY = (c / (fontWidth / charWidth)) * charHeight;
    for (int yOffset=0; yOffset<charHeight; yOffset++) {
        for (int xOffset=0; xOffset<charWidth; xOffset++) {
            uint8_t val = font[(yOffset+charOffsetY)*fontWidth+(xOffset+charOffsetX)];
            localVram[((y+yOffset)*width+(x+xOffset))*4+0] = val;
            localVram[((y+yOffset)*width+(x+xOffset))*4+1] = val;
            localVram[((y+yOffset)*width+(x+xOffset))*4+2] = val;
            localVram[((y+yOffset)*width+(x+xOffset))*4+3] = 255;
        }
    }
}

void scrollUp(int numLines) {
    const int heightLines = height / charHeight;
    for (int y=numLines; y<heightLines; y++) {
        memcpy(&localVram[(y-numLines)*width*charHeight*4], &localVram[y*width*charHeight*4], width*charHeight*4);
    }
    
    memset(&localVram[(heightLines-numLines)*width*charHeight*4], 0, numLines*width*charHeight*4);
    
    cursorY -= charHeight * numLines;
}

//Not really sure how scrolling down is supposed to work...
void scrollDown(int numLines) {
    const int heightLines = height / charHeight;
    for (int y=heightLines-1; y>=numLines; y--) {
        memcpy(&localVram[y*width*charHeight*4], &localVram[(y-numLines)*width*charHeight*4], width*charHeight*4);
    }
    
    memset(localVram, 0, numLines*width*charHeight*4);
    
    //cursorY -= charHeight * numLines;
}

void clearFromCursorRight() {
    for (int y=0; y<charHeight; y++) {
        memset(&localVram[((cursorY+y)*width+cursorX)*4], 0, (width-cursorX)*4);
    }
}

void clearFromCursorDown() {
    clearFromCursorRight();
    memset(&localVram[((cursorY+charHeight)*width*4)], 0, (width*height*4)-((cursorY+charHeight)*width*4));
}

void clearFromCursorLeft() {
    for (int y=0; y<charHeight; y++) {
        memset(&localVram[(cursorY+y)*width*4], 0, cursorX*4);
    }
}

void clearFromCursorUp() {
    clearFromCursorLeft();
    memset(localVram, 0, cursorY*width*4);
}

void clearLine() {
    memset(&localVram[cursorY*width*4], 0, width*charHeight*4);
}

typedef enum {
    NORMAL,
    ESC,
    ESC_BRACKET,
    ESC_OPEN_PAREN,
    ESC_CLOSE_PAREN,
    ESC_POUND,
    ESC_FIVE,
    ESC_SIX,
    ESC_BRACKET_NUM,
    ESC_BRACKET_NUMVALUE,
    ESC_BRACKET_QUESTION,
    ESC_BRACKET_SEMI,
    ESC_BRACKET_NUM_SEMI
} TerminalState;

void writeChar(char c) {
    static TerminalState state = NORMAL;
    static int numA = 0;
    
    switch (state) {
        case NORMAL: {
            if (c == 27) { //Escape code
                state = ESC;
                break;
            }
            
            if (c != '\n' && c != '\r' && c != 8 /*Backspace*/) {
                drawChar(cursorX, cursorY, c);
            } else {
                drawChar(cursorX, cursorY, ' ');
            }
            
            if (c == 8 /*Backspace*/) {
                cursorX -= charWidth;
                break;
            }

            cursorX += charWidth;
            if (cursorX >= width - charWidth || c == '\n') {
                cursorX = 0;
                cursorY += charHeight;
            }

            if (cursorY > height - charHeight) {
                scrollUp(1);
            }
            break;
        }
        case ESC: {
            switch (c) {
                case '[': {
                    state = ESC_BRACKET;
                    break;
                }
                
                case '=': { //Set alternate keypad mode TODO: Decide what we're gonna do about keypad modes
                    state = NORMAL;
                    break;
                }
                case '>': { //Set numeric keypad mode TODO: Decide what we're gonna do about keypad modes
                    state = NORMAL;
                    break;
                }
                
                case '(': {
                    state = ESC_OPEN_PAREN;
                    break;
                }
                case ')': {
                    state = ESC_CLOSE_PAREN;
                    break;
                }
                
                case 'N': { //Set single shift 2
                    state = NORMAL;
                    break;
                }
                case 'O': { //Set single shift 3
                    state = NORMAL;
                    break;
                }
                
                case 'D': { //Move/scroll window up one line FIXME: I don't think this is quite right...
                    scrollUp(1);
                    state = NORMAL;
                    break;
                }
                case 'M': { //Move/scroll window down one line FIXME: I don't think this is quite right...
                    scrollDown(1);
                    state = NORMAL;
                    break;
                }
                case 'E': { //Move to next line FIXME: I don't think this is quite right...
                    scrollUp(1);
                    state = NORMAL;
                    break;
                }
                case '7': { //Save cursor position and attributes
                    backupCursorX = cursorX;
                    backupCursorY = cursorY;
                    state = NORMAL;
                    break;
                }
                case '8': { //Restore cursor position and attributes
                    cursorX = backupCursorX;
                    cursorY = backupCursorY;
                    state = NORMAL;
                    break;
                }
                
                case 'H': { //Set a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                    state = NORMAL;
                    break;
                }
                
                case '#': {
                    state = ESC_POUND;
                    break;
                }
                
                case '5': {
                    state = ESC_FIVE;
                    break;
                }
                
                case '6': {
                    state = ESC_SIX;
                    break;
                }
                
                case 'c': { //Reset terminal to initial state
                    clearScreen();
                    cursorX = 0;
                    cursorY = 0;
                    backupCursorX = 0;
                    backupCursorY = 0;
                    state = NORMAL;
                    break;
                }
                
                case '<': { //Toggle ANSI mode TODO: Implement ANSI mode
                    state = NORMAL;
                    break;
                }
                
                default: { //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET: {
            switch (c) {
                case '?': {
                    state = ESC_BRACKET_QUESTION;
                    break;
                }
                
                case 'm': { //Turn off character attributes TODO: Implement character attributes
                    state = NORMAL;
                    break;
                }
                
                case 'H': { //Move cursor to upper left corner
                    cursorX = 0;
                    cursorY = 0;
                    state = NORMAL;
                    break;
                }
                case ';': {
                    state = ESC_BRACKET_SEMI;
                    break;
                }
                case 'f': { //Move cursor to upper left corner
                    cursorX = 0;
                    cursorY = 0;
                    state = NORMAL;
                    break;
                }
                
                case 'g': { //Clear a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                    state = NORMAL;
                    break;
                }
                
                case 'K': { //Clear line from cursor right
                    clearFromCursorRight();
                    state = NORMAL;
                    break;
                }
                
                case 'J': { //Clear screen from cursor down
                    clearFromCursorDown();
                    state = NORMAL;
                    break;
                }
                
                case 'c': { //Identify what terminal type TODO: Setup keyboard buffer to respond to this
                    state = NORMAL;
                    break;
                }
                
                default: {
                    if (c >= '0' && c <= '9') {
                        numA = c - '0';
                        state = ESC_BRACKET_NUM;
                        break;
                    } else { //Invalid escape code
                        state = NORMAL;
                        break;
                    }
                }
            }
            break;
        }
        case ESC_OPEN_PAREN: {
            switch (c) {
                case 'A': { //Set United Kingdom G0 character set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case 'B': { //Set United States G0 character set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '0': { //Set G0 special chars. & line set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '1': { //Set G0 alternate character ROM TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '2': { //Set G0 alt char ROM and spec. graphics TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_CLOSE_PAREN: {
            switch (c) {
                case 'A': { //Set United Kingdom G1 character set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case 'B': { //Set United States G1 character set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '0': { //Set G1 special chars. & line set TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '1': { //Set G1 alternate character ROM TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                case '2': { //Set G1 alt char ROM and spec. graphics TODO: Figure out if we even need these
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_POUND: {
            switch (c) {
                case '3': { //Double-height letters, top half TODO: Implement
                    state = NORMAL;
                    break;
                }
                case '4': { //Double-height letters, bottom half TODO: Implement
                    state = NORMAL;
                    break;
                }
                case '5': { //Single width, single height letters TODO: Implement
                    state = NORMAL;
                    break;
                }
                case '6': { //Double width, single height letters TODO: Implement
                    state = NORMAL;
                    break;
                }
                
                case '8': { //Screen alignment display TODO: What is this even supposed to do?
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_FIVE: {
            switch (c) {
                case 'n': { //Device status report TODO: Setup keyboard buffer to respond to this
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_SIX: {
            switch (c) {
                case 'n': { //Get cursor position TODO: Setup keyboard buffer to respond to this
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET_NUM: {
            switch (c) {
                case 'm': {
                    switch (numA) {
                        case 0: { //Turn off character attributes TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Turn bold mode on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Turn low intensity mode on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Turn underline mode on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 5: { //Turn blinking mode on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 7: { //Turn reverse video on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        case 8: { //Turn invisible text mode on TODO: Implement character attributes
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'g': {
                    switch (numA) {
                        case 0: { //Clear a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Clear all tabs TODO: Figure out what all this tab stuff is supposed to do
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'K': {
                    switch (numA) {
                        case 0: { //Clear line from cursor right
                            clearFromCursorRight();
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Clear line from cursor left
                            clearFromCursorLeft();
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Clear entire line
                            clearLine();
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'J': {
                    switch (numA) {
                        case 0: { //Clear screen from cursor down
                            clearFromCursorDown();
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Clear screen from cursor up
                            clearFromCursorUp();
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Clear entire screen
                            clearScreen();
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'c': {
                    if (numA == 0) { //Identify what terminal type (another) TODO: Setup keyboard buffer to respond to this
                        state = NORMAL;
                    } else { //Invalid escape code
                        state = NORMAL;
                    }
                    break;
                }
                //TODO: Maybe implement the terminal self-test stuff?
                case 'q': {
                    switch (numA) {
                        case 0: { //Turn off all four leds TODO: Implement the leds
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Turn on LED #1 TODO: Implement the leds
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Turn on LED #2 TODO: Implement the leds
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Turn on LED #3 TODO: Implement the leds
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Turn on LED #4 TODO: Implement the leds
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                default: {
                    if (c >= '0' && c <= '9') {
                        numA *= 10;
                        numA += c - '0';
                        printf("Found number: %d\n", numA);
                        state = ESC_BRACKET_NUMVALUE;
                    }
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        default: {
            printf("TODO: Implement the rest of the escape code stuff\n");
            state = NORMAL;
            break;
        }
    }
}

void writeArray(const char *str, int len) {
    for (int i=0; i<len; i++) {
        writeChar(str[i]);
    }
}

void writeString(const char *str) {
    writeArray(str, strlen(str));
}

void VRAMnPrintf(unsigned long size, const char *format, ...) {
    va_list va;
    va_start(va, format);

    char *buffer = malloc(size);
    vsnprintf(buffer, size, format, va);
    writeString(buffer);
    free(buffer);

    va_end(va);
}

static int ReadKBByte()
{
	/*if( is_eofd ) return 0xffffffff;
	char rxchar = 0;
	int rread = read(fileno(stdin), (char*)&rxchar, 1);

	if( rread > 0 ) // Tricky: getchar can't be used with arrow keys.
		return rxchar;
	else
		return -1;*/

    if (kbBufferLen < 1) {
        return -1;
    }
    char c = keyboardBuffer[0];
    for (int i=0; i<kbBufferLen-1; i++) {
        keyboardBuffer[i] = keyboardBuffer[i+1];
    }
    kbBufferLen -= 1;
    return c;
}

static int IsKBHit()
{
	/*if( is_eofd ) return -1;
	int byteswaiting;
	ioctl(0, FIONREAD, &byteswaiting);
	if( !byteswaiting && write( fileno(stdin), 0, 0 ) != 0 ) { is_eofd = 1; return -1; } // Is end-of-file for
	return !!byteswaiting;*/
    return kbBufferLen > 0;
}

static uint32_t HandleControlStore( uint32_t addy, uint32_t val )
{
	if( addy == 0x10000000 ) //UART 8250 / 16550 Data Buffer
	{
		fprintf(logFile, "%c", val);
        fflush(logFile);
        writeChar(val);
		//printf("%c", val);
        //fflush(stdout);
	}
	return 0;
}

static uint32_t HandleControlLoad( uint32_t addy )
{
	// Emulating a 8250 / 16550 UART
	if( addy == 0x10000005 )
		return 0x60 | IsKBHit();
	else if( addy == 0x10000000 && IsKBHit() )
		return ReadKBByte();
	return 0;
}

static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value )
{
	if( csrno == 0x136 )
	{
        VRAMnPrintf(16, "%d", value); //32 bit number in decimal can't have more than 10 digits
		fprintf( logFile, "%d", value );
        fflush(logFile);
		//printf( "%d", value ); fflush( stdout );
	}
	if( csrno == 0x137 )
	{
        VRAMnPrintf(16, "%08x", value); //32 bit number in decimal can't have more than 8 digits
        fprintf( logFile, "%08x", value );
        fflush(logFile);
		//printf( "%08x", value ); fflush( stdout );
	}
	else if( csrno == 0x138 )
	{
		//Print "string"
		uint32_t ptrstart = value - MINIRV32_RAM_IMAGE_OFFSET;
		uint32_t ptrend = ptrstart;
		if( ptrstart >= MINI_RV32_RAM_SIZE ) {
			fprintf( logFile, "DEBUG PASSED INVALID PTR (%08x)\n", value );
        fflush(logFile);
			printf( "DEBUG PASSED INVALID PTR (%08x)\n", value );
        }
		while( ptrend < MINI_RV32_RAM_SIZE )
		{
			if( image[ptrend] == 0 ) break;
			ptrend++;
		}
		if( ptrend != ptrstart ) {
            writeArray((char *)image + ptrstart, ptrend - ptrstart);
            fwrite( image + ptrstart, ptrend - ptrstart, 1, logFile );
            fflush(logFile);
			//fwrite( image + ptrstart, ptrend - ptrstart, 1, stdout );
        }
	}
	else if( csrno == 0x139 )
	{
        writeChar(value);
        fputc(value, logFile);
        fflush(logFile);
		//putchar( value ); fflush( stdout );
	}
}

static int32_t HandleOtherCSRRead( uint8_t * image, uint16_t csrno )
{
	if( csrno == 0x140 )
	{
		if( !IsKBHit() ) return -1;
		return ReadKBByte();
	}
	return 0;
}

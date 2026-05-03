/*------------------------------------------------------------------------------*\
 | libkarv.c Copyright (c) 2025 StrandedSoftwareDeveloper under the MIT License |
 | The main file of libkarv, responsibilities include:                          |
 |  - Running the emulator                                                      |
 |  - Loading and running Linux                                                 |
 |  - Production of the final framebuffer                                       |
 |                                                                              |
 | To test, run:                                                                |
 | tcc -g -lX11 -DKARV_TEST KARV/libkarv/terminal.c -run KARV/libkarv/libkarv.c |
 | from a folder with `linux.bin` and `Codepage-437.png`                        |
\*------------------------------------------------------------------------------*/
//#define KARV_TEST //Uncomment this line for LSP in the test harness code

#ifdef KARV_TEST
#define CNFG_IMPLEMENTATION
#include "externalDeps/rawdraw_sf.h"
#include "externalDeps/os_generic.h"
#define STBI_NO_SIMD
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "terminal.h"

#define STB_IMAGE_IMPLEMENTATION
#include "externalDeps/stb_image.h"

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
#include "externalDeps/mini-rv32ima.h"

#include "externalDeps/default64mbdtc.h"

static struct MiniRV32IMAState *core;
static uint8_t *ram_image;//[MINI_RV32_RAM_SIZE];
const char * kernel_command_line = 0;
static FILE *logFile;

static char *keyboardBuffer = NULL;
static int32_t kbBufferLen = 0;

static TermGraphicsState termGraphicsState;
static bool first = true;

static void DumpState( struct MiniRV32IMAState * core, uint8_t * ram_image );

void setup(uint16_t screenWidth, uint16_t screenHeight) {
    logFile = fopen("rvlog.txt", "w");

    termGraphicsState.width = screenWidth;
    termGraphicsState.height = screenHeight;
    
    termGraphicsState.charWidth = 9;
    termGraphicsState.charHeight = 16;
    
    termGraphicsState.cursorX = 0;
    termGraphicsState.cursorY = 0;
    termGraphicsState.backupCursorX = 0;
    termGraphicsState.backupCursorY = 0;

    int n = 0;
    termGraphicsState.font = stbi_load("Codepage-437.png", &termGraphicsState.fontWidth, &termGraphicsState.fontHeight, &n, 1);
    if (termGraphicsState.font == NULL) {
        fprintf(logFile, "Error: failed to load font\n");
    }

    ram_image = malloc(MINI_RV32_RAM_SIZE);
    memset(ram_image, 0, MINI_RV32_RAM_SIZE);

    FILE *rom = fopen("linux.bin", "rb");
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

    termGraphicsState.vram = vram;
    if (first) {
        clearScreen(&termGraphicsState);
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
        drawChar(&termGraphicsState, termGraphicsState.cursorX, termGraphicsState.cursorY, 219);
    } else {
        drawChar(&termGraphicsState, termGraphicsState.cursorX, termGraphicsState.cursorY, ' ');
    }
    return ret;
}

void cleanup() {
    fclose(logFile);
    free(ram_image);
    stbi_image_free(termGraphicsState.font);
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
        keycode = tolower(keycode); //Apparently keycodes come in as upper case on Windows
        char c = keycode;
        
        if (keycode == CNFG_KEY_TOP_ARROW) {
            if (globalKBBufferLen < 1024 - 3) {
                globalKBBuffer[globalKBBufferLen++] = 27; //ESC
                globalKBBuffer[globalKBBufferLen++] = 'O';
                globalKBBuffer[globalKBBufferLen++] = 'A';
            }
            return;
        } else if (keycode == CNFG_KEY_BOTTOM_ARROW) {
            if (globalKBBufferLen < 1024 - 3) {
                globalKBBuffer[globalKBBufferLen++] = 27; //ESC
                globalKBBuffer[globalKBBufferLen++] = 'O';
                globalKBBuffer[globalKBBufferLen++] = 'B';
            }
            return;
        } else if (keycode == CNFG_KEY_LEFT_ARROW) {
            if (globalKBBufferLen < 1024 - 3) {
                globalKBBuffer[globalKBBufferLen++] = 27; //ESC
                globalKBBuffer[globalKBBufferLen++] = 'O';
                globalKBBuffer[globalKBBufferLen++] = 'D';
            }
            return;
        } else if (keycode == CNFG_KEY_RIGHT_ARROW) {
            if (globalKBBufferLen < 1024 - 3) {
                globalKBBuffer[globalKBBufferLen++] = 27; //ESC
                globalKBBuffer[globalKBBufferLen++] = 'O';
                globalKBBuffer[globalKBBufferLen++] = 'C';
            }
            return;
        } else if (keycode == CNFG_KEY_SHIFT) {
            leftShift = true;
            return;
        } else if (keycode == CNFG_KEY_SHIFT+1) { //FIXME: This doesn't seem to work on Windows
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
    setup(600, 600);
    CNFGSetup("KARV external test program", 600, 600);
    printf("start\n");
    uint32_t vram[600*600];
    
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
		CNFGBlitImage(vram, 0, 0, 600, 600);
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
        writeChar(&termGraphicsState, val);
		//printf("%c", val);
        //fflush(stdout);
	} else if (addy == 0x11000000) { //Graphics width
        fprintf(stderr, "Guest tried to set width to %u, but guest-set sizes are not supported yet\n", val);
    } else if (addy == 0x11000004) { //Graphics height
        fprintf(stderr, "Guest tried to set height to %u, but guest-set sizes are not supported yet\n", val);
    } else if (addy >= 0x1100000C && addy < termGraphicsState.width*termGraphicsState.height*4+0x1100000C) { //Graphics frame buffer
        uint32_t index = addy-0x1100000C;
        uint8_t r = (val >>  0) & 0xFF;
        uint8_t g = (val >>  8) & 0xFF;
        uint8_t b = (val >> 16) & 0xFF;
        uint8_t a = (val >> 24) & 0xFF;
        
        termGraphicsState.vram[index+0] = r;
        termGraphicsState.vram[index+1] = g;
        termGraphicsState.vram[index+2] = b;
        termGraphicsState.vram[index+3] = a;
    }
	return 0;
}

static uint32_t HandleControlLoad( uint32_t addy )
{
	// Emulating a 8250 / 16550 UART
	if( addy == 0x10000005 ) {
		return 0x60 | IsKBHit();
    } else if( addy == 0x10000000 && IsKBHit() ) {
		return ReadKBByte();
    } else if (addy == 0x11000000) { //Graphics width
        return termGraphicsState.width;
    } else if (addy == 0x11000004) { //Graphics height
        return termGraphicsState.height;
    } else if (addy == 0x11000008) { //Reserved for other graphics data
        return 0;
    } else if (addy >= 0x1100000C && addy < termGraphicsState.width*termGraphicsState.height*4+0x1100000C) { //Graphics frame buffer
        uint32_t index = addy-0x1100000C;
        uint32_t r = termGraphicsState.vram[index];
        uint32_t g = termGraphicsState.vram[index+1];
        uint32_t b = termGraphicsState.vram[index+2];
        uint32_t a = termGraphicsState.vram[index+3];
        uint32_t result = r | g << 8 | b << 16 | a << 24;
        //uint32_t altResult = ((uint32_t*)localVram)[index/4];
        //fprintf(stderr, "result: %x, altResult: %x\n", result, altResult);
        return result;
    }
    
	return 0;
}

static void HandleOtherCSRWrite( uint8_t * image, uint16_t csrno, uint32_t value )
{
	if( csrno == 0x136 )
	{
        VRAMnPrintf(&termGraphicsState, 16, "%d", value); //32 bit number in decimal can't have more than 10 digits
		fprintf( logFile, "%d", value );
        fflush(logFile);
		//printf( "%d", value ); fflush( stdout );
	}
	if( csrno == 0x137 )
	{
        VRAMnPrintf(&termGraphicsState, 16, "%08x", value); //32 bit number in decimal can't have more than 8 digits
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
            writeArray(&termGraphicsState, (char *)image + ptrstart, ptrend - ptrstart);
            fwrite( image + ptrstart, ptrend - ptrstart, 1, logFile );
            fflush(logFile);
			//fwrite( image + ptrstart, ptrend - ptrstart, 1, stdout );
        }
	}
	else if( csrno == 0x139 )
	{
        writeChar(&termGraphicsState, value);
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

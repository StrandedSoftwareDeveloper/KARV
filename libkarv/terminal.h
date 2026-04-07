/*------------------------------------------------------------------------------*\
 | terminal.h Copyright (c) 2025 StrandedSoftwareDeveloper under the MIT License |
 | Header file for terminal.c                                                    |
\*------------------------------------------------------------------------------*/

#include <stdint.h>

typedef struct {
    uint8_t *vram;
    uint16_t width;
    uint16_t height;
    
    uint8_t *font;
    int fontWidth;
    int fontHeight;
    uint16_t charWidth;
    uint16_t charHeight;
    
    uint16_t cursorX;
    uint16_t cursorY;
    uint16_t backupCursorX;
    uint16_t backupCursorY;
} TermGraphicsState;

void clearScreen(TermGraphicsState *tgState);
void drawChar(TermGraphicsState *tgState, uint16_t x, uint16_t y, uint8_t c);
void writeChar(TermGraphicsState *tgState, char c);
void writeArray(TermGraphicsState *tgState, const char *str, int len);
void writeString(TermGraphicsState *tgState, const char *str);
void VRAMnPrintf(TermGraphicsState *tgState, unsigned long size, const char *format, ...);
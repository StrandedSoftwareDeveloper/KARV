#include "terminal.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void clearScreen(TermGraphicsState *tgState) {
    for (int i=0; i<tgState->width*tgState->height; i++) {
        tgState->vram[i*4+0] = 0;
        tgState->vram[i*4+1] = 0;
        tgState->vram[i*4+2] = 0;
        tgState->vram[i*4+3] = 255;
    }
}

void drawChar(TermGraphicsState *tgState, uint16_t x, uint16_t y, uint8_t c) {
    const int charOffsetX = (c % (tgState->fontWidth / tgState->charWidth)) * tgState->charWidth;
    const int charOffsetY = (c / (tgState->fontWidth / tgState->charWidth)) * tgState->charHeight;
    for (int yOffset=0; yOffset<tgState->charHeight; yOffset++) {
        for (int xOffset=0; xOffset<tgState->charWidth; xOffset++) {
            uint8_t val = tgState->font[(yOffset+charOffsetY)*tgState->fontWidth+(xOffset+charOffsetX)];
            tgState->vram[((y+yOffset)*tgState->width+(x+xOffset))*4+0] = val;
            tgState->vram[((y+yOffset)*tgState->width+(x+xOffset))*4+1] = val;
            tgState->vram[((y+yOffset)*tgState->width+(x+xOffset))*4+2] = val;
            tgState->vram[((y+yOffset)*tgState->width+(x+xOffset))*4+3] = 255;
        }
    }
}

void scrollUp(TermGraphicsState *tgState, int numLines) {
    const int heightLines = tgState->height / tgState->charHeight;
    for (int y=numLines; y<heightLines; y++) {
        memcpy(&tgState->vram[(y-numLines)*tgState->width*tgState->charHeight*4], &tgState->vram[y*tgState->width*tgState->charHeight*4], tgState->width*tgState->charHeight*4);
    }
    
    memset(&tgState->vram[(heightLines-numLines)*tgState->width*tgState->charHeight*4], 0, numLines*tgState->width*tgState->charHeight*4);
    
    tgState->cursorY -= tgState->charHeight * numLines;
}

//Not really sure how scrolling down is supposed to work...
void scrollDown(TermGraphicsState *tgState, int numLines) {
    const int heightLines = tgState->height / tgState->charHeight;
    for (int y=heightLines-1; y>=numLines; y--) {
        memcpy(&tgState->vram[y*tgState->width*tgState->charHeight*4], &tgState->vram[(y-numLines)*tgState->width*tgState->charHeight*4], tgState->width*tgState->charHeight*4);
    }
    
    memset(tgState->vram, 0, numLines*tgState->width*tgState->charHeight*4);
    
    //cursorY -= charHeight * numLines;
}

void clearFromCursorRight(TermGraphicsState *tgState) {
    for (int y=0; y<tgState->charHeight; y++) {
        memset(&tgState->vram[((tgState->cursorY+y)*tgState->width+tgState->cursorX)*4], 0, (tgState->width-tgState->cursorX)*4);
    }
}

void clearFromCursorDown(TermGraphicsState *tgState) {
    clearFromCursorRight(tgState);
    memset(&tgState->vram[((tgState->cursorY+tgState->charHeight)*tgState->width*4)], 0, (tgState->width*tgState->height*4)-((tgState->cursorY+tgState->charHeight)*tgState->width*4));
}

void clearFromCursorLeft(TermGraphicsState *tgState) {
    for (int y=0; y<tgState->charHeight; y++) {
        memset(&tgState->vram[(tgState->cursorY+y)*tgState->width*4], 0, tgState->cursorX*4);
    }
}

void clearFromCursorUp(TermGraphicsState *tgState) {
    clearFromCursorLeft(tgState);
    memset(tgState->vram, 0, tgState->cursorY*tgState->width*4);
}

void clearLine(TermGraphicsState *tgState) {
    memset(&tgState->vram[tgState->cursorY*tgState->width*4], 0, tgState->width*tgState->charHeight*4);
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
    ESC_BRACKET_NUM_SEMI,
    ESC_BRACKET_NUM_SEMI_NUM,
    ESC_BRACKET_QUESTION,
    ESC_BRACKET_QUESTION_NUM,
    ESC_BRACKET_SEMI,
} TerminalState;

void writeChar(TermGraphicsState *tgState, char c) {
    static TerminalState state = NORMAL;
    static int numA = 0;
    static int numB = 0;
    
    switch (state) {
        case NORMAL: {
            if (c == 27) { //Escape code
                state = ESC;
                break;
            }
            
            if (c != '\n' && c != '\r' && c != 8 /*Backspace*/ && c != 7 /*Bell*/) {
                drawChar(tgState, tgState->cursorX, tgState->cursorY, c);
            } else {
                drawChar(tgState, tgState->cursorX, tgState->cursorY, ' ');
            }
            
            if (c == 8 /*Backspace*/) {
                tgState->cursorX -= tgState->charWidth;
                break;
            } else if (c == 7 /*Bell*/) { //TODO: C'mon, we gotta do *something* with the bell
                break;
            }

            tgState->cursorX += tgState->charWidth;
            if (tgState->cursorX >= tgState->width - tgState->charWidth || c == '\n') {
                tgState->cursorX = 0;
                tgState->cursorY += tgState->charHeight;
            }

            if (tgState->cursorY > tgState->height - tgState->charHeight) {
                scrollUp(tgState, 1);
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
                    printf("Set alternate keypad mode\n");
                    state = NORMAL;
                    break;
                }
                case '>': { //Set numeric keypad mode TODO: Decide what we're gonna do about keypad modes
                    printf("Set numeric keypad mode\n");
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
                    printf("Set single shift 2\n");
                    state = NORMAL;
                    break;
                }
                case 'O': { //Set single shift 3
                    printf("Set single shift 3\n");
                    state = NORMAL;
                    break;
                }
                
                case 'D': { //Move/scroll window up one line FIXME: I don't think this is quite right...
                    printf("Move/scroll window up one line\n");
                    scrollUp(tgState, 1);
                    state = NORMAL;
                    break;
                }
                case 'M': { //Move/scroll window down one line FIXME: I don't think this is quite right...
                    printf("Move/scroll window down one line\n");
                    scrollDown(tgState, 1);
                    state = NORMAL;
                    break;
                }
                case 'E': { //Move to next line FIXME: I don't think this is quite right...
                    printf("Move to next line\n");
                    scrollUp(tgState, 1);
                    state = NORMAL;
                    break;
                }
                case '7': { //Save cursor position and attributes
                    printf("Save cursor position and attributes\n");
                    tgState->backupCursorX = tgState->cursorX;
                    tgState->backupCursorY = tgState->cursorY;
                    state = NORMAL;
                    break;
                }
                case '8': { //Restore cursor position and attributes
                    printf("Restore cursor position and attributes\n");
                    tgState->cursorX = tgState->backupCursorX;
                    tgState->cursorY = tgState->backupCursorY;
                    state = NORMAL;
                    break;
                }
                
                case 'H': { //Set a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                    printf("Set a tab at the current column\n");
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
                    printf("Reset terminal to initial state\n");
                    clearScreen(tgState);
                    tgState->cursorX = 0;
                    tgState->cursorY = 0;
                    tgState->backupCursorX = 0;
                    tgState->backupCursorY = 0;
                    state = NORMAL;
                    break;
                }
                
                case '<': { //Toggle ANSI mode TODO: Implement ANSI mode
                    printf("Toggle ANSI mode\n");
                    state = NORMAL;
                    break;
                }
                
                default: { //Invalid escape code
                    printf("Invalid escape code ESC\n");
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
                    printf("Turn off character attributes\n");
                    state = NORMAL;
                    break;
                }
                
                case 'H': { //Move cursor to upper left corner
                    printf("Move cursor to upper left corner\n");
                    tgState->cursorX = 0;
                    tgState->cursorY = 0;
                    state = NORMAL;
                    break;
                }
                case ';': {
                    state = ESC_BRACKET_SEMI;
                    break;
                }
                case 'f': { //Move cursor to upper left corner
                    printf("Move cursor to upper left corner\n");
                    tgState->cursorX = 0;
                    tgState->cursorY = 0;
                    state = NORMAL;
                    break;
                }
                
                case 'g': { //Clear a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                    printf("Clear a tab at the current column\n");
                    state = NORMAL;
                    break;
                }
                
                case 'K': { //Clear line from cursor right
                    printf("Clear line from cursor right\n");
                    clearFromCursorRight(tgState);
                    state = NORMAL;
                    break;
                }
                
                case 'J': { //Clear screen from cursor down
                    printf("Clear screen from cursor down\n");
                    clearFromCursorDown(tgState);
                    state = NORMAL;
                    break;
                }
                
                case 'c': { //Identify what terminal type TODO: Setup keyboard buffer to respond to this
                    printf("Identify what terminal type\n");
                    state = NORMAL;
                    break;
                }
                
                default: {
                    if (c >= '0' && c <= '9') {
                        numA = c - '0';
                        state = ESC_BRACKET_NUM;
                        break;
                    } 
                    printf("Invalid escape code ESC_BRACKET\n"); //Invalid escape code
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_OPEN_PAREN: {
            switch (c) {
                case 'A': { //Set United Kingdom G0 character set TODO: Figure out if we even need these
                    printf("Set United Kingdom G0 character set\n");
                    state = NORMAL;
                    break;
                }
                case 'B': { //Set United States G0 character set TODO: Figure out if we even need these
                    printf("Set United States G0 character set\n");
                    state = NORMAL;
                    break;
                }
                case '0': { //Set G0 special chars. & line set TODO: Figure out if we even need these
                    printf("Set G0 special chars. & line set\n");
                    state = NORMAL;
                    break;
                }
                case '1': { //Set G0 alternate character ROM TODO: Figure out if we even need these
                    printf("Set G0 alternate character ROM\n");
                    state = NORMAL;
                    break;
                }
                case '2': { //Set G0 alt char ROM and spec. graphics TODO: Figure out if we even need these
                    printf("Set G0 alt char ROM and spec. graphics\n");
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_OPEN_PAREN\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_CLOSE_PAREN: {
            switch (c) {
                case 'A': { //Set United Kingdom G1 character set TODO: Figure out if we even need these
                    printf("Set United Kingdom G1 character set\n");
                    state = NORMAL;
                    break;
                }
                case 'B': { //Set United States G1 character set TODO: Figure out if we even need these
                    printf("Set United States G1 character set\n");
                    state = NORMAL;
                    break;
                }
                case '0': { //Set G1 special chars. & line set TODO: Figure out if we even need these
                    printf("Set G1 special chars. & line set\n");
                    state = NORMAL;
                    break;
                }
                case '1': { //Set G1 alternate character ROM TODO: Figure out if we even need these
                    printf("Set G1 alternate character ROM\n");
                    state = NORMAL;
                    break;
                }
                case '2': { //Set G1 alt char ROM and spec. graphics TODO: Figure out if we even need these
                    printf("Set G1 alt char ROM and spec. graphics\n");
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_CLOSE_PAREN\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_POUND: {
            switch (c) {
                case '3': { //Double-height letters, top half TODO: Implement
                    printf("Double-height letters, top half\n");
                    state = NORMAL;
                    break;
                }
                case '4': { //Double-height letters, bottom half TODO: Implement
                    printf("Double-height letters, bottom half\n");
                    state = NORMAL;
                    break;
                }
                case '5': { //Single width, single height letters TODO: Implement
                    printf("Single width, single height letters\n");
                    state = NORMAL;
                    break;
                }
                case '6': { //Double width, single height letters TODO: Implement
                    printf("Double width, single height letters\n");
                    state = NORMAL;
                    break;
                }
                
                case '8': { //Screen alignment display TODO: What is this even supposed to do?
                    printf("Screen alignment display\n");
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_POUND\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_FIVE: {
            switch (c) {
                case 'n': { //Device status report TODO: Setup keyboard buffer to respond to this
                    printf("Device status report\n");
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_FIVE\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_SIX: {
            switch (c) {
                case 'n': { //Get cursor position TODO: Setup keyboard buffer to respond to this
                    printf("Get cursor position\n");
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_SIX\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET_NUM: {
            switch (c) {
                case 'h': {
                    if (numA == 20) { //Set new line mode TODO: Figure out what this is supposed to do
                        printf("Set new line mode\n");
                        state = NORMAL;
                        break;
                    } else { //Invalid escape code
                        printf("Invalid escape code ESC_BRACKET_NUM_h\n");
                        state = NORMAL;
                        break;
                    }
                }
                case 'l': {
                    if (numA == 20) { //Set line feed mode TODO: Figure out what this is supposed to do
                        printf("Set line feed mode\n");
                        state = NORMAL;
                        break;
                    } else { //Invalid escape code
                        printf("Invalid escape code ESC_BRACKET_NUM_l\n");
                        state = NORMAL;
                        break;
                    }
                }
                case 'm': {
                    switch (numA) {
                        case 0: { //Turn off character attributes TODO: Implement character attributes
                            printf("Turn off character attributes\n");
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Turn bold mode on TODO: Implement character attributes
                            printf("Turn bold mode on\n");
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Turn low intensity mode on TODO: Implement character attributes
                            printf("Turn low intensity mode on\n");
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Turn underline mode on TODO: Implement character attributes
                            printf("Turn underline mode on\n");
                            state = NORMAL;
                            break;
                        }
                        case 5: { //Turn blinking mode on TODO: Implement character attributes
                            printf("Turn blinking mode on\n");
                            state = NORMAL;
                            break;
                        }
                        case 7: { //Turn reverse video on TODO: Implement character attributes
                            printf("Turn reverse video on\n");
                            state = NORMAL;
                            break;
                        }
                        case 8: { //Turn invisible text mode on TODO: Implement character attributes
                            printf("Turn invisible text mode on\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_m\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case ';': {
                    state = ESC_BRACKET_NUM_SEMI;
                    break;
                }
                
                case 'A': { //Move cursor up numA lines
                    printf("Move cursor up numA lines\n");
                    tgState->cursorY -= tgState->charHeight * numA;
                    state = NORMAL;
                    break;
                }
                case 'B': { //Move cursor down numA lines
                    printf("Move cursor down numA lines\n");
                    tgState->cursorY += tgState->charHeight * numA;
                    state = NORMAL;
                    break;
                }
                case 'C': { //Move cursor right numA lines
                    printf("Move cursor right numA lines\n");
                    tgState->cursorX += tgState->charWidth * numA;
                    state = NORMAL;
                    break;
                }
                case 'D': { //Move cursor left numA lines
                    printf("Move cursor left numA lines\n");
                    tgState->cursorX -= tgState->charWidth * numA;
                    state = NORMAL;
                    break;
                }
                
                case 'g': {
                    switch (numA) {
                        case 0: { //Clear a tab at the current column TODO: Figure out what all this tab stuff is supposed to do
                            printf("Clear a tab at the current column\n");
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Clear all tabs TODO: Figure out what all this tab stuff is supposed to do
                            printf("Clear all tabs\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_g\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'K': {
                    switch (numA) {
                        case 0: { //Clear line from cursor right
                            printf("Clear line from cursor right\n");
                            clearFromCursorRight(tgState);
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Clear line from cursor left
                            printf("Clear line from cursor left\n");
                            clearFromCursorLeft(tgState);
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Clear entire line
                            printf("Clear entire line\n");
                            clearLine(tgState);
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_K\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'J': {
                    switch (numA) {
                        case 0: { //Clear screen from cursor down
                            printf("Clear screen from cursor down\n");
                            clearFromCursorDown(tgState);
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Clear screen from cursor up
                            printf("Clear screen from cursor up\n");
                            clearFromCursorUp(tgState);
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Clear entire screen
                            printf("Clear entire screen\n");
                            clearScreen(tgState);
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_J\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'c': {
                    if (numA == 0) { //Identify what terminal type (another) TODO: Setup keyboard buffer to respond to this
                        printf("Identify what terminal type (another)\n");
                        state = NORMAL;
                    } else { //Invalid escape code
                        printf("Invalid escape code ESC_BRACKET_NUM_c\n");
                        state = NORMAL;
                    }
                    break;
                }
                case 'q': {
                    switch (numA) {
                        case 0: { //Turn off all four leds TODO: Implement the leds
                            printf("Turn off all four leds\n");
                            state = NORMAL;
                            break;
                        }
                        case 1: { //Turn on LED #1 TODO: Implement the leds
                            printf("Turn on LED #1\n");
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Turn on LED #2 TODO: Implement the leds
                            printf("Turn on LED #2\n");
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Turn on LED #3 TODO: Implement the leds
                            printf("Turn on LED #3\n");
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Turn on LED #4 TODO: Implement the leds
                            printf("Turn on LED #4\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_q\n");
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
                        break;
                    }
                    printf("Invalid escape code ESC_BRACKET_NUM\n");
                    state = NORMAL; //Invalid escape code
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET_NUM_SEMI: {
            if (c >= '0' && c <= '9') {
                numB = c - '0';
                state = ESC_BRACKET_NUM_SEMI_NUM;
                break;
            }
            printf("Invalid escape code ESC_BRACKET_NUM_SEMI\n");
            state = NORMAL; //Invalid escape code
            break;
        }
        case ESC_BRACKET_NUM_SEMI_NUM: {
            switch (c) {
                case 'r': { //Set top and bottom line's of a window TODO: Figure out what this is supposed to do
                    printf("Set top and bottom line's of a window\n");
                    state = NORMAL;
                    break;
                }
                case 'H': { //Move cursor to screen location numB, numA NOTE: Coordinates come in y, x format, (1, 1) is the top left corner
                    printf("Move cursor to screen location numB, numA\n");
                    tgState->cursorX = (numB - 1) * tgState->charWidth;
                    tgState->cursorY = (numA - 1) * tgState->charHeight;
                    state = NORMAL;
                    break;
                }
                case 'f': { //Move cursor to screen location numB, numA NOTE: Coordinates come in y, x format, (1, 1) is the top left corner
                    printf("Move cursor to screen location numB, numA\n");
                    tgState->cursorX = (numB - 1) * tgState->charWidth;
                    tgState->cursorY = (numA - 1) * tgState->charHeight;
                    state = NORMAL;
                    break;
                }
                case 'y': { //Terminal self tests
                    if (numA != 2) { //Invalid escape code
                        state = NORMAL;
                        break;
                    }
                    
                    switch (numB) {
                        case 1: { //Confidence power up test TODO: Figure out what this is supposed to do
                            printf("Confidence power up test\n");
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Confidence loopback test TODO: Figure out what this is supposed to do
                            printf("Confidence loopback test\n");
                            state = NORMAL;
                            break;
                        }
                        case 9: { //Repeat power up test TODO: Figure out what this is supposed to do
                            printf("Repeat power up test\n");
                            state = NORMAL;
                            break;
                        }
                        case 10: { //Repeat loopback test TODO: Figure out what this is supposed to do
                            printf("Repeat loopback test\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_NUM_SEMI_NUM_y\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                default: {
                    if (c >= '0' && c <= '9') {
                        numB *= 10;
                        numB += c - '0';
                        break;
                    }
                    printf("Invalid escape code ESC_BRACKET_NUM_SEMI_NUM\n");
                    state = NORMAL; //Invalid escape code
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET_QUESTION: {
            if (c >= '1' && c <= '9') {
                numA = c - '0';
                state = ESC_BRACKET_QUESTION_NUM;
                break;
            }
            printf("Invalid escape code ESC_BRACKET_QUESTION\n");
            state = NORMAL; //Invalid escape code
            break;
        }
        case ESC_BRACKET_QUESTION_NUM: {
            switch (c) {
                case 'h': {
                    switch (numA) {
                        case 1: { //Set cursor key to application TODO: Figure out what this is supposed to do
                            printf("Set cursor key to application\n");
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Set number of columns to 132 TODO: Figure out what this is supposed to do
                            printf("Set number of columns to 132\n");
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Set smooth scrolling TODO: Implement smooth scrolling
                            printf("Set smooth scrolling\n");
                            state = NORMAL;
                            break;
                        }
                        case 5: { //Set reverse video on screen TODO: Implement reverse video
                            printf("Set reverse video on screen\n");
                            state = NORMAL;
                            break;
                        }
                        case 6: { //Set origin to relative TODO: Figure out what this is supposed to do
                            printf("Set origin to relative\n");
                            state = NORMAL;
                            break;
                        }
                        case 7: { //Set auto-wrap mode TODO: Figure out what this is supposed to do
                            printf("Set auto-wrap mode\n");
                            state = NORMAL;
                            break;
                        }
                        case 8: { //Set auto-repeat mode TODO: Figure out what this is supposed to do
                            printf("Set auto-repeat mode\n");
                            state = NORMAL;
                            break;
                        }
                        case 9: { //Set interlacing mode NOTE: This probably will never do anything
                            printf("Set interlacing mode\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_QUESTION_NUM_h\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                case 'l': {
                    switch (numA) {
                        case 1: { //Set cursor key to cursor TODO: Figure out what this is supposed to do
                            printf("Set cursor key to cursor\n");
                            state = NORMAL;
                            break;
                        }
                        case 2: { //Set VT52 (versus ANSI) TODO: Figure out if we want this
                            printf("Set VT52 (versus ANSI)\n");
                            state = NORMAL;
                            break;
                        }
                        case 3: { //Set number of columns to 80 TODO: Figure out exactly what this is supposed to do
                            printf("Set number of columns to 80\n");
                            state = NORMAL;
                            break;
                        }
                        case 4: { //Set jump scrolling TODO: Implement smooth scrolling
                            printf("Set jump scrolling\n");
                            state = NORMAL;
                            break;
                        }
                        case 5: { //Set normal video on screen TODO: Implement reverse video
                            printf("Set normal video on screen\n");
                            state = NORMAL;
                            break;
                        }
                        case 6: { //Set origin to absolute TODO: Figure out what this is supposed to do
                            printf("Set origin to absolute\n");
                            state = NORMAL;
                            break;
                        }
                        case 7: { //Reset auto-wrap mode TODO: Figure out what this is supposed to do
                            printf("Reset auto-wrap mode\n");
                            state = NORMAL;
                            break;
                        }
                        case 8: { //Reset auto-repeat mode TODO: Figure out what this is supposed to do
                            printf("Reset auto-repeat mode\n");
                            state = NORMAL;
                            break;
                        }
                        case 9: { //Reset interlacing mode NOTE: This probably will never do anything
                            printf("Reset interlacing mode\n");
                            state = NORMAL;
                            break;
                        }
                        default: { //Invalid escape code
                            printf("Invalid escape code ESC_BRACKET_QUESTION_NUM_l\n");
                            state = NORMAL;
                            break;
                        }
                    }
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_BRACKET_QUESTION_NUM\n");
                    state = NORMAL;
                    break;
                }
            }
            break;
        }
        case ESC_BRACKET_SEMI: {
            switch (c) {
                case 'H': { //Move cursor to upper left corner
                    printf("Move cursor to upper left corner\n");
                    tgState->cursorX = 0;
                    tgState->cursorY = 0;
                    state = NORMAL;
                    break;
                }
                case 'f': { //Move cursor to upper left corner
                    printf("Move cursor to upper left corner\n");
                    tgState->cursorX = 0;
                    tgState->cursorY = 0;
                    state = NORMAL;
                    break;
                }
                default: { //Invalid escape code
                    printf("Invalid escape code ESC_BRACKET_SEMI\n");
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

void writeArray(TermGraphicsState *tgState, const char *str, int len) {
    for (int i=0; i<len; i++) {
        writeChar(tgState, str[i]);
    }
}

void writeString(TermGraphicsState *tgState, const char *str) {
    writeArray(tgState, str, strlen(str));
}

void VRAMnPrintf(TermGraphicsState *tgState, unsigned long size, const char *format, ...) {
    va_list va;
    va_start(va, format);

    char *buffer = malloc(size);
    vsnprintf(buffer, size, format, va);
    writeString(tgState, buffer);
    free(buffer);

    va_end(va);
}
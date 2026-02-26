/*
 * sdl2_atari_keytst.c
 *
 * SDL2 graphical keyboard test for Atari ST / MiNT.
 * Renders the full Atari ST keyboard layout at 320x200 (ST low).
 * Pressed keys are highlighted; a scrolling event log is shown below.
 *
 * Build (m68k cross-compiler):
 *   m68k-atari-mint-gcc -o keytst.tos sdl2_atari_keytst.c \
 *       -I/path/to/sdl2/include -L/path/to/sdl2/lib -lSDL2
 *
 * Build (host, for testing):
 *   gcc -o keytst sdl2_atari_keytst.c $(sdl2-config --cflags --libs)
 *
 * Layout (320x200):
 *
 *   ┌─────────────────────────────────────┐
 *   │ ATARI ST KEYBOARD TEST        [VER] │  <- header bar
 *   ├─────────────────────────────────────┤
 *   │  [F1][F2][F3][F4][F5][F6][F7][F8]  │  <- function row
 *   │  [ESC][1][2]...[BKSP]              │  <- row 0
 *   │  [TAB][Q][W]...[RETURN]            │  <- row 1
 *   │  [CTRL][A][S]...[RETURN]           │  <- row 2
 *   │  [LSHFT][Z][X]...[RSHFT]           │  <- row 3
 *   │  [CAPS][ALT][SPC][ALTGR][CAPS]     │  <- row 4
 *   ├─────────────────────────────────────┤
 *   │ LAST: <keyname>    SCAN: 0xXX      │  <- status bar
 *   │ [log line 1]                        │
 *   │ [log line 2]                        │
 *   │            PRESS Q OR ESC TO QUIT  │
 *   └─────────────────────────────────────┘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL2/SDL.h"

/* -------------------------------------------------------------------------
 * Screen geometry — Atari ST low: 320x200
 * ------------------------------------------------------------------------- */
#define SCREEN_W   320
#define SCREEN_H   200
#define VERSION    "v1.0"

/* Header */
#define HDR_H       12

/* Keyboard drawing area */
#define KB_Y        (HDR_H + 2)
#define KB_H        138        /* leaves ~50px for status + log */

/* Status / log area */
#define STATUS_Y    (KB_Y + KB_H)
#define STATUS_H    (SCREEN_H - STATUS_Y)
#define LOG_LINES   2
#define LOG_LINE_H  9
#define LOG_X       3
#define LOG_Y       (STATUS_Y + 12)

/* Key sizing */
#define KEY_H       12         /* height of every key in pixels   */
#define KEY_GAP     1          /* 1-px gap between keys            */
#define U           14         /* 1u key width (unit)              */

/* -------------------------------------------------------------------------
 * Palette  (same 16-colour scheme as joystick test)
 * ------------------------------------------------------------------------- */
#define C_BLACK    0
#define C_WHITE    1
#define C_GRAY     2
#define C_DGRAY    3
#define C_RED      4
#define C_DRED     5
#define C_GREEN    6
#define C_DGREEN   7
#define C_BLUE     8
#define C_DBLUE    9
#define C_CYAN     10
#define C_YELLOW   11
#define C_ORANGE   12
#define C_PANEL    13
#define C_HILITE   14
#define C_BORDER   15

static const SDL_Color palette[] = {
    {   0,   0,   0, 255 }, /* 0  BLACK  */
    { 255, 255, 255, 255 }, /* 1  WHITE  */
    { 170, 170, 170, 255 }, /* 2  GRAY   */
    {  85,  85,  85, 255 }, /* 3  DGRAY  */
    { 255,  85,  85, 255 }, /* 4  RED    */
    { 170,   0,   0, 255 }, /* 5  DRED   */
    {  85, 255,  85, 255 }, /* 6  GREEN  */
    {   0, 170,   0, 255 }, /* 7  DGREEN */
    {  85,  85, 255, 255 }, /* 8  BLUE   */
    {   0,   0, 170, 255 }, /* 9  DBLUE  */
    {  85, 255, 255, 255 }, /* 10 CYAN   */
    { 255, 255,  85, 255 }, /* 11 YELLOW */
    { 255, 170,   0, 255 }, /* 12 ORANGE */
    {   0,  42,  85, 255 }, /* 13 PANEL  */
    {   0, 128, 255, 255 }, /* 14 HILITE */
    {   0,  85, 128, 255 }, /* 15 BORDER */
};

/* -------------------------------------------------------------------------
 * Tiny 5×7 bitmap font (ASCII 32-127, same as joystick test)
 * Each glyph: 5 bytes, each byte = one row, MSB = leftmost pixel.
 * ------------------------------------------------------------------------- */
static const Uint8 font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 '\'' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 42 '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 '-' */
    {0x00,0x30,0x30,0x00,0x00}, /* 46 '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* 60 '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* 62 '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 77 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 'V' */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 87 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 89 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91 '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 '\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93 ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 103 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 '}' */
    {0x0C,0x02,0x0C,0x10,0x0C}, /* 126 '~' */
    {0x7F,0x41,0x41,0x41,0x7F}, /* 127 DEL (block) */
};

/* -------------------------------------------------------------------------
 * Renderer handle
 * ------------------------------------------------------------------------- */
static SDL_Renderer *ren = NULL;

/* -------------------------------------------------------------------------
 * Low-level draw helpers
 * ------------------------------------------------------------------------- */
static void SetColor(int c)
{
    SDL_SetRenderDrawColor(ren,
        palette[c].r, palette[c].g, palette[c].b, palette[c].a);
}

static void FillRect(int x, int y, int w, int h, int c)
{
    SDL_Rect r = { x, y, w, h };
    SetColor(c);
    SDL_RenderFillRect(ren, &r);
}

static void DrawRect(int x, int y, int w, int h, int c)
{
    SDL_Rect r = { x, y, w, h };
    SetColor(c);
    SDL_RenderDrawRect(ren, &r);
}

static void HLine(int x, int y, int len, int c)
{
    SetColor(c);
    SDL_RenderDrawLine(ren, x, y, x + len - 1, y);
}

/* Draw a single character at pixel (px,py). Scale=1: 5x7 output. */
static void DrawChar(int px, int py, char ch, int fg, int bg)
{
    int col, row;
    if (ch < 32 || ch > 127) ch = '?';
    for (col = 0; col < 5; col++) {
        Uint8 bits = font5x7[(unsigned char)ch - 32][col];
        for (row = 0; row < 7; row++) {
            int lit = (bits >> row) & 1; 
            if (lit) {
                SetColor(fg);
                SDL_RenderDrawPoint(ren, px + col, py + row);
            } else if (bg >= 0) {
                SetColor(bg);
                SDL_RenderDrawPoint(ren, px + col, py + row);
            }
        }
    }
}

/* Draw a NUL-terminated string. Returns pixel width used. */
static int DrawString(int x, int y, const char *s, int fg, int bg)
{
    int ox = x;
    while (*s) {
        DrawChar(x, y, *s, fg, bg);
        x += 6;  /* 5 pixels + 1 gap */
        s++;
    }
    return x - ox;
}

/* Draw string centred in a box (x,y,w,h). */
static void DrawStringCentered(int bx, int by, int bw, int bh,
                                const char *s, int fg, int bg)
{
    int len = (int)strlen(s);
    int tx  = bx + (bw - len * 6) / 2;
    int ty  = by + (bh - 7) / 2;
    if (tx < bx) tx = bx;
    DrawString(tx, ty, s, fg, bg);
}

/* -------------------------------------------------------------------------
 * Key state — indexed by SDL_Scancode (512 entries, sparse)
 * ------------------------------------------------------------------------- */
#define MAX_SCAN 512
static Uint8 key_state[MAX_SCAN];

/* -------------------------------------------------------------------------
 * Key descriptor
 * Each entry defines one key's position/size and its label(s).
 * x, y in pixels relative to the keyboard drawing area top-left.
 * w, h in pixels.
 * label: short label drawn on the key.
 * scan: SDL_Scancode to watch.
 * ------------------------------------------------------------------------- */
typedef struct {
    int            x, y, w, h;
    const char    *label;
    SDL_Scancode   scan;
} KeyDef;

/* -------------------------------------------------------------------------
 * Atari ST keyboard layout definition
 *
 * The Atari ST keyboard (German/US layout reference) has:
 *   Function row: F1-F10
 *   Row 0:  ESC  1 2 3 4 5 6 7 8 9 0 - = BKSP
 *   Row 1:  TAB  Q W E R T Y U I O P [ ] RETURN
 *   Row 2:  CTRL A S D F G H J K L ; ' ` RETURN (tall)
 *   Row 3:  LSHFT \ Z X C V B N M , . / RSHFT
 *   Row 4:  CAPS  ALT  [      SPACE      ]  (no ALTGR on US ST)
 *   Cursor cluster: UP DOWN LEFT RIGHT
 *
 * All coordinates are relative to KB area (0,0 = top-left of KB area).
 * We have KB_H = 138 pixels, 6 rows of keys each KEY_H=12 with KEY_GAP=1.
 * Total key rows height = 6*(12+1) = 78 px.  We add 2px top margin.
 *
 * X layout: total keyboard width ≈ 316 px (2px left margin, 2px right margin).
 * Unit U = 14 px.
 *
 * Row offsets (y):
 *   Fn  row: y=2
 *   Row 0:   y=17  (small gap after Fn row)
 *   Row 1:   y=30
 *   Row 2:   y=43
 *   Row 3:   y=56
 *   Row 4:   y=69
 *   Cursor:  y=56  (right side, rows 3&4 region)
 * ------------------------------------------------------------------------- */

/* Helpers for building rows */
#define KY_FN   2    /* y for Fn row  */
#define KY_0   17    /* y for row 0   */
#define KY_1   30    /* y for row 1   */
#define KY_2   43    /* y for row 2   */
#define KY_3   56    /* y for row 3   */
#define KY_4   69    /* y for row 4   */
#define KY_CU  56    /* y for cursor  */

/* Standard key height */
#define KH     KEY_H

/* We keep a static array; NULL scan = 0 entries are skipped in draw */
static const KeyDef keys[] = {

    /* --- Function row (F1-F10) ----------------------------------------- */
    /* We compress to fit: each Fn key = 28px wide, 2px gap between groups */
    {  2, KY_FN, 27, KH, "F1",  SDL_SCANCODE_F1  },
    { 31, KY_FN, 27, KH, "F2",  SDL_SCANCODE_F2  },
    { 60, KY_FN, 27, KH, "F3",  SDL_SCANCODE_F3  },
    { 89, KY_FN, 27, KH, "F4",  SDL_SCANCODE_F4  },
    {118, KY_FN, 27, KH, "F5",  SDL_SCANCODE_F5  },
    {147, KY_FN, 27, KH, "F6",  SDL_SCANCODE_F6  },
    {176, KY_FN, 27, KH, "F7",  SDL_SCANCODE_F7  },
    {205, KY_FN, 27, KH, "F8",  SDL_SCANCODE_F8  },
    {234, KY_FN, 27, KH, "F9",  SDL_SCANCODE_F9  },
    {263, KY_FN, 27, KH, "F10", SDL_SCANCODE_F10 },

    /* --- Row 0: ESC 1 2 3 4 5 6 7 8 9 0 - = BKSP ----------------------- */
    {  2, KY_0, 19, KH, "ESC", SDL_SCANCODE_ESCAPE     },
    { 23, KY_0, 13, KH, "1",   SDL_SCANCODE_1          },
    { 38, KY_0, 13, KH, "2",   SDL_SCANCODE_2          },
    { 53, KY_0, 13, KH, "3",   SDL_SCANCODE_3          },
    { 68, KY_0, 13, KH, "4",   SDL_SCANCODE_4          },
    { 83, KY_0, 13, KH, "5",   SDL_SCANCODE_5          },
    { 98, KY_0, 13, KH, "6",   SDL_SCANCODE_6          },
    {113, KY_0, 13, KH, "7",   SDL_SCANCODE_7          },
    {128, KY_0, 13, KH, "8",   SDL_SCANCODE_8          },
    {143, KY_0, 13, KH, "9",   SDL_SCANCODE_9          },
    {158, KY_0, 13, KH, "0",   SDL_SCANCODE_0          },
    {173, KY_0, 13, KH, "-",   SDL_SCANCODE_MINUS      },
    {188, KY_0, 13, KH, "=",   SDL_SCANCODE_EQUALS     },
    {203, KY_0, 29, KH, "BKSP",SDL_SCANCODE_BACKSPACE  },

    /* --- Row 1: TAB Q W E R T Y U I O P [ ] RETURN --------------------- */
    {  2, KY_1, 19, KH, "TAB", SDL_SCANCODE_TAB        },
    { 23, KY_1, 13, KH, "Q",   SDL_SCANCODE_Q          },
    { 38, KY_1, 13, KH, "W",   SDL_SCANCODE_W          },
    { 53, KY_1, 13, KH, "E",   SDL_SCANCODE_E          },
    { 68, KY_1, 13, KH, "R",   SDL_SCANCODE_R          },
    { 83, KY_1, 13, KH, "T",   SDL_SCANCODE_T          },
    { 98, KY_1, 13, KH, "Y",   SDL_SCANCODE_Y          },
    {113, KY_1, 13, KH, "U",   SDL_SCANCODE_U          },
    {128, KY_1, 13, KH, "I",   SDL_SCANCODE_I          },
    {143, KY_1, 13, KH, "O",   SDL_SCANCODE_O          },
    {158, KY_1, 13, KH, "P",   SDL_SCANCODE_P          },
    {173, KY_1, 13, KH, "[",   SDL_SCANCODE_LEFTBRACKET  },
    {188, KY_1, 13, KH, "]",   SDL_SCANCODE_RIGHTBRACKET },
    /* RETURN spans rows 1&2, drawn as tall key */
    {203, KY_1, 29, KH*2+KEY_GAP, "RET", SDL_SCANCODE_RETURN },

    /* --- Row 2: CTRL A S D F G H J K L ; ' ` (Return already drawn) ---- */
    {  2, KY_2, 22, KH, "CTL", SDL_SCANCODE_LCTRL      },
    { 26, KY_2, 13, KH, "A",   SDL_SCANCODE_A          },
    { 41, KY_2, 13, KH, "S",   SDL_SCANCODE_S          },
    { 56, KY_2, 13, KH, "D",   SDL_SCANCODE_D          },
    { 71, KY_2, 13, KH, "F",   SDL_SCANCODE_F          },
    { 86, KY_2, 13, KH, "G",   SDL_SCANCODE_G          },
    {101, KY_2, 13, KH, "H",   SDL_SCANCODE_H          },
    {116, KY_2, 13, KH, "J",   SDL_SCANCODE_J          },
    {131, KY_2, 13, KH, "K",   SDL_SCANCODE_K          },
    {146, KY_2, 13, KH, "L",   SDL_SCANCODE_L          },
    {161, KY_2, 13, KH, ";",   SDL_SCANCODE_SEMICOLON  },
    {176, KY_2, 13, KH, "'",   SDL_SCANCODE_APOSTROPHE },
    {191, KY_2, 11, KH, "`",   SDL_SCANCODE_GRAVE      },

    /* --- Row 3: LSHFT \ Z X C V B N M , . / RSHFT --------------------- */
    {  2, KY_3, 26, KH, "SHF", SDL_SCANCODE_LSHIFT     },
    { 30, KY_3, 13, KH, "\\",  SDL_SCANCODE_BACKSLASH  },
    { 45, KY_3, 13, KH, "Z",   SDL_SCANCODE_Z          },
    { 60, KY_3, 13, KH, "X",   SDL_SCANCODE_X          },
    { 75, KY_3, 13, KH, "C",   SDL_SCANCODE_C          },
    { 90, KY_3, 13, KH, "V",   SDL_SCANCODE_V          },
    {105, KY_3, 13, KH, "B",   SDL_SCANCODE_B          },
    {120, KY_3, 13, KH, "N",   SDL_SCANCODE_N          },
    {135, KY_3, 13, KH, "M",   SDL_SCANCODE_M          },
    {150, KY_3, 13, KH, ",",   SDL_SCANCODE_COMMA      },
    {165, KY_3, 13, KH, ".",   SDL_SCANCODE_PERIOD     },
    {180, KY_3, 13, KH, "/",   SDL_SCANCODE_SLASH      },
    {195, KY_3, 37, KH, "SHFT",SDL_SCANCODE_RSHIFT     },

    /* --- Row 4: CAPS ALT SPACE ----------------------------------------- */
    {  2, KY_4, 22, KH, "CAP", SDL_SCANCODE_CAPSLOCK   },
    { 26, KY_4, 22, KH, "ALT", SDL_SCANCODE_LALT       },
    { 50, KY_4,113, KH, "SPACE", SDL_SCANCODE_SPACE    },
    /* Atari ST has no AltGr/Win; add RALT as modifier for completeness */
    {165, KY_4, 19, KH, "ALGR",SDL_SCANCODE_RALT       },
    {186, KY_4, 19, KH, "DEL", SDL_SCANCODE_DELETE     },
    {207, KY_4, 25, KH, "HELP",SDL_SCANCODE_INSERT     }, /* ST HELP key */

    /* --- Cursor cluster (right side, rows 3&4 zone) -------------------- */
    {244, KY_3, 19, KH, " UP", SDL_SCANCODE_UP         },
    {235, KY_4, 19, KH, "LFT", SDL_SCANCODE_LEFT       },
    {256, KY_4, 19, KH, "DWN", SDL_SCANCODE_DOWN       },
    {277, KY_4, 19, KH, "RGT", SDL_SCANCODE_RIGHT      },

    /* --- Numeric keypad (right zone, rows 0-4) ------------------------- */
    /* We squeeze a mini numpad to the far right, 3 cols x 5 rows */
    /* x starts at ~234 for numpad; but cursor is there — we skip numpad */
    /* to avoid overlap. Only show if screen width allows. */
    /* For 320px display the cursor keys fill the right zone adequately.  */
};

#define NUM_KEYS ((int)(sizeof(keys)/sizeof(keys[0])))

/* -------------------------------------------------------------------------
 * Event log
 * ------------------------------------------------------------------------- */
#define LOG_BUF_W  52
static char log_buf[LOG_LINES][LOG_BUF_W];
static int  log_head = 0; /* next slot to overwrite */

static void LogPush(const char *msg)
{
    SDL_snprintf(log_buf[log_head], LOG_BUF_W, "%-*s", LOG_BUF_W - 1, msg);
    log_head = (log_head + 1) % LOG_LINES;
}

/* Last key info */
static char last_key_name[32] = "---";
static int  last_scancode      = 0;

/* -------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------- */

/* Header bar */
static void DrawHeader(void)
{
    FillRect(0, 0, SCREEN_W, HDR_H, C_PANEL);
    HLine(0, HDR_H, SCREEN_W, C_BORDER);
    DrawString(3, 3, "ATARI ST KEYBOARD TEST", C_CYAN, -1);
    DrawString(SCREEN_W - 6*4 - 3, 3, VERSION, C_YELLOW, -1);
}

/* Draw a single key */
static void DrawKey(const KeyDef *k)
{
    int pressed = (k->scan < MAX_SCAN) ? key_state[k->scan] : 0;
    int bx = k->x;
    int by = KB_Y + k->y;
    int bw = k->w;
    int bh = k->h;

    int body_col   = pressed ? C_HILITE : C_DGRAY;
    int border_col = pressed ? C_WHITE  : C_BORDER;
    int label_col  = pressed ? C_WHITE  : C_GRAY;

    /* Shadow / 3-D bevel */
    if (!pressed) {
        FillRect(bx + 1, by + 1, bw - 1, bh - 1, C_BLACK); /* shadow */
    }
    FillRect(bx, by, bw - 1, bh - 1, body_col);
    DrawRect(bx, by, bw - 1, bh - 1, border_col);

    /* Top/left highlight (3-D feel) */
    if (!pressed) {
        SetColor(C_GRAY);
        SDL_RenderDrawLine(ren, bx, by, bx + bw - 2, by);
        SDL_RenderDrawLine(ren, bx, by, bx, by + bh - 2);
    }

    DrawStringCentered(bx, by, bw - 1, bh - 1, k->label, label_col, -1);
}

static void DrawKeyboard(void)
{
    int i;
    FillRect(0, KB_Y, SCREEN_W, KB_H, C_BLACK);
    for (i = 0; i < NUM_KEYS; i++) {
        DrawKey(&keys[i]);
    }
}

/* Status bar */
static void DrawStatus(void)
{
    char tmp[64];
    FillRect(0, STATUS_Y, SCREEN_W, STATUS_H, C_BLACK);
    HLine(0, STATUS_Y, SCREEN_W, C_BORDER);

    SDL_snprintf(tmp, sizeof(tmp), "LAST:%-12s  SCAN:0x%02X",
        last_key_name, last_scancode);
    DrawString(LOG_X, STATUS_Y + 2, tmp, C_CYAN, -1);
}

/* Event log */
static void DrawLog(void)
{
    int i;
    for (i = 0; i < LOG_LINES; i++) {
        int idx = (log_head + i) % LOG_LINES;
        int y   = LOG_Y + i * LOG_LINE_H;
        DrawString(LOG_X, y, log_buf[idx], C_DGRAY, C_BLACK);
    }
    DrawStringCentered(0, SCREEN_H - 9, SCREEN_W, 9,
        "PRESS Q OR ESC TO QUIT", C_DGRAY, -1);
}

/* Full redraw */
static void Redraw(void)
{
    SetColor(C_BLACK);
    SDL_RenderClear(ren);

    DrawHeader();
    DrawKeyboard();
    DrawStatus();
    DrawLog();

    SDL_RenderPresent(ren);
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    SDL_Window *win  = NULL;
    SDL_Event   event;
    int         running = 1;
    int         i;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    win = SDL_CreateWindow(
        "Atari ST Keyboard Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);

    /* Initialise log buffer */
    memset(key_state, 0, sizeof(key_state));
    memset(log_buf,  ' ', sizeof(log_buf));
    for (i = 0; i < LOG_LINES; i++) log_buf[i][LOG_BUF_W - 1] = '\0';
    LogPush("Press any key...");

    Redraw();

    /* ------------------------------------------------------------------
     * Event loop
     * ------------------------------------------------------------------ */
    while (running) {
        int dirty = 0;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {

            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN: {
                SDL_Scancode sc = event.key.keysym.scancode;
                const char  *kn = SDL_GetScancodeName(sc);
                char          tmp[LOG_BUF_W];

                /* Quit keys */
                if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_Q) {
                    running = 0;
                    break;
                }

                if (sc < MAX_SCAN) key_state[sc] = 1;

                SDL_snprintf(last_key_name, sizeof(last_key_name),
                    "%-11s", kn ? kn : "?");
                last_scancode = (int)sc;

                SDL_snprintf(tmp, sizeof(tmp),
                    "DN %-12s sc=0x%02X sym=0x%04X",
                    kn ? kn : "?",
                    (unsigned)sc,
                    (unsigned)event.key.keysym.sym);
                LogPush(tmp);
                dirty = 1;
                break;
            }

            case SDL_KEYUP: {
                SDL_Scancode sc = event.key.keysym.scancode;
                const char  *kn = SDL_GetScancodeName(sc);
                char          tmp[LOG_BUF_W];

                if (sc < MAX_SCAN) key_state[sc] = 0;

                SDL_snprintf(tmp, sizeof(tmp),
                    "UP %-12s sc=0x%02X",
                    kn ? kn : "?",
                    (unsigned)sc);
                LogPush(tmp);
                dirty = 1;
                break;
            }

            default:
                break;
            }
        }

        if (dirty) Redraw();

        SDL_Delay(16); /* ~60 Hz */
    }

    /* ------------------------------------------------------------------
     * Cleanup
     * ------------------------------------------------------------------ */
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
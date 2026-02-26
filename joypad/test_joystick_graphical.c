/*
 * joytst_atari.c
 *
 * SDL2 graphical joystick test for Atari ST/STE/TT/Falcon.
 * Targets the SDL_ATARI_JoystickDriver: 0 axes, 1 hat, 1 button (fire).
 *
 * Resolution: 320x200 (Atari ST low)
 * Palette:    16 colours (EGA-like, safe on all Atari video modes)
 *
 * Build:
 *   m68k-atari-mint-gcc -o joytst.tos joytst_atari.c \
 *       -I/path/to/sdl2/include -L/path/to/sdl2/lib -lSDL2
 *
 * Layout (320x200):
 *
 *   ┌─────────────────────────────────────┐
 *   │ ATARI JOYSTICK TEST           [VER] │  <- header bar
 *   ├──────────────────┬──────────────────┤
 *   │                  │                  │
 *   │   D-PAD / HAT    │   FIRE BUTTON    │
 *   │   (cross view)   │   (big circle)   │
 *   │                  │                  │
 *   ├──────────────────┴──────────────────┤
 *   │ RAW: 0x00   HAT: CENTER   FIRE: 0   │  <- status bar
 *   │ [event log line 1]                  │
 *   │ [event log line 2]                  │
 *   │ [event log line 3]                  │
 *   │              PRESS Q TO QUIT        │
 *   └─────────────────────────────────────┘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL2/SDL.h"

/* -----------------------------------------------------------------------
 * Geometry — tuned for 320x200
 * ----------------------------------------------------------------------- */
#define SCREEN_W   320
#define SCREEN_H   200

/* Header */
#define HDR_H       14

/* Two main panels side by side */
#define PANEL_Y     (HDR_H + 1)
#define PANEL_H     118
#define LEFT_X      0
#define LEFT_W      160
#define RIGHT_X     160
#define RIGHT_W     160

/* Status / log area */
#define STATUS_Y    (PANEL_Y + PANEL_H + 1)
#define STATUS_H    (SCREEN_H - STATUS_Y)

/* D-pad cross: 3x3 cells, each cell 20x20, centred in left panel */
#define CELL        20
#define CROSS_X     (LEFT_X  + (LEFT_W  - CELL*3) / 2)
#define CROSS_Y     (PANEL_Y + (PANEL_H - CELL*3) / 2)

/* Fire circle centre, radius */
#define FIRE_CX     (RIGHT_X + RIGHT_W / 2)
#define FIRE_CY     (PANEL_Y + PANEL_H  / 2)
#define FIRE_R      28

/* Log: up to 3 lines */
#define LOG_LINES   3
#define LOG_LINE_H  10
#define LOG_X       4
#define LOG_Y       (STATUS_Y + 12)

/* -----------------------------------------------------------------------
 * Colours (Atari ST low palette, 16 colours, 9-bit RGB 0RGB nybble)
 * We store as SDL RGBA for the renderer.
 * ----------------------------------------------------------------------- */
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
    /* 0 BLACK  */ {   0,   0,   0, 255 },
    /* 1 WHITE  */ { 255, 255, 255, 255 },
    /* 2 GRAY   */ { 170, 170, 170, 255 },
    /* 3 DGRAY  */ {  85,  85,  85, 255 },
    /* 4 RED    */ { 255,  85,  85, 255 },
    /* 5 DRED   */ { 170,   0,   0, 255 },
    /* 6 GREEN  */ {  85, 255,  85, 255 },
    /* 7 DGREEN */ {   0, 170,   0, 255 },
    /* 8 BLUE   */ {  85,  85, 255, 255 },
    /* 9 DBLUE  */ {   0,   0, 170, 255 },
    /* 10 CYAN  */ {  85, 255, 255, 255 },
    /* 11 YELL  */ { 255, 255,  85, 255 },
    /* 12 ORNG  */ { 255, 170,   0, 255 },
    /* 13 PANEL */ {   0,  42,  85, 255 },
    /* 14 HILIT */ {   0, 128, 255, 255 },
    /* 15 BORD  */ {   0,  85, 128, 255 },
};

/* -----------------------------------------------------------------------
 * Tiny 5x7 bitmap font  (ASCII 32..127)
 * Each glyph: 5 bytes, each byte = one row of 5 pixels (MSB left)
 * ----------------------------------------------------------------------- */
/* -----------------------------------------------------------------------
 * Drawing helpers
 * ----------------------------------------------------------------------- */
static SDL_Renderer *ren = NULL;

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

static void VLine(int x, int y, int len, int c)
{
    SetColor(c);
    SDL_RenderDrawLine(ren, x, y, x, y + len - 1);
}

/* Filled circle (midpoint) */
static void FillCircle(int cx, int cy, int r, int c)
{
    int x = 0, y = r, d = 1 - r;
    SetColor(c);
    while (x <= y) {
        SDL_RenderDrawLine(ren, cx - x, cy + y, cx + x, cy + y);
        SDL_RenderDrawLine(ren, cx - x, cy - y, cx + x, cy - y);
        SDL_RenderDrawLine(ren, cx - y, cy + x, cx + y, cy + x);
        SDL_RenderDrawLine(ren, cx - y, cy - x, cx + y, cy - x);
        if (d < 0) d += 2 * x + 3;
        else     { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

/* Circle outline */
static void DrawCircle(int cx, int cy, int r, int c)
{
    int x = 0, y = r, d = 1 - r;
    SetColor(c);
    while (x <= y) {
        SDL_RenderDrawPoint(ren, cx+x, cy+y); SDL_RenderDrawPoint(ren, cx-x, cy+y);
        SDL_RenderDrawPoint(ren, cx+x, cy-y); SDL_RenderDrawPoint(ren, cx-x, cy-y);
        SDL_RenderDrawPoint(ren, cx+y, cy+x); SDL_RenderDrawPoint(ren, cx-y, cy+x);
        SDL_RenderDrawPoint(ren, cx+y, cy-x); SDL_RenderDrawPoint(ren, cx-y, cy-x);
        if (d < 0) d += 2 * x + 3;
        else     { d += 2 * (x - y) + 5; y--; }
        x++;
    }
}

/* -----------------------------------------------------------------------
 * 5×7 font — embedded directly to avoid extra file dependency
 * 96 chars (ASCII 32-127), each glyph 5 bytes (rows top-to-bottom, MSB=left)
 * ----------------------------------------------------------------------- */
static const Uint8 font_data[96][5] = {
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
    {0x3C,0x40,0x40,0x40,0x7C}, /* 117 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 '}' */
    {0x08,0x04,0x08,0x10,0x08}, /* 126 '~' */
    {0x00,0x00,0x00,0x00,0x00}, /* 127 DEL */
};

/* Draw a single character at pixel (x,y), scale 1 or 2 */
static void DrawChar(int x, int y, char ch, int fg, int bg, int scale)
{
    int col, row, bit;
    Uint8 rowbits;
    const Uint8 *glyph;

    if (ch < 32 || ch > 127) ch = '?';
    glyph = font_data[(unsigned char)ch - 32];

    if (bg >= 0)
        FillRect(x, y, 5 * scale, 7 * scale, bg);

    SetColor(fg);
for (col = 0; col < 5; col++) {
    rowbits = glyph[col];
    for (row = 0; row < 7; row++) {
        bit = (rowbits >> row) & 1;
            if (bit) {
                if (scale == 1)
                    SDL_RenderDrawPoint(ren, x + col, y + row);
                else
                    SDL_RenderFillRect(ren, &(SDL_Rect){ x + col*scale, y + row*scale, scale, scale });
            }
        }
    }
}

/* Draw a string, returns x after last char */
static int DrawString(int x, int y, const char *s, int fg, int bg, int scale)
{
    while (*s) {
        DrawChar(x, y, *s++, fg, bg, scale);
        x += (5 + 1) * scale;
    }
    return x;
}

/* Centred string in a rect */
static void DrawStringCentered(int rx, int ry, int rw, int rh,
                                const char *s, int fg, int bg)
{
    int len = (int)strlen(s);
    int tw  = len * 6;   /* 5px + 1px gap */
    int tx  = rx + (rw - tw) / 2;
    int ty  = ry + (rh - 7)  / 2;
    DrawString(tx, ty, s, fg, bg, 1);
}

/* -----------------------------------------------------------------------
 * Hat name helper (mirrors HatName() from joytst5.c)
 * ----------------------------------------------------------------------- */
static const char *HatName(Uint8 hat)
{
    switch (hat) {
        case SDL_HAT_CENTERED:  return "CENTER";
        case SDL_HAT_UP:        return "UP";
        case SDL_HAT_DOWN:      return "DOWN";
        case SDL_HAT_LEFT:      return "LEFT";
        case SDL_HAT_RIGHT:     return "RIGHT";
        case SDL_HAT_LEFTUP:    return "LEFT+UP";
        case SDL_HAT_RIGHTUP:   return "RIGHT+UP";
        case SDL_HAT_LEFTDOWN:  return "LEFT+DOWN";
        case SDL_HAT_RIGHTDOWN: return "RIGHT+DOWN";
        default:                return "?";
    }
}

/* -----------------------------------------------------------------------
 * Event log — ring buffer, LOG_LINES entries
 * ----------------------------------------------------------------------- */
static char log_buf[LOG_LINES][48];
static int  log_head = 0;  /* index of oldest line (or next to write) */

static void LogPush(const char *msg)
{
    SDL_strlcpy(log_buf[log_head % LOG_LINES], msg, sizeof(log_buf[0]));
    log_head++;
}

/* -----------------------------------------------------------------------
 * State
 * ----------------------------------------------------------------------- */
typedef struct {
    Uint8  hat;
    Uint8  fire;
    Uint8  raw;         /* reconstructed raw byte for display */
} JoyState;

static JoyState joy = { SDL_HAT_CENTERED, 0, 0 };

/* -----------------------------------------------------------------------
 * Drawing routines
 * ----------------------------------------------------------------------- */

/* Header bar */
static void DrawHeader(void)
{
    FillRect(0, 0, SCREEN_W, HDR_H, C_DBLUE);
    HLine(0, HDR_H, SCREEN_W, C_BORDER);
    DrawStringCentered(0, 0, SCREEN_W, HDR_H,
        "ATARI JOYSTICK DRIVER TEST", C_WHITE, -1);
    DrawString(SCREEN_W - 30, 4, "v5", C_CYAN, -1, 1);
}

/* Divider between left and right panels */
static void DrawPanelBorders(void)
{
    /* Outer border */
    DrawRect(0, PANEL_Y, SCREEN_W, PANEL_H, C_BORDER);
    /* Vertical divider */
    VLine(LEFT_W, PANEL_Y, PANEL_H, C_BORDER);
    /* Panel titles */
    DrawStringCentered(LEFT_X,  PANEL_Y, LEFT_W,  8, "D-PAD / HAT",  C_CYAN,  -1);
    DrawStringCentered(RIGHT_X, PANEL_Y, RIGHT_W, 8, "FIRE BUTTON",  C_CYAN,  -1);
}

/*
 * D-pad cross  — 3×3 grid, centre cell is empty, corners are empty.
 * Lit cells: UP(0,1) DOWN(2,1) LEFT(1,0) RIGHT(1,2)
 * Diagonals: UL(0,0) UR(0,2) DL(2,0) DR(2,2)
 */
static void DrawDpad(void)
{
    /* which directions are active? */
    int up    = (joy.hat & SDL_HAT_UP)    != 0;
    int down  = (joy.hat & SDL_HAT_DOWN)  != 0;
    int left  = (joy.hat & SDL_HAT_LEFT)  != 0;
    int right = (joy.hat & SDL_HAT_RIGHT) != 0;

    /* cell colours: active = bright green, inactive = dark panel */
    static const struct { int row; int col; int *flag; const char *label; } cells[] = {
        { 0, 1, NULL, "^"  },   /* UP    */
        { 1, 0, NULL, "<"  },   /* LEFT  */
        { 1, 2, NULL, ">"  },   /* RIGHT */
        { 2, 1, NULL, "v"  },   /* DOWN  */
    };

    int flags[4];
    int i, cx, cy, bg, fg;
    flags[0] = up; flags[1] = left; flags[2] = right; flags[3] = down;

    /* Background cross shape (clear area) */
    /* Column stripe */
    FillRect(CROSS_X + CELL, CROSS_Y, CELL, CELL*3, C_DGRAY);
    /* Row stripe */
    FillRect(CROSS_X, CROSS_Y + CELL, CELL*3, CELL, C_DGRAY);

    for (i = 0; i < 4; i++) {
        cx = CROSS_X + cells[i].col * CELL;
        cy = CROSS_Y + cells[i].row * CELL;
        bg = flags[i] ? C_GREEN  : C_DGRAY;
        fg = flags[i] ? C_BLACK  : C_GRAY;
        FillRect(cx + 1, cy + 1, CELL - 2, CELL - 2, bg);
        DrawRect (cx,     cy,     CELL,     CELL,     flags[i] ? C_GREEN : C_BORDER);
        DrawStringCentered(cx, cy, CELL, CELL, cells[i].label, fg, -1);
    }

    /* Centre dot (shows centred status) */
    {
        int centred = (joy.hat == SDL_HAT_CENTERED);
        int ccx = CROSS_X + CELL + CELL/2;
        int ccy = CROSS_Y + CELL + CELL/2;
        FillCircle(ccx, ccy, 5, centred ? C_GRAY : C_DGRAY);
        DrawCircle(ccx, ccy, 5, C_BORDER);
    }

    /* Diagonal indicators (tiny dots at corners) */
    {
        struct { int r; int c; int mask; } diag[] = {
            {0,0, SDL_HAT_LEFTUP},   {0,2, SDL_HAT_RIGHTUP},
            {2,0, SDL_HAT_LEFTDOWN}, {2,2, SDL_HAT_RIGHTDOWN}
        };
        int d;
        for (d = 0; d < 4; d++) {
            int dcx = CROSS_X + diag[d].c * CELL + CELL/2;
            int dcy = CROSS_Y + diag[d].r * CELL + CELL/2;
            int active = (joy.hat == diag[d].mask);
            FillCircle(dcx, dcy, 4, active ? C_YELLOW : C_BLACK);
        }
    }

    /* Hat name below the cross */
    {
        char tmp[24];
        SDL_snprintf(tmp, sizeof(tmp), "%-10s", HatName(joy.hat));
        DrawStringCentered(LEFT_X, CROSS_Y + CELL*3 + 4, LEFT_W, 10, tmp, C_YELLOW, -1);
    }
}

/* Fire button */
static void DrawFireButton(void)
{
    int outer_col = joy.fire ? C_RED   : C_DRED;
    int inner_col = joy.fire ? C_RED   : C_PANEL;
    int label_col = joy.fire ? C_BLACK : C_RED;

    /* Outer glow ring when pressed */
    if (joy.fire) {
        FillCircle(FIRE_CX, FIRE_CY, FIRE_R + 6, C_DRED);
    }
    FillCircle (FIRE_CX, FIRE_CY, FIRE_R,     inner_col);
    DrawCircle (FIRE_CX, FIRE_CY, FIRE_R,     outer_col);
    DrawCircle (FIRE_CX, FIRE_CY, FIRE_R - 2, outer_col);

    DrawStringCentered(FIRE_CX - FIRE_R, FIRE_CY - 4,
                       FIRE_R * 2,       8,
                       joy.fire ? "FIRE!" : "FIRE",
                       label_col, -1);

    /* Press count area */
    {
        static int count = 0;
        static Uint8 prev_fire = 0;
        char tmp[16];
        if (joy.fire && !prev_fire) count++;
        prev_fire = joy.fire;
        SDL_snprintf(tmp, sizeof(tmp), "CNT:%d", count);
        DrawStringCentered(RIGHT_X, PANEL_Y + PANEL_H - 14, RIGHT_W, 10, tmp, C_GRAY, -1);
    }
}

/* Status bar */
static void DrawStatus(void)
{
    char tmp[64];
    FillRect(0, STATUS_Y, SCREEN_W, STATUS_H, C_BLACK);
    HLine(0, STATUS_Y, SCREEN_W, C_BORDER);

    SDL_snprintf(tmp, sizeof(tmp), "RAW:0x%02X  HAT:%-10s FIRE:%d",
        (unsigned)joy.raw, HatName(joy.hat), (int)joy.fire);
    DrawString(LOG_X, STATUS_Y + 2, tmp, C_CYAN, -1, 1);
}

/* Event log */
static void DrawLog(void)
{
    int i, y;
    for (i = 0; i < LOG_LINES; i++) {
        /* Oldest first: log_head points to next slot to overwrite */
        int idx = (log_head + i) % LOG_LINES;
        y = LOG_Y + i * LOG_LINE_H;
        DrawString(LOG_X, y, log_buf[idx], C_DGRAY, C_BLACK, 1);
    }
    /* Quit hint */
    DrawStringCentered(0, SCREEN_H - 10, SCREEN_W, 10,
        "PRESS Q OR ESC TO QUIT", C_DGRAY, -1);
}

/* -----------------------------------------------------------------------
 * Full redraw
 * ----------------------------------------------------------------------- */
static void Redraw(void)
{
    SetColor(C_BLACK);
    SDL_RenderClear(ren);

    DrawHeader();
    DrawPanelBorders();
    DrawDpad();
    DrawFireButton();
    DrawStatus();
    DrawLog();

    SDL_RenderPresent(ren);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    SDL_Window   *win  = NULL;
    SDL_Joystick *joy_dev = NULL;
    SDL_Event     event;
    int           running = 1;
    int           n, i;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    /* Create 320×200 window — Atari ST low resolution */
    win = SDL_CreateWindow(
        "Joystick Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_SOFTWARE);   /* SW renderer — no GPU assumed on Atari */
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);

    /* Open first joystick — Atari driver exposes exactly one */
    n = SDL_NumJoysticks();
    if (n == 0) {
        fprintf(stderr, "No joystick found. Is ATARI driver active?\n");
    } else {
        for (i = 0; i < n; i++) {
            joy_dev = SDL_JoystickOpen(i);
            if (joy_dev) break;
        }
        if (!joy_dev)
            fprintf(stderr, "SDL_JoystickOpen: %s\n", SDL_GetError());
        else
            fprintf(stderr, "Opened: %s\n", SDL_JoystickName(joy_dev));
    }

    /* Initialise log buffer */
    memset(log_buf, ' ', sizeof(log_buf));
    for (i = 0; i < LOG_LINES; i++) log_buf[i][47] = '\0';
    LogPush("Driver ready. Move joystick.");

    Redraw();

    /* ----------------------------------------------------------------
     * Event loop
     * ---------------------------------------------------------------- */
    while (running) {
        int dirty = 0;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {

            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE ||
                    event.key.keysym.sym == SDLK_q) {
                    running = 0;
                }
                break;

            /* ---- ATARI driver: 0 axes, 1 hat, 1 button ------------ */

            case SDL_JOYHATMOTION:
                if (joy_dev &&
                    event.jhat.which == SDL_JoystickInstanceID(joy_dev) &&
                    event.jhat.hat   == 0) {

                    char tmp[48];
                    Uint8 prev = joy.hat;
                    joy.hat = event.jhat.value;

                    /* Reconstruct raw byte (driver bit layout) */
                    joy.raw = 0;
                    if (joy.hat & SDL_HAT_UP)    joy.raw |= (1<<0);
                    if (joy.hat & SDL_HAT_DOWN)  joy.raw |= (1<<1);
                    if (joy.hat & SDL_HAT_LEFT)  joy.raw |= (1<<2);
                    if (joy.hat & SDL_HAT_RIGHT) joy.raw |= (1<<3);
                    if (joy.fire)                joy.raw |= (1<<7);

                    SDL_snprintf(tmp, sizeof(tmp),
                        "HAT: %-9s -> %-9s",
                        HatName(prev), HatName(joy.hat));
                    LogPush(tmp);
                    dirty = 1;
                }
                break;

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                if (joy_dev &&
                    event.jbutton.which  == SDL_JoystickInstanceID(joy_dev) &&
                    event.jbutton.button == 0) {

                    char tmp[48];
                    joy.fire = (event.jbutton.state == SDL_PRESSED) ? 1 : 0;

                    joy.raw &= ~(1<<7);
                    if (joy.fire) joy.raw |= (1<<7);

                    SDL_snprintf(tmp, sizeof(tmp),
                        "FIRE: %s  raw=0x%02X",
                        joy.fire ? "PRESSED " : "released",
                        (unsigned)joy.raw);
                    LogPush(tmp);
                    dirty = 1;
                }
                break;

            case SDL_JOYDEVICEADDED:
                LogPush("Joystick connected");
                if (!joy_dev) {
                    joy_dev = SDL_JoystickOpen(event.jdevice.which);
                }
                dirty = 1;
                break;

            case SDL_JOYDEVICEREMOVED:
                LogPush("Joystick removed!");
                if (joy_dev &&
                    event.jdevice.which == SDL_JoystickInstanceID(joy_dev)) {
                    SDL_JoystickClose(joy_dev);
                    joy_dev = NULL;
                    joy.hat  = SDL_HAT_CENTERED;
                    joy.fire = 0;
                    joy.raw  = 0;
                }
                dirty = 1;
                break;

            default:
                break;
            }
        }

        if (dirty)
            Redraw();

        SDL_Delay(16);   /* ~60 Hz */
    }

    /* ----------------------------------------------------------------
     * Cleanup
     * ---------------------------------------------------------------- */
    if (joy_dev) SDL_JoystickClose(joy_dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
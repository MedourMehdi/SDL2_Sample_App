/*
 * sdl2_atari_keytst.c  — v1.1 (resizable)
 *
 * SDL2 graphical keyboard test for Atari ST / MiNT.
 * The window is resizable: all geometry and font scale automatically.
 *
 * Build (m68k cross-compiler):
 *   m68k-atari-mint-gcc -o keytst.tos sdl2_atari_keytst.c \
 *       -I/path/to/sdl2/include -L/path/to/sdl2/lib -lSDL2
 *
 * Build (host, for testing):
 *   gcc -o keytst sdl2_atari_keytst.c $(sdl2-config --cflags --libs)
 *
 * Scaling model
 * -------------
 * All positions / sizes live in a fixed 320×200 "virtual" space.
 * g_sx = win_w / 320.0,  g_sy = win_h / 200.0.
 * SX/SY/SW/SH macros convert virtual → screen coords at draw time.
 * Font "pixels" are g_fw × g_fw blocks, where g_fw = floor(min(g_sx,g_sy)).
 * On resize: UpdateScale() → Redraw().  No SDL_RenderSetLogicalSize used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL2/SDL.h"

/* -------------------------------------------------------------------------
 * Virtual coordinate space
 * ------------------------------------------------------------------------- */
#define BASE_W  320
#define BASE_H  200
#define VERSION "v1.1"

#define V_HDR_H     12
#define V_KB_Y      (V_HDR_H + 2)
#define V_KB_H      138
#define V_STATUS_Y  (V_KB_Y + V_KB_H)
#define V_STATUS_H  (BASE_H - V_STATUS_Y)
#define LOG_LINES   2
#define V_LOG_LINE  9
#define V_LOG_X     3
#define V_LOG_Y     (V_STATUS_Y + 12)
#define V_KEY_H     12
#define KEY_GAP     1

/* -------------------------------------------------------------------------
 * Scale globals — updated on every resize
 * ------------------------------------------------------------------------- */
static float g_sx = 1.0f;
static float g_sy = 1.0f;
static int   g_fw = 1;       /* font block size in screen pixels */

#define SX(vx)  ((int)((vx) * g_sx + 0.5f))
#define SY(vy)  ((int)((vy) * g_sy + 0.5f))
#define SW(vw)  ((int)((vw) * g_sx + 0.5f))
#define SH(vh)  ((int)((vh) * g_sy + 0.5f))

static void UpdateScale(int ww, int wh)
{
    float mn;
    g_sx = (float)ww / (float)BASE_W;
    g_sy = (float)wh / (float)BASE_H;
    mn   = (g_sx < g_sy) ? g_sx : g_sy;
    g_fw = (int)mn;
    if (g_fw < 1) g_fw = 1;
}

/* -------------------------------------------------------------------------
 * Palette
 * ------------------------------------------------------------------------- */
#define C_BLACK   0
#define C_WHITE   1
#define C_GRAY    2
#define C_DGRAY   3
#define C_RED     4
#define C_DRED    5
#define C_GREEN   6
#define C_DGREEN  7
#define C_BLUE    8
#define C_DBLUE   9
#define C_CYAN   10
#define C_YELLOW 11
#define C_ORANGE 12
#define C_PANEL  13
#define C_HILITE 14
#define C_BORDER 15

static const SDL_Color palette[] = {
    {   0,   0,   0, 255 },
    { 255, 255, 255, 255 },
    { 170, 170, 170, 255 },
    {  85,  85,  85, 255 },
    { 255,  85,  85, 255 },
    { 170,   0,   0, 255 },
    {  85, 255,  85, 255 },
    {   0, 170,   0, 255 },
    {  85,  85, 255, 255 },
    {   0,   0, 170, 255 },
    {  85, 255, 255, 255 },
    { 255, 255,  85, 255 },
    { 255, 170,   0, 255 },
    {   0,  42,  85, 255 },
    {   0, 128, 255, 255 },
    {   0,  85, 128, 255 },
};

/* -------------------------------------------------------------------------
 * 5×7 font — bit0 of each column byte = top row (corrected order)
 * ------------------------------------------------------------------------- */
static const Uint8 font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x30,0x30,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},{0x0C,0x02,0x0C,0x10,0x0C},{0x7F,0x41,0x41,0x41,0x7F},
};

/* -------------------------------------------------------------------------
 * Renderer
 * ------------------------------------------------------------------------- */
static SDL_Renderer *ren = NULL;

/* -------------------------------------------------------------------------
 * Draw primitives — all coords/sizes are VIRTUAL
 * ------------------------------------------------------------------------- */
static void SetColor(int c)
{
    SDL_SetRenderDrawColor(ren,
        palette[c].r, palette[c].g, palette[c].b, palette[c].a);
}

static void FillRect(int vx, int vy, int vw, int vh, int c)
{
    SDL_Rect r = { SX(vx), SY(vy), SW(vw), SH(vh) };
    SetColor(c); SDL_RenderFillRect(ren, &r);
}

static void DrawRect(int vx, int vy, int vw, int vh, int c)
{
    SDL_Rect r = { SX(vx), SY(vy), SW(vw), SH(vh) };
    SetColor(c); SDL_RenderDrawRect(ren, &r);
}

static void HLine(int vx, int vy, int vlen, int c)
{
    SetColor(c);
    SDL_RenderDrawLine(ren, SX(vx), SY(vy), SX(vx+vlen-1), SY(vy));
}

/* -------------------------------------------------------------------------
 * Font rendering
 * Each glyph column-byte bit0 = top row.
 * Screen position: absolute screen px (we compute SX/SY outside and step).
 * Advance per glyph: 6 * g_fw  screen pixels.
 * Glyph height:      7 * g_fw  screen pixels.
 * ------------------------------------------------------------------------- */

/* Draw one glyph at screen position (spx, spy). */
static void DrawGlyph(int spx, int spy, char ch, int fg, int bg)
{
    int col, row;
    if (ch < 32 || ch > 127) ch = '?';
    for (col = 0; col < 5; col++) {
        Uint8 bits = font5x7[(unsigned char)ch - 32][col];
        for (row = 0; row < 7; row++) {
            int lit = (bits >> row) & 1;
            SDL_Rect r = { spx + col*g_fw, spy + row*g_fw, g_fw, g_fw };
            if (lit)      { SetColor(fg); SDL_RenderFillRect(ren, &r); }
            else if (bg>=0){ SetColor(bg); SDL_RenderFillRect(ren, &r); }
        }
    }
}

/* Draw string at VIRTUAL (vx,vy). Returns screen pixels consumed. */
static int DrawString(int vx, int vy, const char *s, int fg, int bg)
{
    int spx = SX(vx), spy = SY(vy), sx0 = spx;
    while (*s) { DrawGlyph(spx, spy, *s, fg, bg); spx += 6*g_fw; s++; }
    return spx - sx0;
}

/* Draw string centred inside virtual box (vbx,vby,vbw,vbh). */
static void DrawStringCentered(int vbx, int vby, int vbw, int vbh,
                                const char *s, int fg, int bg)
{
    int len    = (int)strlen(s);
    int str_px = len * 6 * g_fw;
    int box_px = SW(vbw);
    int box_py = SH(vbh);
    int spx    = SX(vbx) + (box_px - str_px) / 2;
    int spy    = SY(vby) + (box_py - 7*g_fw) / 2;
    while (*s) { DrawGlyph(spx, spy, *s, fg, bg); spx += 6*g_fw; s++; }
}

/* -------------------------------------------------------------------------
 * Key state
 * ------------------------------------------------------------------------- */
#define MAX_SCAN 512
static Uint8 key_state[MAX_SCAN];

/* -------------------------------------------------------------------------
 * Key layout — VIRTUAL coordinates
 * ------------------------------------------------------------------------- */
typedef struct {
    int          vx, vy, vw, vh;
    const char  *label;
    SDL_Scancode scan;
} KeyDef;

#define KH   V_KEY_H   /* 12 */
#define KY_FN  2
#define KY_0  17
#define KY_1  30
#define KY_2  43
#define KY_3  56
#define KY_4  69

static const KeyDef keys[] = {
    /* Fn row */
    {  2,KY_FN,27,KH,"F1", SDL_SCANCODE_F1 },{ 31,KY_FN,27,KH,"F2", SDL_SCANCODE_F2 },
    { 60,KY_FN,27,KH,"F3", SDL_SCANCODE_F3 },{ 89,KY_FN,27,KH,"F4", SDL_SCANCODE_F4 },
    {118,KY_FN,27,KH,"F5", SDL_SCANCODE_F5 },{147,KY_FN,27,KH,"F6", SDL_SCANCODE_F6 },
    {176,KY_FN,27,KH,"F7", SDL_SCANCODE_F7 },{205,KY_FN,27,KH,"F8", SDL_SCANCODE_F8 },
    {234,KY_FN,27,KH,"F9", SDL_SCANCODE_F9 },{263,KY_FN,27,KH,"F10",SDL_SCANCODE_F10},

    /* Row 0 */
    {  2,KY_0,19,KH,"ESC", SDL_SCANCODE_ESCAPE   },{ 23,KY_0,13,KH,"1",  SDL_SCANCODE_1        },
    { 38,KY_0,13,KH,"2",   SDL_SCANCODE_2        },{ 53,KY_0,13,KH,"3",  SDL_SCANCODE_3        },
    { 68,KY_0,13,KH,"4",   SDL_SCANCODE_4        },{ 83,KY_0,13,KH,"5",  SDL_SCANCODE_5        },
    { 98,KY_0,13,KH,"6",   SDL_SCANCODE_6        },{113,KY_0,13,KH,"7",  SDL_SCANCODE_7        },
    {128,KY_0,13,KH,"8",   SDL_SCANCODE_8        },{143,KY_0,13,KH,"9",  SDL_SCANCODE_9        },
    {158,KY_0,13,KH,"0",   SDL_SCANCODE_0        },{173,KY_0,13,KH,"-",  SDL_SCANCODE_MINUS    },
    {188,KY_0,13,KH,"=",   SDL_SCANCODE_EQUALS   },{203,KY_0,29,KH,"BKSP",SDL_SCANCODE_BACKSPACE},

    /* Row 1 */
    {  2,KY_1,19,KH,"TAB", SDL_SCANCODE_TAB         },{ 23,KY_1,13,KH,"Q",SDL_SCANCODE_Q           },
    { 38,KY_1,13,KH,"W",   SDL_SCANCODE_W           },{ 53,KY_1,13,KH,"E",SDL_SCANCODE_E           },
    { 68,KY_1,13,KH,"R",   SDL_SCANCODE_R           },{ 83,KY_1,13,KH,"T",SDL_SCANCODE_T           },
    { 98,KY_1,13,KH,"Y",   SDL_SCANCODE_Y           },{113,KY_1,13,KH,"U",SDL_SCANCODE_U           },
    {128,KY_1,13,KH,"I",   SDL_SCANCODE_I           },{143,KY_1,13,KH,"O",SDL_SCANCODE_O           },
    {158,KY_1,13,KH,"P",   SDL_SCANCODE_P           },{173,KY_1,13,KH,"[",SDL_SCANCODE_LEFTBRACKET },
    {188,KY_1,13,KH,"]",   SDL_SCANCODE_RIGHTBRACKET},
    /* Tall Return spans row1+row2 */
    {203,KY_1,29,KH*2+KEY_GAP,"RET",SDL_SCANCODE_RETURN},

    /* Row 2 */
    {  2,KY_2,22,KH,"CTL",SDL_SCANCODE_LCTRL     },{ 26,KY_2,13,KH,"A",SDL_SCANCODE_A         },
    { 41,KY_2,13,KH,"S",  SDL_SCANCODE_S         },{ 56,KY_2,13,KH,"D",SDL_SCANCODE_D         },
    { 71,KY_2,13,KH,"F",  SDL_SCANCODE_F         },{ 86,KY_2,13,KH,"G",SDL_SCANCODE_G         },
    {101,KY_2,13,KH,"H",  SDL_SCANCODE_H         },{116,KY_2,13,KH,"J",SDL_SCANCODE_J         },
    {131,KY_2,13,KH,"K",  SDL_SCANCODE_K         },{146,KY_2,13,KH,"L",SDL_SCANCODE_L         },
    {161,KY_2,13,KH,";",  SDL_SCANCODE_SEMICOLON },{176,KY_2,13,KH,"'",SDL_SCANCODE_APOSTROPHE},
    {191,KY_2,11,KH,"`",  SDL_SCANCODE_GRAVE     },

    /* Row 3 */
    {  2,KY_3,26,KH,"SHF", SDL_SCANCODE_LSHIFT   },{ 30,KY_3,13,KH,"\\",SDL_SCANCODE_BACKSLASH},
    { 45,KY_3,13,KH,"Z",   SDL_SCANCODE_Z        },{ 60,KY_3,13,KH,"X",  SDL_SCANCODE_X       },
    { 75,KY_3,13,KH,"C",   SDL_SCANCODE_C        },{ 90,KY_3,13,KH,"V",  SDL_SCANCODE_V       },
    {105,KY_3,13,KH,"B",   SDL_SCANCODE_B        },{120,KY_3,13,KH,"N",  SDL_SCANCODE_N       },
    {135,KY_3,13,KH,"M",   SDL_SCANCODE_M        },{150,KY_3,13,KH,",",  SDL_SCANCODE_COMMA   },
    {165,KY_3,13,KH,".",   SDL_SCANCODE_PERIOD   },{180,KY_3,13,KH,"/",  SDL_SCANCODE_SLASH   },
    {195,KY_3,37,KH,"SHFT",SDL_SCANCODE_RSHIFT   },

    /* Row 4 */
    {  2,KY_4,22,KH,"CAP",  SDL_SCANCODE_CAPSLOCK},{ 26,KY_4,22,KH,"ALT",SDL_SCANCODE_LALT   },
    { 50,KY_4,113,KH,"SPACE",SDL_SCANCODE_SPACE  },{165,KY_4,19,KH,"ALGR",SDL_SCANCODE_RALT  },
    {186,KY_4,19,KH,"DEL",  SDL_SCANCODE_DELETE  },{207,KY_4,25,KH,"HELP",SDL_SCANCODE_INSERT},

    /* Cursor cluster */
    {244,KY_3,19,KH," UP",SDL_SCANCODE_UP   },
    {235,KY_4,19,KH,"LFT",SDL_SCANCODE_LEFT },
    {256,KY_4,19,KH,"DWN",SDL_SCANCODE_DOWN },
    {277,KY_4,19,KH,"RGT",SDL_SCANCODE_RIGHT},
};
#define NUM_KEYS ((int)(sizeof(keys)/sizeof(keys[0])))

/* -------------------------------------------------------------------------
 * Event log
 * ------------------------------------------------------------------------- */
#define LOG_BUF_W 52
static char log_buf[LOG_LINES][LOG_BUF_W];
static int  log_head = 0;

static void LogPush(const char *msg)
{
    SDL_snprintf(log_buf[log_head], LOG_BUF_W, "%-*s", LOG_BUF_W-1, msg);
    log_head = (log_head + 1) % LOG_LINES;
}

static char last_key_name[32] = "---";
static int  last_scancode      = 0;

/* -------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------- */
static void DrawHeader(void)
{
    int ver_chars = (int)strlen(VERSION);
    FillRect(0, 0, BASE_W, V_HDR_H, C_PANEL);
    HLine(0, V_HDR_H, BASE_W, C_BORDER);
    DrawString(3, 3, "ATARI ST KEYBOARD TEST", C_CYAN,   -1);
    /* right-align version: each virtual glyph ≈ 6 units wide */
    DrawString(BASE_W - ver_chars*6 - 3, 3, VERSION,     C_YELLOW, -1);
}

static void DrawKey(const KeyDef *k)
{
    int pressed    = (k->scan < MAX_SCAN) ? key_state[k->scan] : 0;
    int vx = k->vx,  vy = V_KB_Y + k->vy;
    int vw = k->vw,  vh = k->vh;
    int body   = pressed ? C_HILITE : C_DGRAY;
    int border = pressed ? C_WHITE  : C_BORDER;
    int label  = pressed ? C_WHITE  : C_GRAY;

    /* 1-pixel drop shadow (real pixels — stays crisp at all scales) */
    if (!pressed) {
        SDL_Rect sh = { SX(vx)+1, SY(vy)+1, SW(vw)-1, SH(vh)-1 };
        SetColor(C_BLACK); SDL_RenderFillRect(ren, &sh);
    }
    FillRect(vx, vy, vw-1, vh-1, body);
    DrawRect(vx, vy, vw-1, vh-1, border);

    /* Top/left bevel highlight */
    if (!pressed) {
        SetColor(C_GRAY);
        SDL_RenderDrawLine(ren, SX(vx), SY(vy), SX(vx+vw-1), SY(vy));
        SDL_RenderDrawLine(ren, SX(vx), SY(vy), SX(vx),      SY(vy+vh-1));
    }
    DrawStringCentered(vx, vy, vw-1, vh-1, k->label, label, -1);
}

static void DrawKeyboard(void)
{
    int i;
    FillRect(0, V_KB_Y, BASE_W, V_KB_H, C_BLACK);
    for (i = 0; i < NUM_KEYS; i++) DrawKey(&keys[i]);
}

static void DrawStatus(void)
{
    char tmp[64];
    FillRect(0, V_STATUS_Y, BASE_W, V_STATUS_H, C_BLACK);
    HLine(0,  V_STATUS_Y, BASE_W, C_BORDER);
    SDL_snprintf(tmp, sizeof(tmp), "LAST:%-12s  SCAN:0x%02X",
        last_key_name, last_scancode);
    DrawString(V_LOG_X, V_STATUS_Y + 2, tmp, C_CYAN, -1);
}

static void DrawLog(void)
{
    int i;
    for (i = 0; i < LOG_LINES; i++) {
        int idx = (log_head + i) % LOG_LINES;
        DrawString(V_LOG_X, V_LOG_Y + i*V_LOG_LINE, log_buf[idx], C_DGRAY, C_BLACK);
    }
    DrawStringCentered(0, BASE_H-9, BASE_W, 9,
        "PRESS Q OR ESC TO QUIT", C_DGRAY, -1);
}

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
    SDL_Window *win = NULL;
    SDL_Event   ev;
    int running = 1, i;
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    win = SDL_CreateWindow(
        "Atari ST Keyboard Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        BASE_W, BASE_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError()); SDL_Quit(); return 1; }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());
                SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    /* No SDL_RenderSetLogicalSize — manual scaling */
    { int ww,wh; SDL_GetWindowSize(win,&ww,&wh); UpdateScale(ww,wh); }

    memset(key_state,0,sizeof(key_state));
    memset(log_buf,' ',sizeof(log_buf));
    for (i=0;i<LOG_LINES;i++) log_buf[i][LOG_BUF_W-1]='\0';
    LogPush("Press any key...");
    Redraw();

    while (running) {
        int dirty = 0;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running=0; break;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    UpdateScale(ev.window.data1, ev.window.data2);
                    dirty = 1;
                }
                break;

            case SDL_KEYDOWN: {
                SDL_Scancode sc = ev.key.keysym.scancode;
                const char  *kn = SDL_GetScancodeName(sc);
                char tmp[LOG_BUF_W];
                if (sc==SDL_SCANCODE_ESCAPE||sc==SDL_SCANCODE_Q){running=0;break;}
                if (sc<MAX_SCAN) key_state[sc]=1;
                SDL_snprintf(last_key_name,sizeof(last_key_name),"%-11s",kn?kn:"?");
                last_scancode=(int)sc;
                SDL_snprintf(tmp,sizeof(tmp),"DN %-12s sc=0x%02X sym=0x%04X",
                    kn?kn:"?",(unsigned)sc,(unsigned)ev.key.keysym.sym);
                LogPush(tmp); dirty=1; break;
            }
            case SDL_KEYUP: {
                SDL_Scancode sc = ev.key.keysym.scancode;
                const char  *kn = SDL_GetScancodeName(sc);
                char tmp[LOG_BUF_W];
                if (sc<MAX_SCAN) key_state[sc]=0;
                SDL_snprintf(tmp,sizeof(tmp),"UP %-12s sc=0x%02X",kn?kn:"?",(unsigned)sc);
                LogPush(tmp); dirty=1; break;
            }
            default: break;
            }
        }
        if (dirty) Redraw();
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
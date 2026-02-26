/*
 * sdl2_bgr32_test.c
 *
 * BGR32 pixel-format validation test for a custom Atari SDL2 driver.
 *
 * What it tests
 * -------------
 *  1. BYTE-ORDER STRIP   — pure R / G / B / White bars rendered by writing
 *                          raw Uint32 values so wrong byte-order shows up
 *                          as the wrong colour immediately.
 *
 *  2. GRADIENT RAMPS     — smooth 0-255 ramps for each channel; banding or
 *                          channel swaps are visible at a glance.
 *
 *  3. COLOUR SWATCH GRID — 16 well-known colours compared side-by-side:
 *                          top half = SDL_FillRect path,
 *                          bottom half = raw pixel write path.
 *                          Any driver mismatch stands out instantly.
 *
 *  4. ALPHA / PADDING    — the unused byte (X in BGRX) is set to
 *                          0x00, 0x55, 0xAA, 0xFF; all boxes must be
 *                          identical cyan — pad byte must not affect colour.
 *
 *  5. PIXEL READBACK     — SDL_RenderReadPixels into a BGRX surface;
 *                          retrieved values compared against written values.
 *                          PASS / FAIL counter shown on screen and stderr.
 *
 *  6. CHECKERBOARD       — alternating R/B at single real pixel exposes
 *                          stride, pitch, or byte-address bugs.
 *
 * BGR32 memory layout (BGRX8888, little-endian host like x86):
 *   addr+0=B  addr+1=G  addr+2=R  addr+3=X(pad)
 *   as Uint32:  (X<<24)|(R<<16)|(G<<8)|B
 *
 * BGR32 memory layout (BGRX8888, big-endian host like m68k):
 *   addr+0=X  addr+1=R  addr+2=G  addr+3=B
 *   as Uint32:  (X<<24)|(R<<16)|(G<<8)|B  <- same value, opposite memory
 *
 * SDL_MapRGB handles endianness; we test both paths.
 *
 * Build (host):
 *   gcc -o bgr32test sdl2_bgr32_test.c $(sdl2-config --cflags --libs)
 *
 * Build (m68k cross / MiNT):
 *   m68k-atari-mint-gcc -o bgr32tst.tos sdl2_bgr32_test.c \
 *       -I/path/to/sdl2/include -L/path/to/sdl2/lib -lSDL2
 *
 * Keys:  Q / ESC = quit     R = re-run all tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL2/SDL.h"

/* -------------------------------------------------------------------------
 * Virtual coordinate space (all positions expressed here)
 * ------------------------------------------------------------------------- */
#define BASE_W  640
#define BASE_H  480
#define VERSION "v1.0"

/* Section y-positions */
#define S_HDR_Y   0
#define S_HDR_H  16
#define S1_Y     18     /* byte-order + gradients */
#define S1_H     90
#define S2_Y     (S1_Y + S1_H + 2)
#define S2_H     72     /* swatches */
#define S3_Y     (S2_Y + S2_H + 2)
#define S3_H     46     /* pad byte */
#define S4_Y     (S3_Y + S3_H + 2)
#define S4_H     (BASE_H - S4_Y)  /* readback + checker */

/* The pixel format we are validating */
#define TEST_FMT  SDL_PIXELFORMAT_BGRX8888

/* -------------------------------------------------------------------------
 * Scale globals
 * ------------------------------------------------------------------------- */
static float g_sx = 1.0f, g_sy = 1.0f;
static int   g_fw = 1;

#define SX(v)  ((int)((v)*g_sx+0.5f))
#define SY(v)  ((int)((v)*g_sy+0.5f))
#define SW(v)  ((int)((v)*g_sx+0.5f))
#define SH(v)  ((int)((v)*g_sy+0.5f))

static void UpdateScale(int ww, int wh)
{
    float mn;
    g_sx = (float)ww/(float)BASE_W;
    g_sy = (float)wh/(float)BASE_H;
    mn   = (g_sx < g_sy) ? g_sx : g_sy;
    g_fw = (int)mn; if (g_fw < 1) g_fw = 1;
}

/* -------------------------------------------------------------------------
 * Named colours
 * ------------------------------------------------------------------------- */
#define C_BLACK   0
#define C_WHITE   1
#define C_GRAY    2
#define C_DGRAY   3
#define C_RED     4
#define C_GREEN   5
#define C_BLUE    6
#define C_CYAN    7
#define C_MAG     8
#define C_YEL     9
#define C_PANEL  10
#define C_BORDER 11
#define C_PASS   12
#define C_FAIL   13

static const SDL_Color pal[] = {
    {  0,  0,  0,255},{255,255,255,255},{170,170,170,255},{ 85, 85, 85,255},
    {255,  0,  0,255},{  0,255,  0,255},{  0,  0,255,255},{  0,255,255,255},
    {255,  0,255,255},{255,255,  0,255},{  0, 42, 85,255},{  0, 85,128,255},
    {  0,220, 60,255},{220, 40, 40,255},
};

/* -------------------------------------------------------------------------
 * 5x7 font — bit0=top row (corrected order)
 * ------------------------------------------------------------------------- */
static const Uint8 F[96][5] = {
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
static SDL_Window   *win = NULL;

/* -------------------------------------------------------------------------
 * Primitive helpers
 * ------------------------------------------------------------------------- */
static void SetC(int c)
{ SDL_SetRenderDrawColor(ren,pal[c].r,pal[c].g,pal[c].b,255); }

static void SetRGB(Uint8 r,Uint8 g,Uint8 b)
{ SDL_SetRenderDrawColor(ren,r,g,b,255); }

static void FillR(int vx,int vy,int vw,int vh,int c)
{ SDL_Rect r={SX(vx),SY(vy),SW(vw),SH(vh)}; SetC(c); SDL_RenderFillRect(ren,&r); }

static void FillRGB(int vx,int vy,int vw,int vh,Uint8 r,Uint8 g,Uint8 b)
{ SDL_Rect rc={SX(vx),SY(vy),SW(vw),SH(vh)}; SetRGB(r,g,b); SDL_RenderFillRect(ren,&rc); }

static void DrawR(int vx,int vy,int vw,int vh,int c)
{ SDL_Rect r={SX(vx),SY(vy),SW(vw),SH(vh)}; SetC(c); SDL_RenderDrawRect(ren,&r); }

static void HLine(int vx,int vy,int vl,int c)
{ SetC(c); SDL_RenderDrawLine(ren,SX(vx),SY(vy),SX(vx+vl-1),SY(vy)); }

/* Glyph at screen-pixel position */
static void Glyph(int spx,int spy,char ch,int fg,int bg)
{
    int col,row;
    if(ch<32||ch>127) ch='?';
    for(col=0;col<5;col++){
        Uint8 bits=F[(unsigned char)ch-32][col];
        for(row=0;row<7;row++){
            int lit=(bits>>row)&1;
            SDL_Rect r={spx+col*g_fw,spy+row*g_fw,g_fw,g_fw};
            if(lit){SetC(fg);SDL_RenderFillRect(ren,&r);}
            else if(bg>=0){SetC(bg);SDL_RenderFillRect(ren,&r);}
        }
    }
}
static void Str(int vx,int vy,const char *s,int fg,int bg)
{ int spx=SX(vx),spy=SY(vy); while(*s){Glyph(spx,spy,*s,fg,bg);spx+=6*g_fw;s++;} }

static void StrC(int vx,int vy,int vw,int vh,const char *s,int fg,int bg)
{
    int len=(int)strlen(s);
    int spx=SX(vx)+(SW(vw)-len*6*g_fw)/2;
    int spy=SY(vy)+(SH(vh)-7*g_fw)/2;
    while(*s){Glyph(spx,spy,*s,fg,bg);spx+=6*g_fw;s++;}
}
static void StrFmt(int vx,int vy,int fg,int bg,const char *fmt,...)
{
    char buf[160]; va_list ap;
    va_start(ap,fmt); SDL_vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    Str(vx,vy,buf,fg,bg);
}

/* -------------------------------------------------------------------------
 * Raw pixel fill — writes exact bytes into an SDL_Surface
 *
 * pixel value is the Uint32 in native host byte order.
 * BytesPerPixel is taken from the surface format.
 * This is the data your driver will actually receive.
 * ------------------------------------------------------------------------- */
static void RawFill(SDL_Surface *s, SDL_Rect *rc, Uint32 px)
{
    int y,x, bpp=s->format->BytesPerPixel;
    Uint8 *base=(Uint8*)s->pixels + rc->y*s->pitch + rc->x*bpp;
    for(y=0;y<rc->h;y++){
        Uint8 *row=base+y*s->pitch;
        for(x=0;x<rc->w;x++){
            Uint8 *p=row+x*bpp;
            /* Write in host byte order (SDL_MapRGB already handled endian) */
            switch(bpp){
            case 4: p[3]=(px>>24)&0xFF; p[2]=(px>>16)&0xFF;
                    p[1]=(px>> 8)&0xFF; p[0]=(px    )&0xFF; break;
            case 3: p[2]=(px>>16)&0xFF;
                    p[1]=(px>> 8)&0xFF; p[0]=(px    )&0xFF; break;
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Swatch table used by tests 3, 4, 5
 * ------------------------------------------------------------------------- */
static const struct { Uint8 r,g,b; const char *n; } SW16[] = {
    {255,  0,  0,"RED" },{  0,255,  0,"GRN" },{  0,  0,255,"BLU" },{255,255,  0,"YEL" },
    {  0,255,255,"CYN" },{255,  0,255,"MAG" },{255,128,  0,"ORG" },{128,  0,255,"PRP" },
    {  0,128,255,"SKY" },{255,255,255,"WHT" },{128,128,128,"GRY" },{  0,  0,  0,"BLK" },
    { 64,  0,  0,"DRD" },{  0, 64,  0,"DGN" },{  0,  0, 64,"DBL" },{255,192,203,"PNK" },
};
#define N_SW 16

/* -------------------------------------------------------------------------
 * Global test results
 * ------------------------------------------------------------------------- */
static int g_pass=0, g_fail=0;
static char g_rb_status[128]="not run";

/* =========================================================================
 * TEST 1 — Byte-order bars
 * ======================================================================= */
static void T1_ByteOrder(void)
{
    int x=4, y=S1_Y+14, bw=58, bh=S1_H-26, gap=4;

    Str(4, S1_Y+3, "[1] BYTE ORDER  (must be R G B W)", C_GRAY, -1);

    /* These go through SDL_SetRenderDrawColor — the renderer maps to BGR32 */
    FillRGB(x,              y, bw, bh, 255,  0,   0); /* RED   */
    FillRGB(x+bw+gap,       y, bw, bh,   0,255,   0); /* GREEN */
    FillRGB(x+2*(bw+gap),   y, bw, bh,   0,  0, 255); /* BLUE  */
    FillRGB(x+3*(bw+gap),   y, bw, bh, 255,255, 255); /* WHITE */

    /* Also write the same via a raw BGR32 surface to test that path */
    {
        SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0,bw,bh,32,TEST_FMT);
        if(s){
            SDL_PixelFormat *f=s->format;
            Uint32 rawR=SDL_MapRGB(f,255,  0,  0);
            Uint32 rawG=SDL_MapRGB(f,  0,255,  0);
            Uint32 rawB=SDL_MapRGB(f,  0,  0,255);
            Uint32 rawW=SDL_MapRGB(f,255,255,255);
            SDL_Rect rc={0,0,bw,bh};
            SDL_Texture *t;

            /* Print raw Uint32 values — key diagnostic on stderr */
            fprintf(stderr,"[T1] MapRGB raw values (BGRX8888):\n");
            fprintf(stderr,"     RED  =0x%08X  bytes:[%02X %02X %02X %02X]\n",
                (unsigned)rawR,
                (unsigned)(rawR    )&0xFF,(unsigned)(rawR>> 8)&0xFF,
                (unsigned)(rawR>>16)&0xFF,(unsigned)(rawR>>24)&0xFF);
            fprintf(stderr,"     GREEN=0x%08X  bytes:[%02X %02X %02X %02X]\n",
                (unsigned)rawG,
                (unsigned)(rawG    )&0xFF,(unsigned)(rawG>> 8)&0xFF,
                (unsigned)(rawG>>16)&0xFF,(unsigned)(rawG>>24)&0xFF);
            fprintf(stderr,"     BLUE =0x%08X  bytes:[%02X %02X %02X %02X]\n",
                (unsigned)rawB,
                (unsigned)(rawB    )&0xFF,(unsigned)(rawB>> 8)&0xFF,
                (unsigned)(rawB>>16)&0xFF,(unsigned)(rawB>>24)&0xFF);

            /* Draw small raw-write versions below the renderer bars */
            int ry = y + bh + 2;
            int rh = 8;

            RawFill(s,&rc,rawR);
            t=SDL_CreateTextureFromSurface(ren,s);
            if(t){ SDL_Rect d={SX(x),SY(ry),SW(bw),SH(rh)}; SDL_RenderCopy(ren,t,NULL,&d); SDL_DestroyTexture(t); }

            RawFill(s,&rc,rawG);
            t=SDL_CreateTextureFromSurface(ren,s);
            if(t){ SDL_Rect d={SX(x+bw+gap),SY(ry),SW(bw),SH(rh)}; SDL_RenderCopy(ren,t,NULL,&d); SDL_DestroyTexture(t); }

            RawFill(s,&rc,rawB);
            t=SDL_CreateTextureFromSurface(ren,s);
            if(t){ SDL_Rect d={SX(x+2*(bw+gap)),SY(ry),SW(bw),SH(rh)}; SDL_RenderCopy(ren,t,NULL,&d); SDL_DestroyTexture(t); }

            RawFill(s,&rc,rawW);
            t=SDL_CreateTextureFromSurface(ren,s);
            if(t){ SDL_Rect d={SX(x+3*(bw+gap)),SY(ry),SW(bw),SH(rh)}; SDL_RenderCopy(ren,t,NULL,&d); SDL_DestroyTexture(t); }

            SDL_FreeSurface(s);
        }
    }

    /* Labels */
    StrC(x,            y+bh-9, bw,9, "R", C_WHITE,-1);
    StrC(x+bw+gap,     y+bh-9, bw,9, "G", C_WHITE,-1);
    StrC(x+2*(bw+gap), y+bh-9, bw,9, "B", C_WHITE,-1);
    StrC(x+3*(bw+gap), y+bh-9, bw,9, "W", C_BLACK,-1);

    Str(x, S1_Y+S1_H-8, "top=SDL  bot=raw", C_DGRAY,-1);

    /* Borders */
    DrawR(x-1,            y-1, bw+2, bh+2, C_BORDER);
    DrawR(x+bw+gap-1,     y-1, bw+2, bh+2, C_BORDER);
    DrawR(x+2*(bw+gap)-1, y-1, bw+2, bh+2, C_BORDER);
    DrawR(x+3*(bw+gap)-1, y-1, bw+2, bh+2, C_BORDER);
}

/* =========================================================================
 * TEST 2 — Gradient ramps
 * ======================================================================= */
static void T2_Gradients(void)
{
    int x0=330, y0=S1_Y+6;
    int totw=BASE_W-x0-4, barh=22, gap=4;
    int i;

    Str(x0, S1_Y+3, "[2] GRADIENTS  0->255", C_GRAY,-1);

    for(i=0; i<totw; i++){
        Uint8 v=(Uint8)(i*255/(totw-1));
        int spx = SX(x0) + i * SW(totw) / totw;
        int sw  = SW(totw)/totw + 1;
        SDL_Rect rR={spx,SY(y0),               sw,SH(barh)};
        SDL_Rect rG={spx,SY(y0+barh+gap),      sw,SH(barh)};
        SDL_Rect rB={spx,SY(y0+2*(barh+gap)),  sw,SH(barh)};
        SDL_SetRenderDrawColor(ren,v,0,0,255); SDL_RenderFillRect(ren,&rR);
        SDL_SetRenderDrawColor(ren,0,v,0,255); SDL_RenderFillRect(ren,&rG);
        SDL_SetRenderDrawColor(ren,0,0,v,255); SDL_RenderFillRect(ren,&rB);
    }

    Str(x0,    y0+3*(barh+gap)+2, "R:", C_RED,   -1);
    Str(x0+14, y0+3*(barh+gap)+2, "G:", C_GREEN, -1);
    Str(x0+28, y0+3*(barh+gap)+2, "B:", C_BLUE,  -1);
    Str(x0+42, y0+3*(barh+gap)+2, "all must be smooth, no banding", C_DGRAY,-1);
}

/* =========================================================================
 * TEST 3 — Colour swatches: SDL fill vs raw BGR32 write
 * ======================================================================= */
static void T3_Swatches(void)
{
    int x0=4, y0=S2_Y+14, sw=34, sh=20, gap=3, cols=8;
    int i;
    SDL_Surface *surf;

    Str(4, S2_Y+3, "[3] SWATCHES  top=SDL_renderer  bot=raw_BGRX_surface", C_GRAY,-1);

    surf=SDL_CreateRGBSurfaceWithFormat(0,SW(sw),SH(sh/2)+1,32,TEST_FMT);
    if(!surf){ Str(4,y0,"surf alloc FAILED",C_FAIL,-1); return; }

    for(i=0;i<N_SW;i++){
        int col=i%cols, row=i/cols;
        int vx=x0+col*(sw+gap), vy=y0+row*(sh+gap+10);
        Uint8 r=SW16[i].r, g=SW16[i].g, b=SW16[i].b;

        /* Top half: SDL renderer path */
        FillRGB(vx, vy, sw, sh/2, r, g, b);

        /* Bottom half: raw surface write → texture */
        {
            Uint32 px=SDL_MapRGB(surf->format,r,g,b);
            SDL_Rect src={0,0,SW(sw),SH(sh/2)+1};
            SDL_Rect dst={SX(vx),SY(vy+sh/2),SW(sw),SH(sh/2)+1};
            SDL_Texture *t;
            RawFill(surf,&src,px);
            t=SDL_CreateTextureFromSurface(ren,surf);
            if(t){ SDL_RenderCopy(ren,t,&src,&dst); SDL_DestroyTexture(t); }
        }

        /* Divider */
        HLine(vx, vy+sh/2, sw, C_BLACK);
        DrawR(vx-1,vy-1,sw+2,sh+2,C_BORDER);
        StrC(vx,vy+sh+1,sw,8,SW16[i].n,C_GRAY,-1);
    }
    SDL_FreeSurface(surf);
}

/* =========================================================================
 * TEST 4 — Padding / alpha byte
 * Write cyan with pad byte = 00, 55, AA, FF.  All must look identical.
 * ======================================================================= */
static void T4_PadByte(void)
{
    static const Uint8 pads[]={0x00,0x55,0xAA,0xFF};
    static const char *labs[]={"pad=00","pad=55","pad=AA","pad=FF"};
    int x0=4, y0=S3_Y+14, bw=100, bh=S3_H-26, gap=10;
    int i;
    SDL_Surface *surf=SDL_CreateRGBSurfaceWithFormat(0,SW(bw),SH(bh),32,TEST_FMT);

    Str(4, S3_Y+3, "[4] PAD BYTE  (all 4 boxes MUST be identical CYAN)", C_GRAY,-1);
    if(!surf){ Str(4,y0,"surf alloc FAILED",C_FAIL,-1); return; }

    for(i=0;i<4;i++){
        int vx=x0+i*(bw+gap);
        Uint32 px=SDL_MapRGB(surf->format, 0,255,255); /* cyan */

        /* Force the pad/alpha byte */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        /* big-endian: byte[0] is bits 31-24 */
        px = (px & 0x00FFFFFF) | ((Uint32)pads[i]<<24);
#else
        /* little-endian: byte[3] is bits 31-24 */
        px = (px & 0x00FFFFFF) | ((Uint32)pads[i]<<24);
#endif
        {
            SDL_Rect rc={0,0,SW(bw),SH(bh)};
            SDL_Rect dst={SX(vx),SY(y0),SW(bw),SH(bh)};
            SDL_Rect src={0,0,SW(bw),SH(bh)};
            SDL_Texture *t;
            RawFill(surf,&rc,px);
            t=SDL_CreateTextureFromSurface(ren,surf);
            if(t){ SDL_RenderCopy(ren,t,&src,&dst); SDL_DestroyTexture(t); }
        }

        DrawR(vx-1,y0-1,bw+2,bh+2,C_BORDER);
        StrC(vx,y0+bh+1,bw,8,labs[i],C_GRAY,-1);

        /* Show raw hex */
        { char hex[12]; SDL_snprintf(hex,sizeof(hex),"0x%08X",(unsigned)px);
          StrC(vx,y0+bh+10,bw,8,hex,C_DGRAY,-1); }
    }
    SDL_FreeSurface(surf);
}

/* =========================================================================
 * TEST 5 — Pixel readback
 * ======================================================================= */
static void T5_Readback(void)
{
    /* We blit a strip of N_SW colour patches and read them back */
    int bw=24, bh=14;
    int strip_x=SX(320), strip_y=SY(S4_Y+16);
    SDL_Surface *ws, *rs;
    SDL_Texture *t;
    int i;

    Str(320, S4_Y+3, "[5] READBACK", C_GRAY,-1);

    ws=SDL_CreateRGBSurfaceWithFormat(0,bw*N_SW,bh,32,TEST_FMT);
    if(!ws){ SDL_strlcpy(g_rb_status,"ws alloc FAIL",sizeof(g_rb_status));return; }

    for(i=0;i<N_SW;i++){
        Uint32 px=SDL_MapRGB(ws->format,SW16[i].r,SW16[i].g,SW16[i].b);
        SDL_Rect rc={i*bw,0,bw,bh};
        RawFill(ws,&rc,px);
    }

    t=SDL_CreateTextureFromSurface(ren,ws);
    if(!t){ SDL_FreeSurface(ws); SDL_strlcpy(g_rb_status,"tex FAIL",sizeof(g_rb_status));return; }

    { SDL_Rect dst={strip_x,strip_y,bw*N_SW,bh}; SDL_RenderCopy(ren,t,NULL,&dst); }
    SDL_DestroyTexture(t);

    /* Read back immediately */
    rs=SDL_CreateRGBSurfaceWithFormat(0,bw*N_SW,bh,32,TEST_FMT);
    if(!rs){ SDL_FreeSurface(ws); SDL_strlcpy(g_rb_status,"rs alloc FAIL",sizeof(g_rb_status));return; }

    if(SDL_RenderReadPixels(ren,
            &(SDL_Rect){strip_x,strip_y,bw*N_SW,bh},
            TEST_FMT, rs->pixels, rs->pitch)!=0){
        SDL_snprintf(g_rb_status,sizeof(g_rb_status),
            "RenderReadPixels FAILED: %s",SDL_GetError());
        SDL_FreeSurface(ws); SDL_FreeSurface(rs); return;
    }

    g_pass=0; g_fail=0;
    fprintf(stderr,"[T5] Readback results:\n");
    for(i=0;i<N_SW;i++){
        Uint8 wr,wg,wb, rr,rg,rb;
        int cx=i*bw+bw/2, cy=bh/2;
        Uint32 wpx,rpx;
        SDL_memcpy(&wpx,(Uint8*)ws->pixels+cy*ws->pitch+cx*4,4);
        SDL_memcpy(&rpx,(Uint8*)rs->pixels+cy*rs->pitch+cx*4,4);
        SDL_GetRGB(wpx,ws->format,&wr,&wg,&wb);
        SDL_GetRGB(rpx,rs->format,&rr,&rg,&rb);

        if(abs((int)wr-(int)rr)<=1 &&
           abs((int)wg-(int)rg)<=1 &&
           abs((int)wb-(int)rb)<=1){
            g_pass++;
            fprintf(stderr,"  PASS %s: W(%3d,%3d,%3d) R(%3d,%3d,%3d)\n",
                SW16[i].n,wr,wg,wb,rr,rg,rb);
        } else {
            g_fail++;
            fprintf(stderr,"  FAIL %s: W(%3d,%3d,%3d) R(%3d,%3d,%3d)  <<< MISMATCH\n",
                SW16[i].n,wr,wg,wb,rr,rg,rb);
        }
    }
    SDL_snprintf(g_rb_status,sizeof(g_rb_status),
        "PASS=%d  FAIL=%d",g_pass,g_fail);
    fprintf(stderr,"[T5] %s\n",g_rb_status);

    SDL_FreeSurface(ws);
    SDL_FreeSurface(rs);
}

/* =========================================================================
 * TEST 6 — 1-pixel R/B checkerboard
 * ======================================================================= */
static void T6_Checker(void)
{
    int x0=4, y0=S4_Y+16;
    int cw=280, ch=S4_H-30;
    int px,py;

    Str(4, S4_Y+3, "[6] 1-PX CHECKER  R/B  (stripe/stride/addr test)", C_GRAY,-1);

    for(py=0;py<SH(ch);py++){
        for(px=0;px<SW(cw);px++){
            SDL_Rect r={SX(x0)+px, SY(y0)+py, 1,1};
            if((px+py)&1) SDL_SetRenderDrawColor(ren,255,0,0,255);
            else          SDL_SetRenderDrawColor(ren,0,0,255,255);
            SDL_RenderFillRect(ren,&r);
        }
    }
    DrawR(x0-1,y0-1,cw+2,ch+2,C_BORDER);
    Str(x0+cw+6, y0,    "Should look",C_DGRAY,-1);
    Str(x0+cw+6, y0+10, "purple at",  C_DGRAY,-1);
    Str(x0+cw+6, y0+20, "low zoom,",  C_DGRAY,-1);
    Str(x0+cw+6, y0+30, "crisp R/B",  C_DGRAY,-1);
    Str(x0+cw+6, y0+40, "at 1:1 px.", C_DGRAY,-1);
}

/* =========================================================================
 * Header + dividers
 * ======================================================================= */
static void DrawHeader(void)
{
    char buf[64];
    FillR(0,0,BASE_W,S_HDR_H,C_PANEL);
    HLine(0,S_HDR_H,BASE_W,C_BORDER);
    Str(4,5,"SDL2 BGR32 / BGRX8888 VALIDATION",C_CYAN,-1);
    SDL_snprintf(buf,sizeof(buf),"fmt=0x%08X",(unsigned)TEST_FMT);
    StrC(BASE_W/2-80,0,160,S_HDR_H,buf,C_GRAY,-1);
    Str(BASE_W-(int)strlen(VERSION)*6-4,5,VERSION,C_YEL,-1);
}

static void DrawDividers(void)
{
    HLine(0,S1_Y-1,BASE_W,C_BORDER);
    HLine(0,S2_Y-1,BASE_W,C_BORDER);
    HLine(0,S3_Y-1,BASE_W,C_BORDER);
    HLine(0,S4_Y-1,BASE_W,C_BORDER);
    /* vertical inside section 1 */
    { int sx=SX(320); SetC(C_BORDER);
      SDL_RenderDrawLine(ren,sx,SY(S1_Y),sx,SY(S1_Y+S1_H)); }
}

static void DrawReadbackStatus(void)
{
    int col = (g_fail==0 && g_pass>0) ? C_PASS : C_FAIL;
    StrFmt(320, S4_Y+14+10, col,-1,
        "Result: %s", g_rb_status);
    /* Endian note */
    StrFmt(320, S4_Y+14+22, C_DGRAY,-1,
        "Host: %s  BPP:%d",
        SDL_BYTEORDER==SDL_BIG_ENDIAN ? "BIG-ENDIAN(m68k)" : "LITTLE-ENDIAN(x86)",
        SDL_BYTESPERPIXEL(TEST_FMT));
}

/* =========================================================================
 * Full redraw
 * ======================================================================= */
static void Redraw(void)
{
    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);

    DrawHeader();
    DrawDividers();
    T1_ByteOrder();
    T2_Gradients();
    T3_Swatches();
    T4_PadByte();
    T6_Checker();
    T5_Readback();           /* must come after T6 since it reads pixels */
    DrawReadbackStatus();

    StrC(0,BASE_H-9,BASE_W,9,"Q/ESC=QUIT   R=RERUN",C_DGRAY,-1);
    SDL_RenderPresent(ren);
}

/* =========================================================================
 * main
 * ======================================================================= */
int main(int argc,char *argv[])
{
    SDL_Event ev;
    int running=1;
    (void)argc;(void)argv;

    if(SDL_Init(SDL_INIT_VIDEO)<0){fprintf(stderr,"SDL_Init:%s\n",SDL_GetError());return 1;}
    // SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    fprintf(stderr,"=== SDL2 BGR32 Validation ===\n");
    fprintf(stderr,"Format name : %s\n",SDL_GetPixelFormatName(TEST_FMT));
    fprintf(stderr,"Format value: 0x%08X\n",(unsigned)TEST_FMT);
    fprintf(stderr,"BytesPerPixel:%d\n",SDL_BYTESPERPIXEL(TEST_FMT));
    fprintf(stderr,"Host order  : %s\n",
        SDL_BYTEORDER==SDL_BIG_ENDIAN?"BIG (m68k)":"LITTLE (x86)");

    win=SDL_CreateWindow("SDL2 BGR32 Validation",
        SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
        BASE_W,BASE_H,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!win){fprintf(stderr,"CreateWindow:%s\n",SDL_GetError());SDL_Quit();return 1;}

    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);
    if(!ren){fprintf(stderr,"CreateRenderer:%s\n",SDL_GetError());SDL_DestroyWindow(win);SDL_Quit();return 1;}

    {int ww,wh;SDL_GetWindowSize(win,&ww,&wh);UpdateScale(ww,wh);}

    Redraw();

    while(running){
        int dirty=0;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
            case SDL_QUIT: running=0; break;
            case SDL_WINDOWEVENT:
                if(ev.window.event==SDL_WINDOWEVENT_RESIZED||
                   ev.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){
                    UpdateScale(ev.window.data1,ev.window.data2); dirty=1;
                }
                break;
            case SDL_KEYDOWN:
                if(ev.key.keysym.sym==SDLK_ESCAPE||ev.key.keysym.sym==SDLK_q) running=0;
                if(ev.key.keysym.sym==SDLK_r) dirty=1;
                break;
            default:break;
            }
        }
        if(dirty) Redraw();
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
/*
 * sdl2_atari_demo.c  —  optimised for Motorola 68000
 * SDL2 demonstration for Atari ST / TT / Falcon
 * Medour Mehdi - 2026
 *
 * Per-scene optimisation log (each item is a concrete removed operation):
 *
 *  ALL STATIC SCENES (palette, gradient, primitives)
 *    — drawn ONCE on first visit, result cached in scene_dirty flag.
 *      Subsequent frames cost zero draw calls until scene changes.
 *
 *  PLASMA  (320×200 = 64,000 pixels/frame — the real hot path)
 *    — usin_table[256]: sine stored pre-offset (+127) as Uint8.
 *      Eliminates 2 ADD instructions per pixel (128,000 ADDs/frame gone).
 *    — g_row precomputed per row as (usin[y+t8h] >> 3) giving the
 *      green bits already shifted into RGB332 position (bits 4..2).
 *      Eliminates 1 SHIFT per pixel (64,000 shifts/frame gone).
 *    — *dst++ auto-increment pointer replaces row_ptr[x] indexed store.
 *      On 68000: MOVE.B Dn,(An)+ vs MOVE.B Dn,(An,Dn) — 1 cycle saved/pixel.
 *    — row_ptr advanced additively (+=pitch), no y*pitch multiply ever.
 *    — Inner loop: 2 table loads, 2 shifts, 2 ANDs, 1 OR, 1 OR, 1 store.
 *      Zero MUL, zero DIV, zero function calls.
 *
 *  CHECKERS  (80×50 = 4000 tiles/frame)
 *    — pitch2/pitch3/pitch4 precomputed outside all loops.
 *      Eliminates pitch*2 and pitch*3 MULs inside tile loop (8000 MULs gone).
 *      row0 += pitch4 replaces pitch*4 MUL per row (200 MULs gone).
 *    — Checker parity toggled with ^= 1 each column instead of
 *      (tx + scroll + ty + phase) & 1 (eliminates 4000 ADDs + 4000 ANDs).
 *    — Word replication: w=b|(b<<8); w|=(w<<16) — 4 ops vs 6 ops before.
 *    — col_ptr advances by 4 bytes (additive) — no index arithmetic.
 *
 *  GRADIENT  — Bresenham accumulator: zero MUL, zero DIV in ramp loop.
 *  PALETTE   — col/row counters + additive x/y: zero % and / in loop.
 *              RGB332 expand from precomputed tables: zero shifts in loop.
 *  PRIMITIVES— additive accumulators for rect size and colour: zero MUL.
 *
 * Controls:  SPACE/RIGHT = next   LEFT = prev   ESC = quit
 *
 * Build:
 *   m68k-atari-mint-gcc -o sdl2_atari_demo.tos sdl2_atari_demo.c \
 *       -lSDL2main -lSDL2 -O2
 */

#include <SDL2/SDL.h>

#define SCREEN_W      320
#define SCREEN_H      200
#define NUM_SCENES    5

#define SCENE_PALETTE     0
#define SCENE_GRADIENT    1
#define SCENE_PRIMITIVES  2
#define SCENE_PLASMA      3
#define SCENE_CHECKERS    4

/* ============================================================
   Unsigned sine table — pre-offset by +127 so values are 0..254.
   This eliminates the +127 addition that was needed per pixel in
   the plasma inner loop when using a signed (-127..127) table.
   Index wraps naturally as Uint8 — no masking needed.
   ============================================================ */
static const Uint8 usin_table[256] = {
    127, 130, 133, 136, 139, 142, 145, 148, 151, 155, 158, 161, 164, 167, 170, 173,
    176, 178, 181, 184, 187, 190, 192, 195, 198, 200, 203, 205, 208, 210, 212, 215,
    217, 219, 221, 223, 225, 227, 229, 231, 233, 234, 236, 238, 239, 240, 242, 243,
    244, 245, 247, 248, 249, 249, 250, 251, 252, 252, 253, 253, 253, 254, 254, 254,
    254, 254, 254, 254, 253, 253, 253, 252, 252, 251, 250, 249, 249, 248, 247, 245,
    244, 243, 242, 240, 239, 238, 236, 234, 233, 231, 229, 227, 225, 223, 221, 219,
    217, 215, 212, 210, 208, 205, 203, 200, 198, 195, 192, 190, 187, 184, 181, 178,
    176, 173, 170, 167, 164, 161, 158, 155, 151, 148, 145, 142, 139, 136, 133, 130,
    127, 124, 121, 118, 115, 112, 109, 106, 103,  99,  96,  93,  90,  87,  84,  81,
     78,  76,  73,  70,  67,  64,  62,  59,  56,  54,  51,  49,  46,  44,  42,  39,
     37,  35,  33,  31,  29,  27,  25,  23,  21,  20,  18,  16,  15,  14,  12,  11,
     10,   9,   7,   6,   5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,
      0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,
     10,  11,  12,  14,  15,  16,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,
     37,  39,  42,  44,  46,  49,  51,  54,  56,  59,  62,  64,  67,  70,  73,  76,
     78,  81,  84,  87,  90,  93,  96,  99, 103, 106, 109, 112, 115, 118, 121, 124
};

/* ============================================================
   RGB332 expand tables — built once at startup.
   Palette scene reads directly: no shifts inside the draw loop.
   ============================================================ */
static Uint8 rgb332_r[256];
static Uint8 rgb332_g[256];
static Uint8 rgb332_b[256];

static void build_rgb332_table(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        Uint8 r3 = (Uint8)((i >> 5) & 7);
        Uint8 g3 = (Uint8)((i >> 2) & 7);
        Uint8 b2 = (Uint8)(i & 3);
        rgb332_r[i] = (r3 << 5) | (r3 << 2) | (r3 >> 1);
        rgb332_g[i] = (g3 << 5) | (g3 << 2) | (g3 >> 1);
        rgb332_b[i] = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
    }
}

/* ============================================================
   Scene state
   scene_dirty: set when we switch scenes so static scenes
   redraw exactly once instead of every frame.
   ============================================================ */
static int      current_scene = SCENE_PALETTE;
static Uint32   frame         = 0;
static SDL_bool scene_dirty   = SDL_TRUE;

static void log_scene_name(void)
{
    static const char *names[NUM_SCENES] = {
        "1/5 Palette   [static, drawn once]",
        "2/5 Gradient  [static, drawn once]",
        "3/5 Primitives[static, drawn once]",
        "4/5 Plasma    [direct pixels, zero SDL in inner loop]",
        "5/5 Checkers  [Uint32 writes, toggle parity, no MUL]"
    };
    SDL_Log("Scene: %s  [SPACE=next LEFT=prev ESC=quit]",
            names[current_scene]);
}

/* ============================================================
   SCENE 1 — Palette grid (static — drawn ONCE per visit)

   vs original:
   • col/row: counters, no % or /
   • cell.x/y: additive, no *
   • RGB: precomputed tables, no shifts in loop
   • scene_dirty guard: 0 SDL calls on frames 2..N
   ============================================================ */
static void draw_palette(SDL_Renderer *r)
{
    SDL_Rect cell;
    int i, col, row;
    const int cell_w = SCREEN_W / 16;         /* 20  — exact */
    const int base_h = SCREEN_H / 16;         /* 12  */
    const int last_h = SCREEN_H - 15*base_h;  /* 20  — absorbs 8px remainder */

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    cell.w = cell_w;
    cell.x = 0;
    cell.y = 0;
    col    = 0;
    row    = 0;

    for (i = 0; i < 256; i++) {
        cell.h = (row == 15) ? last_h : base_h;

        SDL_SetRenderDrawColor(r, rgb332_r[i], rgb332_g[i], rgb332_b[i], 255);
        SDL_RenderFillRect(r, &cell);

        cell.x += cell_w;   /* additive — no * */
        col++;
        if (col == 16) {
            col    = 0;
            cell.x = 0;
            row++;
            cell.y += base_h;   /* additive — no * */
        }
    }
}

/* ============================================================
   SCENE 2 — RGB Gradients (static — drawn ONCE per visit)

   vs original:
   • (x*255)/(SCREEN_W-1) → Bresenham accumulator: zero MUL, zero DIV
   • bar_h2 precomputed: no * in loop
   ============================================================ */
static void draw_gradients(SDL_Renderer *r)
{
    int x, v, acc;
    const int base_h = SCREEN_H / 3;           /* 66 */
    const int last_h = SCREEN_H - 2*base_h;    /* 68 — absorbs 2px remainder */
    const int bar_h2 = base_h + base_h;        /* 132 — precomputed, no * in loop */
    SDL_Rect  px;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    px.w = 1;
    v    = 0;
    acc  = 0;

    for (x = 0; x < SCREEN_W; x++) {
        px.x = x;

        px.y = 0;      px.h = base_h;
        SDL_SetRenderDrawColor(r, (Uint8)v, 0, 0, 255);
        SDL_RenderFillRect(r, &px);

        px.y = base_h; px.h = base_h;
        SDL_SetRenderDrawColor(r, 0, (Uint8)v, 0, 255);
        SDL_RenderFillRect(r, &px);

        px.y = bar_h2; px.h = last_h;
        SDL_SetRenderDrawColor(r, 0, 0, (Uint8)v, 255);
        SDL_RenderFillRect(r, &px);

        /* Bresenham: zero MUL, zero DIV — only ADD + compare */
        acc += 255;
        if (acc >= (SCREEN_W - 1)) {
            acc -= (SCREEN_W - 1);
            v++;
        }
    }

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawLine(r, 0, base_h,  SCREEN_W-1, base_h);
    SDL_RenderDrawLine(r, 0, bar_h2, SCREEN_W-1, bar_h2);
}

/* ============================================================
   SCENE 3 — Primitives (static — drawn ONCE per visit)

   vs original:
   • m, rw, rh tracked additively — no * per iteration
   • rv, gv, bv updated with += / -= — no * per iteration
   ============================================================ */
static void draw_primitives(SDL_Renderer *r)
{
    int i, j;
    SDL_Rect rect;
    int m  = 0,  rv = 0,   gv = 255, bv = 0;
    int rw = SCREEN_W, rh = SCREEN_H;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    for (i = 0; i < 8; i++) {
        SDL_SetRenderDrawColor(r, (Uint8)rv, (Uint8)gv, (Uint8)bv, 255);
        rect.x = m;  rect.y = m;
        rect.w = rw; rect.h = rh;
        SDL_RenderDrawRect(r, &rect);
        m  += 10; rw -= 20; rh -= 20;  /* additive — no * */
        rv += 30; gv -= 20; bv += 36;
    }

    SDL_SetRenderDrawColor(r, 255, 255,   0, 255);
    SDL_RenderDrawLine(r, 0, 0, SCREEN_W-1, SCREEN_H-1);
    SDL_SetRenderDrawColor(r,   0, 255, 255, 255);
    SDL_RenderDrawLine(r, SCREEN_W-1, 0, 0, SCREEN_H-1);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawLine(r, SCREEN_W/2, 0, SCREEN_W/2, SCREEN_H-1);
    SDL_RenderDrawLine(r, 0, SCREEN_H/2, SCREEN_W-1, SCREEN_H/2);

    SDL_SetRenderDrawColor(r, 255, 0, 255, 255);
    for (i = 0; i < SCREEN_W; i += 20)
        for (j = 0; j < SCREEN_H; j += 20)
            SDL_RenderDrawPoint(r, i, j);
}

/* ============================================================
   SCENE 4 — Plasma (direct pixel write, fully optimised inner loop)

   Inner loop per pixel (320×200 = 64,000 pixels/frame):
     x8++               1 add   (x phase advance)
     xy_phase++         1 add   (x+y phase advance)
     usin_table[x8]     1 load  (no +127 add needed — table pre-offset)
     >> 5, & 7          1 shift, 1 and  → r3 (red bits)
     usin_table[xy]     1 load
     >> 6, & 3          1 shift, 1 and  → b2 (blue bits)
     r3<<5 | g_row | b2 1 shift, 2 or   → packed RGB332 byte
     *dst++             1 byte store    (auto-increment, cheapest 68000 mode)
   ──────────────────────────────────────────────────
   Total: 2 adds, 2 loads, 2 shifts, 3 ands/ors, 1 store = ~10 ops/pixel
   Zero MUL, zero DIV, zero function calls.

   Per-row setup (200 times/frame):
     usin_table[y+t8h] >> 3   1 load, 1 shift → g_row (green already positioned)
     row_ptr += pitch          1 add            → no y*pitch MUL ever
   ============================================================ */
static void draw_plasma(SDL_Surface *surf, Uint32 t)
{
    Uint8 *row_ptr, *dst;
    int    pitch, y;
    Uint8  t8      = (Uint8)t;
    Uint8  t8h     = (Uint8)(t >> 1);
    Uint8  t8q     = (Uint8)(t >> 2);
    Uint8  x8, xy_phase;
    Uint8  g_row;     /* green bits pre-shifted into position 2..4, constant per row */
    Uint8  r3, b2;

    if (SDL_LockSurface(surf) < 0) return;

    row_ptr = (Uint8 *)surf->pixels;
    pitch   = surf->pitch;

    for (y = 0; y < SCREEN_H; y++) {
        /* Green component: constant for this entire row.
         * usin_table already has +127 baked in, so no ADD needed.
         * >>3 gives top 5 bits; we want bits [4:2] of the RGB332 byte,
         * so >>3 then &0x1C isolates them already in position. */
        g_row   = (usin_table[(Uint8)(y + t8h)] >> 3) & 0x1C;

        /* x phase starts at t8 (when x=0, index = 0 + t8) */
        x8       = t8;
        /* x+y phase starts at (y + t8q) */
        xy_phase = (Uint8)(y + t8q);

        dst = row_ptr;

        /* Inner loop — no function calls, no MUL, no DIV */
        while (dst < row_ptr + SCREEN_W) {
            r3  = (usin_table[x8]       >> 5);         /* bits [7:5] → 0..7  */
            b2  = (usin_table[xy_phase] >> 6);         /* bits [7:6] → 0..3  */
            *dst++ = (r3 << 5) | g_row | b2;           /* pack RRRGGGBB      */
            x8++;
            xy_phase++;
        }

        row_ptr += pitch;   /* additive advance — no y*pitch MUL */
    }

    SDL_UnlockSurface(surf);
}

/* ============================================================
   SCENE 5 — Checkerboard (Uint32 block writes, zero MUL in loops)

   vs original:
   • pitch*2, pitch*3 inside tile loop → precomputed pitch2/pitch3 (8000 MULs gone)
   • row0 += pitch*4 per row → precomputed pitch4 (200 MULs gone)
   • checker = (tx+scroll+ty+phase)&1 → checker ^= 1 each column
     (4000 ADDs + 4000 ANDs gone — just a bit flip)
   • Word replication: w=b|(b<<8); w|=(w<<16) — 4 ops, was 6 ops
   • col_ptr += 4 advances pointer additively — no index arithmetic
   ============================================================ */
static void draw_checkers(SDL_Surface *surf, Uint32 t)
{
    Uint8 *pixels, *row0, *col_ptr;
    int    pitch, pitch2, pitch3, pitch4;
    int    tile_x, tile_y;
    int    row_parity, checker;
    int    scroll = (int)(t >> 2);
    int    phase  = (int)(t & 1);
    Uint32 word;
    Uint8  px8;

    if (SDL_LockSurface(surf) < 0) return;

    pixels = (Uint8 *)surf->pixels;
    pitch  = surf->pitch;

    /* Precompute pitch multiples — eliminates all MULs inside loops */
    pitch2 = pitch  + pitch;
    pitch3 = pitch2 + pitch;
    pitch4 = pitch3 + pitch;

    row0   = pixels;
    /* ty starts at 0, increments by 1 per 4-row block */
    for (tile_y = 0; tile_y < SCREEN_H; tile_y += 4) {
        int ty = tile_y >> 2;

        /* Parity for this row, computed ONCE outside the column loop.
         * checker will toggle with ^=1 on each column — zero ADDs/ANDs. */
        row_parity = (scroll + ty + phase) & 1;
        checker    = row_parity;

        col_ptr = row0;

        for (tile_x = 0; tile_x < SCREEN_W; tile_x += 4) {
            if (checker) {
                Uint8 cv_r = (Uint8)((tile_x + (int)t)                    );
                Uint8 cv_g = (Uint8)((tile_y + (int)(t >> 1))              );
                Uint8 cv_b = (Uint8)(((tile_x + tile_y) + (int)(t >> 2))   );
                /* RGB332 pack — shifts only, no MUL */
                px8  = (Uint8)((cv_r & 0xE0) |         /* top 3 bits of R in place */
                               ((cv_g >> 3) & 0x1C) |  /* top 3 bits of G → pos 4:2 */
                               (cv_b >> 6));            /* top 2 bits of B → pos 1:0 */
                /* Replicate byte into longword — 4 ops (was 6) */
                word = (Uint32)px8 | ((Uint32)px8 << 8);
                word = word | (word << 16);
            } else {
                word = 0;
            }

            /* 4 Uint32 stores cover the 4×4 tile — no indexed addressing */
            *((Uint32 *)(col_ptr         )) = word;
            *((Uint32 *)(col_ptr + pitch )) = word;
            *((Uint32 *)(col_ptr + pitch2)) = word;
            *((Uint32 *)(col_ptr + pitch3)) = word;

            col_ptr += 4;       /* advance 4 bytes (4 pixels) — additive */

            checker ^= 1;       /* toggle parity — replaces (tx+...+phase)&1 */
        }

        row0 += pitch4;         /* skip 4 rows — additive, precomputed */
    }

    SDL_UnlockSurface(surf);
}

/* ============================================================
   Input
   ============================================================ */
static SDL_bool handle_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: return SDL_FALSE;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: return SDL_FALSE;
                    case SDLK_SPACE:
                    case SDLK_RIGHT:
                        current_scene = (current_scene + 1) % NUM_SCENES;
                        frame = 0; scene_dirty = SDL_TRUE;
                        log_scene_name(); break;
                    case SDLK_LEFT:
                        current_scene = (current_scene + NUM_SCENES - 1) % NUM_SCENES;
                        frame = 0; scene_dirty = SDL_TRUE;
                        log_scene_name(); break;
                    default: break;
                }
                break;
            default: break;
        }
    }
    return SDL_TRUE;
}

/* ============================================================
   Main
   ============================================================ */
int main(int argc, char *argv[])
{
    SDL_Window   *window   = NULL;
    SDL_Surface  *surface  = NULL;
    SDL_Renderer *renderer = NULL;
    Uint32 t0, t1, frames_since_log = 0;
    SDL_bool running;
    SDL_bool is_animated;
    (void)argc; (void)argv;

    build_rgb332_table();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("SDL2 Atari Demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit(); return 1;
    }

    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        SDL_Log("SDL_GetWindowSurface failed: %s", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) {
        SDL_Log("SDL_CreateSoftwareRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }

    SDL_Log("SDL2 Atari Demo (optimised) — %dx%d pitch=%d bpp=%d",
            SCREEN_W, SCREEN_H, surface->pitch,
            surface->format->BitsPerPixel);
    SDL_Log("SPACE/RIGHT=next  LEFT=prev  ESC=quit");
    log_scene_name();

    t0      = SDL_GetTicks();
    running = SDL_TRUE;

    while (running) {
        running = handle_events();

        is_animated = (current_scene == SCENE_PLASMA ||
                       current_scene == SCENE_CHECKERS);

        if (is_animated) {
            /* Direct pixel write — no SDL renderer involvement */
            switch (current_scene) {
                case SCENE_PLASMA:
                    draw_plasma(surface, frame);
                    break;
                case SCENE_CHECKERS:
                    draw_checkers(surface, frame);
                    break;
                default: break;
            }
            /* No SDL_RenderPresent — pixels already in surface */
        } else {
            /* Static scene: only draw on first visit (scene_dirty) */
            if (scene_dirty) {
                switch (current_scene) {
                    case SCENE_PALETTE:    draw_palette(renderer);    break;
                    case SCENE_GRADIENT:   draw_gradients(renderer);  break;
                    case SCENE_PRIMITIVES: draw_primitives(renderer); break;
                    default: break;
                }
                SDL_RenderPresent(renderer);
                scene_dirty = SDL_FALSE;
            }
        }

        SDL_UpdateWindowSurface(window);

        frame++;
        frames_since_log++;

        t1 = SDL_GetTicks();
        if (t1 - t0 >= 5000) {
            SDL_Log("Scene %d: %u frames / %u ms = %u fps",
                current_scene, frames_since_log,
                t1 - t0, (frames_since_log * 1000) / (t1 - t0));
            frames_since_log = 0;
            t0 = t1;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
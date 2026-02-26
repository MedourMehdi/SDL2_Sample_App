/*
 * key2joySMS_w740.c
 * SDL2 visual keyboard tester for SMS emulator key bindings.
 *
 * Compile:
 *   gcc key2joySMS_w740.c -o key2joySMS_w740 $(sdl2-config --cflags --libs)
 *
 *
 * What it tests:
 *   - All SMS joystick bindings (Z, X, arrows, R, Enter)
 *   - Ctrl+S / Ctrl+L (state save/load simulation)
 *   - Ctrl+Shift passthrough (empty branch — no crash)
 *   - Escape to quit (matching #ifndef EMSCRIPTEN path)
 *   - Key repeat: only "down" events should light up buttons (not repeats)
 *
 * Visual layout mirrors a Master System controller.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* =========================================================
   Screen & layout constants
   ========================================================= */
#define WIN_W        740
#define WIN_H        480
#define FPS          60

/* =========================================================
   State that mirrors on_key_event logic exactly
   ========================================================= */
typedef struct {
    /* Joypad buttons */
    int up, down, left, right;
    int btn_a, btn_b;
    int reset, pause;

    /* Meta */
    int ctrl_pressed;
    int shift_pressed;

    /* One-shot events (cleared after one frame render) */
    int save_triggered;
    int load_triggered;

    /* Last unknown key */
    SDL_Scancode last_unknown;
    int unknown_frame;            /* countdown to clear */

    /* Event log ring buffer */
    char log[8][80];
    int  log_head;

    /* Running flag */
    int running;
} TestState;

static TestState g;

/* =========================================================
   Minimal pixel font  (5×7, ASCII 32–127)
   We bake a tiny 1-bit font to avoid TTF dependency.
   ========================================================= */

/* Since we can't include a separate header, embed the font directly. */
/* We'll use SDL_RenderDrawPoint to draw characters. */

/* Simple 3×5 pixel font for digits/letters – just enough for labels */
static void draw_char(SDL_Renderer *r, int x, int y, char c, int scale,
                      Uint8 R, Uint8 G, Uint8 B)
{
    /* 5x7 bitmap font, chars 32-127 */
    static const Uint8 fnt[][5] = {
        {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
        {0x00,0x00,0x5F,0x00,0x00}, /* ! */
        {0x00,0x07,0x00,0x07,0x00}, /* " */
        {0x14,0x7F,0x14,0x7F,0x14}, /* # */
        {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
        {0x23,0x13,0x08,0x64,0x62}, /* % */
        {0x36,0x49,0x55,0x22,0x50}, /* & */
        {0x00,0x05,0x03,0x00,0x00}, /* ' */
        {0x00,0x1C,0x22,0x41,0x00}, /* ( */
        {0x00,0x41,0x22,0x1C,0x00}, /* ) */
        {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
        {0x08,0x08,0x3E,0x08,0x08}, /* + */
        {0x00,0x50,0x30,0x00,0x00}, /* , */
        {0x08,0x08,0x08,0x08,0x08}, /* - */
        {0x00,0x60,0x60,0x00,0x00}, /* . */
        {0x20,0x10,0x08,0x04,0x02}, /* / */
        {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
        {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
        {0x42,0x61,0x51,0x49,0x46}, /* 2 */
        {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
        {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
        {0x27,0x45,0x45,0x45,0x39}, /* 5 */
        {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
        {0x01,0x71,0x09,0x05,0x03}, /* 7 */
        {0x36,0x49,0x49,0x49,0x36}, /* 8 */
        {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
        {0x00,0x36,0x36,0x00,0x00}, /* : */
        {0x00,0x56,0x36,0x00,0x00}, /* ; */
        {0x08,0x14,0x22,0x41,0x00}, /* < */
        {0x14,0x14,0x14,0x14,0x14}, /* = */
        {0x00,0x41,0x22,0x14,0x08}, /* > */
        {0x02,0x01,0x51,0x09,0x06}, /* ? */
        {0x32,0x49,0x79,0x41,0x3E}, /* @ */
        {0x7E,0x11,0x11,0x11,0x7E}, /* A */
        {0x7F,0x49,0x49,0x49,0x36}, /* B */
        {0x3E,0x41,0x41,0x41,0x22}, /* C */
        {0x7F,0x41,0x41,0x22,0x1C}, /* D */
        {0x7F,0x49,0x49,0x49,0x41}, /* E */
        {0x7F,0x09,0x09,0x09,0x01}, /* F */
        {0x3E,0x41,0x49,0x49,0x7A}, /* G */
        {0x7F,0x08,0x08,0x08,0x7F}, /* H */
        {0x00,0x41,0x7F,0x41,0x00}, /* I */
        {0x20,0x40,0x41,0x3F,0x01}, /* J */
        {0x7F,0x08,0x14,0x22,0x41}, /* K */
        {0x7F,0x40,0x40,0x40,0x40}, /* L */
        {0x7F,0x02,0x04,0x02,0x7F}, /* M */
        {0x7F,0x04,0x08,0x10,0x7F}, /* N */
        {0x3E,0x41,0x41,0x41,0x3E}, /* O */
        {0x7F,0x09,0x09,0x09,0x06}, /* P */
        {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
        {0x7F,0x09,0x19,0x29,0x46}, /* R */
        {0x46,0x49,0x49,0x49,0x31}, /* S */
        {0x01,0x01,0x7F,0x01,0x01}, /* T */
        {0x3F,0x40,0x40,0x40,0x3F}, /* U */
        {0x1F,0x20,0x40,0x20,0x1F}, /* V */
        {0x3F,0x40,0x38,0x40,0x3F}, /* W */
        {0x63,0x14,0x08,0x14,0x63}, /* X */
        {0x07,0x08,0x70,0x08,0x07}, /* Y */
        {0x61,0x51,0x49,0x45,0x43}, /* Z */
        {0x00,0x7F,0x41,0x41,0x00}, /* [ */
        {0x02,0x04,0x08,0x10,0x20}, /* \ */
        {0x00,0x41,0x41,0x7F,0x00}, /* ] */
        {0x04,0x02,0x01,0x02,0x04}, /* ^ */
        {0x40,0x40,0x40,0x40,0x40}, /* _ */
        {0x00,0x01,0x02,0x04,0x00}, /* ` */
        {0x20,0x54,0x54,0x54,0x78}, /* a */
        {0x7F,0x48,0x44,0x44,0x38}, /* b */
        {0x38,0x44,0x44,0x44,0x20}, /* c */
        {0x38,0x44,0x44,0x48,0x7F}, /* d */
        {0x38,0x54,0x54,0x54,0x18}, /* e */
        {0x08,0x7E,0x09,0x01,0x02}, /* f */
        {0x08,0x14,0x54,0x54,0x3C}, /* g */
        {0x7F,0x08,0x04,0x04,0x78}, /* h */
        {0x00,0x44,0x7D,0x40,0x00}, /* i */
        {0x20,0x40,0x44,0x3D,0x00}, /* j */
        {0x7F,0x10,0x28,0x44,0x00}, /* k */
        {0x00,0x41,0x7F,0x40,0x00}, /* l */
        {0x7C,0x04,0x18,0x04,0x78}, /* m */
        {0x7C,0x08,0x04,0x04,0x78}, /* n */
        {0x38,0x44,0x44,0x44,0x38}, /* o */
        {0x7C,0x14,0x14,0x14,0x08}, /* p */
        {0x08,0x14,0x14,0x18,0x7C}, /* q */
        {0x7C,0x08,0x04,0x04,0x08}, /* r */
        {0x48,0x54,0x54,0x54,0x20}, /* s */
        {0x04,0x3F,0x44,0x40,0x20}, /* t */
        {0x3C,0x40,0x40,0x40,0x7C}, /* u */
        {0x1C,0x20,0x40,0x20,0x1C}, /* v */
        {0x3C,0x40,0x30,0x40,0x3C}, /* w */
        {0x44,0x28,0x10,0x28,0x44}, /* x */
        {0x0C,0x50,0x50,0x50,0x3C}, /* y */
        {0x44,0x64,0x54,0x4C,0x44}, /* z */
        {0x00,0x08,0x36,0x41,0x00}, /* { */
        {0x00,0x00,0x7F,0x00,0x00}, /* | */
        {0x00,0x41,0x36,0x08,0x00}, /* } */
        {0x08,0x08,0x2A,0x1C,0x08}, /* ~ */
        {0x08,0x1C,0x2A,0x08,0x08}, /* DEL */
    };

    if (c < 32 || c > 127) c = '?';
    const Uint8 *col = fnt[(int)(c - 32)];
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    for (int col_i = 0; col_i < 5; col_i++) {
        for (int row_i = 0; row_i < 7; row_i++) {
            if (col[col_i] & (1 << row_i)) {
                SDL_Rect px = { x + col_i*scale, y + row_i*scale, scale, scale };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

static void draw_text(SDL_Renderer *r, int x, int y, const char *str, int scale,
                      Uint8 R, Uint8 G, Uint8 B)
{
    for (; *str; str++, x += 6*scale)
        draw_char(r, x, y, *str, scale, R, G, B);
}

static void draw_textf(SDL_Renderer *r, int x, int y, int scale,
                       Uint8 R, Uint8 G, Uint8 B, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text(r, x, y, buf, scale, R, G, B);
}

/* =========================================================
   Button rendering helper
   ========================================================= */
typedef struct { int x, y, w, h; const char *label; int *state; } Button;

static void draw_button(SDL_Renderer *r, Button *b, 
                        Uint8 ar, Uint8 ag, Uint8 ab,  /* active color */
                        Uint8 ir, Uint8 ig, Uint8 ib)  /* inactive color */
{
    int active = b->state && *b->state;
    SDL_Rect rect = { b->x, b->y, b->w, b->h };

    /* Shadow */
    SDL_SetRenderDrawColor(r, 10, 10, 10, 200);
    SDL_Rect shadow = { b->x+3, b->y+3, b->w, b->h };
    SDL_RenderFillRect(r, &shadow);

    /* Fill */
    if (active) SDL_SetRenderDrawColor(r, ar, ag, ab, 255);
    else        SDL_SetRenderDrawColor(r, ir, ig, ib, 255);
    SDL_RenderFillRect(r, &rect);

    /* Border */
    SDL_SetRenderDrawColor(r, active ? 255 : 80, active ? 255 : 80, active ? 200 : 80, 255);
    SDL_RenderDrawRect(r, &rect);

    /* Glow when active */
    if (active) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, ar, ag, ab, 60);
        SDL_Rect glow = { b->x-3, b->y-3, b->w+6, b->h+6 };
        SDL_RenderFillRect(r, &glow);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }

    /* Label */
    int lx = b->x + b->w/2 - (int)SDL_strlen(b->label)*3;
    int ly = b->y + b->h/2 - 3;
    if (active) draw_text(r, lx, ly, b->label, 1, 10, 10, 10);
    else        draw_text(r, lx, ly, b->label, 1, 180, 180, 180);
}

/* =========================================================
   Log helpers
   ========================================================= */
static void log_event(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    SDL_vsnprintf(g.log[g.log_head % 8], 80, fmt, ap);
    va_end(ap);
    g.log_head++;
}

/* =========================================================
   on_key_event() — exact replica of the foreign code
   ========================================================= */
static void on_key_event(const SDL_KeyboardEvent *e)
{
    const int down  = (e->type == SDL_KEYDOWN);
    const int ctrl  = (e->keysym.mod & KMOD_CTRL)  > 0;
    const int shift = (e->keysym.mod & KMOD_SHIFT) > 0;

    /* Track modifier state for display */
    if (e->keysym.scancode == SDL_SCANCODE_LCTRL ||
        e->keysym.scancode == SDL_SCANCODE_RCTRL)
        g.ctrl_pressed = down;
    if (e->keysym.scancode == SDL_SCANCODE_LSHIFT ||
        e->keysym.scancode == SDL_SCANCODE_RSHIFT)
        g.shift_pressed = down;

    if (ctrl) {
        if (shift) {
            /* ---- Ctrl+Shift: empty branch (mirrors foreign code) ---- */
            if (down)
                log_event("CTRL+SHIFT+%s -> (no binding)", SDL_GetScancodeName(e->keysym.scancode));
        } else {
            switch (e->keysym.scancode) {
                case SDL_SCANCODE_S:
                    if (down) { g.save_triggered = 8; log_event("CTRL+S -> mgb_save_state_file()"); }
                    break;
                case SDL_SCANCODE_L:
                    if (down) { g.load_triggered = 8; log_event("CTRL+L -> mgb_load_state_file()"); }
                    break;
                default:
                    if (down)
                        log_event("CTRL+%s -> (no binding)", SDL_GetScancodeName(e->keysym.scancode));
                    break;
            }
        }
        return;
    }

    switch (e->keysym.scancode) {
        case SDL_SCANCODE_X:      g.btn_b  = down; log_event("%s: X     -> JOY1_B_BUTTON %s",   down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_Z:      g.btn_a  = down; log_event("%s: Z     -> JOY1_A_BUTTON %s",   down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_UP:     g.up     = down; log_event("%s: UP    -> JOY1_UP_BUTTON %s",  down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_DOWN:   g.down   = down; log_event("%s: DOWN  -> JOY1_DOWN_BUTTON %s",down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_LEFT:   g.left   = down; log_event("%s: LEFT  -> JOY1_LEFT_BUTTON %s",down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_RIGHT:  g.right  = down; log_event("%s: RIGHT -> JOY1_RIGHT_BUTTON %s",down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_R:      g.reset  = down; log_event("%s: R     -> RESET_BUTTON %s",    down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_RETURN: g.pause  = down; log_event("%s: ENTER -> PAUSE_BUTTON %s",    down?"DN":"UP", down?"ON":"OFF"); break;
        case SDL_SCANCODE_ESCAPE:
            if (down) { g.running = 0; log_event("ESCAPE -> running = false"); }
            break;
        default:
            if (down) {
                g.last_unknown = e->keysym.scancode;
                g.unknown_frame = 120;
                log_event("UNKNOWN: %s (scancode %d)", SDL_GetScancodeName(e->keysym.scancode), e->keysym.scancode);
            }
            break;
    }
}

/* =========================================================
   Render pass
   ========================================================= */
static void render(SDL_Renderer *r)
{
    /* ---------- Background ---------- */
    SDL_SetRenderDrawColor(r, 18, 18, 24, 255);
    SDL_RenderClear(r);

    /* Subtle scanline effect */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 30);
    for (int y = 0; y < WIN_H; y += 2) {
        SDL_RenderDrawLine(r, 0, y, WIN_W, y);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* ---------- Title ---------- */
    draw_text(r, 16, 10, "SMS EMULATOR - KEYBOARD BINDING TESTER", 2, 200, 60, 60);
    draw_text(r, 16, 30, "Press keys to test. ESC = quit.", 1, 100, 100, 120);

    /* ---------- Controller outline ---------- */
    int cx = 60, cy = 80;
    SDL_SetRenderDrawColor(r, 50, 50, 70, 255);
    SDL_Rect ctrl_bg = { cx - 10, cy - 10, 420, 220 };
    SDL_RenderFillRect(r, &ctrl_bg);
    SDL_SetRenderDrawColor(r, 80, 80, 110, 255);
    SDL_RenderDrawRect(r, &ctrl_bg);

    draw_text(r, cx, cy - 6, "CONTROLLER (port A + B)", 1, 80, 80, 120);

    /* D-PAD */
    int dx = cx + 30, dy = cy + 30;
    Button dpad[4] = {
        { dx+30,  dy,    40, 30, "UP",    &g.up    },
        { dx+30,  dy+60, 40, 30, "DOWN",  &g.down  },
        { dx,     dy+30, 40, 30, "LEFT",  &g.left  },
        { dx+60,  dy+30, 40, 30, "RIGHT", &g.right },
    };
    for (int i = 0; i < 4; i++)
        draw_button(r, &dpad[i], 60, 200, 60, 35, 55, 35);

    /* D-pad center */
    SDL_SetRenderDrawColor(r, 35, 55, 35, 255);
    SDL_Rect center = { dx+30, dy+30, 40, 30 };
    SDL_RenderFillRect(r, &center);

    /* RESET / PAUSE */
    Button sys[2] = {
        { cx+175, cy+55, 50, 22, "RESET",  &g.reset },
        { cx+240, cy+55, 50, 22, "PAUSE",  &g.pause },
    };
    for (int i = 0; i < 2; i++)
        draw_button(r, &sys[i], 200, 160, 60, 50, 45, 30);
    draw_text(r, cx+175, cy+43, "[R]", 1, 90, 90, 80);
    draw_text(r, cx+240, cy+43, "[Enter]", 1, 90, 90, 80);

    /* A / B buttons */
    Button ab[2] = {
        { cx+335, cy+60, 44, 44, "B",  &g.btn_b },
        { cx+285, cy+90, 44, 44, "A",  &g.btn_a },
    };
    draw_button(r, &ab[0], 200, 60,  60,  55, 30, 30);  /* B = red tint */
    draw_button(r, &ab[1], 60,  60,  200, 30, 30, 55);  /* A = blue tint */
    draw_text(r, cx+335, cy+48, "[X]", 1, 90, 90, 80);
    draw_text(r, cx+285, cy+78, "[Z]", 1, 90, 90, 80);

    /* ---------- Keyboard shortcut panel ---------- */
    int kx = 500, ky = 80;
    SDL_Rect kpanel = { kx-8, ky-8, 230, 140 };
    SDL_SetRenderDrawColor(r, 30, 28, 45, 255);
    SDL_RenderFillRect(r, &kpanel);
    SDL_SetRenderDrawColor(r, 70, 60, 110, 255);
    SDL_RenderDrawRect(r, &kpanel);

    draw_text(r, kx, ky-4, "SHORTCUTS", 1, 120, 90, 200);

    /* Ctrl indicator */
    SDL_Rect ctrl_ind = { kx, ky+14, 50, 14 };
    SDL_SetRenderDrawColor(r, g.ctrl_pressed ? 200 : 50, g.ctrl_pressed ? 100 : 50, g.ctrl_pressed ? 200 : 50, 255);
    SDL_RenderFillRect(r, &ctrl_ind);
    draw_text(r, kx+3, ky+17, "CTRL", 1, g.ctrl_pressed ? 10 : 180, 10, g.ctrl_pressed ? 10 : 180);

    /* Shift indicator */
    SDL_Rect shift_ind = { kx+60, ky+14, 55, 14 };
    SDL_SetRenderDrawColor(r, g.shift_pressed ? 200 : 50, g.shift_pressed ? 180 : 50, g.shift_pressed ? 50 : 50, 255);
    SDL_RenderFillRect(r, &shift_ind);
    draw_text(r, kx+63, ky+17, "SHIFT", 1, 10, g.shift_pressed ? 10 : 180, g.shift_pressed ? 10 : 180);

    draw_text(r, kx, ky+40, "Ctrl+S   = Save State", 1, 150, 150, 200);
    draw_text(r, kx, ky+54, "Ctrl+L   = Load State", 1, 150, 150, 200);
    draw_text(r, kx, ky+68, "Ctrl+Sh  = (empty/ok)", 1, 100, 150, 100);
    draw_text(r, kx, ky+82, "ESC      = Quit", 1, 200, 100, 100);

    /* Save/Load flash */
    if (g.save_triggered > 0) {
        SDL_Rect flash = { kx, ky+96, 100, 16 };
        SDL_SetRenderDrawColor(r, 40, 200, 80, 255);
        SDL_RenderFillRect(r, &flash);
        draw_text(r, kx+3, ky+99, "SAVE FIRED!", 1, 10, 10, 10);
        g.save_triggered--;
    }
    if (g.load_triggered > 0) {
        SDL_Rect flash = { kx+110, ky+96, 100, 16 };
        SDL_SetRenderDrawColor(r, 80, 160, 220, 255);
        SDL_RenderFillRect(r, &flash);
        draw_text(r, kx+113, ky+99, "LOAD FIRED!", 1, 10, 10, 10);
        g.load_triggered--;
    }

    /* ---------- Unknown key warning ---------- */
    if (g.unknown_frame > 0) {
        g.unknown_frame--;
        SDL_Rect warn = { 16, 310, 460, 18 };
        SDL_SetRenderDrawColor(r, 180, 80, 20, 255);
        SDL_RenderFillRect(r, &warn);
        draw_textf(r, 20, 314, 1, 240, 200, 100,
                   "UNHANDLED KEY: %s (scancode %d) -> default: break",
                   SDL_GetScancodeName(g.last_unknown), (int)g.last_unknown);
    }

    /* ---------- Event log ---------- */
    int lx = 16, ly = 340;
    draw_text(r, lx, ly, "EVENT LOG:", 1, 80, 80, 120);
    ly += 14;
    for (int i = 0; i < 8; i++) {
        int idx = (g.log_head - 8 + i + 8) % 8;
        int age = 8 - ((g.log_head - i - 1 + 8) % 8 + 1); /* 0=newest */
        int brightness = 200 - age * 20;
        if (brightness < 50) brightness = 50;
        if (g.log[idx][0])
            draw_text(r, lx, ly + i*14, g.log[idx], 1,
                      (Uint8)brightness, (Uint8)brightness, (Uint8)(brightness+20));
    }

    /* ---------- Cheat sheet ---------- */
    draw_text(r, 500, 250, "KEY MAP:", 1, 100, 100, 140);
    const char *map[][2] = {
        {"Z",       "A Button"},
        {"X",       "B Button"},
        {"Arrows",  "D-Pad"},
        {"R",       "Reset"},
        {"Enter",   "Pause"},
        {"Ctrl+S",  "Save State"},
        {"Ctrl+L",  "Load State"},
        {"ESC",     "Quit"},
    };
    for (int i = 0; i < 8; i++) {
        draw_textf(r, 500, 264+i*14, 1, 160, 130, 80, "%-10s -> %s", map[i][0], map[i][1]);
    }

    SDL_RenderPresent(r);
}

/* =========================================================
   Main
   ========================================================= */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "SMS Keyboard Binding Tester",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer *rend = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    if (!rend) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 1; }

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

    SDL_memset(&g, 0, sizeof(g));
    g.running = 1;

    printf("=== SMS Keyboard Binding Tester ===\n");
    printf("Test every binding from on_key_event().\n");
    printf("Press ESC to quit, or close the window.\n\n");

    Uint32 frame_ms = 1000 / FPS;

    while (g.running) {
        Uint32 t0 = SDL_GetTicks();
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { g.running = 0; break; }

            /* Feed keyboard events through the exact foreign handler */
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                /* 
                 * BUG CHECK: The foreign code does not guard against
                 * key repeat. SDL sends SDL_KEYDOWN with repeat=1
                 * for held keys. This tester highlights it:
                 */
                if (ev.key.repeat) {
                    log_event("REPEAT ignored: %s (repeat=%d)",
                              SDL_GetScancodeName(ev.key.keysym.scancode),
                              ev.key.repeat);
                    /* Forward anyway — lets you see if repeat causes issues */
                }
                on_key_event(&ev.key);
            }
        }

        render(rend);

        Uint32 elapsed = SDL_GetTicks() - t0;
        if (elapsed < frame_ms) SDL_Delay(frame_ms - elapsed);
    }

    printf("Exiting cleanly.\n");
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
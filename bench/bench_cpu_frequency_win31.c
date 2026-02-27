/*
 * SDL2FREQ - m68k CPU Frequency Benchmark
 * Dark Win 3.1 Style - GEM Safe - Optimized
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdint.h>
#include <osbind.h>

/* --------------------------------------------------------- */
/* --- Atari 200Hz system timer access --------------------- */
/* --------------------------------------------------------- */

static volatile long g_timer_value;

static void read_timer_supervisor(void)
{
    g_timer_value = *((volatile long*)0x4BA);
}

static long get_ticks(void)
{
    Supexec(read_timer_supervisor);
    return g_timer_value;
}

/* Tight benchmark loop */
static void cpu_bench(uint32_t count)
{
    asm volatile (
        "1:\n\t"
        "subq.l   #1,%0\n\t"
        "bne.b    1b\n\t"
        : "+r" (count)
        :
        : "cc"
    );
}

/* --------------------------------------------------------- */

#define WINDOW_W 400
#define WINDOW_H 300

/* Dark Win 3.1 palette */
#define COL_BG_R 160
#define COL_BG_G 160
#define COL_BG_B 160

#define COL_BTN_R 128
#define COL_BTN_G 128
#define COL_BTN_B 128

#define COL_LIGHT 220
#define COL_SHADOW 64

/* --------------------------------------------------------- */

static void draw_text(SDL_Renderer* ren, TTF_Font* font,
                      const char* text, int x, int y)
{
    SDL_Color black = {0,0,0,255};

    SDL_Surface* surf = TTF_RenderText_Solid(font, text, black);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect r = { x, y, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &r);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

/* --------------------------------------------------------- */

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

    SDL_Window* win = SDL_CreateWindow(
        "m68k CPU Frequency",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        0
    );

    if (!win) {
        printf("Window creation failed\n");
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_SOFTWARE
    );

    if (!ren) {
        printf("Renderer creation failed\n");
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("../font/font.ttf", 16);
    if (!font) {
        printf("Font load failed\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    char mhz_str[64] = "Click LAUNCH to start benchmark";
    char cpu_type[64] = "CPU: Waiting...";

    SDL_Rect btn = { 100, 200, 200, 40 };

    int running = 1;
    int needs_redraw = 1;

    while (running) {

        SDL_Event e;

        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_QUIT)
                running = 0;

            else if (e.type == SDL_MOUSEBUTTONDOWN) {

                if (e.button.x >= btn.x &&
                    e.button.x <= btn.x + btn.w &&
                    e.button.y >= btn.y &&
                    e.button.y <= btn.y + btn.h)
                {
                    const uint32_t ITER = 50000000UL;

                    /* Align to tick edge */
                    long t0 = get_ticks();
                    long t1 = get_ticks();
                    while (t1 == t0)
                        t1 = get_ticks();

                    t0 = get_ticks();
                    cpu_bench(ITER);
                    t1 = get_ticks();

                    long elapsed = t1 - t0;
                    if (elapsed <= 0) elapsed = 1;

                    unsigned long long numerator =
                        (unsigned long long)ITER * 4ULL * 200ULL;

                    unsigned long long denominator =
                        (unsigned long long)elapsed * 1000000ULL;

                    unsigned long long mhz =
                        numerator / denominator;

                    unsigned long long mhz_dec =
                        (numerator % denominator) * 100ULL / denominator;

                    snprintf(mhz_str, sizeof(mhz_str),
                             "%llu.%02llu MHz  (%ld ticks)",
                             mhz, mhz_dec, elapsed);

                    if (mhz < 10)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68000 @ 8 MHz");
                    else if (mhz < 20)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68000/10 @ 16 MHz");
                    else if (mhz < 30)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68020/030 @ 16-25 MHz");
                    else if (mhz < 40)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68030 @ 32 MHz");
                    else if (mhz < 50)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68040 @ 40 MHz");
                    else if (mhz < 70)
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: 68060 @ 50-66 MHz");
                    else
                        snprintf(cpu_type,sizeof(cpu_type),"CPU: Emulator");

                    needs_redraw = 1;
                }
            }

            else if (e.type == SDL_WINDOWEVENT)
                needs_redraw = 1;
        }

        if (needs_redraw) {

            /* Background */
            SDL_SetRenderDrawColor(ren, COL_BG_R, COL_BG_G, COL_BG_B, 255);
            SDL_RenderClear(ren);

            /* Button body */
            SDL_SetRenderDrawColor(ren, COL_BTN_R, COL_BTN_G, COL_BTN_B, 255);
            SDL_RenderFillRect(ren, &btn);

            /* 3D highlight */
            SDL_SetRenderDrawColor(ren, COL_LIGHT, COL_LIGHT, COL_LIGHT, 255);
            SDL_RenderDrawLine(ren, btn.x, btn.y,
                                     btn.x+btn.w, btn.y);
            SDL_RenderDrawLine(ren, btn.x, btn.y,
                                     btn.x, btn.y+btn.h);

            /* 3D shadow */
            SDL_SetRenderDrawColor(ren, COL_SHADOW, COL_SHADOW, COL_SHADOW, 255);
            SDL_RenderDrawLine(ren, btn.x, btn.y+btn.h,
                                     btn.x+btn.w, btn.y+btn.h);
            SDL_RenderDrawLine(ren, btn.x+btn.w, btn.y,
                                     btn.x+btn.w, btn.y+btn.h);

            draw_text(ren, font, "LAUNCH BENCH",
                      btn.x + 35, btn.y + 10);

            draw_text(ren, font, mhz_str,
                      60, 80);

            draw_text(ren, font, cpu_type,
                      60, 120);

            SDL_RenderPresent(ren);
            needs_redraw = 0;
        }

        SDL_Delay(10); /* Idle sleep */
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
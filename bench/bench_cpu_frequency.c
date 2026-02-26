/* Compiler: m68k-atari-mint-gcc -O2 -lSDL2 -lSDL2_ttf */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdint.h>
#include <osbind.h>

/* --- ORIGINAL LOGIC --- */
static volatile long g_timer_value;

static void read_timer_supervisor(void) {
    g_timer_value = *((long*)0x4BA);
}

static long get_ticks(void) {
    Supexec(read_timer_supervisor);
    return g_timer_value;
}

static void cpu_bench(uint32_t count) {
    asm volatile (
        "1:\n\t"
        "subq.l   #1,%0\n\t"
        "bne.b    1b\n\t"
        : "+r" (count)
        :
        : "cc"
    );
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Use Software Renderer to avoid continuous GPU polling
    SDL_Window* win = SDL_CreateWindow("m68k CPU Freq", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 300, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    TTF_Font* font = TTF_OpenFont("font.ttf", 18);

    char mhz_str[64] = "Click to Start";
    char cpu_type[64] = "Waiting...";
    SDL_Rect btn = { 100, 200, 200, 50 };
    int needs_redraw = 1;

    while (1) {
        SDL_Event e;
        // SDL_WaitEvent puts the app to sleep until a click occurs
        if (SDL_WaitEvent(&e)) {
            if (e.type == SDL_QUIT) break;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.x >= btn.x && e.button.x <= btn.x + btn.w &&
                    e.button.y >= btn.y && e.button.y <= btn.y + btn.h) {
                    
                    /* --- ORIGINAL BENCHMARK EXECUTION --- */
                    const uint32_t ITERATIONS = 50000000UL;
                    
                    // Align on tick edge
                    long t0 = get_ticks();
                    long t1 = get_ticks();
                    while (t1 == t0) { t1 = get_ticks(); }
                    
                    t0 = get_ticks();
                    cpu_bench(ITERATIONS);
                    t1 = get_ticks();
                    
                    long elapsed = t1 - t0;
                    if (elapsed <= 0) elapsed = 1;

                    // Original 64-bit calculation
                    unsigned long long numerator = (unsigned long long)ITERATIONS * 4ULL * 200ULL;
                    unsigned long long denominator = (unsigned long long)elapsed * 1000000ULL;
                    unsigned long long mhz = numerator / denominator;
                    unsigned long long mhz_dec = (numerator % denominator) * 100ULL / denominator;

                    sprintf(mhz_str, "%llu.%02llu MHz (%ld Ticks)", mhz, mhz_dec, elapsed);

                    // Original CPU detection thresholds
                    if (mhz < 10)       sprintf(cpu_type, "CPU: 68000 @ 8 MHz");
                    else if (mhz < 20)  sprintf(cpu_type, "CPU: 68000/10 @ 16 MHz");
                    else if (mhz < 30)  sprintf(cpu_type, "CPU: 68020/30 @ 16-25 MHz");
                    else if (mhz < 40)  sprintf(cpu_type, "CPU: 68030 @ 32 MHz");
                    else if (mhz < 50)  sprintf(cpu_type, "CPU: 68040 @ 40 MHz");
                    else if (mhz < 70)  sprintf(cpu_type, "CPU: 68060 @ 50-66 MHz");
                    else                sprintf(cpu_type, "CPU: Ultra Emulator");

                    needs_redraw = 1;
                }
            }
            if (e.type == SDL_WINDOWEVENT) needs_redraw = 1;
        }

        if (needs_redraw) {
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
            SDL_RenderClear(ren);

            // Draw Button
            SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            SDL_RenderFillRect(ren, &btn);
            
            SDL_Color white = {255, 255, 255, 255};
            
            // Draw Rocket Icon (^) and Launch Text in Button
            SDL_Surface* s_btn = TTF_RenderText_Blended(font, "^ LAUNCH BENCH", white);
            SDL_Texture* t_btn = SDL_CreateTextureFromSurface(ren, s_btn);
            SDL_Rect r_btn = { btn.x + (btn.w - s_btn->w)/2, btn.y + (btn.h - s_btn->h)/2, s_btn->w, s_btn->h };
            SDL_RenderCopy(ren, t_btn, NULL, &r_btn);

            // Draw Frequency
            SDL_Surface* s_mhz = TTF_RenderText_Blended(font, mhz_str, white);
            SDL_Texture* t_mhz = SDL_CreateTextureFromSurface(ren, s_mhz);
            SDL_Rect r_mhz = { (400 - s_mhz->w)/2, 60, s_mhz->w, s_mhz->h };
            SDL_RenderCopy(ren, t_mhz, NULL, &r_mhz);

            // Draw CPU Type
            SDL_Surface* s_cpu = TTF_RenderText_Blended(font, cpu_type, white);
            SDL_Texture* t_cpu = SDL_CreateTextureFromSurface(ren, s_cpu);
            SDL_Rect r_cpu = { (400 - s_cpu->w)/2, 110, s_cpu->w, s_cpu->h };
            SDL_RenderCopy(ren, t_cpu, NULL, &r_cpu);

            SDL_RenderPresent(ren);

            // Clean textures to avoid leaks
            SDL_DestroyTexture(t_btn); SDL_FreeSurface(s_btn);
            SDL_DestroyTexture(t_mhz); SDL_FreeSurface(s_mhz);
            SDL_DestroyTexture(t_cpu); SDL_FreeSurface(s_cpu);
            needs_redraw = 0;
        }
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

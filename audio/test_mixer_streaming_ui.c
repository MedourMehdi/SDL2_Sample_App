/* * Retro SDL_mixer Stream Player (Low Memory / Atari ST Optimized) */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

/* --- Constants --- */
#define WIN_W 320
#define WIN_H 120
#define COL_BG       0xC0C0C0 
#define COL_BTN_FACE 0xD0D0D0 
#define COL_LIGHT    0xFFFFFF 
#define COL_SHADOW   0x808080 
#define COL_DARK     0x000000 
#define COL_TEXT     0x000000
#define COL_BLUE     0x000080 
#define COL_PROGRESS 0x000080 

#define BTN_SIZE     26      
#define BAR_H        18
#define BAR_Y        45
#define MARGIN       10

struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    Mix_Music* music;  /* Replaces the raw buffer pointers */
    int is_playing;    /* UI State tracker */
    int is_paused;
} app;

/* --- Drawing Helpers (Identical to your version) --- */
void set_color(Uint32 color) {
    SDL_SetRenderDrawColor(app.renderer, 
        (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
}

void draw_bevel(int x, int y, int w, int h, int sunken) {
    set_color(sunken ? COL_SHADOW : COL_LIGHT);
    SDL_RenderDrawLine(app.renderer, x, y, x + w - 2, y);
    SDL_RenderDrawLine(app.renderer, x, y, x, y + h - 2);
    set_color(sunken ? COL_LIGHT : COL_SHADOW);
    SDL_RenderDrawLine(app.renderer, x + w - 1, y, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(app.renderer, x, y + h - 1, x + w - 1, y + h - 1);
    set_color(sunken ? COL_DARK : COL_SHADOW);
    SDL_RenderDrawLine(app.renderer, x + 1, y + 1, x + w - 3, y + 1);
    SDL_RenderDrawLine(app.renderer, x + 1, y + 1, x + 1, y + h - 3);
    set_color(sunken ? COL_BG : COL_DARK);
    SDL_RenderDrawLine(app.renderer, x + w - 2, y + 1, x + w - 2, y + h - 2);
    SDL_RenderDrawLine(app.renderer, x + 1, y + h - 2, x + w - 2, y + h - 2);
}

void draw_symbol(int x, int y, int w, int h, char type) {
    int i, h_off;
    int center_y = y + (h / 2);
    int center_x = x + (w / 2);
    set_color(COL_TEXT);

    if (type == 'P') { 
        int triangle_w = 10;
        int start_x = center_x - 3; 
        for(i = 0; i < triangle_w; i++) {
            h_off = (triangle_w - i) / 2;
            SDL_RenderDrawLine(app.renderer, start_x + i, center_y - h_off, start_x + i, center_y + h_off);
        }
    } else if (type == 'S') { 
        SDL_Rect r = {center_x - 6, center_y - 6, 12, 12};
        SDL_RenderFillRect(app.renderer, &r);
    } else if (type == 'F') { 
        int triangle_w = 8;
        int start_x = center_x - 5;
        SDL_Rect bar = {center_x + 4, center_y - 6, 3, 13};
        for(i = 0; i < triangle_w; i++) {
            h_off = (triangle_w - i) / 2;
            SDL_RenderDrawLine(app.renderer, start_x + i, center_y - h_off, start_x + i, center_y + h_off);
        }
        SDL_RenderFillRect(app.renderer, &bar);
    }
}

void draw_button(int x, int y, int w, int h, char symbol, int pressed) {
    SDL_Rect r = {x, y, w, h};
    set_color(pressed ? COL_BG : COL_BTN_FACE);
    SDL_RenderFillRect(app.renderer, &r);
    draw_bevel(x, y, w, h, pressed);
    draw_symbol(x + (pressed ? 1 : 0), y + (pressed ? 1 : 0), w, h, symbol);
}

void draw_ui() {
    SDL_Rect rect;
    int btn_y = WIN_H - BTN_SIZE - MARGIN;
    
    set_color(COL_BG);
    SDL_RenderClear(app.renderer);

    /* Blue Bar */
    rect.x = 3; rect.y = 3; rect.w = WIN_W - 6; rect.h = 18;
    set_color(COL_BLUE);
    SDL_RenderFillRect(app.renderer, &rect);
    draw_bevel(0, 0, WIN_W, WIN_H, 0);

    /* Progress Bar BG */
    rect.x = MARGIN; rect.y = BAR_Y; rect.w = WIN_W - (MARGIN * 2); rect.h = BAR_H;
    set_color(COL_BG); 
    SDL_RenderFillRect(app.renderer, &rect);
    draw_bevel(rect.x, rect.y, rect.w, rect.h, 1);

    /* Progress Fill 
       NOTE: When streaming, we don't always know total length in bytes.
       We visualize activity with a 'pulse' instead of a byte-counter. */
    if (Mix_PlayingMusic() && !Mix_PausedMusic()) {
        /* Simple animation based on time to show system is alive */
        int pulse = (SDL_GetTicks() / 5) % (rect.w - 4);
        SDL_Rect p = { MARGIN + 2, BAR_Y + 2, pulse, BAR_H - 4 };
        set_color(COL_PROGRESS);
        SDL_RenderFillRect(app.renderer, &p);
    }

    /* Logic to make the Play button look "pressed" when active */
    int playing_visual = (Mix_PlayingMusic() && !Mix_PausedMusic());

    draw_button(MARGIN, btn_y, 50, BTN_SIZE, 'P', playing_visual);
    draw_button(MARGIN + 55, btn_y, 40, BTN_SIZE, 'S', 0);
    draw_button(WIN_W - MARGIN - 40, btn_y, 40, BTN_SIZE, 'F', 0);

    SDL_RenderPresent(app.renderer);
}

int main(int argc, char* argv[]) {
    SDL_Event e;
    int running = 1;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;

    /* 1. Initialize Mixer for STREAMING (Buffers 2048 bytes at a time) */
    /* AUDIO_S16SYS automatically handles Big Endian for Atari ST */
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 4096) < 0) {
        printf("Mixer Error: %s\n", Mix_GetError());
        return 1;
    }

    /* 2. Load Music (Streams from disk, low RAM usage) */
    /* Mix_LoadMUS automatically detects AIFF and handles endianness */
    app.music = Mix_LoadMUS(argv[1]);
    if (!app.music) {
        printf("Load Error: %s\n", Mix_GetError());
        /* Proceed to loop so user can see error in console, or close manually */
    }

    app.window = SDL_CreateWindow("RetroWAV Stream", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);

    draw_ui();

    while (running) {
        if (SDL_WaitEventTimeout(&e, 100)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x, y = e.button.y;
                int btn_y = WIN_H - BTN_SIZE - MARGIN;
                
                if (y >= btn_y && y <= btn_y + BTN_SIZE) {
                    /* PLAY */
                    if (x >= MARGIN && x <= MARGIN + 50) { 
                        if (!Mix_PlayingMusic()) {
                            Mix_PlayMusic(app.music, 0); 
                        } else if (Mix_PausedMusic()) {
                            Mix_ResumeMusic();
                        }
                        draw_ui();
                    } 
                    /* STOP */
                    else if (x >= MARGIN + 55 && x <= MARGIN + 95) { 
                        Mix_HaltMusic();
                        draw_ui();
                    } 
                    /* FORWARD */
                    else if (x >= WIN_W - MARGIN - 40 && x <= WIN_W - MARGIN) { 
                        /* Streaming Seek: Jump forward 5 seconds */
                        /* Note: GetMusicPosition support varies by SDL version */
                        double current = Mix_GetMusicPosition(app.music);
                        Mix_SetMusicPosition(current + 5.0);
                        draw_ui();
                    }
                }
            } else if (e.type == SDL_MOUSEBUTTONUP) draw_ui();
        }
        
        /* Redraw while playing to update progress bar */
        if (Mix_PlayingMusic() && !Mix_PausedMusic()) {
            draw_ui();
        }
    }

    /* Cleanup */
    Mix_FreeMusic(app.music);
    Mix_CloseAudio();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
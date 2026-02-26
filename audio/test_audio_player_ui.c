/* * Retro SDL WAV Player v2.3 (GEM-Compatible) */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

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

/* --- Global State --- */
struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    Uint8* audio_start;
    Uint8* audio_pos;
    Uint32 audio_len;
    Uint32 total_len;
    int is_playing;
    Uint32 bytes_per_pixel; 
} app;

void audio_callback(void* userdata, Uint8* stream, int len) {
    (void)userdata;
    SDL_memset(stream, 0, len);
    if (app.audio_len == 0) return;
    len = (len > (int)app.audio_len) ? (int)app.audio_len : len;
    SDL_MixAudio(stream, app.audio_pos, len, SDL_MIX_MAXVOLUME);
    app.audio_pos += len;
    app.audio_len -= len;
    
    if (app.audio_len == 0) {
        app.is_playing = 0;
        SDL_PauseAudio(1);
        app.audio_pos = app.audio_start;
        app.audio_len = app.total_len;
    }
}

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

    if (type == 'P') { /* Play: Triangle points RIGHT */
        int triangle_w = 10;
        int start_x = center_x - 3; 
        for(i = 0; i < triangle_w; i++) {
            h_off = (triangle_w - i) / 2;
            SDL_RenderDrawLine(app.renderer, start_x + i, center_y - h_off, start_x + i, center_y + h_off);
        }
    } else if (type == 'S') { /* Stop: Square */
        SDL_Rect r = {center_x - 6, center_y - 6, 12, 12};
        SDL_RenderFillRect(app.renderer, &r);
    } else if (type == 'F') { /* Forward: Triangle Right + Bar */
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
    int max_w = WIN_W - (MARGIN*2) - 4;
    
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

    /* Progress Fill */
    if (app.total_len > 0) {
        Uint32 played = app.total_len - app.audio_len;
        int progress_w = 0;
        
        if (app.bytes_per_pixel > 0) {
            progress_w = (int)(played / app.bytes_per_pixel);
        } else {
            progress_w = (played * max_w) / app.total_len;
        }

        if (progress_w > max_w) progress_w = max_w;

        rect.x = MARGIN + 2; rect.y = BAR_Y + 2; 
        rect.w = progress_w; rect.h = BAR_H - 4;
        set_color(COL_PROGRESS);
        SDL_RenderFillRect(app.renderer, &rect);
    }

    draw_button(MARGIN, btn_y, 50, BTN_SIZE, 'P', app.is_playing);
    draw_button(MARGIN + 55, btn_y, 40, BTN_SIZE, 'S', 0);
    draw_button(WIN_W - MARGIN - 40, btn_y, 40, BTN_SIZE, 'F', 0);

    SDL_RenderPresent(app.renderer);
}

int main(int argc, char* argv[]) {
    SDL_AudioSpec wav_spec;
    Uint32 wav_length;
    SDL_Event e;
    int running = 1;
    
    if (argc < 2) {
        printf("Usage: %s <file.wav>\n", argv[0]);
        return 1;
    }
    
    /* ============================================================
       Set SDL hints BEFORE SDL_Init to prevent OpenGL
       attempts and ensure software rendering on GEM
       ============================================================ */
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
    if (SDL_LoadWAV(argv[1], &wav_spec, &app.audio_start, &wav_length) == NULL) {
        fprintf(stderr, "SDL_LoadWAV failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    wav_spec.callback = audio_callback;
    wav_spec.samples = 4096 * 2;
    wav_spec.userdata = NULL;
    app.audio_pos = app.audio_start;
    app.audio_len = wav_length;
    app.total_len = wav_length;
    app.is_playing = 0;

    /* Calculate bytes per pixel for progress bar */
    {
        int bar_width_px = WIN_W - (MARGIN * 2) - 4;
        app.bytes_per_pixel = app.total_len / bar_width_px;
        if (app.total_len < (Uint32)bar_width_px) app.bytes_per_pixel = 0;
    }

    if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
        fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
        SDL_FreeWAV(app.audio_start);
        SDL_Quit();
        return 1;
    }
    
    /* ============================================================
       Create window WITHOUT showing it first
       This prevents the "flash" issue on GEM
       ============================================================ */
    app.window = SDL_CreateWindow("RetroWAV v2.3", 
                                   SDL_WINDOWPOS_CENTERED, 
                                   SDL_WINDOWPOS_CENTERED, 
                                   WIN_W, WIN_H, 
                                   SDL_WINDOW_HIDDEN);
    
    if (!app.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_CloseAudio();
        SDL_FreeWAV(app.audio_start);
        SDL_Quit();
        return 1;
    }
    
    /* ============================================================
       Create software renderer explicitly
       SDL_RENDERER_SOFTWARE flag ensures no OpenGL attempt
       ============================================================ */
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
    
    if (!app.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_CloseAudio();
        SDL_FreeWAV(app.audio_start);
        SDL_Quit();
        return 1;
    }

    /* ============================================================
       Draw UI content BEFORE making window visible
       This ensures window appears with content already rendered
       ============================================================ */
    draw_ui();
    
    /* Now show the window - it already has content */
    SDL_ShowWindow(app.window);

    /* Event loop */
    while (running) {
        if (SDL_WaitEventTimeout(&e, 100)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x, y = e.button.y;
                int btn_y = WIN_H - BTN_SIZE - MARGIN;
                
                if (y >= btn_y && y <= btn_y + BTN_SIZE) {
                    /* Play button */
                    if (x >= MARGIN && x <= MARGIN + 50) {
                        if (!app.is_playing) { 
                            SDL_PauseAudio(0); 
                            app.is_playing = 1; 
                            draw_ui(); 
                        }
                    } 
                    /* Stop button */
                    else if (x >= MARGIN + 55 && x <= MARGIN + 95) {
                        SDL_PauseAudio(1); 
                        app.is_playing = 0;
                        app.audio_pos = app.audio_start; 
                        app.audio_len = app.total_len;
                        draw_ui();
                    } 
                    /* Forward button */
                    else if (x >= WIN_W - MARGIN - 40 && x <= WIN_W - MARGIN) {
                        SDL_LockAudio();
                        Uint32 skip_amt = app.total_len / 10;
                        if (app.audio_len > skip_amt) {
                            app.audio_pos += skip_amt;
                            app.audio_len -= skip_amt;
                        } else {
                            app.audio_len = 0;
                        }
                        SDL_UnlockAudio();
                        draw_ui();
                    }
                }
            } 
            else if (e.type == SDL_MOUSEBUTTONUP) {
                draw_ui();
            }
        }
        
        /* Update UI while playing to show progress */
        if (app.is_playing) {
            draw_ui();
        }
    }

    /* Cleanup */
    SDL_CloseAudio();
    SDL_FreeWAV(app.audio_start);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    
    return 0;
}
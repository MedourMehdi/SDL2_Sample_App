#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>

#define WIN_W 640
#define WIN_H 200

/* Audio shared state */
static uint8_t *audio_buf = NULL;
static uint32_t audio_len = 0;
static volatile uint32_t audio_pos = 0;

/* Peak meters (0-100) */
static volatile uint8_t level_l = 0;
static volatile uint8_t level_r = 0;

/* Player state */
static volatile uint8_t is_playing = 1;
static volatile uint8_t is_paused = 0;

/* Button rectangles */
static SDL_Rect play_button = {WIN_W/2 - 90, WIN_H - 50, 50, 30};
static SDL_Rect pause_button = {WIN_W/2 - 30, WIN_H - 50, 50, 30};
static SDL_Rect stop_button = {WIN_W/2 + 30, WIN_H - 50, 50, 30};

/* CRITICAL: Safe audio callback with bounds checking */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    uint32_t remaining, to_copy;
    int16_t *samples;
    int count, i;
    uint16_t peak_l, peak_r;
    uint16_t l, r;
    
    /* Safety checks */
    if (!stream || len <= 0) return;
    
    if (!audio_buf || audio_len == 0 || !is_playing || is_paused) {
        SDL_memset(stream, 0, len);
        level_l = level_r = 0;
        return;
    }

    /* Calculate safe copy amount */
    remaining = audio_len - audio_pos;
    to_copy = (remaining > (uint32_t)len) ? (uint32_t)len : remaining;

    /* Copy audio data */
    if (to_copy > 0) {
        SDL_memcpy(stream, audio_buf + audio_pos, to_copy);
        audio_pos += to_copy;
    }

    /* Fill remainder with silence */
    if (to_copy < (uint32_t)len) {
        SDL_memset(stream + to_copy, 0, len - to_copy);
    }

    /* Compute peaks using integer math */
    samples = (int16_t *)stream;
    count = to_copy / sizeof(int16_t);
    
    peak_l = 0;
    peak_r = 0;

    for (i = 0; i + 1 < count; i += 2) {
        l = abs((int16_t)SDL_SwapLE16(samples[i]));
        r = abs((int16_t)SDL_SwapLE16(samples[i + 1]));
        
        if (l > peak_l) peak_l = l;
        if (r > peak_r) peak_r = r;
    }

    /* Scale to 0-100 range */
    level_l = (uint8_t)((peak_l * 100) >> 15);
    level_r = (uint8_t)((peak_r * 100) >> 15);
    
    /* Loop at end of file */
    if (audio_pos >= audio_len) {
        audio_pos = 0;
    }
}

int main(int argc, char *argv[])
{
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_AudioSpec wav_spec, want, have;
    SDL_AudioDeviceID dev = 0;
    int running = 1;
    SDL_Event e;
    uint8_t draw_l = 0, draw_r = 0;
    uint8_t need_full_render = 1;
    uint8_t prev_is_playing = 1;
    uint8_t prev_is_paused = 0;
    
    if (argc < 2) {
        printf("Usage: %s file.wav\n", argv[0]);
        return 1;
    }

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Create window */
    win = SDL_CreateWindow(
        "Vintage Audio Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0
    );
    
    if (!win) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* Load WAV */
    SDL_Log("Loading WAV file: %s", argv[1]);
    
    if (!SDL_LoadWAV(argv[1], &wav_spec, &audio_buf, &audio_len)) {
        printf("Failed to load WAV: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_Log("WAV loaded: freq=%d, channels=%d, format=0x%04X, len=%u bytes",
            wav_spec.freq, wav_spec.channels, wav_spec.format, audio_len);

    audio_pos = 0;

    /* Setup audio device */
    want = wav_spec;
    want.callback = audio_callback;
    want.userdata = NULL;
    
    /* Request smaller buffer for lower latency on 68000 */
    if (want.samples == 0 || want.samples > 2048) {
        want.samples = 1024;
    }

    SDL_Log("Opening audio device...");
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (!dev) {
        printf("Audio open failed: %s\n", SDL_GetError());
        SDL_FreeWAV(audio_buf);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_Log("Audio device opened: freq=%d, channels=%d, format=0x%04X, samples=%d",
            have.freq, have.channels, have.format, have.samples);

    /* Start playback */
    SDL_PauseAudioDevice(dev, 0);
    SDL_Log("Audio playback started");

    /* Main loop */
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x;
                int y = e.button.y;
                
                SDL_Log("Mouse click at (%d, %d)", x, y);
                
                /* Play button */
                if (x >= play_button.x && x <= play_button.x + play_button.w &&
                    y >= play_button.y && y <= play_button.y + play_button.h) {
                    SDL_Log("Play button clicked");
                    is_playing = 1;
                    is_paused = 0;
                    need_full_render = 1;
                }
                /* Pause button */
                else if (x >= pause_button.x && x <= pause_button.x + pause_button.w &&
                         y >= pause_button.y && y <= pause_button.y + pause_button.h) {
                    is_paused = !is_paused;
                    SDL_Log("Pause button clicked (state: %s)", 
                            is_paused ? "paused" : "playing");
                    need_full_render = 1;
                }
                /* Stop button */
                else if (x >= stop_button.x && x <= stop_button.x + stop_button.w &&
                         y >= stop_button.y && y <= stop_button.y + stop_button.h) {
                    SDL_Log("Stop button clicked");
                    is_playing = 0;
                    is_paused = 0;
                    audio_pos = 0;
                    need_full_render = 1;
                }
            }
        }

        /* Smooth the meters using integer math */
        draw_l = (draw_l * 9 + level_l) / 10;
        draw_r = (draw_r * 9 + level_r) / 10;
        
        /* Check for state changes */
        if (is_playing != prev_is_playing || is_paused != prev_is_paused) {
            prev_is_playing = is_playing;
            prev_is_paused = is_paused;
            need_full_render = 1;
        }

        /* Full render when needed */
        if (need_full_render) {
            int meter_width = 200;
            int meter_height = 20;
            int meter_x = (WIN_W - meter_width) / 2;
            int meter_y = 30;
            int waveform_height = 60;
            int waveform_y = 80;
            
            /* Grey background */
            SDL_SetRenderDrawColor(ren, 180, 180, 180, 255);
            SDL_RenderClear(ren);

            /* Waveform background */
            SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
            SDL_Rect waveform_bg = {meter_x, waveform_y, meter_width, waveform_height};
            SDL_RenderFillRect(ren, &waveform_bg);
            
            /* Meter backgrounds */
            SDL_Rect l_bg = {meter_x, meter_y, meter_width/2 - 5, meter_height};
            SDL_Rect r_bg = {meter_x + meter_width/2 + 5, meter_y, meter_width/2 - 5, meter_height};
            SDL_RenderFillRect(ren, &l_bg);
            SDL_RenderFillRect(ren, &r_bg);
            
            /* Draw buttons */
            SDL_SetRenderDrawColor(ren, is_playing && !is_paused ? 100 : 150, 100, 100, 255);
            SDL_RenderFillRect(ren, &play_button);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderDrawRect(ren, &play_button);
            
            SDL_SetRenderDrawColor(ren, is_paused ? 100 : 150, 100, 100, 255);
            SDL_RenderFillRect(ren, &pause_button);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderDrawRect(ren, &pause_button);
            SDL_Rect pause_bar1 = {pause_button.x + 15, pause_button.y + 8, 5, 14};
            SDL_Rect pause_bar2 = {pause_button.x + 30, pause_button.y + 8, 5, 14};
            SDL_RenderFillRect(ren, &pause_bar1);
            SDL_RenderFillRect(ren, &pause_bar2);
            
            SDL_SetRenderDrawColor(ren, !is_playing ? 100 : 150, 100, 100, 255);
            SDL_RenderFillRect(ren, &stop_button);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderDrawRect(ren, &stop_button);
            SDL_Rect stop_square = {stop_button.x + 12, stop_button.y + 8, 26, 14};
            SDL_RenderFillRect(ren, &stop_square);
            
            need_full_render = 0;
        }

        /* Always draw meters */
        int meter_width = 200;
        int meter_height = 20;
        int meter_x = (WIN_W - meter_width) / 2;
        int meter_y = 30;
        
        /* Clear meter areas */
        SDL_Rect l_clear = {meter_x, meter_y, meter_width/2 - 5, meter_height};
        SDL_Rect r_clear = {meter_x + meter_width/2 + 5, meter_y, meter_width/2 - 5, meter_height};
        SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
        SDL_RenderFillRect(ren, &l_clear);
        SDL_RenderFillRect(ren, &r_clear);
        
        /* Draw meter levels */
        int l_width = (draw_l * (meter_width/2 - 5)) / 100;
        int r_width = (draw_r * (meter_width/2 - 5)) / 100;
        SDL_Rect l_level = {meter_x, meter_y, l_width, meter_height};
        SDL_Rect r_level = {meter_x + meter_width/2 + 5, meter_y, r_width, meter_height};
        SDL_SetRenderDrawColor(ren, 0, 200, 0, 255);
        SDL_RenderFillRect(ren, &l_level);
        SDL_RenderFillRect(ren, &r_level);

        SDL_RenderPresent(ren);
        SDL_Delay(33);  /* ~30 FPS for 68000 */
    }

    /* Cleanup */
    SDL_Log("Shutting down...");
    
    if (dev) {
        SDL_Log("Closing audio device...");
        SDL_CloseAudioDevice(dev);
        SDL_Delay(100);  /* Extra delay for safety */
    }
    
    if (audio_buf) {
        SDL_Log("Freeing WAV buffer...");
        SDL_FreeWAV(audio_buf);
        audio_buf = NULL;
    }
    
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    
    SDL_Quit();
    SDL_Log("Shutdown complete");

    return 0;
}
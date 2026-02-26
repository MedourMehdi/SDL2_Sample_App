/* * Retro SDL AIFF/WAV Player v2.3 (Direct AIFF Support) */

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

/* --- AIFF Loader --- */
typedef struct {
    Uint32 sampleRate;      /* Sample rate in Hz */
    Uint16 numChannels;     /* 1 = mono, 2 = stereo */
    Uint32 numSampleFrames; /* Number of sample frames */
    Uint16 sampleSize;      /* Bits per sample */
} AIFFCommon;

/* Read big-endian values */
static Uint32 read_be32(SDL_RWops* src) {
    Uint32 value;
    SDL_RWread(src, &value, 4, 1);
    return SDL_SwapBE32(value);
}

static Uint16 read_be16(SDL_RWops* src) {
    Uint16 value;
    SDL_RWread(src, &value, 2, 1);
    return SDL_SwapBE16(value);
}

/* Parse 80-bit extended float sample rate (simplified) */
static Uint32 read_extended(SDL_RWops* src) {
    Uint8 buf[10];
    SDL_RWread(src, buf, 10, 1);
    
    /* Quick decode for common rates */
    Uint16 exponent = ((buf[0] & 0x7F) << 8) | buf[1];
    Uint32 mantissa = ((Uint32)buf[2] << 24) | ((Uint32)buf[3] << 16) | 
                      ((Uint32)buf[4] << 8) | buf[5];
    
    if (exponent == 0 && mantissa == 0) return 0;
    
    /* Common sample rates shortcut */
    if (exponent == 0x400E && (mantissa >> 17) == 0x5622) return 44100;
    if (exponent == 0x400D && (mantissa >> 17) == 0x5622) return 22050;
    if (exponent == 0x400E && (mantissa >> 17) == 0x2E11) return 48000;
    
    /* General formula (simplified) */
    int exp_val = (int)exponent - 16383 - 31;
    Uint32 rate = mantissa;
    if (exp_val > 0) rate <<= exp_val;
    else if (exp_val < 0) rate >>= -exp_val;
    
    return rate;
}

SDL_AudioSpec* Load_AIFF(SDL_RWops* src, int freesrc, SDL_AudioSpec* spec, 
                         Uint8** audio_buf, Uint32* audio_len) {
    char chunk_id[4];
    Uint32 chunk_size;
    char form_type[4];
    AIFFCommon common = {0};
    Uint8* ssnd_data = NULL;
    Uint32 ssnd_size = 0;
    int found_comm = 0, found_ssnd = 0;
    
    /* Read FORM header */
    if (SDL_RWread(src, chunk_id, 4, 1) != 1) {
        SDL_SetError("Failed to read FORM chunk ID");
        goto fail;
    }
    
    if (memcmp(chunk_id, "FORM", 4) != 0) {
        SDL_SetError("Not an AIFF file (no FORM header)");
        goto fail;
    }
    
    chunk_size = read_be32(src);
    
    if (SDL_RWread(src, form_type, 4, 1) != 1) {
        SDL_SetError("Failed to read form type");
        goto fail;
    }
    
    if (memcmp(form_type, "AIFF", 4) != 0 && memcmp(form_type, "AIFC", 4) != 0) {
        SDL_SetError("Not an AIFF/AIFC file (form type: %.4s)", form_type);
        goto fail;
    }
    
    printf("AIFF file detected, size=%u, type=%.4s\n", chunk_size, form_type);
    
    /* Parse chunks */
    while (SDL_RWread(src, chunk_id, 4, 1) == 1) {
        if (SDL_RWread(src, &chunk_size, 4, 1) != 1) {
            break; /* End of file */
        }
        chunk_size = SDL_SwapBE32(chunk_size);
        
        printf("Chunk: %.4s, size=%u\n", chunk_id, chunk_size);
        
        if (memcmp(chunk_id, "COMM", 4) == 0) {
            if (chunk_size < 18) {
                SDL_SetError("Invalid COMM chunk size");
                goto fail;
            }
            common.numChannels = read_be16(src);
            common.numSampleFrames = read_be32(src);
            common.sampleSize = read_be16(src);
            common.sampleRate = read_extended(src);
            found_comm = 1;
            
            printf("COMM: channels=%d, frames=%u, bits=%d, rate=%u\n",
                   common.numChannels, common.numSampleFrames, 
                   common.sampleSize, common.sampleRate);
            
            /* Skip any remaining COMM data */
            if (chunk_size > 18) {
                SDL_RWseek(src, chunk_size - 18, RW_SEEK_CUR);
            }
        } 
        else if (memcmp(chunk_id, "SSND", 4) == 0) {
            if (chunk_size < 8) {
                SDL_SetError("Invalid SSND chunk size");
                goto fail;
            }
            
            Uint32 offset = read_be32(src);
            Uint32 block_size = read_be32(src);
            (void)block_size;
            
            ssnd_size = chunk_size - 8;
            
            printf("SSND: offset=%u, blocksize=%u, datasize=%u\n", 
                   offset, block_size, ssnd_size);
            
            ssnd_data = (Uint8*)SDL_malloc(ssnd_size);
            if (!ssnd_data) {
                SDL_SetError("Out of memory allocating %u bytes", ssnd_size);
                goto fail;
            }
            
            /* Skip offset bytes, then read data */
            if (offset > 0) {
                SDL_RWseek(src, offset, RW_SEEK_CUR);
            }
            
            if (SDL_RWread(src, ssnd_data, 1, ssnd_size) != ssnd_size) {
                SDL_SetError("Failed to read SSND data");
                goto fail;
            }
            
            found_ssnd = 1;
        }
        else {
            /* Skip unknown chunk */
            printf("Skipping chunk %.4s (size=%u)\n", chunk_id, chunk_size);
            SDL_RWseek(src, chunk_size, RW_SEEK_CUR);
        }
        
        /* Handle padding byte */
        if (chunk_size & 1) {
            SDL_RWseek(src, 1, RW_SEEK_CUR);
        }
    }
    
    if (!found_comm || !found_ssnd) {
        SDL_SetError("Invalid AIFF file (missing COMM or SSND)");
        goto fail;
    }
    
    /* Fill SDL_AudioSpec */
    spec->freq = common.sampleRate;
    spec->channels = common.numChannels;
    
    if (common.sampleSize == 16) {
        spec->format = AUDIO_S16MSB; /* AIFF is always big-endian */
    } else if (common.sampleSize == 8) {
        spec->format = AUDIO_S8;
    } else {
        SDL_SetError("Unsupported sample size: %d", common.sampleSize);
        goto fail;
    }
    
    *audio_buf = ssnd_data;
    *audio_len = ssnd_size;
    
    if (freesrc) SDL_RWclose(src);
    return spec;
    
fail:
    if (ssnd_data) SDL_free(ssnd_data);
    if (freesrc) SDL_RWclose(src);
    return NULL;
}

/* Wrapper to match SDL_LoadWAV API */
SDL_AudioSpec* Load_AIFF_File(const char* file, SDL_AudioSpec* spec, 
                               Uint8** audio_buf, Uint32* audio_len) {
    SDL_RWops* src = SDL_RWFromFile(file, "rb");
    if (!src) return NULL;
    return Load_AIFF(src, 1, spec, audio_buf, audio_len);
}

/* --- Audio Callback --- */
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
    SDL_AudioSpec wav_spec, *loaded_spec;
    Uint32 wav_length;
    SDL_Event e;
    int running = 1;
    const char* filename;
    
    if (argc < 2) {
        filename = "A.V.G16SBE.aiff"; /* Default file */
    } else {
        filename = argv[1];
    }
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    /* Try to load as AIFF first, then fall back to WAV */
    loaded_spec = Load_AIFF_File(filename, &wav_spec, &app.audio_start, &wav_length);
    if (!loaded_spec) {
        printf("Trying as WAV file...\n");
        loaded_spec = SDL_LoadWAV(filename, &wav_spec, &app.audio_start, &wav_length);
    }
    
    if (!loaded_spec) {
        printf("Failed to load audio file: %s\n", SDL_GetError());
        return 1;
    }
    
    printf("Loaded: %d Hz, %d channels, format=%d, length=%d bytes\n",
           wav_spec.freq, wav_spec.channels, wav_spec.format, wav_length);

    wav_spec.callback = audio_callback;
    wav_spec.samples = 4096;
    wav_spec.userdata = NULL;
    app.audio_pos = app.audio_start;
    app.audio_len = wav_length;
    app.total_len = wav_length;
    app.is_playing = 0;

    /* Calculate bytes per pixel */
    {
        int bar_width_px = WIN_W - (MARGIN * 2) - 4;
        app.bytes_per_pixel = app.total_len / bar_width_px;
        if (app.total_len < (Uint32)bar_width_px) app.bytes_per_pixel = 0;
    }

    if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
        printf("SDL_OpenAudio failed: %s\n", SDL_GetError());
        return 1;
    }
    
    app.window = SDL_CreateWindow("AIFF/WAV Player v2.3", 
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                   WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);

    draw_ui();

    while (running) {
        if (SDL_WaitEventTimeout(&e, 100)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x = e.button.x, y = e.button.y;
                int btn_y = WIN_H - BTN_SIZE - MARGIN;
                if (y >= btn_y && y <= btn_y + BTN_SIZE) {
                    if (x >= MARGIN && x <= MARGIN + 50) { /* Play */
                        if (!app.is_playing) { 
                            SDL_PauseAudio(0); 
                            app.is_playing = 1; 
                            draw_ui(); 
                        }
                    } else if (x >= MARGIN + 55 && x <= MARGIN + 95) { /* Stop */
                        SDL_PauseAudio(1); 
                        app.is_playing = 0;
                        app.audio_pos = app.audio_start; 
                        app.audio_len = app.total_len;
                        draw_ui();
                    } else if (x >= WIN_W - MARGIN - 40 && x <= WIN_W - MARGIN) { /* Forward */
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
            } else if (e.type == SDL_MOUSEBUTTONUP) draw_ui();
        }
        if (app.is_playing) draw_ui();
    }

    SDL_CloseAudio();
    SDL_free(app.audio_start);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
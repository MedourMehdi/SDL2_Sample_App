#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define WIN_W 320  // Reduced resolution for better performance
#define WIN_H 200
#define SAMPLE_RATE 49170  // Lower sample rate to reduce processing
#define FFT_SIZE 4096  // Smaller FFT for faster processing
#define NUM_BARS 32  // Fewer bars to render
#define FIXED_POINT_SHIFT 10  // For fixed-point arithmetic (2^10 = 1024)
#define FIXED_POINT_SCALE (1 << FIXED_POINT_SHIFT)

/* =======================
   Fixed-point arithmetic macros
   ======================= */
#define INT_TO_FIXED(x) ((x) << FIXED_POINT_SHIFT)
#define FIXED_TO_INT(x) ((x) >> FIXED_POINT_SHIFT)
#define FIXED_MUL(a, b) (((int64_t)(a) * (b)) >> FIXED_POINT_SHIFT)
#define FIXED_DIV(a, b) (((int64_t)(a) << FIXED_POINT_SHIFT) / (b))

/* =======================
   Audio shared state
   ======================= */
static uint8_t *audio_buf;
static uint32_t audio_len;
static uint32_t audio_pos;

/* Peak meters (0.0 – 1.0) in fixed-point */
static volatile int32_t level_l = 0;
static volatile int32_t level_r = 0;

/* FFT and spectrum data in fixed-point */
static int16_t fft_input[FFT_SIZE * 2];
static int16_t fft_output[FFT_SIZE * 2];
static int16_t spectrum[NUM_BARS];
static int16_t waveform[FFT_SIZE];
static int waveform_pos = 0;

/* Visualization mode */
static int viz_mode = 1; // 0: spectrum, 1: waveform, 2: peak meters

/* Precomputed tables for optimization */
static int16_t sin_table[FFT_SIZE];
static int16_t cos_table[FFT_SIZE];
static int16_t window_table[FFT_SIZE];

/* =======================
   Precompute tables for optimization
   ======================= */
static void precompute_tables(void) {
    for (int i = 0; i < FFT_SIZE; i++) {
        // Fixed-point sine and cosine tables
        float angle = 2.0f * M_PI * i / FFT_SIZE;
        sin_table[i] = (int16_t)(sin(angle) * FIXED_POINT_SCALE);
        cos_table[i] = (int16_t)(cos(angle) * FIXED_POINT_SCALE);
        
        // Hanning window for better FFT results
        float window = 0.5f * (1.0f - cos(2.0f * M_PI * i / (FFT_SIZE - 1)));
        window_table[i] = (int16_t)(window * FIXED_POINT_SCALE);
    }
}

/* =======================
   Optimized FFT for m68000
   ======================= */
static void fft_fixed(int16_t* data) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 1; i < FFT_SIZE - 1; i++) {
        int k = FFT_SIZE >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
        
        if (i < j) {
            // Swap real parts
            int16_t temp_real = data[i << 1];
            data[i << 1] = data[j << 1];
            data[j << 1] = temp_real;
            
            // Swap imaginary parts
            int16_t temp_imag = data[(i << 1) + 1];
            data[(i << 1) + 1] = data[(j << 1) + 1];
            data[(j << 1) + 1] = temp_imag;
        }
    }

    // Cooley-Tukey with precomputed trig values
    for (int L = 2; L <= FFT_SIZE; L <<= 1) {
        int k = L >> 1;
        for (int j = 0; j < k; j++) {
            int angle = (j * FFT_SIZE) / L;
            int16_t w_real = cos_table[angle];
            int16_t w_imag = sin_table[angle];
            
            for (int i = j; i < FFT_SIZE; i += L) {
                int idx1 = i << 1;
                int idx2 = (i + k) << 1;
                
                // Get values
                int16_t u_real = data[idx1];
                int16_t u_imag = data[idx1 + 1];
                int16_t t_real = data[idx2];
                int16_t t_imag = data[idx2 + 1];
                
                // Complex multiplication: t * w
                int32_t tw_real = FIXED_MUL(t_real, w_real) - FIXED_MUL(t_imag, w_imag);
                int32_t tw_imag = FIXED_MUL(t_real, w_imag) + FIXED_MUL(t_imag, w_real);
                
                // Update values
                data[idx1] = (int16_t)(u_real + (tw_real >> FIXED_POINT_SHIFT));
                data[idx1 + 1] = (int16_t)(u_imag + (tw_imag >> FIXED_POINT_SHIFT));
                data[idx2] = (int16_t)(u_real - (tw_real >> FIXED_POINT_SHIFT));
                data[idx2 + 1] = (int16_t)(u_imag - (tw_imag >> FIXED_POINT_SHIFT));
            }
        }
    }
}

/* =======================
   Optimized audio callback
   ======================= */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    if (audio_len == 0) {
        SDL_memset(stream, 0, len);
        level_l = level_r = 0;
        return;
    }

    uint32_t remaining = audio_len - audio_pos;
    uint32_t to_copy = (remaining > (uint32_t)len) ? len : remaining;

    SDL_memcpy(stream, audio_buf + audio_pos, to_copy);
    audio_pos += to_copy;

    if (to_copy < (uint32_t)len)
        SDL_memset(stream + to_copy, 0, len - to_copy);

    /* Process audio data for visualization */
    int16_t *samples = (int16_t *)stream;
    int count = to_copy >> 1;  // Divide by 2 using bit shift

    int16_t peak_l = 0, peak_r = 0;

    for (int i = 0; i + 1 < count; i += 2) {
        // Handle endianness - WAV is always little-endian
        int16_t l_raw = (int16_t)SDL_SwapLE16(samples[i]);
        int16_t r_raw = (int16_t)SDL_SwapLE16(samples[i + 1]);
        
        // Absolute values using bit manipulation
        int16_t l = l_raw < 0 ? -l_raw : l_raw;
        int16_t r = r_raw < 0 ? -r_raw : r_raw;
        
        if (l > peak_l) peak_l = l;
        if (r > peak_r) peak_r = r;
        
        // Add to FFT input (left channel only)
        if (waveform_pos < FFT_SIZE) {
            // Apply window function
            int32_t windowed = FIXED_MUL(l_raw, window_table[waveform_pos]);
            fft_input[waveform_pos << 1] = (int16_t)(windowed >> FIXED_POINT_SHIFT);
            fft_input[(waveform_pos << 1) + 1] = 0;
            waveform[waveform_pos] = l_raw;
            waveform_pos++;
        }
    }

    // Convert peaks to fixed-point (0.0-1.0 range)
    level_l = (peak_l << FIXED_POINT_SHIFT) / 32768;
    level_r = (peak_r << FIXED_POINT_SHIFT) / 32768;
    
    // Process FFT when we have enough samples
    if (waveform_pos >= FFT_SIZE) {
        // Perform FFT
        fft_fixed(fft_input);
        
        // Calculate magnitude spectrum
        for (int i = 0; i < NUM_BARS; i++) {
            int idx = (i * FFT_SIZE / 2) / NUM_BARS;
            int16_t real = fft_input[idx << 1];
            int16_t imag = fft_input[(idx << 1) + 1];
            
            // Approximate magnitude without sqrt: |z| ≈ max(|x|,|y|) + min(|x|,|y|)/2
            int16_t abs_real = real < 0 ? -real : real;
            int16_t abs_imag = imag < 0 ? -imag : imag;
            int16_t max_val = abs_real > abs_imag ? abs_real : abs_imag;
            int16_t min_val = abs_real > abs_imag ? abs_imag : abs_real;
            int16_t magnitude = max_val + (min_val >> 1);
            
            // Apply smoothing with bit shifts
            spectrum[i] = (spectrum[i] * 7 + magnitude) >> 3;  // 0.875 * old + 0.125 * new
        }
        
        waveform_pos = 0;
    }
    
    /* Reset at end of file */
    if (audio_pos >= audio_len) {
        audio_pos = 0;  // Loop playback
    }
}

/* =======================
   Optimized drawing functions
   ======================= */
static void draw_spectrum(SDL_Renderer *ren) {
    int bar_width = WIN_W / NUM_BARS;
    int max_bar_height = WIN_H - 20;
    
    for (int i = 0; i < NUM_BARS; i++) {
        // Convert fixed-point to int and scale
        int height = (spectrum[i] * max_bar_height) >> FIXED_POINT_SHIFT;
        
        // Simple color calculation with bit shifts
        int r = (i * 255) / NUM_BARS;
        int g = 255 - r;
        int b = 128;
        
        SDL_SetRenderDrawColor(ren, r, g, b, 255);
        
        SDL_Rect bar = {
            i * bar_width,
            WIN_H - height,
            bar_width - 1,  // Leave 1 pixel between bars
            height
        };
        SDL_RenderFillRect(ren, &bar);
    }
}

static void draw_waveform(SDL_Renderer *ren) {
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    
    int center_y = WIN_H >> 1;  // Divide by 2 using bit shift
    int scale = WIN_H >> 2;     // Divide by 4 using bit shift
    
    for (int i = 0; i < FFT_SIZE - 1; i++) {
        int x1 = (i * WIN_W) / FFT_SIZE;
        int y1 = center_y - ((waveform[i] * scale) >> 15);  // 15 = 16-1 for signed 16-bit
        int x2 = ((i + 1) * WIN_W) / FFT_SIZE;
        int y2 = center_y - ((waveform[i + 1] * scale) >> 15);
        
        SDL_RenderDrawLine(ren, x1, y1, x2, y2);
    }
}

static void draw_peak_meters(SDL_Renderer *ren) {
    /* Smooth the meters slightly using bit shifts */
    static int32_t draw_l = 0, draw_r = 0;
    draw_l = (draw_l * 9 + level_l) >> 3;  // 0.875 * old + 0.125 * new
    draw_r = (draw_r * 9 + level_r) >> 3;

    int bar_max = WIN_H - 20;
    int bar_width = 20;

    // Left channel bar
    int bar_l_height = (draw_l * bar_max) >> FIXED_POINT_SHIFT;
    for (int i = 0; i < bar_l_height; i++) {
        // Color gradient with bit shifts
        int intensity = (i << 8) / bar_max;  // 0-255 range
        SDL_SetRenderDrawColor(ren, intensity, 255, 0, 255);
        SDL_RenderDrawLine(ren, 
                          (WIN_W >> 2) - (bar_width >> 1), 
                          WIN_H - i - 10, 
                          (WIN_W >> 2) + (bar_width >> 1), 
                          WIN_H - i - 10);
    }

    // Right channel bar
    int bar_r_height = (draw_r * bar_max) >> FIXED_POINT_SHIFT;
    for (int i = 0; i < bar_r_height; i++) {
        // Color gradient with bit shifts
        int intensity = (i << 8) / bar_max;  // 0-255 range
        SDL_SetRenderDrawColor(ren, 0, 128 + (intensity >> 1), 255, 255);
        SDL_RenderDrawLine(ren, 
                          (WIN_W >> 1) + (WIN_W >> 2) - (bar_width >> 1), 
                          WIN_H - i - 10, 
                          (WIN_W >> 1) + (WIN_W >> 2) + (bar_width >> 1), 
                          WIN_H - i - 10);
    }
}

/* =======================
   Main
   ======================= */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s file.wav\n", argv[0]);
        printf("Press 1, 2, or 3 during playback to switch visualization modes:\n");
        printf("  1: Spectrum Analyzer\n");
        printf("  2: Waveform\n");
        printf("  3: Peak Meters\n");
        return 1;
    }

    // Precompute tables for optimization
    precompute_tables();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_Window *win = SDL_CreateWindow(
        "Atari Audio Visualizer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0
    );
    
    // Use software renderer for better compatibility
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    /* Load WAV */
    SDL_AudioSpec wav_spec, want, have;

    if (!SDL_LoadWAV(argv[1], &wav_spec, &audio_buf, &audio_len)) {
        printf("Failed to load WAV: %s\n", SDL_GetError());
        return 1;
    }

    printf("WAV format: %d Hz, %d bits, %d channels\n", 
           wav_spec.freq, wav_spec.format & 0xFF, wav_spec.channels);

    audio_pos = 0;

    want = wav_spec;
    want.callback = audio_callback;
    want.userdata = NULL;
    want.samples = FFT_SIZE; // Match our FFT size

    // Ensure we're using signed 16-bit format
    if (want.format != AUDIO_S16LSB && want.format != AUDIO_S16MSB) {
        printf("Converting audio format to 16-bit\n");
        want.format = AUDIO_S16LSB;  // WAV is always little-endian
    }

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(
        NULL, 0, &want, &have, 0
    );

    if (!dev) {
        printf("Audio open failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("Audio device format: %d Hz, %d bits, %d channels\n", 
           have.freq, have.format & 0xFF, have.channels);

    SDL_PauseAudioDevice(dev, 0);

    int running = 1;
    SDL_Event e;

    printf("Visualization started. Press 1, 2, or 3 to switch modes.\n");

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_1:
                        viz_mode = 0;
                        printf("Switched to Spectrum Analyzer\n");
                        break;
                    case SDLK_2:
                        viz_mode = 1;
                        printf("Switched to Waveform\n");
                        break;
                    case SDLK_3:
                        viz_mode = 2;
                        printf("Switched to Peak Meters\n");
                        break;
                    case SDLK_ESCAPE:
                        running = 0;
                        break;
                }
            }
        }

        // Clear screen
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        
        // Draw based on current visualization mode
        switch (viz_mode) {
            case 0:
                draw_spectrum(ren);
                break;
            case 1:
                draw_waveform(ren);
                break;
            case 2:
                draw_peak_meters(ren);
                break;
        }
        
        SDL_RenderPresent(ren);
        SDL_Delay(33);  // Cap at ~30 FPS for better performance
    }

    SDL_CloseAudioDevice(dev);
    SDL_FreeWAV(audio_buf);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
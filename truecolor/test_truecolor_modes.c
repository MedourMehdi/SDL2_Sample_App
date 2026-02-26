/* ============================================================================
 * SDL2 Atari TrueColor Test - initially in 800x600 16bpp/24bpp/32bpp - down to lower rez for ST
 * Tests high resolution TrueColor modes
 * Compile: gcc -O2 -o test_truecolor_modes test_truecolor_modes.c `sdl2-config --cflags --libs`
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Test configuration */
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define TEST_DURATION_MS 5000

/* Test modes */
typedef enum {
    TEST_GRADIENT,      /* Smooth color gradient */
    TEST_PLASMA,        /* Animated plasma effect */
    TEST_SPRITE,        /* Moving sprite */
    TEST_LINES,         /* Random lines */
    TEST_FILL,          /* Solid fills */
    TEST_TEXT,          /* Scrolling text bars */
    TEST_COUNT
} TestMode;

const char* test_names[TEST_COUNT] = {
    "Smooth gradient",
    "Animated plasma",
    "Moving sprite",
    "Random lines",
    "Solid fills",
    "Scrolling text bars"
};

/* Global state */
SDL_Window* window = NULL;
SDL_Surface* surface = NULL;
int running = 1;
int current_test = 0;
Uint32 frame_count = 0;
Uint32 test_start_time = 0;
float current_fps = 0;

/* Test objects */
int sprite_x = 100, sprite_y = 100;
int sprite_dx = 3, sprite_dy = 2;
int scroll_offset = 0;
float plasma_time = 0;

/* Utility: Get current time */
static Uint32 get_time_ms(void) {
    return SDL_GetTicks();
}

/* Utility: Put pixel (TrueColor aware) */
static void put_pixel(int x, int y, Uint32 color) {
    if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= WINDOW_HEIGHT) return;
    
    Uint8 *pixels = (Uint8*)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;
    
    switch (surface->format->BytesPerPixel) {
        case 2: *(Uint16*)pixels = (Uint16)color; break;
        case 3:
            pixels[0] = (color >> 16) & 0xFF;
            pixels[1] = (color >> 8) & 0xFF;
            pixels[2] = color & 0xFF;
            break;
        case 4: *(Uint32*)pixels = color; break;
    }
}

/* Utility: Get RGB color for current format */
static Uint32 make_color(Uint8 r, Uint8 g, Uint8 b) {
    return SDL_MapRGB(surface->format, r, g, b);
}

/* Utility: Clear screen */
static void clear_screen(Uint8 r, Uint8 g, Uint8 b) {
    SDL_FillRect(surface, NULL, make_color(r, g, b));
}

/* Utility: Draw rectangle */
static void draw_rect(int x, int y, int w, int h, Uint32 color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(surface, &rect, color);
}

/* Utility: Draw horizontal gradient */
static void draw_gradient_h(int y, int h, Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2) {
    int x;
    for (x = 0; x < WINDOW_WIDTH; x++) {
        Uint8 r = r1 + ((r2 - r1) * x / WINDOW_WIDTH);
        Uint8 g = g1 + ((g2 - g1) * x / WINDOW_WIDTH);
        Uint8 b = b1 + ((b2 - b1) * x / WINDOW_WIDTH);
        draw_rect(x, y, 1, h, make_color(r, g, b));
    }
}

/* Utility: Draw vertical gradient */
static void draw_gradient_v(int x, int w, Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2) {
    int y;
    for (y = 0; y < WINDOW_HEIGHT; y++) {
        Uint8 r = r1 + ((r2 - r1) * y / WINDOW_HEIGHT);
        Uint8 g = g1 + ((g2 - g1) * y / WINDOW_HEIGHT);
        Uint8 b = b1 + ((b2 - b1) * y / WINDOW_HEIGHT);
        draw_rect(x, y, w, 1, make_color(r, g, b));
    }
}

/* ============================================================================
 * TEST SCENARIOS
 * ============================================================================ */

/* TEST 1: Smooth horizontal gradient */
static void test_gradient(void) {
    int y;
    Uint32 now = SDL_GetTicks();
    Uint8 phase = (now / 20) % 256;
    
    for (y = 0; y < WINDOW_HEIGHT; y += 4) {
        Uint8 r = (y + phase) % 256;
        Uint8 g = (y * 2 + phase) % 256;
        Uint8 b = (y / 2 + phase) % 256;
        draw_gradient_h(y, 4, r, g*0, b, r*0, g, b*0);
    }
}

/* TEST 2: Animated plasma effect */
static void test_plasma(void) {
    int x, y;
    plasma_time += 0.05f;
    
    for (y = 0; y < WINDOW_HEIGHT; y += 2) {
        for (x = 0; x < WINDOW_WIDTH; x += 2) {
            float v = sinf(x / 30.0f + plasma_time) + 
                    sinf(y / 25.0f + plasma_time * 0.7f) +
                    sinf((x + y) / 40.0f + plasma_time * 0.3f);
            int c = (int)((v + 3) * 42);
            Uint8 r = c;
            Uint8 g = (c + 85) % 256;
            Uint8 b = (c + 170) % 256;
            draw_rect(x, y, 2, 2, make_color(r, g, b));
        }
    }
}

/* TEST 3: Moving sprite with alpha blending simulation */
static void test_sprite(void) {
    int x, y;
    int size = 64;
    Uint32 now = SDL_GetTicks();
    
    clear_screen(32, 32, 64);
    
    /* Move sprite */
    sprite_x += sprite_dx;
    sprite_y += sprite_dy;
    if (sprite_x <= 0 || sprite_x >= WINDOW_WIDTH - size) sprite_dx = -sprite_dx;
    if (sprite_y <= 0 || sprite_y >= WINDOW_HEIGHT - size) sprite_dy = -sprite_dy;
    
    /* Draw "shadow" (previous position, darker) */
    draw_rect(sprite_x - 8, sprite_y + 8, size, size, make_color(255, 255, 255));
    
    /* Draw sprite with gradient */
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            int cx = x - size/2;
            int cy = y - size/2;
            int dist = (cx*cx + cy*cy) / 256;
            if (dist < 16) {
                Uint8 r = 255 - dist * 8;
                Uint8 g = 200 - dist * 4;
                Uint8 b = 100;
                put_pixel(sprite_x + x, sprite_y + y, make_color(r, g, b));
            }
        }
    }
    
    /* Draw trail */
    (void)now;
}

/* TEST 4: Random lines (stress test) */
static void test_lines(void) {
    int i;
    clear_screen(0, 0, 0);
    
    for (i = 0; i < 100; i++) {
        int x1 = rand() % WINDOW_WIDTH;
        int y1 = rand() % WINDOW_HEIGHT;
        int x2 = rand() % WINDOW_WIDTH;
        int y2 = rand() % WINDOW_HEIGHT;
        Uint8 r = rand() % 256;
        Uint8 g = rand() % 256;
        Uint8 b = rand() % 256;
        
        /* Simple line drawing */
        int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy, e2;
        
        while (1) {
            put_pixel(x1, y1, make_color(r, g, b));
            if (x1 == x2 && y1 == y2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }
}

/* TEST 5: Solid color fills (bandwidth test) */
static void test_fill(void) {
    int i;
    int bands = 8;
    int band_h = WINDOW_HEIGHT / bands;
    
    for (i = 0; i < bands; i++) {
        Uint8 r = (i * 32) % 256;
        Uint8 g = (i * 64 + 128) % 256;
        Uint8 b = (i * 16 + 64) % 256;
        draw_rect(0, i * band_h, WINDOW_WIDTH, band_h, make_color(r, g, b));
    }
    
    /* Animated overlay */
    Uint32 now = SDL_GetTicks();
    int pulse = (now / 10) % WINDOW_WIDTH;
    draw_rect(pulse, 0, 20, WINDOW_HEIGHT, make_color(255, 255, 255));
}

/* TEST 6: Scrolling text bars */
static void test_text(void) {
    int i;
    int bar_h = 40;
    int num_bars = WINDOW_HEIGHT / bar_h;
    
    scroll_offset = (scroll_offset + 2) % WINDOW_WIDTH;
    
    for (i = 0; i < num_bars; i++) {
        Uint8 r = (i * 50) % 256;
        Uint8 g = 255 - (i * 30) % 256;
        Uint8 b = (i * 70 + 100) % 256;
        
        int x = (i % 2 == 0) ? scroll_offset : WINDOW_WIDTH - scroll_offset - 100;
        draw_rect(x, i * bar_h + 5, 100, bar_h - 10, make_color(r, g, b));
    }
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

static void run_test(void) {
    Uint32 now = get_time_ms();
    Uint32 elapsed = now - test_start_time;
    
    /* Run current test */
    switch (current_test) {
        case TEST_GRADIENT: test_gradient(); break;
        case TEST_PLASMA:   test_plasma(); break;
        case TEST_SPRITE:   test_sprite(); break;
        case TEST_LINES:    test_lines(); break;
        case TEST_FILL:     test_fill(); break;
        case TEST_TEXT:     test_text(); break;
        default: break;
    }
    
    /* Update window surface */
    SDL_UpdateWindowSurface(window);
    frame_count++;
    
    /* Calculate FPS */
    if (elapsed > 0) {
        current_fps = (frame_count * 1000.0f) / elapsed;
    }
    
    /* Check if test duration complete */
    if (elapsed >= TEST_DURATION_MS) {
        printf("\n--- %s ---\n", test_names[current_test]);
        printf("Frames: %u, Time: %ums\n", frame_count, elapsed);
        printf("Average FPS: %.2f\n", current_fps);
        
        current_test++;
        frame_count = 0;
        test_start_time = now;
        
        if (current_test >= TEST_COUNT) {
            printf("\n=== ALL TESTS COMPLETE ===\n");
            running = 0;
        }
    }
}

int main(int argc, char* argv[]) {
    SDL_Event event;
    Uint32 window_format = SDL_PIXELFORMAT_RGB565; /* Default 16bpp */
    
    (void)argc;
    (void)argv;
    
    /* Check for format argument */
    if (argc > 1) {
        if (strcmp(argv[1], "24") == 0) window_format = SDL_PIXELFORMAT_RGB888;
        if (strcmp(argv[1], "32") == 0) window_format = SDL_PIXELFORMAT_ARGB8888;
    }
    
    printf("SDL2 Atari TrueColor Test Suite\n");
    printf("Window: %dx%d, Format: %s\n\n", 
           WINDOW_WIDTH, WINDOW_HEIGHT,
           SDL_GetPixelFormatName(window_format));
    
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    /* Create window with specific size */
    window = SDL_CreateWindow("SDL2 TrueColor Test",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              0);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    /* Get surface */
    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        fprintf(stderr, "Surface creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Surface format: %s\n", SDL_GetPixelFormatName(surface->format->format));
    printf("Bytes per pixel: %d\n", surface->format->BytesPerPixel);
    printf("Pitch: %d\n\n", surface->pitch);
    
    /* Initialize test */
    srand((unsigned)time(NULL));
    test_start_time = get_time_ms();
    
    /* Main loop */
    while (running) {
        /* Handle events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    frame_count = 999999; /* Force test end */
                }
            }
        }
        
        /* Run current test */
        run_test();
    }
    
    /* Cleanup */
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("\nTest suite completed.\n");
    return 0;
}
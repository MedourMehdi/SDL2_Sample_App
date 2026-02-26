/* ============================================================================
 * SDL2 Atari Performance Test Suite -
 * Uses Window Surface API instead of Render API for direct access
 * Should match SDL1 performance (400+ FPS)
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH  320
#define HEIGHT 200
#define TEST_DURATION_MS 5000

typedef enum {
    TEST_STATIC,
    TEST_CURSOR,
    TEST_SMALL_SPRITE,
    TEST_MEDIUM_UPDATE,
    TEST_FULL_SCROLL,
    TEST_RANDOM_RECTS,
    TEST_MIXED,
    TEST_COUNT
} TestMode;

const char* test_names[] = {
    "Static (no changes)",
    "Text cursor (2 rows)",
    "Small sprite (32 rows)",
    "Medium update (100 rows)",
    "Full scroll (200 rows)",
    "Random rects",
    "Mixed workload"
};

/* Global State */
SDL_Window *window = NULL;
SDL_Surface *surface = NULL;  /* ← DIRECT SURFACE ACCESS (like SDL1) */

int running = 1;
TestMode current_test = TEST_STATIC;
Uint32 test_start_time = 0;
Uint32 frame_count = 0;

void run_test() {
    Uint32 now = SDL_GetTicks();
    int x, y, i;
    SDL_Rect r;
    Uint8 *pixels;
    Uint8 color = (Uint8)(now % 255);

    /* Check if test duration is over */
    if (now - test_start_time >= TEST_DURATION_MS) {
        float avg_fps = (float)frame_count / (TEST_DURATION_MS / 1000.0f);
        printf("--- %s ---\n", test_names[current_test]);
        printf("Frames: %u, Time: %ums\n", frame_count, now - test_start_time);
        printf("Average FPS: %.2f\n\n", avg_fps);

        current_test++;
        if (current_test >= TEST_COUNT) {
            printf("=== ALL TESTS COMPLETE ===\n");
            running = 0;
            return;
        }
        test_start_time = now;
        frame_count = 0;
        SDL_FillRect(surface, NULL, 0);  /* Clear for next test */
    }

    /* Lock surface for direct pixel access (if needed) */
    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }
    
    pixels = (Uint8*)surface->pixels;

    /* Test Implementations - Choose between SDL_FillRect or direct pixel access */
    switch (current_test) {
        case TEST_STATIC:
            /* No changes - triggers checksum early exit */
            break;

        case TEST_CURSOR:
            /* Blink 2 rows using SDL_FillRect for clean dirty rect */
            r.x = 10; 
            r.y = HEIGHT - 20; 
            r.w = 20; 
            r.h = 2;
            SDL_FillRect(surface, &r, (now / 250) % 2 ? 0xFF : 0x00);
            break;

        case TEST_SMALL_SPRITE:
            /* 32x32 moving sprite - using SDL_FillRect */
            x = (now / 10) % (WIDTH - 32);
            y = (now / 20) % (HEIGHT - 32);
            r.x = x; 
            r.y = y; 
            r.w = 32; 
            r.h = 32;
            SDL_FillRect(surface, &r, color);
            break;

        case TEST_MEDIUM_UPDATE:
            /* 100 rows */
            r.x = 0; 
            r.y = 50; 
            r.w = WIDTH; 
            r.h = 100;
            SDL_FillRect(surface, &r, color);
            break;

        case TEST_FULL_SCROLL:
            /* Full screen */
            SDL_FillRect(surface, NULL, color);
            break;

        case TEST_RANDOM_RECTS:
            /* Random rects - using SDL_FillRect */
            for(i = 0; i < 10; i++) {
                r.x = rand() % (WIDTH - 10);
                r.y = rand() % (HEIGHT - 10);
                r.w = 10;
                r.h = 10;
                SDL_FillRect(surface, &r, (Uint8)rand());
            }
            break;

        case TEST_MIXED:
            /* Moving cursor */
            r.x = (now / 5) % (WIDTH - 20);
            r.y = 10;
            r.w = 20;
            r.h = 20;
            SDL_FillRect(surface, &r, 0xAA);
            break;
    }

    /* Unlock surface if needed */
    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    /* Single update - calls GEM_UpdateWindowFramebuffer directly */
    SDL_UpdateWindowSurface(window);
    
    frame_count++;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Create window */
    window = SDL_CreateWindow("SDL2 Perf Test - (Surface API)",
                              SDL_WINDOWPOS_UNDEFINED, 
                              SDL_WINDOWPOS_UNDEFINED,
                              WIDTH, HEIGHT, 0);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    /* Get window surface - DIRECT ACCESS (like SDL1's SDL_SetVideoMode) */
    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        fprintf(stderr, "Surface creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("SDL2 Atari Performance Test Suite - OPTIMIZED\n");
    printf("Window: %dx%d, Format: %s\n", 
           WIDTH, HEIGHT, SDL_GetPixelFormatName(surface->format->format));
    printf("Using Window Surface API (NOT Render API)\n\n");

    srand((unsigned)time(NULL));
    test_start_time = SDL_GetTicks();

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                running = 0;
        }
        run_test();
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("\nTest suite completed.\n");
    return 0;
}
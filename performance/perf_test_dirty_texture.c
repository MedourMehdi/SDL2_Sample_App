/* ============================================================================
 * SDL2 Atari Performance Test Suite - #3
 * Properly tracks dirty rectangles for partial updates
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH  320
#define HEIGHT 200
#define TEST_DURATION_MS 5000
#define MAX_DIRTY_RECTS 64

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
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
Uint8 pixels[WIDTH * HEIGHT];

int running = 1;
TestMode current_test = TEST_STATIC;
Uint32 test_start_time = 0;
Uint32 frame_count = 0;

/* Dirty rect tracking */
SDL_Rect dirty_rects[MAX_DIRTY_RECTS];
int num_dirty_rects = 0;

void add_dirty_rect(int x, int y, int w, int h) {
    if (num_dirty_rects < MAX_DIRTY_RECTS) {
        dirty_rects[num_dirty_rects].x = x;
        dirty_rects[num_dirty_rects].y = y;
        dirty_rects[num_dirty_rects].w = w;
        dirty_rects[num_dirty_rects].h = h;
        num_dirty_rects++;
    }
}

void run_test() {
    Uint32 now = SDL_GetTicks();
    int x, y, i;
    Uint8 color = (Uint8)(now % 255);
    
    num_dirty_rects = 0;  /* Reset dirty rects each frame */

    /* Check if test duration is over */
    if (now - test_start_time >= TEST_DURATION_MS) {
        float avg_fps = (float)frame_count / (TEST_DURATION_MS / 1000.0f);
        printf("--- %s ---\n", test_names[current_test]);
        printf("Average FPS: %.2f\n\n", avg_fps);

        current_test++;
        if (current_test >= TEST_COUNT) {
            printf("=== ALL TESTS COMPLETE ===\n");
            running = 0;
            return;
        }
        test_start_time = now;
        frame_count = 0;
        memset(pixels, 0, WIDTH * HEIGHT);
        
        /* Clear the screen on test change */
        SDL_UpdateTexture(texture, NULL, pixels, WIDTH);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        return;
    }

    /* Test Implementations - Modifying the pixel buffer AND tracking dirty rects */
    switch (current_test) {
        case TEST_STATIC:
            /* No changes, no dirty rects */
            break;

        case TEST_CURSOR:
            /* Blinking cursor at bottom of screen */
            if ((now / 250) % 2) {
                for(y = HEIGHT-20; y < HEIGHT-18; y++)
                    for(x = 10; x < 30; x++) 
                        pixels[y * WIDTH + x] = 0xFF;
            } else {
                for(y = HEIGHT-20; y < HEIGHT-18; y++)
                    for(x = 10; x < 30; x++) 
                        pixels[y * WIDTH + x] = 0x00;
            }
            add_dirty_rect(10, HEIGHT-20, 20, 2);
            break;

        case TEST_SMALL_SPRITE:
            /* Moving 32x32 sprite */
            x = (now / 10) % (WIDTH - 32);
            y = (now / 20) % (HEIGHT - 32);
            
            /* Clear old position (simplified - just redraw everything) */
            memset(pixels, 0, WIDTH * HEIGHT);
            
            /* Draw sprite at new position */
            for(i = 0; i < 32; i++)
                memset(&pixels[(y + i) * WIDTH + x], color, 32);
            
            add_dirty_rect(0, 0, WIDTH, HEIGHT);  /* Full update for simplicity */
            break;

        case TEST_MEDIUM_UPDATE:
            /* Update middle 100 rows */
            for(y = 50; y < 150; y++)
                memset(&pixels[y * WIDTH], color, WIDTH);
            add_dirty_rect(0, 50, WIDTH, 100);
            break;

        case TEST_FULL_SCROLL:
            /* Full screen color change */
            memset(pixels, color, WIDTH * HEIGHT);
            add_dirty_rect(0, 0, WIDTH, HEIGHT);
            break;

        case TEST_RANDOM_RECTS:
            /* Draw 10 random rectangles */
            for(i=0; i<10; i++) {
                int rx = rand() % (WIDTH-10);
                int ry = rand() % (HEIGHT-10);
                Uint8 rc = (Uint8)rand();
                for(y=0; y<10; y++) 
                    memset(&pixels[(ry+y)*WIDTH + rx], rc, 10);
                add_dirty_rect(rx, ry, 10, 10);
            }
            break;

        case TEST_MIXED:
            /* Moving horizontal bar */
            for(y = 10; y < 30; y++) {
                int mx = (now / 5) % (WIDTH - 20);
                memset(&pixels[y * WIDTH + mx], 0xAA, 20);
                
                /* Clear old position (previous frame) */
                int old_mx = ((now - 16) / 5) % (WIDTH - 20);
                if (old_mx != mx) {
                    memset(&pixels[y * WIDTH + old_mx], 0x00, 20);
                }
            }
            /* Add both old and new positions */
            {
                int mx = (now / 5) % (WIDTH - 20);
                int old_mx = ((now - 16) / 5) % (WIDTH - 20);
                int min_x = (mx < old_mx) ? mx : old_mx;
                int max_x = (mx > old_mx) ? mx : old_mx;
                add_dirty_rect(min_x, 10, (max_x - min_x) + 20, 20);
            }
            break;
    }

    /* Update the texture ONLY with dirty rectangles */
    if (num_dirty_rects == 0) {
        /* No changes - don't update texture at all */
    } else if (num_dirty_rects == 1 && 
               dirty_rects[0].w == WIDTH && 
               dirty_rects[0].h == HEIGHT) {
        /* Full update */
        SDL_UpdateTexture(texture, NULL, pixels, WIDTH);
    } else {
        /* Partial updates - update each dirty rect */
        for (i = 0; i < num_dirty_rects; i++) {
            SDL_Rect *r = &dirty_rects[i];
            SDL_UpdateTexture(texture, r, 
                            pixels + (r->y * WIDTH) + r->x, 
                            WIDTH);
        }
    }
    
    /* Render to screen */
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    frame_count++;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    // SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    
    window = SDL_CreateWindow("SDL2 Perf Test", 
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                              WIDTH, HEIGHT, 0);
    
    /* Force Software Renderer to test your GEM driver's dirty rect logic */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    
    /* Use RGB332 to match your optimized paths */
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, 
                                SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

    printf("SDL2 Atari Performance Test Suite\n");
    printf("Window: %dx%d, Format: RGB332\n\n", WIDTH, HEIGHT);

    test_start_time = SDL_GetTicks();

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        run_test();
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
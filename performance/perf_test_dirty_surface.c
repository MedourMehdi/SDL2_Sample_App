/* ============================================================================
 * SDL2 Atari Performance Test Suite
 * Tests Step 4 dirty region optimization with various scenarios
 * Compile: gcc -O2 -o sdl2_perf_test sdl2_perf_test.c `sdl2-config --cflags --libs`
 * ============================================================================ */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Test configuration */
#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 200
#define TEST_DURATION_MS 5000  /* 5 seconds per test */
#define FPS_HISTORY_SIZE 100

/* Test modes */
typedef enum {
    TEST_STATIC,        /* No changes - should skip updates */
    TEST_CURSOR,        /* Text cursor blink - 2 rows */
    TEST_SMALL_SPRITE,  /* 32x32 sprite moving - ~32 rows */
    TEST_MEDIUM_UPDATE, /* 100x100 region - ~100 rows */
    TEST_FULL_SCROLL,   /* Full screen scrolling - all rows */
    TEST_RANDOM_RECTS,  /* Random small rectangles */
    TEST_MIXED,         /* Mix of all above */
    TEST_COUNT
} TestMode;

const char* test_names[TEST_COUNT] = {
    "Static (no changes)",
    "Text cursor (2 rows)",
    "Small sprite (32 rows)",
    "Medium update (100 rows)",
    "Full scroll (200 rows)",
    "Random rects",
    "Mixed workload"
};

/* Global state */
SDL_Window* window = NULL;
SDL_Surface* surface = NULL;
int running = 1;
int current_test = 0;
Uint32 frame_count = 0;
Uint32 test_start_time = 0;
float fps_history[FPS_HISTORY_SIZE];
int fps_index = 0;
float current_fps = 0;

/* Test objects */
int cursor_visible = 0;
int cursor_x = 160, cursor_y = 100;
int sprite_x = 100, sprite_y = 80;
int sprite_dx = 2, sprite_dy = 1;
int scroll_offset = 0;

/* Utility: Get current time in milliseconds */
static Uint32 get_time_ms(void) {
    return SDL_GetTicks();
}

/* Utility: Clear screen to color */
static void clear_screen(Uint8 r, Uint8 g, Uint8 b) {
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, r, g, b));
}

/* Utility: Draw rectangle */
static void draw_rect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, r, g, b));
}

/* Utility: Draw text (simple bitmap font simulation) */
static void draw_char(int x, int y, char c, Uint8 r, Uint8 g, Uint8 b) {
    /* Simulate text by drawing a small filled rectangle */
    SDL_Rect rect = {x, y, 8, 16};
    Uint32 color = SDL_MapRGB(surface->format, r, g, b);
    SDL_FillRect(surface, &rect, color);
    
    /* Draw "pixel" pattern to simulate character */
    (void)c; /* Unused for simplicity */
}

static void draw_string(int x, int y, const char* str, Uint8 r, Uint8 g, Uint8 b) {
    while (*str) {
        draw_char(x, y, *str, r, g, b);
        x += 8;
        str++;
    }
}

/* ============================================================================
 * TEST SCENARIOS
 * ============================================================================ */

/* TEST 1: Static - no changes, should trigger Step 4 skip */
static void test_static(void) {
    /* Draw once, never change */
    if (frame_count == 0) {
        clear_screen(0, 0, 64); /* Dark blue */
        draw_string(10, 10, "STATIC TEST", 255, 255, 255);
        draw_string(10, 30, "No pixels changing", 200, 200, 200);
        draw_string(10, 50, "Step 4 should SKIP updates", 0, 255, 0);
    }
    /* Intentionally empty - no drawing after first frame */
}

/* TEST 2: Text cursor - 2 rows changing (optimal for Step 4) */
static void test_cursor(void) {
    /* Background */
    clear_screen(0, 0, 0);
    
    /* Draw some "text" lines */
    for (int i = 0; i < 10; i++) {
        draw_string(10, 20 + i * 20, "This is a line of text...", 200, 200, 200);
    }
    
    /* Blinking cursor at row 5 (y=100) - only 2 rows change */
    cursor_visible = (frame_count / 30) % 2; /* Blink every 30 frames */
    if (cursor_visible) {
        draw_rect(cursor_x, cursor_y, 8, 16, 255, 255, 255);
    }
    
    draw_string(10, 180, "Cursor blink (2 rows)", 0, 255, 0);
}

/* TEST 3: Small sprite - ~32 rows changing */
static void test_small_sprite(void) {
    /* Background */
    clear_screen(32, 32, 32);
    
    /* Draw grid to show movement */
    for (int y = 0; y < WINDOW_HEIGHT; y += 40) {
        draw_rect(0, y, WINDOW_WIDTH, 1, 64, 64, 64);
    }
    
    /* Move sprite */
    sprite_x += sprite_dx;
    sprite_y += sprite_dy;
    if (sprite_x <= 0 || sprite_x >= WINDOW_WIDTH - 32) sprite_dx = -sprite_dx;
    if (sprite_y <= 0 || sprite_y >= WINDOW_HEIGHT - 32) sprite_dy = -sprite_dy;
    
    /* Draw 32x32 sprite (affects ~32 rows) */
    draw_rect(sprite_x, sprite_y, 32, 32, 255, 0, 0);
    draw_rect(sprite_x + 8, sprite_y + 8, 16, 16, 255, 255, 0);
    
    draw_string(10, 180, "Small sprite (32 rows)", 0, 255, 0);
}

/* TEST 4: Medium update - ~100 rows (borderline beneficial) */
static void test_medium_update(void) {
    clear_screen(0, 32, 0);
    
    /* Draw large animated panel in center */
    int panel_y = 50 + (frame_count % 60) - 30; /* Oscillate */
    draw_rect(60, panel_y, 200, 100, 64, 128, 255);
    draw_rect(70, panel_y + 10, 180, 80, 128, 192, 255);
    
    /* Some static UI */
    draw_string(10, 10, "MEDIUM UPDATE TEST", 255, 255, 255);
    draw_string(10, 180, "100 rows changing", 0, 255, 0);
}

/* TEST 5: Full scroll - all 200 rows (Step 4 penalty case) */
static void test_full_scroll(void) {
    scroll_offset = (frame_count * 2) % WINDOW_HEIGHT;
    
    /* Starfield effect - full screen changes */
    for (int i = 0; i < 50; i++) {
        int x = (i * 37) % WINDOW_WIDTH;
        int y = ((i * 23) + scroll_offset) % WINDOW_HEIGHT;
        draw_rect(x, y, 2, 2, 255, 255, 255);
    }
    
    /* Scrolling text at bottom */
    int text_x = WINDOW_WIDTH - ((frame_count * 3) % (WINDOW_WIDTH + 100));
    draw_string(text_x, 180, "FULL SCREEN SCROLLING...", 255, 0, 0);
}

/* TEST 6: Random small rectangles */
static void test_random_rects(void) {
    clear_screen(16, 16, 16);
    
    /* Draw 10 random small rectangles each frame */
    for (int i = 0; i < 10; i++) {
        int x = rand() % (WINDOW_WIDTH - 20);
        int y = rand() % (WINDOW_HEIGHT - 20);
        int r = rand() % 256;
        int g = rand() % 256;
        int b = rand() % 256;
        draw_rect(x, y, 16, 16, r, g, b);
    }
    
    draw_string(10, 180, "Random rects", 0, 255, 0);
}

/* TEST 7: Mixed workload - varies each frame */
static void test_mixed(void) {
    /* Cycle through different patterns based on frame */
    int phase = (frame_count / 60) % 4;
    
    switch (phase) {
        case 0: /* Static for 60 frames */
            if (frame_count % 60 == 0) {
                clear_screen(0, 0, 64);
                draw_string(10, 100, "STATIC PHASE", 255, 255, 255);
            }
            break;
        case 1: /* Cursor for 60 frames */
            test_cursor();
            break;
        case 2: /* Sprite for 60 frames */
            test_small_sprite();
            break;
        case 3: /* Full update for 60 frames */
            test_full_scroll();
            break;
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
        case TEST_STATIC:       test_static(); break;
        case TEST_CURSOR:       test_cursor(); break;
        case TEST_SMALL_SPRITE: test_small_sprite(); break;
        case TEST_MEDIUM_UPDATE:test_medium_update(); break;
        case TEST_FULL_SCROLL:  test_full_scroll(); break;
        case TEST_RANDOM_RECTS: test_random_rects(); break;
        case TEST_MIXED:        test_mixed(); break;
        default: break;
    }
    
    /* Update window surface */
    SDL_UpdateWindowSurface(window);
    frame_count++;
    
    /* Calculate FPS */
    if (elapsed > 0) {
        current_fps = (frame_count * 1000.0f) / elapsed;
    }
    
    /* Record FPS history */
    if (frame_count < FPS_HISTORY_SIZE) {
        fps_history[frame_count - 1] = current_fps;
    }
    
    /* Check if test duration complete */
    if (elapsed >= TEST_DURATION_MS) {
        /* Print results for this test */
        printf("\n--- %s ---\n", test_names[current_test]);
        printf("Frames: %u, Time: %ums\n", frame_count, elapsed);
        printf("Average FPS: %.2f\n", current_fps);
        
        /* Calculate min/max/avg */
        float min_fps = 9999, max_fps = 0, avg_fps = 0;
        int samples = (frame_count < FPS_HISTORY_SIZE) ? frame_count : FPS_HISTORY_SIZE;
        for (int i = 0; i < samples; i++) {
            if (fps_history[i] < min_fps) min_fps = fps_history[i];
            if (fps_history[i] > max_fps) max_fps = fps_history[i];
            avg_fps += fps_history[i];
        }
        if (samples > 0) {
            avg_fps /= samples;
            printf("Min/Max/Avg: %.2f / %.2f / %.2f\n", min_fps, max_fps, avg_fps);
        }
        
        /* Move to next test */
        current_test++;
        frame_count = 0;
        fps_index = 0;
        test_start_time = now;
        
        /* Clear history */
        memset(fps_history, 0, sizeof(fps_history));
        
        if (current_test >= TEST_COUNT) {
            printf("\n=== ALL TESTS COMPLETE ===\n");
            running = 0;
        }
    }
}

int main(int argc, char* argv[]) {
    SDL_Event event;
    
    (void)argc;
    (void)argv;
    
    printf("SDL2 Atari Performance Test Suite\n");
    printf("Window: %dx%d, Duration: %ds per test\n\n", 
           WINDOW_WIDTH, WINDOW_HEIGHT, TEST_DURATION_MS / 1000);
    
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    /* Create window */
    window = SDL_CreateWindow("SDL2 Perf Test",
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
                    /* Skip to next test */
                    frame_count = 999999; /* Force test end */
                }
            }
        }
        
        /* Run current test */
        run_test();
        
        /* Small delay to prevent total CPU hogging on fast systems */
        /* SDL_Delay(1); */ /* Uncomment if needed */
    }
    
    /* Cleanup */
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("\nTest suite completed.\n");
    return 0;
}
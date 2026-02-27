/*
 * test_256color.c - SDL2 256-color mode test for Atari GEM
 * Compile with: gcc -o test_256color test_256color.c `sdl2-config --cflags --libs`
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 200

/* RGB332 palette for 256 colors */
static void build_rgb332_palette(SDL_Color *colors) {
    int i;
    for (i = 0; i < 256; i++) {
        Uint8 r3 = (i >> 5) & 0x07;  /* 3 bits */
        Uint8 g3 = (i >> 2) & 0x07;  /* 3 bits */
        Uint8 b2 = i & 0x03;         /* 2 bits */
        
        /* Expand to 8-bit */
        colors[i].r = (r3 << 5) | (r3 << 2) | (r3 >> 1);
        colors[i].g = (g3 << 5) | (g3 << 2) | (g3 >> 1);
        colors[i].b = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
        colors[i].a = 255;
    }
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Surface *surface = NULL;
    SDL_Event event;
    int running = 1;
    int frame = 0;
    SDL_Color palette[256];
    int i, x, y;
    
    (void)argc;
    (void)argv;
    
    printf("SDL2 256-Color Test for Atari GEM\n");
    printf("=================================\n");
    
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    /* Request 256-color window (RGB332) */
    window = SDL_CreateWindow("256-Color Test",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN);
    
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    /* Get window surface */
    surface = SDL_GetWindowSurface(window);
    if (!surface) {
        fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Surface format: %s\n", SDL_GetPixelFormatName(surface->format->format));
    printf("Surface bpp: %d\n", surface->format->BitsPerPixel);
    printf("Surface pitch: %d\n", surface->pitch);
    
    /* Check if we got RGB332 (8-bit indexed) */
    if (surface->format->format != SDL_PIXELFORMAT_RGB332) {
        printf("WARNING: Not in RGB332 format! 256-color test may not work correctly.\n");
    }
    
    /* Set palette if indexed format */
    if (surface->format->palette) {
        build_rgb332_palette(palette);
        SDL_SetPaletteColors(surface->format->palette, palette, 0, 256);
        printf("Palette set with 256 RGB332 colors\n");
    } else {
        printf("No palette - format is likely TrueColor\n");
    }
    
    printf("\nTest patterns:\n");
    printf("- Frame 0: Color bars (all 256 colors)\n");
    printf("- Frame 1: Gradient red\n");
    printf("- Frame 2: Gradient green\n");
    printf("- Frame 3: Gradient blue\n");
    printf("- Frame 4: Checkerboard pattern\n");
    printf("Click window or press key to cycle, ESC to quit\n\n");
    
    /* Main loop */
    while (running) {
        Uint8 *pixels = (Uint8 *)surface->pixels;
        int pitch = surface->pitch;
        
        /* Handle events */
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    } else {
                        frame = (frame + 1) % 5;
                        printf("Frame %d\n", frame);
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    frame = (frame + 1) % 5;
                    printf("Frame %d\n", frame);
                    break;
            }
        }
        
        /* Clear screen */
        SDL_LockSurface(surface);
        memset(pixels, 0, pitch * WINDOW_HEIGHT);
        
        switch (frame) {
            case 0: {
                /* Color bars - display all 256 colors */
                int bar_width = WINDOW_WIDTH / 16;
                int bar_height = WINDOW_HEIGHT / 16;
                for (y = 0; y < 16; y++) {
                    for (x = 0; x < 16; x++) {
                        Uint8 color = (y << 4) | x;  /* 0-255 */
                        int bx = x * bar_width;
                        int by = y * bar_height;
                        int bw = (x == 15) ? (WINDOW_WIDTH - bx) : bar_width;
                        int bh = (y == 15) ? (WINDOW_HEIGHT - by) : bar_height;
                        
                        for (i = by; i < by + bh && i < WINDOW_HEIGHT; i++) {
                            memset(pixels + i * pitch + bx, color, bw);
                        }
                    }
                }
                break;
            }
            
            case 1: {
                /* Red gradient - test 3-bit red (0-7) */
                for (y = 0; y < WINDOW_HEIGHT; y++) {
                    Uint8 red = ((y * 8) / WINDOW_HEIGHT) << 5;  /* 0, 32, 64, 96, 128, 160, 192, 224 */
                    for (x = 0; x < WINDOW_WIDTH; x++) {
                        pixels[y * pitch + x] = red | 0x00;  /* Red only */
                    }
                }
                break;
            }
            
            case 2: {
                /* Green gradient - test 3-bit green (0-7) */
                for (y = 0; y < WINDOW_HEIGHT; y++) {
                    Uint8 green = ((y * 8) / WINDOW_HEIGHT) << 2;  /* 0, 4, 8, 12, 16, 20, 24, 28 */
                    for (x = 0; x < WINDOW_WIDTH; x++) {
                        pixels[y * pitch + x] = green | 0x00;  /* Green only */
                    }
                }
                break;
            }
            
            case 3: {
                /* Blue gradient - test 2-bit blue (0-3) */
                for (y = 0; y < WINDOW_HEIGHT; y++) {
                    Uint8 blue = (y * 4) / WINDOW_HEIGHT;  /* 0, 1, 2, 3 */
                    for (x = 0; x < WINDOW_WIDTH; x++) {
                        pixels[y * pitch + x] = blue;  /* Blue only */
                    }
                }
                break;
            }
            
            case 4: {
                /* Checkerboard - tests bit patterns */
                for (y = 0; y < WINDOW_HEIGHT; y++) {
                    for (x = 0; x < WINDOW_WIDTH; x++) {
                        int check = ((x / 8) + (y / 8)) & 1;
                        /* Alternating between dark gray (0x6D) and light gray (0xB6) */
                        pixels[y * pitch + x] = check ? 0xB6 : 0x6D;
                    }
                }
                break;
            }
        }
        
        SDL_UnlockSurface(surface);
        
        /* Update window */
        SDL_UpdateWindowSurface(window);
        
        /* Small delay to prevent hogging CPU */
        SDL_Delay(50);
    }
    
    printf("\nTest complete.\n");
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
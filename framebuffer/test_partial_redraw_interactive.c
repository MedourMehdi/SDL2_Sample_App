#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Test program to verify partial screen updates
 * This program:
 * 1. Creates a white window
 * 2. Draws random colored rectangles when you click
 * 3. Only updates the clicked rectangle (not the whole screen)
 * 4. Prints debug info to show what's being updated
 */

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

/* Generate random color */
Uint32 random_color(SDL_PixelFormat *format) {
    Uint8 r = rand() % 256;
    Uint8 g = rand() % 256;
    Uint8 b = rand() % 256;
    return SDL_MapRGB(format, r, g, b);
}

/* Draw a filled rectangle in the surface */
void draw_rect(SDL_Surface *surface, int x, int y, int w, int h, Uint32 color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_FillRect(surface, &rect, color);
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Surface *screen = NULL;
    int running = 1;
    int rect_size = 80;
    int update_count = 0;
    
    srand(time(NULL));
    
    printf("SDL2 Partial Redraw Test\n");
    printf("========================\n");
    printf("Click anywhere to draw a colored rectangle\n");
    printf("Only the clicked area should be updated (not the whole screen)\n");
    printf("Press ESC to quit\n\n");
    
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    /* Create window */
    window = SDL_CreateWindow("Partial Redraw Test",
                             SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED,
                             WINDOW_WIDTH, WINDOW_HEIGHT,
                             0);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
    /* Get window surface */
    screen = SDL_GetWindowSurface(window);
    if (!screen) {
        fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("Window created: %dx%d, %d bpp\n", 
           screen->w, screen->h, 
           SDL_BITSPERPIXEL(screen->format->format));
    printf("Pixel format: %s\n\n", SDL_GetPixelFormatName(screen->format->format));
    
    /* Fill screen with white initially */
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));
    SDL_UpdateWindowSurface(window);
    
    printf("Initial white screen drawn\n");
    printf("Waiting for mouse clicks...\n\n");
    
    /* Main loop */
    while (running) {
        SDL_Event event;
        
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN: {
                    int x = event.button.x;
                    int y = event.button.y;
                    
                    /* Center the rectangle on the click */
                    int rect_x = x - rect_size / 2;
                    int rect_y = y - rect_size / 2;
                    
                    /* Clamp to window bounds */
                    if (rect_x < 0) rect_x = 0;
                    if (rect_y < 0) rect_y = 0;
                    if (rect_x + rect_size > WINDOW_WIDTH) rect_x = WINDOW_WIDTH - rect_size;
                    if (rect_y + rect_size > WINDOW_HEIGHT) rect_y = WINDOW_HEIGHT - rect_size;
                    
                    /* Generate random color */
                    Uint32 color = random_color(screen->format);
                    
                    /* Draw rectangle */
                    draw_rect(screen, rect_x, rect_y, rect_size, rect_size, color);
                    
                    /* CRITICAL: Update ONLY the changed rectangle */
                    SDL_Rect update_rect = {rect_x, rect_y, rect_size, rect_size};
                    
                    printf("Update #%d: Drawing rect at (%d,%d) size %dx%d\n", 
                           ++update_count, rect_x, rect_y, rect_size, rect_size);
                    printf("  -> Calling SDL_UpdateWindowSurfaceRects with 1 rect\n");
                    
                    /* This should only update the specified rectangle! */
                    if (SDL_UpdateWindowSurfaceRects(window, &update_rect, 1) < 0) {
                        fprintf(stderr, "SDL_UpdateWindowSurfaceRects failed: %s\n", 
                                SDL_GetError());
                    } else {
                        printf("  -> Update successful\n\n");
                    }
                    
                    /* Alternative test: uncomment to test full update
                     * SDL_UpdateWindowSurface(window);
                     * printf("  -> Full window update performed\n\n");
                     */
                    
                    break;
                }
            }
        }
        
        SDL_Delay(10);
    }
    
    printf("\nTest completed. Total updates: %d\n", update_count);
    
    /* Cleanup */
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
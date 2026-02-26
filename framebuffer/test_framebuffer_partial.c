/* =============================================================================
 * SDL2 Direct Framebuffer Test (No Renderer)
 * This bypasses SDL_Renderer entirely to test GEM driver directly
 * ============================================================================= */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define WIDTH  320
#define HEIGHT 200

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Surface *screen = NULL;
    Uint8 *pixels;
    int pitch;
    int running = 1;
    Uint32 frame = 0;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    
    window = SDL_CreateWindow("Direct Framebuffer Test", 
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                              WIDTH, HEIGHT, 0);
    
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    /* Get the window surface - this calls your CreateWindowFramebuffer */
    screen = SDL_GetWindowSurface(window);
    if (!screen) {
        printf("SDL_GetWindowSurface failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    printf("===========================================\n");
    printf("Window surface: %dx%d\n", screen->w, screen->h);
    printf("Format: 0x%08X (%s)\n", screen->format->format, 
           SDL_GetPixelFormatName(screen->format->format));
    printf("Pitch: %d bytes\n", screen->pitch);
    printf("BPP: %d\n", screen->format->BytesPerPixel);
    printf("===========================================\n\n");
    
    pixels = (Uint8*)screen->pixels;
    pitch = screen->pitch;
    
    /* Initial clear */
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_UpdateWindowSurface(window);
    
    printf("Press Ctrl+C to quit...\n\n");
    
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        
        frame++;
        
        /* Test 1: Blinking cursor (partial update) */
        if (frame < 300) {
            SDL_Rect cursor_rect = {10, HEIGHT - 20, 20, 2};
            Uint32 color = ((frame / 30) % 2) ? 
                SDL_MapRGB(screen->format, 255, 255, 255) :  /* White */
                SDL_MapRGB(screen->format, 0, 0, 0);         /* Black */
            
            SDL_FillRect(screen, &cursor_rect, color);
            
            /* THIS IS THE KEY: Update ONLY the cursor rectangle */
            SDL_UpdateWindowSurfaceRects(window, &cursor_rect, 1);
            
            if (frame == 1) printf("Test 1: Blinking cursor (should see white rect blinking)\n");
        }
        /* Test 2: Moving sprite (partial update) */
        else if (frame < 600) {
            if (frame == 300) {
                printf("Test 2: Moving red square\n");
                SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
                SDL_UpdateWindowSurface(window);
            }
            
            /* Clear old position */
            static int old_x = 0, old_y = 0;
            if (frame > 300) {
                SDL_Rect old_rect = {old_x, old_y, 32, 32};
                SDL_FillRect(screen, &old_rect, SDL_MapRGB(screen->format, 0, 0, 0));
            }
            
            /* Draw new position */
            int x = ((frame - 300) * 2) % (WIDTH - 32);
            int y = ((frame - 300)) % (HEIGHT - 32);
            SDL_Rect sprite_rect = {x, y, 32, 32};
            SDL_FillRect(screen, &sprite_rect, SDL_MapRGB(screen->format, 255, 0, 0)); /* Red */
            
            /* Update both old and new positions */
            SDL_Rect update_rects[2];
            int num_rects = 1;
            update_rects[0] = sprite_rect;
            if (frame > 300) {
                SDL_Rect old_rect = {old_x, old_y, 32, 32};
                update_rects[1] = old_rect;
                num_rects = 2;
            }
            
            SDL_UpdateWindowSurfaceRects(window, update_rects, num_rects);
            
            old_x = x;
            old_y = y;
        }
        /* Test 3: Full screen color change */
        else if (frame < 900) {
            if (frame == 600) printf("Test 3: Full screen color changes\n");
            
            Uint8 r = (Uint8)((frame - 600) % 256);
            SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, r, r, r));
            SDL_UpdateWindowSurface(window);
        }
        else {
            running = 0;
        }
        
        SDL_Delay(16);  /* ~60 FPS */
    }

    printf("\n=== TEST COMPLETE ===\n");
    printf("If you saw:\n");
    printf("  1. Blinking white cursor → Partial updates WORK ✓\n");
    printf("  2. Moving red square → Partial updates WORK ✓\n");
    printf("  3. Color changes → Full updates WORK ✓\n");
    printf("\nIf you only saw #3 → BUG in UpdateWindowFramebuffer\n");
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
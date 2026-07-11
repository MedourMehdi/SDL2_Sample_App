/*
 * GEM Window Management Stress Test for SDL2
 * Designed to validate Atari AES window states, resizing, and handle exhaustion fixes.
 * C90 Compliant.
 */

#include <SDL2/SDL.h>
#include <stdio.h>

/* Setup colors (RGB332 friendly for 8-bit, but works in TrueColor) */
#define COLOR_BG      SDL_MapRGB(fmt, 0x00, 0x00, 0x80) /* Dark Blue */
#define COLOR_BORDER  SDL_MapRGB(fmt, 0xFF, 0x00, 0x00) /* Red */
#define COLOR_MOUSE   SDL_MapRGB(fmt, 0x00, 0xFF, 0x00) /* Green */

/* State trackers */
static SDL_bool is_fullscreen = SDL_FALSE;
static SDL_bool is_bordered = SDL_TRUE;
static SDL_bool is_resizable = SDL_TRUE;

static void DrawTestPattern(SDL_Surface *surface, int mouse_x, int mouse_y)
{
    SDL_PixelFormat *fmt;
    SDL_Rect rect;

    if (!surface) return;
    fmt = surface->format;

    /* Fill background */
    SDL_FillRect(surface, NULL, COLOR_BG);

    /* Draw a 5-pixel inner border to verify clipping and work area dimensions */
    rect.x = 0; rect.y = 0; rect.w = surface->w; rect.h = 5;
    SDL_FillRect(surface, &rect, COLOR_BORDER); /* Top */
    rect.x = 0; rect.y = surface->h - 5; rect.w = surface->w; rect.h = 5;
    SDL_FillRect(surface, &rect, COLOR_BORDER); /* Bottom */
    rect.x = 0; rect.y = 0; rect.w = 5; rect.h = surface->h;
    SDL_FillRect(surface, &rect, COLOR_BORDER); /* Left */
    rect.x = surface->w - 5; rect.y = 0; rect.w = 5; rect.h = surface->h;
    SDL_FillRect(surface, &rect, COLOR_BORDER); /* Right */

    /* Draw a 10x10 block at the mouse cursor to verify coordinate translation */
    if (mouse_x >= 0 && mouse_y >= 0) {
        rect.x = mouse_x - 5;
        rect.y = mouse_y - 5;
        rect.w = 10;
        rect.h = 10;
        SDL_FillRect(surface, &rect, COLOR_MOUSE);
    }
}

static void PrintHelp(void)
{
    SDL_Log("=================================================");
    SDL_Log(" GEM Window Management Test");
    SDL_Log("=================================================");
    SDL_Log(" [F] - Toggle Fullscreen (Desktop)");
    SDL_Log(" [M] - Maximize Window");
    SDL_Log(" [R] - Restore Window");
    SDL_Log(" [I] - Iconify (Minimize) Window");
    SDL_Log(" [B] - Toggle Bordered / Borderless (Tests GEM_ReopenWindow)");
    SDL_Log(" [S] - Toggle Resizable (Tests GEM_ReopenWindow)");
    SDL_Log(" [ESC] - Quit");
    SDL_Log("-------------------------------------------------");
    SDL_Log(" Try resizing the window manually. It is clamped to");
    SDL_Log(" 320x200 minimum and 800x600 maximum to test WM_SIZED.");
    SDL_Log("=================================================");
}

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    SDL_Surface *surface = NULL;
    SDL_Event event;
    int running = 1;
    int mouse_x = -1;
    int mouse_y = -1;

    (void)argc;
    (void)argv;

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* Create window centered, allowing AES to calculate WC_BORDER */
    window = SDL_CreateWindow("GEM AES Test",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              640, 400,
                              SDL_WINDOW_RESIZABLE);

    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Enforce size constraints to test the WM_SIZED clamping logic in SDL_gemevents.c */
    SDL_SetWindowMinimumSize(window, 320, 200);
    SDL_SetWindowMaximumSize(window, 800, 600);

    PrintHelp();

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            running = 0;
                            break;

                        case SDLK_f:
                            SDL_Log("Toggling Fullscreen...");
                            is_fullscreen = !is_fullscreen;
                            SDL_SetWindowFullscreen(window, is_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                            break;

                        case SDLK_m:
                            SDL_Log("Maximizing...");
                            SDL_MaximizeWindow(window);
                            break;

                        case SDLK_r:
                            SDL_Log("Restoring...");
                            SDL_RestoreWindow(window);
                            break;

                        case SDLK_i:
                            SDL_Log("Iconifying...");
                            SDL_MinimizeWindow(window);
                            break;

                        case SDLK_b:
                            SDL_Log("Toggling Borders...");
                            is_bordered = !is_bordered;
                            SDL_SetWindowBordered(window, is_bordered ? SDL_TRUE : SDL_FALSE);
                            break;

                        case SDLK_s:
                            SDL_Log("Toggling Resizable...");
                            is_resizable = !is_resizable;
                            SDL_SetWindowResizable(window, is_resizable ? SDL_TRUE : SDL_FALSE);
                            break;
                            
                        default:
                            break;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    mouse_x = event.motion.x;
                    mouse_y = event.motion.y;
                    break;

                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            SDL_Log("Window resized to: %dx%d", event.window.data1, event.window.data2);
                            /* Invalidate surface so it is fetched again */
                            surface = NULL; 
                            break;
                        case SDL_WINDOWEVENT_MAXIMIZED:
                            SDL_Log("Event: Window Maximized");
                            break;
                        case SDL_WINDOWEVENT_RESTORED:
                            SDL_Log("Event: Window Restored");
                            break;
                        case SDL_WINDOWEVENT_MINIMIZED:
                            SDL_Log("Event: Window Minimized");
                            break;
                        case SDL_WINDOWEVENT_EXPOSED:
                            /* Force a redraw */
                            break;
                    }
                    break;
            }
        }

        /* Fetch the framebuffer surface (re-fetches automatically if resized) */
        if (!surface) {
            surface = SDL_GetWindowSurface(window);
            if (!surface) {
                SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "GetWindowSurface failed: %s", SDL_GetError());
                continue;
            }
        }

        DrawTestPattern(surface, mouse_x, mouse_y);
        SDL_UpdateWindowSurface(window);

        /* Yield briefly to not hog the 68000 CPU completely */
        SDL_Delay(16); 
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
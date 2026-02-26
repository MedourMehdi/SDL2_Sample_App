#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    SDL_Window *window = NULL;
    SDL_Surface *screenSurface = NULL;
    SDL_Surface *image = NULL;
    SDL_Rect dest;
    int quit = 0;
    SDL_Event e;

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    // if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS) < 0) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // window = SDL_CreateWindow("SDL Test",
    //     SDL_WINDOWPOS_UNDEFINED,
    //     SDL_WINDOWPOS_UNDEFINED,
    //     320, 200,
    //     SDL_WINDOW_SHOWN | SDL_WINDOW_FOREIGN);

    /* Create window */
    // window = SDL_CreateWindow("SDL Test",
    //                         SDL_WINDOWPOS_UNDEFINED,
    //                         SDL_WINDOWPOS_UNDEFINED,
    //                         320, 200,
    //                         SDL_WINDOW_SHOWN);

        window = SDL_CreateWindow("TEST2",
            200, 200,
            320, 200,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);                            
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    /* Get window surface */
    screenSurface = SDL_GetWindowSurface(window);
    if (!screenSurface) {
        printf("Could not get window surface! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    /* Create a test pattern */
    image = SDL_CreateRGBSurface(0, 320, 200, 16, 0xF800, 0x07E0, 0x001F, 0);
    if (!image) {
        printf("Could not create test surface! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    /* Draw some test pattern */
    SDL_FillRect(image, NULL, SDL_MapRGB(image->format, 0, 0, 255));
    
    /* Draw a red rectangle in the center */
    SDL_Rect centerRect = {
        (320 - 50) / 2,
        (200 - 50) / 2,
        50,
        50
    };
    SDL_FillRect(image, &centerRect, SDL_MapRGB(image->format, 255, 0, 0));

    /* Event loop */
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
                case SDL_QUIT:
                    quit = 1;
                    break;
                    
                case SDL_WINDOWEVENT:
                    printf("Window event: ");
                    switch (e.window.event) {
                        case SDL_WINDOWEVENT_MOVED:
                            printf("moved to %d,%d\n", e.window.data1, e.window.data2);
                            break;
                        case SDL_WINDOWEVENT_EXPOSED:
                            printf("exposed\n");
                            break;
                        case SDL_WINDOWEVENT_SHOWN:
                            printf("shown\n");
                            break;
                        case SDL_WINDOWEVENT_HIDDEN:
                            printf("hidden\n");
                            break;
                        default:
                            printf("unknown window event: %d\n", e.window.event);
                            break;
                    }
                    break;
            }
        }

        /* Clear screen */
        SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0, 0, 0));

        /* Blit test pattern */
        dest.x = 0;
        dest.y = 0;
        dest.w = image->w;
        dest.h = image->h;
        SDL_BlitSurface(image, NULL, screenSurface, &dest);

        /* Update the surface */
        SDL_UpdateWindowSurface(window);
    }

    SDL_FreeSurface(image);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

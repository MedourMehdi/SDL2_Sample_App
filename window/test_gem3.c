/* Minimal SDL2 window + renderer test for Atari GEM */
#include <SDL2/SDL.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    SDL_Window   *window   = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Event     e;
    int           running  = 1;
    int           frame    = 0;

    (void)argc; (void)argv;

    printf("Step 1: SDL_Init VIDEO\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("Step 1 OK\n");

    printf("Step 2: SDL_CreateWindow\n");
    window = SDL_CreateWindow("Test",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              320, 120,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Step 2 OK\n");

    printf("Step 3: SDL_CreateRenderer\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Step 3 OK\n");

    printf("Step 4: draw loop (10 frames then quit)\n");
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 128, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        {
            SDL_Rect r = { 10, 10, 100, 20 };
            SDL_RenderFillRect(renderer, &r);
        }
        SDL_RenderPresent(renderer);

        SDL_Delay(100);
        if (++frame >= 10) running = 0;
    }
    printf("Step 4 OK\n");

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("All steps passed.\n");
    return 0;
}
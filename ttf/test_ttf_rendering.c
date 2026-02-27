#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#define SCREEN_W 320
#define SCREEN_H 200

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("Atari Geek Motivation", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    TTF_Font* font = TTF_OpenFont("../font/arial.ttf", 18);
    if (!font) {
        printf("Font error: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Color white = {255, 255, 255, 255};
    
    /* A longer motivational string that requires wrapping */
    const char* message = "KEEP THE 68K FLAME ALIVE: CODE ON ATARI! THE DEMOSCENE NEVER DIES.";
    
    /* * FIX: Use TTF_RenderText_Blended_Wrapped
     * wrapLength (280) tells SDL to start a new line after 280 pixels.
     */
    SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, message, white, 280);
    if (!surface) return 1;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    /* Centering logic remains the same, but now accounts for the multi-line height */
    SDL_Rect textRect = { 
        (SCREEN_W - surface->w) / 2, 
        (SCREEN_H - surface->h) / 2, 
        surface->w, 
        surface->h 
    };
    
    SDL_FreeSurface(surface);

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &textRect);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
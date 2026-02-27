#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // 1. Initialize SDL2 and TTF
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("SDL2_ttf Example", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    // 2. Load the Font (Size 24)
    // Replace "arial.ttf" with the path to your actual font file
    TTF_Font* font = TTF_OpenFont("../font/arial.ttf", 24);
    if (!font) {
        printf("Font error: %s\n", TTF_GetError());
        return 1;
    }

    // 3. Create Text Surface and Texture
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surface = TTF_RenderText_Blended(font, "m68k CPU: 8.00 MHz", white);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    // Define where the text goes
    SDL_Rect textRect = { (640 - surface->w) / 2, (480 - surface->h) / 2, surface->w, surface->h };
    
    SDL_FreeSurface(surface); // Free surface once texture is created

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 4. Draw the texture
        SDL_RenderCopy(renderer, texture, NULL, &textRect);

        SDL_RenderPresent(renderer);
    }

    // 5. Cleanup
    SDL_DestroyTexture(texture);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#define SCREEN_W 320
#define SCREEN_H 200

int main(int argc, char *argv[])
{
    SDL_Window   *window   = NULL;
    SDL_Renderer *renderer = NULL;
    Mix_Chunk    *wave     = NULL;
    TTF_Font     *font     = NULL;
    SDL_Texture  *textTex  = NULL;
    SDL_Rect      textRect;
    int channel;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename.wav>\n", argv[0]);
        return 1;
    }

    /* 1. Initialize Video, Audio, and TTF */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    if (TTF_Init() < 0) return 1;

    window = SDL_CreateWindow("Atari Audio Info", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    /* 2. Initialize Mixer (Corrected: 2048 chunk size for 68k balance) */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
        return 1;
    }

    /* 3. Load WAV and Font */
    wave = Mix_LoadWAV(argv[1]);
    font = TTF_OpenFont("../font/arial.ttf", 16); 
    if (!wave || !font) {
        SDL_Log("Loading error: %s", (!wave) ? Mix_GetError() : TTF_GetError());
        return 1;
    }

    /* 4. Prepare Wrapped Text to use the Height (H) 
     * We use a wrap width of 260 to force the text to grow vertically.
     */
    SDL_Color white = {255, 255, 255, 255};
    char info[512];
    sprintf(info, "NOW PLAYING:\n%s\n\nSTATS: 44.1kHz / 16-bit / Stereo\n\nKEEP CODING ON ATARI!", argv[1]);
    
    SDL_Surface* textSurf = TTF_RenderText_Blended_Wrapped(font, info, white, 260);
    if (textSurf) {
        textTex = SDL_CreateTextureFromSurface(renderer, textSurf);
        /* Center the multi-line block using the window height */
        textRect.x = (SCREEN_W - textSurf->w) / 2;
        textRect.y = (SCREEN_H - textSurf->h) / 2;
        textRect.w = textSurf->w;
        textRect.h = textSurf->h;
        SDL_FreeSurface(textSurf);
    }

    /* 5. Start Playback */
    channel = Mix_PlayChannel(-1, wave, 0);

    /* 6. Main Loop: Responsive while sound plays */
    int running = 1;
    SDL_Event event;
    while (running && Mix_Playing(channel)) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN) running = 0;
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        
        if (textTex) SDL_RenderCopy(renderer, textTex, NULL, &textRect);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(20); /* Save CPU cycles */
    }

    /* 7. Cleanup */
    if (textTex) SDL_DestroyTexture(textTex);
    TTF_CloseFont(font);
    Mix_FreeChunk(wave);
    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
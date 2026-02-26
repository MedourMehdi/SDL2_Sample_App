#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_AUDIO);
    
    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        printf("SDL_mixer Error: %s\n", Mix_GetError());
        return -1;
    }
    
    // Load AIFF file
    Mix_Chunk *sound = Mix_LoadWAV(argv[1]);
    if (!sound) {
        printf("Failed to load AIFF: %s\n", Mix_GetError());
        return -1;
    }
    
    // Play the sound
    Mix_PlayChannel(-1, sound, 0);
    
    // Wait for it to finish
    SDL_Delay(3000);
    
    // Cleanup
    Mix_FreeChunk(sound);
    Mix_CloseAudio();
    SDL_Quit();
    
    return 0;
}
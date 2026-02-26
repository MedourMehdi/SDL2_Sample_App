/*
 * SDL2 Mixer WAV Player Example
 *
 * Compile:
 *   gcc play_wav.c -o play_wav -lSDL2 -lSDL2_mixer
 *
 * Usage:
 *   ./play_wav sound.wav
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <pthread.h>

int main(int argc, char *argv[])
{
    Mix_Chunk *wave = NULL;
    int channel;

    /* 1. Check Arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename.wav>\n", argv[0]);
        return 1;
    }

    /* 2. Initialize SDL */
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* 3. Initialize Mixer 
     * 44100Hz, Default Format (16-bit usually), 2 Channels (Stereo), 512 Chunk size
     */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }

    /* 4. Allocate Channels (Optional, but good practice) */
    Mix_AllocateChannels(1);

    /* 5. Load the WAV file */
    wave = Mix_LoadWAV(argv[1]);
    if (wave == NULL) {
        fprintf(stderr, "Mix_LoadWAV failed: %s\n", Mix_GetError());
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    /* 6. Play the sound 
     * Channel -1 (first available free channel)
     * Play 0 times (0 means play once, 1 means play twice, -1 means loop forever)
     */
    channel = Mix_PlayChannel(-1, wave, 0);
    if (channel == -1) {
        fprintf(stderr, "Mix_PlayChannel failed: %s\n", Mix_GetError());
        Mix_FreeChunk(wave);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    printf("Playing %s... (Press Ctrl+C to stop)\n", argv[1]);

    /* 7. Wait until the sound finishes playing */
    /* We use a simple loop here. For a game, you'd do this inside your main loop. */
    while (Mix_Playing(channel) != 0) {
        /* Delay 100ms to save CPU */
        // SDL_Delay(100); 
        pthread_yield();
    }

    printf("Finished playing.\n");

    /* 8. Cleanup */
    Mix_FreeChunk(wave);
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}
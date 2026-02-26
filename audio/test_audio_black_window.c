#include <SDL2/SDL.h>
#include <stdio.h>

/* --- Simple Audio Structure --- */
typedef struct {
    Uint8 *pos;
    Uint32 len;
} AudioData;

/* --- Audio Callback --- */
void audio_callback(void* userdata, Uint8* stream, int len) {
    AudioData* audio = (AudioData*)userdata;

    if (audio->len == 0) {
        SDL_memset(stream, 0, len);
        return;
    }

    /* Mix only what is left */
    Uint32 mix_len = (Uint32)len;
    if (mix_len > audio->len) mix_len = audio->len;

    /* * NOTE: Since standard WAV is Little Endian and Atari is Big Endian,
     * SDL_MixAudio handles the conversion if spec.format is set correctly.
     */
    SDL_memset(stream, 0, len);
    SDL_MixAudio(stream, audio->pos, mix_len, SDL_MIX_MAXVOLUME);

    audio->pos += mix_len;
    audio->len -= mix_len;
}

int main(int argc, char* argv[]) {
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_AudioSpec wav_spec;
    Uint8* wav_buffer = NULL;
    Uint32 wav_length = 0;
    AudioData audio;

    if (argc < 2) {
        printf("Usage: %s <file.wav>\n", argv[0]);
        return 1;
    }

    /* Initialize SDL Video and Audio */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        return 1;
    }

    /* Load the WAV file (Standard RIFF) */
    if (SDL_LoadWAV(argv[1], &wav_spec, &wav_buffer, &wav_length) == NULL) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Set up our data for the callback */
    audio.pos = wav_buffer;
    audio.len = wav_length;

    wav_spec.callback = audio_callback;
    wav_spec.userdata = &audio;

    /* Open Audio Device */
    if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
        fprintf(stderr, "Audio Error: %s\n", SDL_GetError());
        SDL_FreeWAV(wav_buffer);
        SDL_Quit();
        return 1;
    }

    /* Create a small black window */
    window = SDL_CreateWindow("SDL2 Player", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              160, 100, SDL_WINDOW_SHOWN);
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    /* Fill black background once */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    /* Start playing */
    SDL_PauseAudio(0);

    /* --- Efficient Event Loop --- */
    SDL_Event e;
    int running = 1;
    while (running) {
        /* * SDL_WaitEvent is perfect for Atari/Falcon. 
         * It puts the process to sleep until you click the [X] button.
         */
        if (SDL_WaitEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            }
        }
    }

    /* Cleanup */
    SDL_CloseAudio();
    SDL_FreeWAV(wav_buffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
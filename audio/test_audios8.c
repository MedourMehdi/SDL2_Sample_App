/* Simple SDL2 mono 8-bit signed PCM playback test for Atari STE
   Generates a 440 Hz sine wave as AUDIO_S8 mono and plays it.
   No file loading - pure synthesis so nothing else can go wrong. */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>

#define SAMPLE_RATE  25033   /* Real STE 25 kHz mode */
#define CHANNELS     1
#define SAMPLES      512
#define AMPLITUDE    100     /* 0..127 */

static Uint32 audio_pos = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    Sint8 *out = (Sint8 *)stream;
    int i;
    (void)userdata;

    for (i = 0; i < len; i++) {
        double t = (double)audio_pos / SAMPLE_RATE;
        out[i] = (Sint8)(AMPLITUDE * sin(2.0 * M_PI * 440.0 * t));
        audio_pos++;
    }
}

int main(int argc, char *argv[])
{
    SDL_AudioSpec want, got;
    SDL_AudioDeviceID dev;
    int i;
    (void)argc; (void)argv;

    printf("Step 1: SDL_Init AUDIO\n");
    fflush(stdout);
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("Step 1 OK\n");
    fflush(stdout);

    SDL_zero(want);
    want.freq     = SAMPLE_RATE;
    want.format   = AUDIO_S8;
    want.channels = CHANNELS;
    want.samples  = SAMPLES;
    want.callback = audio_callback;
    want.userdata = NULL;

    printf("Step 2: SDL_OpenAudioDevice freq=%d fmt=AUDIO_S8 ch=%d\n",
           want.freq, want.channels);
    fflush(stdout);

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (dev == 0) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    printf("Step 2 OK\n");
    printf("  got: freq=%d fmt=0x%04X ch=%d samples=%d size=%d\n",
           got.freq, got.format, got.channels, got.samples, got.size);
    fflush(stdout);

    printf("Step 3: SDL_PauseAudioDevice (start playback)\n");
    fflush(stdout);
    SDL_PauseAudioDevice(dev, 0);
    printf("Step 3 OK - playing 3 seconds...\n");
    fflush(stdout);

    for (i = 0; i < 30; i++) {
        SDL_Delay(100);
        printf("  t=%d00ms pos=%lu\n", i+1, (unsigned long)audio_pos);
        fflush(stdout);
    }

    printf("Step 4: SDL_CloseAudioDevice\n");
    fflush(stdout);
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    printf("Done.\n");
    return 0;
}
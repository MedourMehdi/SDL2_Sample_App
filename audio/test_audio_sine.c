#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>

#define AMPLITUDE 28000
#define FREQUENCY 440.0

void AudioCallback(void* userdata, Uint8* stream, int len)
{
    static double phase = 0;
    Sint16* buf = (Sint16*)stream;
    int samples = len / sizeof(Sint16);
    
    for(int i = 0; i < samples; i++) {
        buf[i] = (Sint16)(AMPLITUDE * sin(phase));
        phase += 2.0 * M_PI * FREQUENCY / 16000.0;
        if(phase > 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }
}

int main(int argc, char* argv[])
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    printf("SDL Audio test\n");

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    if(SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_zero(want);
    want.freq = 16000;
    want.format = AUDIO_S16MSB;  // Atari uses big-endian
    want.channels = 2;           // Stereo mode
    want.samples = 1024;         // Match the DMA buffer size
    want.callback = AudioCallback;

    printf("Before SDL_OpenAudioDevice\n");
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if(dev == 0) {
        printf("Open audio failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("Audio configuration:\n");
    printf("Frequency: %d\n", have.freq);
    printf("Format: 0x%x\n", have.format);
    printf("Channels: %d\n", have.channels);
    printf("Samples: %d\n", have.samples);

    printf("Before SDL_PauseAudioDevice\n");

    SDL_PauseAudioDevice(dev, 0);

    printf("Before SDL_Delay\n");
    SDL_Delay(5000);

    printf("Before SDL_CloseAudioDevice\n");
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN_W 360
#define WIN_H 180

static Uint8 *audio_buf;
static Uint32 audio_len;
static Uint32 audio_pos;

static volatile float peak_l = 0.0f;
static volatile float peak_r = 0.0f;

/* ================= AUDIO CALLBACK ================= */

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    if (audio_len == 0) {
        SDL_memset(stream, 0, len);
        peak_l = peak_r = 0.0f;
        return;
    }

    Uint32 to_copy = (len > audio_len) ? audio_len : len;
    SDL_memcpy(stream, audio_buf + audio_pos, to_copy);

    Sint16 *s = (Sint16 *)stream;
    int samples = to_copy / sizeof(Sint16);

    float max_l = 0.0f;
    float max_r = 0.0f;

    for (int i = 0; i + 1 < samples; i += 2) {
        float l = abs((int16_t)SDL_SwapLE16(s[i]))     / 32768.0f;
        float r = abs((int16_t)SDL_SwapLE16(s[i + 1])) / 32768.0f;
        if (l > max_l) max_l = l;
        if (r > max_r) max_r = r;
    }

    peak_l = max_l;
    peak_r = max_r;

    audio_pos += to_copy;
    audio_len -= to_copy;

    if (to_copy < (Uint32)len)
        SDL_memset(stream + to_copy, 0, len - to_copy);
}

/* ================= UI HELPERS ================= */

static void draw_bevel(SDL_Renderer *r, SDL_Rect rc, int raised)
{
    SDL_Color light = {255,255,255,255};
    SDL_Color dark  = {128,128,128,255};

    SDL_SetRenderDrawColor(r,
        raised ? light.r : dark.r,
        raised ? light.g : dark.g,
        raised ? light.b : dark.b, 255);

    SDL_RenderDrawLine(r, rc.x, rc.y, rc.x + rc.w - 1, rc.y);
    SDL_RenderDrawLine(r, rc.x, rc.y, rc.x, rc.y + rc.h - 1);

    SDL_SetRenderDrawColor(r,
        raised ? dark.r : light.r,
        raised ? dark.g : light.g,
        raised ? dark.b : light.b, 255);

    SDL_RenderDrawLine(r, rc.x + rc.w - 1, rc.y,
                          rc.x + rc.w - 1, rc.y + rc.h - 1);
    SDL_RenderDrawLine(r, rc.x, rc.y + rc.h - 1,
                          rc.x + rc.w - 1, rc.y + rc.h - 1);
}

static void draw_peak(SDL_Renderer *r, int x, int y, int h, float v)
{
    int level = (int)(v * (h - 4));
    if (level < 0) level = 0;
    if (level > h - 4) level = h - 4;

    SDL_Rect box = { x, y, 32, h };
    SDL_SetRenderDrawColor(r, 192,192,192,255);
    SDL_RenderFillRect(r, &box);
    draw_bevel(r, box, 0);

    SDL_Rect bar = { x + 2, y + h - level - 2, 28, level };
    SDL_SetRenderDrawColor(r, 0,160,0,255);
    SDL_RenderFillRect(r, &bar);
}

/* ================= MAIN ================= */

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: %s file.wav\n", argv[0]);
        return 1;
    }

    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "Retro WAV Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0);

    SDL_Renderer *ren =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    SDL_AudioSpec spec;
    Uint8 *wavbuf;
    Uint32 wavlen;

    if (!SDL_LoadWAV(argv[1], &spec, &wavbuf, &wavlen)) {
        printf("SDL_LoadWAV failed\n");
        return 1;
    }

    if (spec.channels != 2 || spec.format != AUDIO_S16) {
        printf("Only 16-bit stereo WAV supported\n");
        return 1;
    }

    audio_buf = wavbuf;
    audio_len = wavlen;
    audio_pos = 0;

    spec.callback = audio_callback;
    spec.userdata = NULL;

    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        SDL_SetRenderDrawColor(ren, 192,192,192,255);
        SDL_RenderClear(ren);

        SDL_Rect panel = { 40, 50, 280, 80 };
        draw_bevel(ren, panel, 1);

        draw_peak(ren, 90, 60, 60, peak_l);
        draw_peak(ren, 190, 60, 60, peak_r);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudio();
    SDL_FreeWAV(wavbuf);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

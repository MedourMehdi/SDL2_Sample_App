#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>

#define WIDTH  600
#define HEIGHT 300

int main(int argc, char *argv[])
{
    SDL_Window   *window   = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture  *texture  = NULL;
    SDL_Event     event;

    int running = 1;
    float time = 0.0f;

    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(
        "SDL2 Plasma Demo (C)",
        20,
        60,
        WIDTH,
        HEIGHT,
        SDL_WINDOW_SHOWN
    );

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_SOFTWARE
    );

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        WIDTH,
        HEIGHT
    );

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        uint32_t *pixels;
        int pitch;

        SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
        pitch /= sizeof(uint32_t);

        for (int y = 0; y < HEIGHT; y++)
        {
            for (int x = 0; x < WIDTH; x++)
            {
                float v =
                    sinf(x * 0.02f + time) +
                    sinf(y * 0.02f + time) +
                    sinf((x + y) * 0.01f + time);

                uint8_t r = (uint8_t)(128 + 127 * sinf(v));
                uint8_t g = (uint8_t)(128 + 127 * sinf(v + 2.0f));
                uint8_t b = (uint8_t)(128 + 127 * sinf(v + 4.0f));

                pixels[y * pitch + x] =
                    (255 << 24) | (r << 16) | (g << 8) | b;
            }
        }

        SDL_UnlockTexture(texture);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        time += 0.03f;
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}


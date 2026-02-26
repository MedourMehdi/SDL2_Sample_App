/* ============================================================
 * test_bgra8888_streaming.c
 *
 * Tests SDL_PIXELFORMAT_BGRA8888 + SDL_TEXTUREACCESS_STREAMING
 * on the Atari GEM renderer.
 *
 * Exercises three paths:
 *   1. SDL_LockTexture / SDL_UnlockTexture  (streaming write)
 *   2. SDL_UpdateTexture                   (direct pixel upload)
 *   3. SDL_RenderCopy                      (blit to window)
 *
 * Expected on screen (8bpp Atari):
 *   Frame 1-60:  Red rectangle top-left
 *   Frame 61-120: Green rectangle top-left
 *   Frame 121-180: Blue rectangle top-left
 *   Background: grey checkerboard
 *
 * If screen is black or colours are wrong, BGRA8888->RGB332
 * conversion is not working.
 * ============================================================ */

#include <SDL2/SDL.h>

#define WIN_W   320
#define WIN_H   200
#define RECT_W  80
#define RECT_H  60

/* BGRA8888 pixel: memory layout [B][G][R][A] */
static Uint32 make_bgra(Uint8 b, Uint8 g, Uint8 r, Uint8 a)
{
    /* On big-endian (68000): Uint32 stores MSB first.
     * BGRA8888 means B in byte[0], so B must be in bits 24-31. */
    return ((Uint32)b << 24) | ((Uint32)g << 16) | ((Uint32)r << 8) | a;
}

static void fill_bgra_surface(Uint32 *pixels, int pitch_bytes,
                               int w, int h,
                               Uint32 fg_color, Uint32 bg_color,
                               int rect_x, int rect_y,
                               int rect_w, int rect_h)
{
    int x, y;
    int pitch = pitch_bytes / 4;  /* pitch in pixels */

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            /* Checkerboard background */
            Uint32 c = (((x >> 3) + (y >> 3)) & 1)
                       ? make_bgra(0x60, 0x60, 0x60, 0xFF)
                       : make_bgra(0x40, 0x40, 0x40, 0xFF);

            /* Foreground rectangle */
            if (x >= rect_x && x < rect_x + rect_w &&
                y >= rect_y && y < rect_y + rect_h) {
                c = fg_color;
            }

            pixels[y * pitch + x] = c;
        }
    }
}

int main(int argc, char *argv[])
{
    SDL_Window   *window   = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture  *texture  = NULL;
    SDL_Event     event;
    int running = 1;
    int frame   = 0;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("BGRA8888 test",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              WIN_W, WIN_H, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Try GEM hardware renderer first, fall back to software */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("Hardware renderer failed, trying software: %s", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    {
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(renderer, &info) == 0)
            SDL_Log("Renderer: %s", info.name);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_BGRA8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                WIN_W, WIN_H);
    if (!texture) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Log("Texture created: BGRA8888 %dx%d", WIN_W, WIN_H);

    while (running) {
        Uint32 fg;
        void  *pixels;
        int    pitch;
        int    phase = (frame / 60) % 3;

        /* Colour cycles: Red → Green → Blue */
        if      (phase == 0) fg = make_bgra(0x00, 0x00, 0xFF, 0xFF); /* Red   */
        else if (phase == 1) fg = make_bgra(0x00, 0xFF, 0x00, 0xFF); /* Green */
        else                 fg = make_bgra(0xFF, 0x00, 0x00, 0xFF); /* Blue  */

        /* --- PATH 1: LockTexture / UnlockTexture (odd frames) --- */
        /* --- PATH 2: UpdateTexture               (even frames) --- */

        if (frame & 1) {
            /* PATH 1 */
            if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
                fill_bgra_surface((Uint32 *)pixels, pitch,
                                  WIN_W, WIN_H, fg,
                                  make_bgra(0x40, 0x40, 0x40, 0xFF),
                                  10, 10, RECT_W, RECT_H);
                SDL_UnlockTexture(texture);
            } else {
                SDL_Log("LockTexture failed: %s", SDL_GetError());
            }
        } else {
            /* PATH 2 */
            /* Allocate a temp buffer for UpdateTexture */
            int buf_pitch = WIN_W * 4;
            Uint32 *buf = (Uint32 *)SDL_malloc((size_t)WIN_H * buf_pitch);
            if (buf) {
                fill_bgra_surface(buf, buf_pitch,
                                  WIN_W, WIN_H, fg,
                                  make_bgra(0x60, 0x60, 0x60, 0xFF),
                                  10, 10, RECT_W, RECT_H);
                SDL_UpdateTexture(texture, NULL, buf, buf_pitch);
                SDL_free(buf);
            }
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        /* Events */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) running = 0;
            }
        }

        frame++;

        /* Log first few frames for diagnosis */
        if (frame <= 3) {
            SDL_Log("Frame %d: path=%s phase=%d",
                    frame,
                    (frame & 1) ? "Lock/Unlock" : "UpdateTexture",
                    phase);
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
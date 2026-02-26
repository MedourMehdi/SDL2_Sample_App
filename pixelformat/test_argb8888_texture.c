/* Compiler: m68k-atari-mint-gcc -O2 -lSDL2 -lgem
 *
 * Tests ARGB8888 texture creation and rendering through the GEM renderer.
 * Creates a texture manually in ARGB8888 format with known pixel values
 * so you can see exactly what the renderer does with it.
 */

#include <SDL2/SDL.h>
#include <string.h>

int main(int argc, char *argv[])
{
    SDL_Window   *win     = NULL;
    SDL_Renderer *ren     = NULL;
    SDL_Texture  *texture = NULL;
    SDL_Event     e;
    int           running = 1;

    /* 64x64 ARGB8888 pixel buffer — filled with known colours */
    static Uint32 pixels[64 * 64];
    int x, y;

    (void)argc; (void)argv;

    /* Fill the texture with 4 coloured quadrants so we can see
     * immediately if the colour conversion is correct:
     *   top-left    = red
     *   top-right   = green
     *   bottom-left = blue
     *   bottom-right= white
     * If colours are wrong or swapped we know the byte order is off.
     * If it crashes, we know CreateTexture or UpdateTexture is the problem. */
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            Uint32 *p = &pixels[y * 64 + x];
            if      (x < 32 && y < 32) *p = 0xFFFF0000; /* red   */
            else if (x >= 32 && y < 32) *p = 0xFF00FF00; /* green */
            else if (x < 32 && y >= 32) *p = 0xFF0000FF; /* blue  */
            else                        *p = 0xFFFFFFFF; /* white */
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    win = SDL_CreateWindow("ARGB8888 texture test", 50, 50, 400, 300, 0);
    if (!win) { SDL_Log("CreateWindow: %s", SDL_GetError()); goto done; }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { SDL_Log("CreateRenderer: %s", SDL_GetError()); goto done; }

    SDL_Log("Renderer: %s", SDL_GetCurrentVideoDriver());

    /* Step 1 — create ARGB8888 texture */
    SDL_Log("Creating ARGB8888 texture...");
    texture = SDL_CreateTexture(ren,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STATIC,
                                64, 64);
    if (!texture) {
        SDL_Log("CreateTexture FAILED: %s", SDL_GetError());
        goto done;
    }
    SDL_Log("CreateTexture OK");

    /* Step 2 — upload pixels */
    SDL_Log("Calling SDL_UpdateTexture...");
    if (SDL_UpdateTexture(texture, NULL, pixels, 64 * sizeof(Uint32)) < 0) {
        SDL_Log("UpdateTexture FAILED: %s", SDL_GetError());
        goto done;
    }
    SDL_Log("UpdateTexture OK");

    /* Step 3 — render */
    SDL_Log("Rendering...");
    SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
    SDL_RenderClear(ren);

    {
        /* Draw the texture twice at different sizes to test scaling path too */
        SDL_Rect dst1 = { 10,  10, 64, 64  }; /* 1:1 */
        SDL_Rect dst2 = { 90,  10, 128, 128 }; /* 2x  */
        SDL_RenderCopy(ren, texture, NULL, &dst1);
        SDL_Log("RenderCopy 1:1 OK");
        SDL_RenderCopy(ren, texture, NULL, &dst2);
        SDL_Log("RenderCopy 2x OK");
    }

    SDL_RenderPresent(ren);
    SDL_Log("RenderPresent OK - if you see 4 coloured squares the renderer is working");

    while (running) {
        if (!SDL_WaitEvent(&e)) continue;
        switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_q) running = 0;
                break;
            case SDL_WINDOWEVENT:
                SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
                SDL_RenderClear(ren);
                { SDL_Rect d1 = {10,10,64,64}; SDL_Rect d2 = {90,10,128,128};
                  SDL_RenderCopy(ren, texture, NULL, &d1);
                  SDL_RenderCopy(ren, texture, NULL, &d2); }
                SDL_RenderPresent(ren);
                break;
            default: break;
        }
    }

done:
    if (texture) SDL_DestroyTexture(texture);
    if (ren)     SDL_DestroyRenderer(ren);
    if (win)     SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
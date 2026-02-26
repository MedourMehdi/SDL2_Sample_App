#include <SDL2/SDL.h>
#include <stdbool.h>

int main(int argc, char* argv[]) {
    // 1. Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return 1;
    }
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    // 2. Create Window (160x100)
    SDL_Window* win = SDL_CreateWindow(
        "Fence Test", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED, 
        160, 100, 
        0
    );

    // 3. Create Renderer
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    // 4. Draw White Background
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255); // White
    SDL_RenderClear(ren);

    // 5. Draw Black Fence
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); // Black

    // Draw Horizontal Rails
    SDL_Rect topRail = { 0, 30, 160, 4 };
    SDL_Rect bottomRail = { 0, 60, 160, 4 };
    SDL_RenderFillRect(ren, &topRail);
    SDL_RenderFillRect(ren, &bottomRail);

    // Draw Vertical Posts
    for (int x = 15; x < 160; x += 30) {
        SDL_Rect post = { x, 10, 10, 80 };
        SDL_RenderFillRect(ren, &post);
    }

    // 6. Present the drawing to the screen
    SDL_RenderPresent(ren);

    // 7. Event Loop (Wait for user to close window)
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        SDL_Delay(10); // Small delay to prevent high CPU usage
    }

    // 8. Cleanup
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
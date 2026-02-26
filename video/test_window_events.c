#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#define LOG_EVENT(fmt, ...) printf("[EVENT] " fmt "\n", ##__VA_ARGS__)
#define LOG_STATE(fmt, ...) printf("[STATE] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_TEST(fmt, ...) printf("[TEST] " fmt "\n", ##__VA_ARGS__)

typedef struct {
    int x, y;
    int w, h;
    Uint32 flags;
    bool is_maximized;
    bool is_minimized;
    bool is_fullscreen;
} WindowState;

WindowState g_current_state = {0};
WindowState g_previous_state = {0};

void update_window_state(SDL_Window* window) {
    g_previous_state = g_current_state;
    
    SDL_GetWindowPosition(window, &g_current_state.x, &g_current_state.y);
    SDL_GetWindowSize(window, &g_current_state.w, &g_current_state.h);
    g_current_state.flags = SDL_GetWindowFlags(window);
    g_current_state.is_maximized = (g_current_state.flags & SDL_WINDOW_MAXIMIZED) != 0;
    g_current_state.is_minimized = (g_current_state.flags & SDL_WINDOW_MINIMIZED) != 0;
    g_current_state.is_fullscreen = (g_current_state.flags & SDL_WINDOW_FULLSCREEN) != 0;
}

void print_window_state(const char* context) {
    LOG_STATE("%s: pos=(%d, %d) size=(%d x %d) flags=0x%08X %s%s%s",
              context,
              g_current_state.x, g_current_state.y,
              g_current_state.w, g_current_state.h,
              g_current_state.flags,
              g_current_state.is_maximized ? "[MAX]" : "",
              g_current_state.is_minimized ? "[MIN]" : "",
              g_current_state.is_fullscreen ? "[FULL]" : "");
}

void print_state_change() {
    if (g_current_state.x != g_previous_state.x || 
        g_current_state.y != g_previous_state.y) {
        LOG_STATE("Position changed: (%d, %d) -> (%d, %d)",
                  g_previous_state.x, g_previous_state.y,
                  g_current_state.x, g_current_state.y);
    }
    if (g_current_state.w != g_previous_state.w || 
        g_current_state.h != g_previous_state.h) {
        LOG_STATE("Size changed: %dx%d -> %dx%d",
                  g_previous_state.w, g_previous_state.h,
                  g_current_state.w, g_current_state.h);
    }
}

void handle_window_event(SDL_WindowEvent* event) {
    switch (event->event) {
        case SDL_WINDOWEVENT_SHOWN:
            LOG_EVENT("Window shown");
            break;
        case SDL_WINDOWEVENT_HIDDEN:
            LOG_EVENT("Window hidden");
            break;
        case SDL_WINDOWEVENT_EXPOSED:
            LOG_EVENT("Window exposed (needs redraw)");
            break;
        case SDL_WINDOWEVENT_MOVED:
            LOG_EVENT("Window moved to (%d, %d)", event->data1, event->data2);
            break;
        case SDL_WINDOWEVENT_RESIZED:
            LOG_EVENT("Window resized to %dx%d", event->data1, event->data2);
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            LOG_EVENT("Window size changed to %dx%d", event->data1, event->data2);
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            LOG_EVENT("Window minimized");
            break;
        case SDL_WINDOWEVENT_MAXIMIZED:
            LOG_EVENT("Window maximized");
            break;
        case SDL_WINDOWEVENT_RESTORED:
            LOG_EVENT("Window restored");
            break;
        case SDL_WINDOWEVENT_ENTER:
            LOG_EVENT("Mouse entered window");
            break;
        case SDL_WINDOWEVENT_LEAVE:
            LOG_EVENT("Mouse left window");
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            LOG_EVENT("Window gained focus");
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            LOG_EVENT("Window lost focus");
            break;
        case SDL_WINDOWEVENT_CLOSE:
            LOG_EVENT("Close requested");
            break;
        case SDL_WINDOWEVENT_TAKE_FOCUS:
            LOG_EVENT("Take focus event");
            break;
        case SDL_WINDOWEVENT_HIT_TEST:
            LOG_EVENT("Hit test event");
            break;
        default:
            LOG_EVENT("Unknown window event: %d", event->event);
            break;
    }
}

void test_user_initiated_actions(SDL_Window* window) {
    LOG_TEST("\n=== Testing User-Initiated Actions (SDL API calls) ===");
    
    // Test 1: Maximize via SDL API
    LOG_TEST("1. Calling SDL_MaximizeWindow()...");
    SDL_MaximizeWindow(window);
    SDL_Delay(500);
    
    // Test 2: Restore via SDL API
    LOG_TEST("2. Calling SDL_RestoreWindow()...");
    SDL_RestoreWindow(window);
    SDL_Delay(500);
    
    // Test 3: Move via SDL API
    LOG_TEST("3. Calling SDL_SetWindowPosition(100, 100)...");
    SDL_SetWindowPosition(window, 100, 100);
    SDL_Delay(500);
    
    // Test 4: Resize via SDL API
    LOG_TEST("4. Calling SDL_SetWindowSize(800, 600)...");
    SDL_SetWindowSize(window, 800, 600);
    SDL_Delay(500);
    
    // Test 5: Minimize via SDL API
    LOG_TEST("5. Calling SDL_MinimizeWindow()...");
    SDL_MinimizeWindow(window);
    SDL_Delay(1000);
    
    // Test 6: Restore from minimized
    LOG_TEST("6. Calling SDL_RestoreWindow() from minimized...");
    SDL_RestoreWindow(window);
    SDL_Delay(500);
    
    // Test 7: Fullscreen toggle
    LOG_TEST("7. Toggling fullscreen...");
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Delay(1000);
    SDL_SetWindowFullscreen(window, 0);
    SDL_Delay(500);
    
    LOG_TEST("=== User-Initiated Actions Test Complete ===\n");
}

void test_window_state_consistency(SDL_Window* window) {
    LOG_TEST("\n=== Testing Window State Consistency ===");
    
    int reported_x, reported_y;
    int reported_w, reported_h;
    Uint32 reported_flags;
    
    // Get state via SDL API
    SDL_GetWindowPosition(window, &reported_x, &reported_y);
    SDL_GetWindowSize(window, &reported_w, &reported_h);
    reported_flags = SDL_GetWindowFlags(window);
    
    LOG_TEST("SDL Reports: pos=(%d, %d) size=%dx%d flags=0x%08X",
             reported_x, reported_y, reported_w, reported_h, reported_flags);
    LOG_TEST("Internal State: pos=(%d, %d) size=%dx%d flags=0x%08X",
             g_current_state.x, g_current_state.y,
             g_current_state.w, g_current_state.h,
             g_current_state.flags);
    
    // Check consistency
    bool consistent = true;
    if (reported_x != g_current_state.x) {
        LOG_ERROR("X position mismatch: SDL=%d, State=%d", reported_x, g_current_state.x);
        consistent = false;
    }
    if (reported_y != g_current_state.y) {
        LOG_ERROR("Y position mismatch: SDL=%d, State=%d", reported_y, g_current_state.y);
        consistent = false;
    }
    if (reported_w != g_current_state.w) {
        LOG_ERROR("Width mismatch: SDL=%d, State=%d", reported_w, g_current_state.w);
        consistent = false;
    }
    if (reported_h != g_current_state.h) {
        LOG_ERROR("Height mismatch: SDL=%d, State=%d", reported_h, g_current_state.h);
        consistent = false;
    }
    if (reported_flags != g_current_state.flags) {
        LOG_ERROR("Flags mismatch: SDL=0x%08X, State=0x%08X", reported_flags, g_current_state.flags);
        consistent = false;
    }
    
    if (consistent) {
        LOG_TEST("✓ Window state is consistent");
    } else {
        LOG_TEST("✗ Window state is INCONSISTENT");
    }
    
    LOG_TEST("=== Consistency Test Complete ===\n");
}

void print_instructions() {
    printf("\n========================================\n");
    printf("SDL2 Window Behavior Test\n");
    printf("========================================\n");
    printf("This program tests window event handling.\n\n");
    printf("HOW TO TEST:\n");
    printf("1. Keep this window visible\n");
    printf("2. Perform these actions MANUALLY:\n");
    printf("   - Move the window (WM_MOVED)\n");
    printf("   - Resize the window (WM_SIZED)\n");
    printf("   - Maximize via window titlebar (WM_FULLED)\n");
    printf("   - Minimize via window titlebar\n");
    printf("   - Restore from minimized\n");
    printf("   - Trigger expose/redraw (cover/uncover window)\n");
    printf("3. Watch the event log\n");
    printf("4. The program will also test SDL API calls automatically\n");
    printf("\nPress SPACE to run automated SDL API tests\n");
    printf("Press ESC to quit\n");
    printf("========================================\n\n");
}

int main(int argc, char* argv[]) {
    printf("Starting SDL2 Window Behavior Test...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    printf("SDL_Init succeeded.\n");
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
    printf("SDL Logging set to DEBUG level.\n");
    print_instructions();
    printf("Creating test window...\n");
    // Create window with all the flags we want to test
    SDL_Window* window = SDL_CreateWindow(
        "SDL2 Window Test - Move/Resize/Maximize this window",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE 
    );
    printf("Test window created.\n");
    if (!window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully. Initializing renderer...\n");
    // SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    printf("Renderer initialized successfully.\n");
    printf("Entering main event loop. Perform actions on the window as described.\n");
    // Initial state
    update_window_state(window);
    printf("Initial window state:\n");
    print_window_state("Initial");
    printf("You can now interact with the window.\n");
    bool quit = false;
    SDL_Event event;
    bool run_automated_tests = false;
    
    while (!quit) {
        // Render a simple pattern to visualize redraws
        SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
        SDL_RenderClear(renderer);
        
        // Draw a gradient to see redraw areas
        for (int y = 0; y < g_current_state.h; y += 20) {
            SDL_SetRenderDrawColor(renderer, 
                                  (y * 255 / g_current_state.h) % 255,
                                  100,
                                  200,
                                  255);
            SDL_RenderDrawLine(renderer, 0, y, g_current_state.w, y);
        }
        
        SDL_RenderPresent(renderer);
        
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    LOG_EVENT("SDL_QUIT received");
                    quit = true;
                    break;
                    
                case SDL_WINDOWEVENT:
                    handle_window_event(&event.window);
                    update_window_state(window);
                    print_state_change();
                    break;
                    
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                        case SDLK_SPACE:
                            if (!run_automated_tests) {
                                run_automated_tests = true;
                                test_user_initiated_actions(window);
                                test_window_state_consistency(window);
                            }
                            break;
                        case SDLK_m:
                            LOG_TEST("Manual test: Pressed M - calling SDL_MaximizeWindow");
                            SDL_MaximizeWindow(window);
                            break;
                        case SDLK_r:
                            LOG_TEST("Manual test: Pressed R - calling SDL_RestoreWindow");
                            SDL_RestoreWindow(window);
                            break;
                    }
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    LOG_EVENT("Mouse button %d at (%d, %d)", 
                             event.button.button, event.button.x, event.button.y);
                    break;
            }
        }
        
        SDL_Delay(16); // ~60 FPS
    }
    
    LOG_TEST("\n=== Final Window State ===");
    print_window_state("Final");
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

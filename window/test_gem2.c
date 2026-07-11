/*
 * gem_window_test.c
 * SDL2 GEM driver regression / fix validation test suite.
 * Atari ST/TT/Falcon - GCC / Pure C - C90 compliant.
 *
 * Build:
 *   gcc -o gem_test.tos gem_window_test.c -lSDL2
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

/* ============================================================
   Helpers
   ============================================================ */

#define PASS(name)       printf("[PASS] %s\n", name)
#define FAIL(name, why)  printf("[FAIL] %s : %s\n", name, why)

/*
 * fill_window
 * Fills the window surface with a solid color and presents it.
 * Call this whenever the window needs visible content — after
 * create, after resize, after restore, etc.
 * Returns 0 on success, -1 if no surface could be obtained.
 */
static int fill_window(SDL_Window *win, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_Surface *surf = SDL_GetWindowSurface(win);
    if (!surf) return -1;
    SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, r, g, b));
    SDL_UpdateWindowSurface(win);
    return 0;
}

/* Pump events and wait ms milliseconds.
 * Returns SDL_WINDOWEVENT subtype if one arrives, or -1. */
static int wait_window_event(SDL_Window *win, Uint32 ms)
{
    SDL_Event e;
    Uint32 deadline = SDL_GetTicks() + ms;
    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_WINDOWEVENT &&
                SDL_GetWindowFromID(e.window.windowID) == win)
                return e.window.event;
        }
        SDL_Delay(10);
    }
    return -1;
}

/* Pump events for ms ms, discard all. */
static void flush_events(Uint32 ms)
{
    SDL_Event e;
    Uint32 deadline = SDL_GetTicks() + ms;
    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&e)) { (void)e; }
        SDL_Delay(10);
    }
}

/* ============================================================
   TEST 1 — WF_FULLXYWH work-rect misuse (GEM_CreateWindow)
   Fill: white — maximized window should cover work area visibly.
   ============================================================ */
static void TEST1_fullxywh_work_rect(void)
{
    const char *name = "T1:WF_FULLXYWH-work-rect";
    SDL_Window  *win;
    SDL_Rect     desktop;
    int          ww, wh;

    if (SDL_GetDisplayBounds(0, &desktop) != 0) {
        FAIL(name, "SDL_GetDisplayBounds failed");
        return;
    }

    win = SDL_CreateWindow("T1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320, 200, SDL_WINDOW_MAXIMIZED);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255); /* white */
    flush_events(400);

    SDL_GetWindowSize(win, &ww, &wh);

    if (ww > 0 && wh > 0 &&
        !(ww == desktop.w && wh == desktop.h))
    {
        PASS(name);
    } else {
        FAIL(name, "window size equals raw desktop bounds (decoration not subtracted)");
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 2 — aligned_w set in GEM_SetWindowSize (wrong place)
   Fill: white at initial size, black after resize — lets you
   see the framebuffer was reallocated for the correct final size.
   ============================================================ */
static void TEST2_aligned_w_placement(void)
{
    const char *name = "T2:aligned_w-placement";
    SDL_Window   *win;
    SDL_Surface  *surf;
    int           initial_w = 640, initial_h = 400;
    int           final_w   = 320, final_h   = 200;
    int           pw, ph;

    win = SDL_CreateWindow("T2",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           initial_w, initial_h, 0);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255); /* white at initial size */
    flush_events(300);

    SDL_SetWindowSize(win, final_w, final_h);
    flush_events(150);

    fill_window(win, 0, 0, 0); /* black at final size */
    flush_events(300);

    surf = SDL_GetWindowSurface(win);
    if (!surf) { FAIL(name, SDL_GetError()); SDL_DestroyWindow(win); return; }

    SDL_GetWindowSize(win, &pw, &ph);

    {
        int bpp        = surf->format->BytesPerPixel;
        int row_pixels = (bpp > 0) ? (surf->pitch / bpp) : 0;
        int stride16   = (pw + 15) & ~15;

        if (row_pixels == stride16) {
            PASS(name);
        } else {
            FAIL(name, "pitch does not match MFDB_STRIDE(final_w)");
        }
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 3 — Double AES call in GEM_RestoreWindow
   Fill: white = normal, black = minimized (not visible),
         white again after restore so you can confirm it came back.
   ============================================================ */
static void TEST3_restore_window(void)
{
    const char *name = "T3:restore-window";
    SDL_Window *win;
    int ox, oy, ow, oh;
    int rx, ry, rw, rh;
    int ev;
    int restored_count = 0;
    int got_moved      = 0;
    SDL_Event e;
    Uint32 deadline;

    win = SDL_CreateWindow("T3",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320, 200, SDL_WINDOW_RESIZABLE);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255);
    flush_events(300);

    SDL_GetWindowPosition(win, &ox, &oy);
    SDL_GetWindowSize(win, &ow, &oh);

    SDL_MinimizeWindow(win);
    ev = wait_window_event(win, 1000);
    if (ev != SDL_WINDOWEVENT_MINIMIZED) {
        FAIL(name, "no MINIMIZED event");
        SDL_DestroyWindow(win);
        return;
    }
    flush_events(200);

    SDL_RestoreWindow(win);

    deadline = SDL_GetTicks() + 800;
    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_WINDOWEVENT &&
                SDL_GetWindowFromID(e.window.windowID) == win) {
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                    restored_count++;
                if (e.window.event == SDL_WINDOWEVENT_MOVED)
                    got_moved = 1;
            }
        }
        SDL_Delay(10);
    }

    /* Refill after restore so the window has visible content again */
    fill_window(win, 255, 255, 255);
    flush_events(300);

    SDL_GetWindowPosition(win, &rx, &ry);
    SDL_GetWindowSize(win, &rw, &rh);

    if (restored_count != 1) {
        FAIL(name, "expected exactly 1 RESTORED event");
    } else if (got_moved) {
        FAIL(name, "spurious MOVED event after restore");
    } else if (rw != ow || rh != oh) {
        FAIL(name, "window size changed after restore");
    } else if (rx != ox || ry != oy) {
        FAIL(name, "window position changed after restore");
    } else {
        PASS(name);
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 4 — Two SDL calls in GEM_MaximizeWindow
   Fill: white after maximize so content is visible at full size.
   ============================================================ */
static void TEST4_maximize_window(void)
{
    const char *name = "T4:maximize-single-event";
    SDL_Window *win;
    SDL_Event   e;
    Uint32      deadline;
    int         maximized_count = 0;
    int         spurious_resize = 0;
    int         ww, wh;
    SDL_Rect    work;

    win = SDL_CreateWindow("T4",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320, 200, SDL_WINDOW_RESIZABLE);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255);
    flush_events(300);

    SDL_MaximizeWindow(win);

    deadline = SDL_GetTicks() + 800;
    while (SDL_GetTicks() < deadline) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_WINDOWEVENT &&
                SDL_GetWindowFromID(e.window.windowID) == win) {
                if (e.window.event == SDL_WINDOWEVENT_MAXIMIZED)
                    maximized_count++;
                if (e.window.event == SDL_WINDOWEVENT_RESIZED &&
                    maximized_count == 0)
                    spurious_resize++;
            }
        }
        SDL_Delay(10);
    }

    fill_window(win, 255, 255, 255); /* refill at maximized size */
    flush_events(300);

    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GetDisplayUsableBounds(0, &work);

    if (maximized_count != 1) {
        FAIL(name, "expected exactly 1 MAXIMIZED event");
    } else if (spurious_resize > 0) {
        FAIL(name, "RESIZED event fired before MAXIMIZED (two-step path)");
    } else if (ww <= 0 || wh <= 0) {
        FAIL(name, "invalid window size after maximize");
    } else {
        PASS(name);
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 5 — Stale handle on GEM_ReopenWindow failure
   Fill: white -> borderless (black) -> bordered (white again).
   Lets you see each reopen visually and confirm the window
   survives both transitions with valid content.
   ============================================================ */
static void TEST5_reopen_window_handle(void)
{
    const char *name = "T5:reopen-window-handle";
    SDL_Window *win;
    int w1, h1, w2, h2;

    win = SDL_CreateWindow("T5",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320, 200, 0);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255);
    flush_events(300);

    SDL_GetWindowSize(win, &w1, &h1);

    SDL_SetWindowBordered(win, SDL_FALSE);
    flush_events(200);
    fill_window(win, 0, 0, 0); /* black: borderless */
    flush_events(300);

    SDL_SetWindowBordered(win, SDL_TRUE);
    flush_events(200);
    fill_window(win, 255, 255, 255); /* white: bordered again */
    flush_events(300);

    SDL_GetWindowSize(win, &w2, &h2);

    if (w2 == w1 && h2 == h1) {
        PASS(name);
    } else {
        FAIL(name, "window size inconsistent after double reopen");
    }

    SDL_DestroyWindow(win);
    flush_events(100);
    printf("      (no crash on DestroyWindow — handle was valid)\n");
}

/* ============================================================
   TEST 6 — restore_rect clobbered on fullscreen exit + maximize
   Fill: white=normal, grey=maximized, black=fullscreen,
         white again after restore to confirm correct size.
   ============================================================ */
static void TEST6_restore_rect_after_fs(void)
{
    const char *name = "T6:restore-rect-after-fullscreen";
    SDL_Window *win;
    int bx = 50, by = 50, bw = 320, bh = 200;
    int fw, fh;
    int rw, rh;
    int ev;

    win = SDL_CreateWindow("T6", bx, by, bw, bh, SDL_WINDOW_RESIZABLE);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255); /* white: normal */
    flush_events(300);

    SDL_MaximizeWindow(win);
    ev = wait_window_event(win, 1000);
    if (ev != SDL_WINDOWEVENT_MAXIMIZED) {
        FAIL(name, "no MAXIMIZED event");
        SDL_DestroyWindow(win); return;
    }
    fill_window(win, 180, 180, 180); /* grey: maximized */
    flush_events(300);

    SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    flush_events(300);
    fill_window(win, 0, 0, 0); /* black: fullscreen */
    flush_events(400);
    SDL_GetWindowSize(win, &fw, &fh);

    SDL_SetWindowFullscreen(win, 0);
    flush_events(300);
    fill_window(win, 180, 180, 180); /* grey: back to maximized */
    flush_events(300);

    SDL_RestoreWindow(win);
    ev = wait_window_event(win, 1000);
    flush_events(200);
    fill_window(win, 255, 255, 255); /* white: should be baseline size */
    flush_events(400);

    SDL_GetWindowSize(win, &rw, &rh);

    if (rw == bw && rh == bh) {
        PASS(name);
    } else {
        FAIL(name, "restore returned wrong size (restore_rect was clobbered)");
        printf("      expected %dx%d got %dx%d\n", bw, bh, rw, rh);
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 7 — WF_ICONIFY called with -1,-1,-1,-1
   Fill: white before minimize, white again after restore.
   ============================================================ */
static void TEST7_minimize_iconify(void)
{
    const char *name = "T7:minimize-iconify";
    SDL_Window *win;
    int ev;
    int ww, wh;

    win = SDL_CreateWindow("T7",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           320, 200, 0);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255);
    flush_events(300);

    SDL_MinimizeWindow(win);
    ev = wait_window_event(win, 1000);

    if (ev != SDL_WINDOWEVENT_MINIMIZED) {
        FAIL(name, "no MINIMIZED event received");
        SDL_DestroyWindow(win);
        return;
    }
    flush_events(200);

    SDL_RestoreWindow(win);
    ev = wait_window_event(win, 1000);
    flush_events(200);

    fill_window(win, 255, 255, 255); /* refill after restore */
    flush_events(300);

    SDL_GetWindowSize(win, &ww, &wh);

    if (ev == SDL_WINDOWEVENT_RESTORED && ww == 320 && wh == 200) {
        PASS(name);
    } else {
        FAIL(name, "window state invalid after minimize/restore cycle");
        printf("      ev=%d size=%dx%d\n", ev, ww, wh);
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   TEST 8 — win_* drift in GEM_SetWindowPosition
   Fill: white at A, black at B so you can see the move happened.
   ============================================================ */
static void TEST8_position_drift(void)
{
    const char *name = "T8:position-drift";
    SDL_Window *win;
    int ax = 60,  ay = 60;
    int bx = 120, by = 80;
    int qx, qy;

    win = SDL_CreateWindow("T8", ax, ay, 320, 200, 0);
    if (!win) { FAIL(name, SDL_GetError()); return; }

    fill_window(win, 255, 255, 255); /* white at A */
    flush_events(300);

    SDL_SetWindowBordered(win, SDL_FALSE);
    flush_events(150);
    fill_window(win, 200, 200, 200);
    flush_events(150);

    SDL_SetWindowBordered(win, SDL_TRUE);
    flush_events(150);
    fill_window(win, 255, 255, 255);
    flush_events(150);

    SDL_SetWindowPosition(win, ax, ay); /* early-exit path */
    flush_events(100);

    SDL_SetWindowPosition(win, bx, by);
    flush_events(200);
    fill_window(win, 0, 0, 0); /* black at B */
    flush_events(300);

    SDL_GetWindowPosition(win, &qx, &qy);

    if (qx == bx && qy == by) {
        PASS(name);
    } else {
        FAIL(name, "position mismatch after reopen + early-exit + move");
        printf("      expected (%d,%d) got (%d,%d)\n", bx, by, qx, qy);
    }

    SDL_DestroyWindow(win);
    flush_events(100);
}

/* ============================================================
   Main
   ============================================================ */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("=== GEM SDL2 Window Driver Test Suite ===\n\n");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("--- Fix 1: WF_FULLXYWH work-rect ---\n");
    TEST1_fullxywh_work_rect();
    printf("\n");

    printf("--- Fix 2: aligned_w placement ---\n");
    TEST2_aligned_w_placement();
    printf("\n");

    printf("--- Fix 3: RestoreWindow double-call ---\n");
    TEST3_restore_window();
    printf("\n");

    printf("--- Fix 4: MaximizeWindow single event ---\n");
    TEST4_maximize_window();
    printf("\n");

    printf("--- Fix 5: ReopenWindow stale handle ---\n");
    TEST5_reopen_window_handle();
    printf("\n");

    printf("--- Fix 6: restore_rect after fullscreen ---\n");
    TEST6_restore_rect_after_fs();
    printf("\n");

    printf("--- Fix 7: MinimizeWindow iconify position ---\n");
    TEST7_minimize_iconify();
    printf("\n");

    printf("--- Fix 8: SetWindowPosition drift ---\n");
    TEST8_position_drift();
    printf("\n");

    printf("=== Done ===\n");

    SDL_Quit();
    return 0;
}
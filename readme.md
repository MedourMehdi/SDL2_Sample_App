# SDL2 Atari ST Validation Test Suite

This repository contains a comprehensive test suite for validating the SDL2 implementation on the Atari ST family (including STe, TT, Falcon). The tests cover video rendering, audio playback, input handling (keyboard, joystick), performance, and platform‑specific features like the GEM window manager.

The suite is designed to be used with the SDL2 Atari port available in the [`atari-2.32.x` branch](https://github.com/MedourMehdi/SDL/tree/atari-2.32.x) of the main SDL repository. Running these tests helps ensure that the port works correctly on real hardware or emulators (Hatari, Aranym).

## Directory Structure

```
.
├── audio/          – Audio playback tests (WAV, AIFF, SDL_mixer, visualizers)
├── bench/          – CPU frequency benchmarks
├── framebuffer/    – Direct framebuffer and partial redraw tests
├── joypad/         – Graphical joystick test (Atari ST low resolution)
├── keyboard/       – Graphical keyboard tests (ST layout, resizable, SMS bindings)
├── misc/           – Simple smoke tests (e.g. fence drawing)
├── performance/    – Dirty‑rectangle performance tests
├── pixelformat/    – Tests for BGRA8888, ARGB8888 texture formats
├── truecolor/      – High colour mode tests (16/24/32‑bit)
├── ttf/            – SDL_ttf text rendering test
└── video/          – Basic video output tests (plasma, window events, surfaces)
```

## Building the Tests

### Prerequisites

- A cross‑compiler targeting **m68k-atari-mint** (e.g., from the [MiNT toolchain](https://freemint.github.io/)).
- **Pthread support:** SDL2 and these tests require the pthread API.  
  Use the following custom forks that provide native libc and kernel pthread support:
  - [mintlib-rc (pthreads branch)](https://github.com/MedourMehdi/mintlib-rc/tree/pthreads)
  - [freemint-rc (Threads-1 branch)](https://github.com/MedourMehdi/freemint-rc/tree/Threads-1)
- SDL2, SDL2_mixer, and SDL2_ttf libraries built for Atari (using the `atari-2.32.x` branch).
- `make` (GNU Make).

The build process automatically detects which tests require SDL_mixer or SDL_ttf by scanning the source files.

### Compilation

Run `make` from the root directory. All binaries will be placed in the `builds/` folder with the `test_` prefix stripped and a `.prg` extension. For example, `audio/test_audio_player_ui.c` becomes `builds/audio_player_ui.prg`.

```bash
make
```

To clean the build directory:
```bash
make clean
```

### Compiler Flags

The Makefile uses the following settings:

- Compiler: `m68k-atari-mint-gcc`
- Architecture: `-m68020-60` (optimised for 68020–68060, adjust if you need pure 68000 compatibility)
- Optimisation: `-O2`
- Libraries: `-lSDL2 -lgem -lm`, plus `-lSDL2_mixer` or `-lSDL2_ttf` when required.

## Running the Tests

Copy the desired `.prg` file(s) to your Atari (or emulator with a suitable hard disk image) and execute them. Most tests are self‑explanatory and will display instructions on screen.

- **Keyboard tests** – Press keys to see the layout light up.
- **Joystick test** – Move the joystick and press fire; the on‑screen D‑pad and button respond.
- **Audio tests** – Play WAV/AIFF files; some include a graphical user interface.
- **Performance tests** – Run for a few seconds and report FPS to the console.
- **Window tests** – Try resizing, moving, and covering the window to verify redraw behaviour.

## Reporting Issues

If you encounter any failures (black screens, missing colours, no audio, etc.), please open an issue in this repository or in the main SDL repository’s `atari-2.32.x` branch. Include details about your hardware/emulator and the exact test that failed.

## License

The test programs are provided under the same license as SDL2 (zlib). See `LICENSE.txt` for details.
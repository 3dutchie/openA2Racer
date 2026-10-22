# OpenA2

An open-source native reimplementation of **A2 Racer (1997, Dutch release)** for
Windows, macOS and Linux. It reads the original game's data from an extracted CD
directory or disc image. No original executable, Windows compatibility layer,
legacy graphics driver or asset conversion is required at runtime.

**Version 0.4.0 is a playable reconstruction.** Original routes, car/scenery meshes,
textures, artwork, soundtracks and movies are loaded directly. Driving dynamics,
AI and several numerical game rules are reconstructed and still need comparison
against the running original. See [reconstruction status](docs/RECONSTRUCTION.md)
for the exact distinction. Original saves are not compatible with this engine.

## Build and play

Requirements: a C++20 compiler and CMake 3.24+. The build uses an installed SDL2
or downloads a pinned SDL2 source release into the build directory.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Linux/macOS:

```sh
./build/opena2 --data "/path/to/A2-Racer_Win_NL/CD"
```

Windows (Visual Studio build):

```powershell
.\build\Release\opena2.exe --data "C:\Games\A2-Racer_Win_NL\CD"
```

The supplied raw MODE2/2352 `CD.iso` is also accepted, along with plain ISO9660
images. You can drop a CD directory or image onto the game window. Data is
automatically found in nearby `A2-Racer_Win_NL/CD`, `CD` or `data` folders, or via
`OPENA2_DATA`. Original files are opened read-only.

For this workspace, running `./build/opena2` from its root finds the supplied data.
Use `--race` to go straight to racing or `--skip-intro` to open the menu.

Full platform, offline build, test and packaging instructions:
[docs/BUILD.md](docs/BUILD.md).

## Implemented

- All six stages, including the return journey, using the three original roads.
- Six selectable original cars, traffic, rival racers, police, collisions and fines.
- Hittable red/white roadwork posts and original red rank meshes above racers.
- Original scenery placement, terrain tile atlas, textured models and guardrails.
- Fuel, countdown, race timer, money/time/fuel bonuses, results and progression.
- Cockpit, chase and helicopter cameras, radar and original cockpit artwork.
- Original graphical time/money/position HUD, animated dashboard gauges and steering wheel.
- Live enhanced graphics switch: 1280×960 rendering, edge antialiasing, 3× draw
  distance, directional lighting, soft projected shadows and filtered 2× textures.
- Keyboard and SDL game-controller input; resizable window and fullscreen.
- Original PCM sound effects, spoken pickup announcements, engine pitch and all six music tracks.
- Intro, completion and exit Smacker movies with decoded sound; skippable playback.
- Original menu buttons, route cards, player dialog and real loading-screen artwork.
- Ten named player profiles with campaign progress, money, travel time and earned
  overall/per-stage score tables.
- Four remappable driving keys; separate music/effects volumes, sound mode,
  horizon and dashboard options, with OK/Cancel editing.
- Ten portable save slots per player, plus quick save/load and access to older
  port-wide saves. Saves include airborne roadwork posts.
- Deterministic simulation, headless rendering, data validation and BMP screenshots.

The menu uses original CD artwork and new portable interaction/storage code.
Dynamic text uses a bundled open font; original Windows GDI glyphs and old save
formats are not reproduced. Exact handling/AI, weather, some original road features and complete
visual equivalence remain fidelity work. This release targets the supplied Dutch
release; other editions have not been verified. See [menus and profiles](docs/MENUS.md).

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Accelerate / brake | Up / Down or W / S | RT / LT; A also accelerates |
| Steer | Left / Right or A / D | Left stick |
| Handbrake | Space | X |
| Cockpit / chase / helicopter | F1 / F2 / F3 | Right shoulder cycles |
| Enhanced / classic graphics | F4 or G | Left shoulder |
| Radar | F5 | — |
| Pause / resume | P; Enter resumes | Start |
| Dismiss fine | Enter | — |
| Quick save / load (slot 1) | F6 / F9 | — |
| Restart stage | R | — |
| Menu | Escape | B |
| Toggle sound / fullscreen | M / F11 | — |
| Select menu item | Arrows + Enter, or mouse | D-pad + A |
| Skip movie | Enter, Space, Escape, or click | A / B |

BEDIENING remaps the four driving keys. WASD aliases apply while the default
arrow bindings are selected. In the main menu, C opens car selection, T opens
unlocked stages and S/L opens save slots. F1 opens menu help; Left/Right switches
score tabs. The original 3D Accelerator option selects this port's enhanced mode;
320×200 selects low-resolution presentation and 640×480 selects classic mode.

The game pauses when its window loses focus. Saves and settings live in SDL's
per-user `OpenA2/OpenA2` preference directory, separately from the CD data. Slot
files use a versioned format with route fingerprints; invalid loads leave the
current game unchanged. Menu saves and F6 write the selected slot via a temporary
file before replacement. Profile progress and scores are stored separately in
`profiles.db`. Version 0.1–0.3 race saves remain readable through the old-slot
load action; original Windows SLOT.A2R/SCORE.A2R files are not imported.

## Validation and reproducible runs

```sh
ctest --test-dir build -C Release --output-on-failure
./build/opena2 --data "/path/to/CD.iso" --validate
./build/opena2 --data "/path/to/CD" --headless --demo --frames 600 --screenshot race.bmp
```

`--enhanced` starts with enhanced graphics; `--classic` forces the original look.
F4 or G changes modes immediately, and the choice is remembered in settings.
See [graphics modes](docs/GRAPHICS.md) for details and performance tradeoffs.

`--stage 1..6` and `--car 1..6` select a race. `--movie A2INTRO.SMK` plays a movie
directly. `--help` lists options. Synthetic tests do not need original data;
`-DA2_TEST_DATA=/path/to/CD` additionally exercises the supplied resource inventory
and movies, plus original HUD/menu decoding and roadwork/rank rendering.
Headless runs use fixed simulated time and do not need a display. `--prefs DIR`
isolates profiles/settings for testing; `--menu players` (or main, controls,
options, scores, loading) previews a menu. Headless demos do not award profile scores.

The macOS arm64 build was compiled and exercised locally, including native
window/audio startup, real-data rendering and resource decoding. Windows/Linux
CI jobs are supplied; their presence is not a claim that those remote jobs have
already passed. Native platform testing remains part of release validation.

## Source and data licensing

New engine source is provided under the [MIT license](LICENSE). Original A2 Racer
art, audio, movies, manuals and executables are **not** covered by that license
and are not included in source or native distribution archives. Supply your own
game data.

SDL2 uses the zlib license. Menu text uses Liberation Serif (SIL OFL 1.1) and
stb_truetype (MIT). The included libsmacker decoder uses LGPL-2.1; its
source, license and provenance are preserved under `third_party/libsmacker`.
Native packages include the corresponding OpenA2 source archive so that the
decoder can be modified and relinked. See [third-party notices](docs/THIRD_PARTY.md).

```sh
python3 tools/package_source.py
```

This produces `dist/OpenA2-0.4.0-source.zip` using a source allowlist. It excludes
the original media, local reverse engineering evidence, decoded previews and
build output. The private investigation in `analysis/` is retained locally for
continued research; it is not part of the open-source distribution.

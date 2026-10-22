# Build and run OpenA2

OpenA2 is a C++20 application using SDL2 for desktop input, window presentation,
and audio. CMake 3.24 or newer and a C++20 compiler are required. Original A2 Racer
data is supplied separately by the player; no CD files are downloaded by the build.

## Native build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

On Linux/macOS, run `build/opena2`. On Visual Studio's multi-configuration build,
run `build/Release/opena2.exe`. See the main README for game-data arguments and controls.

The build first looks for an installed SDL2 CMake package. If necessary, point
`CMAKE_PREFIX_PATH` at its installation prefix or set `SDL2_DIR` to the directory
containing `sdl2-config.cmake`. If SDL2 is absent, the default build downloads
the official SDL2 2.32.10 source archive and verifies its SHA-256 before building
it statically inside the build directory. It does not install software system-wide.

For an offline build with an installed SDL2:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DA2_FETCH_SDL2=OFF
```

For a release package using the pinned static SDL2 regardless of local packages:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DA2_USE_SYSTEM_SDL2=OFF
cmake --build build-release --config Release --parallel
cpack --config build-release/CPackConfig.cmake -C Release
```

Packaging also requires Python 3 to generate the included corresponding source
archive for the statically linked LGPL movie decoder. Ordinary compilation does
not require Python. Packages appear under `build-release/packages/`. On macOS, add
`-DA2_MACOS_BUNDLE=ON` to place the program in an app bundle; the executable is then
`opena2.app/Contents/MacOS/opena2`. The packages are unsigned development builds.
New macOS build directories default to a macOS 11.0 deployment target unless
the caller supplies another target. A prebuilt system SDL2 may require a newer
OS; use the pinned source build for release packages intended for the default
baseline. Running on macOS 11 still needs validation on that OS.

For Linux, install normal compiler/CMake and SDL development prerequisites for
the desktop backends you want (X11/Wayland and ALSA/PulseAudio). The CI workflow
lists its tested runner setup. Windows source builds use the Visual Studio 2022
C++ toolchain. macOS builds use Xcode Command Line Tools. CI is configured to
compile and run synthetic tests on all three platforms; workflow configuration
alone does not establish that a remote build has passed.

## Tests

Synthetic tests require no original game assets:

```sh
ctest --test-dir build -C Release --output-on-failure
```

To enable complete asset integration checks, configure with a directory or disc image:

```sh
cmake -S . -B build -DA2_TEST_DATA="/path/to/A2-Racer_Win_NL/CD"
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

To build only the core library and tests without SDL2:

```sh
cmake -S . -B build-core -DA2_BUILD_GAME=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```

Clang/GCC builds can enable `-DA2_SANITIZERS=ON` to check address and undefined
behaviour errors during tests.

## Source distribution

```sh
python3 tools/package_source.py
```

This creates `dist/OpenA2-0.4.0-source.zip` from an explicit allowlist: new source,
tests, Markdown documentation, build tools, CI configuration and the vendored
LGPL libsmacker source/license. Original media,
asset previews, binary/decompiler evidence, and build output are excluded. The ZIP
is byte-for-byte deterministic for the same selected source bytes.

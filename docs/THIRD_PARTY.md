# Third-party components and original game content

The newly written OpenA2 source is under the MIT license in `LICENSE`.

## libsmacker

Original SMK movie playback uses libsmacker 1.2.0 by Greg Kennedy, licensed under
LGPL-2.1-or-later. Its complete compiled source and license are retained under
`third_party/libsmacker/`; `UPSTREAM.md` records the immutable source identity.
It is statically linked. Native packages therefore include the full OpenA2 and
libsmacker source/build archive under `docs/source/` for modification and
relinking, along with the LGPL text under `docs/libsmacker/`. Modify the decoder
and rebuild with the CMake instructions to relink your own executable.

Original A2 Racer programs, textures, models, sound, movies, manuals, and disc
images remain separate game content. The OpenA2 license does not grant rights to
that content. The player provides the original CD directory or supported image.
Source and native release packages deliberately exclude original game files and
decompiler evidence.

## SDL2

OpenA2 uses Simple DirectMedia Layer 2. A configured system package can be used,
or CMake can build the official SDL2 2.32.10 source release:

- Source: https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
- SHA-256: `5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165`
- Project: https://www.libsdl.org/

The following is the SDL2 2.32.10 license notice:

```text
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## Portable menu font

Dynamic menu text uses the unmodified Liberation Serif Regular font from
Liberation Fonts 2.1.5, under the SIL Open Font License 1.1. The exact font bytes
are embedded in the executable; no platform font installation is required.
`third_party/liberation/UPSTREAM.md` records its official release and SHA-256.
The source ZIP includes the TTF, generated header and license. Native packages
include the full license under `docs/liberation/`. This font is a portable visual
substitute, not a recovered original Windows GDI font.

The pinned stb_truetype header rasterizes this bundled font under the MIT option.
Its revision, SHA-256 and license are recorded in `third_party/stb/`; native
packages retain its notices under `docs/stb/`. No user-supplied fonts are parsed.

# Reconstruction status and provenance

The portable engine reads the supplied original Dutch CD data. Original
executables are used as static reverse engineering evidence, not executed by the
new runtime. Source structures and routines in `src/` are new implementation.
This document distinguishes recovered content/structure from rules which still
need original-game runtime comparison.

## Recovered with executable evidence

Target: `A2SPEL.DAT`, SHA-256
`069d39e815fc631ef433550459e8105042cf740dfc311e4f9285d4cca148e726`.
Provider: REA/Hopper. Evidence exported to
`analysis/rea/gameplay-evidence.json`, with a resumable observation snapshot in
`analysis/rea/gameplay-snapshot.json`.

### Road data

The road loader at `0x41e158` reads a u32 row count, followed by rows containing
two floats, three u32 words, and two arrays whose element count is the third
word. In this release the array count is 11, so rows are 108 bytes. The two
floats become vertex x and z. The **second** integer is converted with `FILD`
and becomes vertex y (elevation). Sequential disassembly confirms that the
first word is read into stack+0x100, the second into stack+0xf8, and `0x41e335`
and `0x41e4a2` convert stack+0xf8 into vertex y. The first word's meaning remains
unknown. The third word is the terrain-array count. This corrects an earlier
misreading of the loader's nonsequential analysis output.

Evidence: `ev_97cab71b0376fe619ba435e465973e7e03f26bc56dcd77d2e72a3a0b4f9b4b40`.
The sequential byte-checked read is retained in
`analysis/rails/road-loader-full.asm`; its derived PE header layout and unchanged
code/RVA provenance are recorded in `analysis/rails/derived-layout.json`.

Road geometry generator `0x41f358` receives a constant 2 as its fourth argument
from this loader. It computes `(2 + 2) * 30 = 120` as the lateral edge distance
in each direction, a 5-unit rail/curb height, and 60-unit terrain strips. The
portable renderer can therefore preserve a 240-unit carriageway envelope
instead of estimating road width from model size.

Evidence: `ev_d8f69f2820579ecbd629c7878e32108c22398bb52f4f23fbd566928051ec8f22`;
static doubles at VAs `0x44cfb9=2`, `0x44cfc1=30`, `0x44cfc9=5`; float at
`0x44cfd5=60`.

Opponent placement routine `0x40684c` reads the lane coordinate table at
`0x4578d4`. Lane indices 0 through 10 map to lateral coordinates
`[108,85,60,30,0,0,0,-30,-60,-85,-108]`. Racer placement at `0x411928` chooses
lane 2/3 or 8/7 according to heading. The portable simulation uses 30 and 60 as
the two travel lane centres relative to travel direction, and 85/120 as
shoulder/outer boundaries.

Evidence: `ev_02478f8e83171f194e7369018e14847c21b0182fe51bcb61a801974c4b250290`,
`ev_e892de8edd924d5dcca31a813c8a73a3a06903227e217524bc11443336ec8045`.

### Road artwork and terrain atlas

The standard road initialization (`0x401f4d` and `0x401f5c`) explicitly loads
`2baans.pcx` into the road texture handle at `0x4576c8`, and `grass2.pcx` into
the terrain handle at `0x4576cc`. `asfalt.pcx` is not the texture selected by
that road initializer.

The main execute-buffer builder `0x420431` emits two pairs of vertices for
each road section: centre plus the +120-unit edge, then centre plus the
-120-unit edge. Each half receives the complete road image: u=0 at the
centre, u=1 at the outer edge, v advancing 0 to 1 between consecutive source
rows. Mirroring that image across the centre gives the four travel lanes.
The image's grass shoulder starts near u=0.7, matching the 85/108-unit
shoulder waypoints. The execute-buffer builder unconditionally emits four
guardrail strips at lateral -120, -5, +120 and +5, each five units high. Each
uses the full width of the first 1/8 row of `grass2.pcx`, whose artwork supplies
the metal beam, bolts, posts and grass beneath. The two median strips remain
separate. The portable renderer draws both sides without duplicating reversed
faces. Original evidence and precise UV reads are retained in
`analysis/rails/FINDINGS.md` and `analysis/rails/execute-builder-full.asm`.

The simulation now stops the player at lateral +/-10 when approaching the
median, allowing five units of half-car clearance beyond each recovered rail
face at +/-5. Its reflected heading and speed reduction reuse this engine's
approximate outer-barrier response; the original collision response and exact
car contact dimensions have not been translated. The existing +/-118 outer
collision limit is retained. Original rail vertices follow a tilted road up
normal on slopes; the current horizontal road frames approximate that small
tilt with a vertical five-unit height.

The two arrays following the row header are **terrain tile codes**, not
carriageway materials. Tile initializer `0x425f55` fills an 8×8 UV table at
`0x4767d8`, with 32-byte records and UV increments of 0.125. The road builder
indexes it with `code << 5`. A tile code therefore chooses column `code % 8`
and row `code / 8` of the 256×256 `grass2.pcx` atlas (32×32 source pixels).

For the supplied 11-element arrays, the builder loops `count-1`, producing
ten quads on each side. The first array (`RoadRecord.fields[3..13]`) controls
negative-right terrain at lateral boundaries -120,-180,...,-720; the second
(`fields[14..24]`) controls positive-right terrain +120,+180,...,+720. Each
quad is 60 world units wide. The last element in each array does not produce
an additional quad for that segment. Terrain vertices at the road edge retain
road height; the outward ground vertices are at y=0.

Tile UV bounds are u0=column/8, u1=(column+1)/8, v0=row/8, v1=(row+1)/8. The
table's four UV slots are `(u0,v1),(u0,v0),(u1,v0),(u1,v1)`. The execute-buffer
builder assigns these to four per-quad vertices and uses two triangles; its
complete vertex-order listing is retained in the geometry evidence. Surface
triangle orientation should follow that listing rather than estimating a
texture's appearance.

Evidence: road texture selection
`ev_4ca85388080050364f4683254ce2172ee49249ed6aea80921cbc282a02547c98`;
UV initializer
`ev_af95324c41813970dc2859e90b1e2a9c622cd32c544a950dafd75609ee6ac6e8`;
execute-buffer builder
`ev_45209fa15765c878d7da587a9e3d7186b2bdb421a96385bb01fae3ec6cdaa2b7`,
`ev_2fb44e32f8e5c54fc327d0afeabb1715b6d394653adab92820dd1a9a4b3753a3`.

### OMP scenery placement

Original loader `0x411131` reads each 20-byte OMP record as:

| Offset | Representation | Original diagnostic/usage |
| --- | --- | --- |
| 0 | u32 | Road chunk index |
| 4 | u32 | Angle/orientation selector |
| 8 | u32 | Position selector, 0 through 4 |
| 12 | u32 | Static model ID, 0 through 47 |
| 16 | float | Offset, rounded to integer before the placement call |

The ordering is verified by registers, stack locations and the loader's
`ID/chunk/positie/angle/offset` diagnostic arguments. It calls `0x407ac4` with
model ID in EAX, chunk in EDX, position in EBX, angle in ECX and offset on the
stack. Position 0 uses the road centre; 1 and 2 subtract/add offset times the
road perpendicular; 3 and 4 subtract/add `(model bounding radius + 2*offset)`
times the perpendicular. Model radius is computed from mesh bounds. Position
0/1/2 derives elevation from the road plane; 3/4 use zero terrain elevation.
The angle selector controls orientation relative to the road/side. Exact
legacy integer-angle convention still needs complete verification.

Evidence: `ev_b545b32140aecd74a793022143b98528fc6bdfd31a20a8cbf7b041aaca17f892`,
`ev_1371afba2ce3df7fe3766e4d315de6a718063b4becc92f6f711d2ccb21694a8d`,
`ev_02478f8e83171f194e7369018e14847c21b0182fe51bcb61a801974c4b250290`.

`src/legacy_geometry.hpp` recovers the scenery yaw branch structure. Let x,z
be the road's unit perpendicular and let the negative-side group be position
1 or 3. The stored legacy angle A is:

| Selector | Negative-side group | Other positions (0/2/4) |
| --- | --- | --- |
| 0 | x==0 ? 0 : trunc(atan2(z,x) × 1024 / 6.283185307) | x==0 ? 512 : trunc(atan2(-z,-x) × 1024 / 6.283185307) |
| 1 | z==0 ? 786 : trunc(atan2(x,-z) × 1024 / 6.283185307) | z==0 ? 256 : trunc(atan2(-x,z) × 1024 / 6.283185307) |

The final matrix angle is `(1023 - (A & 1023) + 256) & 1023`. The literal
786 (`0x312`) is present in the original assembly and is preserved despite
its unusual zero-component result. The rounding helper `0x425e58` temporarily
sets the x87 rounding mode to truncation. The original trigonometric table
initializer uses `i / 1024 * 2 * 3.141596`. The helper returns this angle in
radians for the new renderer's matching Y-rotation convention.

This preserves the recovered branch/scalar semantics; it does not claim
bit-identical native x87 transcendental results on all hosts. The original
atan routine can select a separate software path based on an FPU flag, and
the new engine uses the standard library. Near an angle quantization boundary,
precision can therefore differ by one legacy angular unit.

Evidence: placement branches
`ev_1371afba2ce3df7fe3766e4d315de6a718063b4becc92f6f711d2ccb21694a8d`;
final conversion
`ev_842346600495ebe94c68904268bfd6604e633e028b8c9a916bae91a5353b78a2`;
Y matrix
`ev_84ae6bf66cf424b65b089110fc29a68d58d90fc12f35c24c6c91ca99908c0638`;
truncation
`ev_03a88477ba805a0a523ce8c395a1f1acbb8ef490ed6c7c02857c9b3e3f92e373`;
trigonometric table
`ev_8d356887ae7d6efa87bd8b66b79a742a1054316d5acb2b547218b0995be0dd27`.

The static model table starts at VA `0x457c58` with 16-byte entries:
mesh-name pointer, texture-name pointer and two initially -1 cache slots.
Mappings used by the renderer are grounded in this table:

| IDs | Mesh / texture |
| --- | --- |
| 0–9 | sky/sky, bovw/50, arena/arena, viaduct/viaduct, molent/molen, shell/shell, roadsgn/roadsgn, uit/uit, zaltboml/zaltboml, lekbrug/lekbrug |
| 10–19 | gebouwg/gebouw, gebouw2g/tile, gebouw3g/tile2, trees/trees, wegresg2/wegresg2, elek4/elek4, huisje/huisje, rijtjes/rijtjes, werkweg/werkweg, werkweg2/werkweg2 |
| 20–29 | flits/flits, praatp/praatp, start/start, finish/finish, geld/geld, benzine/benzine, tijd/tijd, lantaarn/lantaarn, eindh/eindh, amsterd/amsterd |
| 30–37 | denbosch/denbosch, maastr/maastr, utrecht/utrecht, s_maastr/s_maastr, s_utr/s_utr, s_denb/s_denb, s_adam/s_adam, s_eindho/s_eindho |
| 38–47 | number1 through number6, hek/hek, koe/koe, 100/100, babe/babe |

Names above omit `.d3d` and `.pcx`. Some names may be absent from the archive;
the runtime should skip an unavailable optional model instead of replacing the
whole asset load with failure.

None of the supplied OMP records uses bonus IDs 24–26. The executable has
separate bonus object allocation and generation routines. Generating bonuses
independently of the scenery list is therefore consistent with its structure;
current bonus positions, rewards and collection bounds remain reconstructed.

### Cockpit instruments and graphical HUD

The supplied `GAMESPR` archive contains the exact red cockpit instruments,
steering animation, yellow numeric glyphs, `Tijd`, `Geld`, `Positie /6`, start
lights and pause artwork. The original animation table at `0x458a54` maps
these sprites into 35 groups. Native drawing code `0x409383..0x409c83`
establishes their original 640x480 placement, animation ordering and whether
each call uses a signed sprite hotspot or a direct top-left position. The
portable HUD reads those original assets at runtime using the `SURSCRN.pcx`
palette. Sprite transparency is index 255; enclosed black instrument faces
and the VDsys screen remain opaque. The counter arithmetic divides by 60
and 3600, so the elapsed display preserves HH:MM:SS.

The original speed gauge has 40 frames, RPM and fuel 20 each, and the wheel 15.
Their pixels, order and positions are recovered; driving their frames from
the new simulation requires adapters. Speed uses the printed0..250km/h
face, fuel maps the port's 0..100 level over 20 frames, and RPM is estimated
from speed and gear with 1000 RPM idle. Steering frames follow the port's
heading. These calibrations, engine dynamics, start-light timing and radar
dot projection remain reconstructions. The source-specific extraction,
animation-table file offsets and disassembly are retained in
`analysis/hud/FINDINGS.md` and `analysis/hud/animations.json`.

`tests/hud_test.cpp` uses procedural sprites to verify black opacity,
transparency, counters, moving instruments, steering, pause and the minimal
fixture fallback. Its optional original-CD integration verifies changes in
all three recovered gauge regions. These checks establish composition and
responsiveness, not equivalence to the original driving physics.

### Other recovered facts

- Six stages refer to route files in order `1,2,3,3,2,1`; return-stage travel is
  represented by reversing progress along the same source route.
- The six selectable car models/textures are Porsche (`porche`), Camaro,
  BMW, Jaguar, Lotus and Viper, as named by the race tables.
- The original has gear/sound handling (`mycontro.cpp`), lane-choice AI
  (`driver.cpp`), police, fuel, money, time bonuses, and fines. The manual and
  release notes establish their presence and user-facing controls.

## Reconstructed behaviour which needs fidelity work

The portable simulation is deliberately playable, but it is not a claim of
bit-exact behavioural equivalence. These rules are implemented as explicit,
deterministic reconstruction values rather than decompiler translations:

- acceleration, drag, brake/handbrake forces, top speed and steering response;
- conversion between world units, metres and displayed kilometres per hour;
- countdown, initial time, fuel consumption and per-stage fuel refill;
- traffic density, speed, overtaking, police pursuit/fine calculation;
- collision envelopes and response;
- bonus locations and amounts, race awards and campaign money lifecycle;
- opponent time/position competition and return-route traffic placement.

The simulation operates in bounded substeps of at most 1/120 second, with
deterministic seeded placement. Those are new-engine design choices. Original
timer precision, update scheduling and floating-point behaviour have not been
established. Input/output replay against the running original is still needed
to replace reconstructed constants with verified behaviour.

No recovered original source, local-variable definitions or complete C++ type
tables are asserted. Hopper pseudocode loses Watcom register arguments and
has incomplete function-body coverage; assembly/register evidence was used
for the concrete loader findings above.

## Simulation verification

`tests/simulation_test.cpp` uses synthetic routes and verifies sampling,
orthogonal frames, acceleration, braking, steering, one-shot collection,
vehicle collision response, time/fuel failure, reversed-stage forward travel,
stage completion and six-stage progression. These tests verify new-engine
consistency; they do not establish equivalence to original gameplay.

## Portable saves

`Race::save` / `Race::load` implement a new versioned save format headed
`A2RACER_SAVE 1`. It preserves player state, complete entity lists, collected
bonuses, simulation phase, countdown, fine/collision/police timers, seeded RNG,
failure reason and queued events. Float text uses enough digits for exact float
round trips. Fingerprints cover all road and OMP records in each route, so saves
from incompatible route data are rejected.

This format does not claim compatibility with original `INFIO.INI` or player
slots. Loads are capped at 2 MiB with bounded entity counts, finite/range
validation and complete-document checks. A failed load leaves the running race
unchanged. Tests verify complete-state roundtrip, deterministic continuation,
wrong versions/fingerprints, truncation, trailing content, NaN, excessive counts
and oversize rejection. The application should write a temporary save file and
rename it after successful serialization so a failed write preserves a previous
slot.

### Pickup speech

Money, fuel and time bonuses trigger the original `sp_b3.wav`, `sp_b2.wav`
and `sp_b1.wav` speech, respectively, alongside their short `bonusg.wav`,
`bonusb.wav` and `bonust.wav` effects. Previous releases played only the effects.
The mapping is recovered from the original fixed-width sound-name table at
`0x458db4`, playback selector `0x4104bc`, and bonus handlers `0x411fc7`,
`0x412078` and `0x412101`. The recordings remain on the user's CD and are
decoded at runtime. Bonus amounts and locations still use reconstructed rules.

### Optional enhanced renderer

OpenA2 0.3.0 adds an independent enhanced presentation mode, selected with
F4/G. Higher resolution, extended visibility, reconstructed texture filtering,
interpolated lighting and soft planar shadows are new port features, not
recovered behavior of the 1997 renderer. The original-mode path and simulation
remain available. See [GRAPHICS.md](GRAPHICS.md) for algorithms and limitations.

## Menu and race-detail restoration in 0.4

Original A2MRC sprite and A2MRCS PCX resources supply the menu buttons, dialogs,
controls, options, score tabs, route cards and loading artwork. Menu executable
analysis established the ten-slot player list, initial money 1000, bounded names,
separate player/score databases and progress/time/name score ordering. The new
portable profile database and exactly-once result lifecycle are reconstruction
choices; original database import and settings-per-profile semantics are not
implemented. Dynamic text uses an open serif font rather than recovered GDI
glyphs. See MENUS.md for interactions and storage behavior.

Retained evidence is indexed in `analysis/menu/FINDINGS.md` and
`analysis/profiles/FINDINGS.md`. REA menu bootstrap evidence
`ev_6e5e9aaa5d52c0ba33b6a7e59576c93fdaf7277ea13a1cb921c4aaf395ac830f`
identifies the original slot/score initializers. Focused offline LLVM inspection
reuses original executable bytes with only zero PE section VirtualSize fields
filled from RawSize; derived-layout files record the exact header changes and
hashes. The original executables remain unchanged and were not run in this pass.

Roadwork initializer `0x41158d`, creation `0x413dca` and update/collision
`0x413a5c` establish four/five zones of ten `werkweg2` delineators. Outbound and
return source chunks run in opposite directions, on the 85/108 shoulder lanes.
Impact launches and spins a post, plays `bump.wav`, and hides it after landing.
The port recovers this layout and behavior but reconstructs seconds-based
launch/gravity/damping and uses a separate deterministic RNG. Original trajectory
and CRT random-sequence equivalence remain unverified. Original per-zone speed
cameras/enforcement are outside this change; existing placed cameras still render.

Rank initializer `0x411296` creates original `number1` through `number6` meshes.
Update callbacks follow the ranked car, its heading, and a ten-unit vertical
position offset. The port renders these meshes with ordinary world occlusion and
updates ranks from race progress; stable tie ordering is reconstructed. Evidence:
`ev_bab215367ff04a2740cecd0aa0df820dc29efc6bfcf989827b6d2c62a0b876cd`,
`ev_3e8215bd07dac56d03eac714f06ee5ff475bce8b778fc15111e510ef2bbc0446`;
full instruction ranges and limits are retained in
`analysis/race_details/FINDINGS.md`. Both renderer modes and all cameras share
these objects. Schema 2 snapshots retain airborne prop motion; schema 1 migration
restores pristine zones while preserving prior state and RNG.

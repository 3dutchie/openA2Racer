# Verification of OpenA2 0.4.0

Local checks performed on 21 October 2026, macOS arm64:

- C++20 release build with SDL2 2.32.10 compiled from pinned source.
- Native executable links only Apple system frameworks/libraries. Its deployment
  target is macOS 11.0; running on an actual macOS 11 installation was not tested.
- All fourteen CTest checks pass: assets, simulation, renderer, movies, legacy
  geometry, guardrails, HUD, enhanced graphics, profiles, original menus, roadwork/rank details, independent generated data, and headless
  whole-application gameplay in both graphics modes.
- Complete supplied data: 121 PCX images, 63 meshes, 98 PCM WAV files, 661 sprites,
  three road/placement pairs. Directory and raw disc-image access both checked.
- All 944 frames of the three original SMK movies and their decoded audio pass
  inspection, including rewind and end-of-stream checks.
- All eleven core tests also pass with AddressSanitizer and
  UndefinedBehaviorSanitizer, including guardrails and HUD composition. Renderer
  regressions cover
  extreme finite coordinates/UVs and float-to-integer bounds.
- Pickup regressions verify the original money/fuel/time speech clip mapping,
  alongside each bonus effect, and ensure collected items cannot repeat either.
- Procedural HUD fixtures verify original sprite placement, palette-index 255
  transparency, opaque black instruments, changing gauges and steering, elapsed
  time/money counters, pause artwork, and fallback for incomplete test assets.
- Supplied-CD HUD integration checks verify windshield transparency and changing
  original speedometer, tachometer and fuel-gauge artwork.
- Graphics fixtures verify texture reconstruction, mip/atlas boundaries, transparent
  cutouts, diagonal-edge antialiasing, projected shadow occlusion, HD dimensions,
  and repeated live switching without changing the classic image or race/save state.
- One-sided mesh fixtures verify front/back winding selection and rotation in
  both modes, preventing mirrored rear numeral faces from filling glyph openings.
- Guardrail fixtures verify the recovered atlas row, texture range, visible
  five-unit height and rendering from both sides.
- A deterministic driving agent completed all six supplied routes through normal
  simulation updates, without changing player distance or bypassing race rules.
- Native UI exercised: menu race start, acceleration, pause, quick save, quick
  load, and cockpit selection. Dummy SDL video/audio startup and cleanup also
  passed.
- Native F4/G switching and the Options graphics selector were exercised in the
  running macOS window; each changed mode without recreating the race.
- The parallel graphics test also passes ThreadSanitizer with the supplied CD.
- Profile fixtures cover ten players, UTF-8 names, isolated save paths, sequential
  campaign results, duplicate/stale result tokens, replay scores, restart/delete
  retention, bounded top-ten tables, malformed input and atomic database writes.
- Menu fixtures check recovered button animation/states, per-page palette colors,
  opaque black versus index-255 transparency, portable font rendering, real loading
  pixels and fallback behavior with incomplete synthetic data.
- Roadwork fixtures check six stage layouts/directions, swept player/AI impacts,
  launch/landing, one-shot sound, deterministic airborne save continuation,
  version-1 migration and atomic invalid-load rejection. Supplied-CD render checks
  distinguish posts and changing numeral meshes in three cameras and both modes.
- Native 0.4 frontend exercised with isolated preferences: name entry and profile
  selection, persistence after closing/reopening, four sequential remapped keys,
  custom-key acceleration, Options edits/Cancel, stage score tab switching,
  mouse dialog cancellation, quick save and F4 switching. Menu screenshots were
  rendered and visually checked against the supplied references. Nonempty score
  layout previews use explicitly isolated test records.
- All 674 supplied original files retain their recorded SHA-256 identities.
- A clean extraction of the source ZIP builds the core without SDL or original
  data and passes all eleven core tests. The native ZIP contains an identical
  corresponding source ZIP; both archives exclude original media and analysis.

Synthetic fixtures contain independently generated checkerboards, cube meshes,
tones and fictional routes. They never read or copy original content and can run
in public CI. They verify engine behaviour, not fidelity to the original game.

Windows and Linux native CI jobs are configured, but were not run remotely in
this local task. Controller hardware, actual macOS 11, and full original-game
input/output comparisons remain unverified. See RECONSTRUCTION.md for recovered
versus reconstructed game rules.

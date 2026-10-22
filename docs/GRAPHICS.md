# Graphics modes

Press **F4** or **G** during a race to change between original and enhanced
graphics. The controller's left shoulder button does the same. The Options menu
has a `GRAFISCH` choice; the selection is saved between launches. No race restart,
asset conversion, downloads or additional original files are needed.

Command-line selection overrides the saved preference:

```sh
opena2 --data /path/to/CD.iso --enhanced --race
opena2 --data /path/to/CD.iso --classic --race
```

| Feature | Original | Enhanced |
| --- | --- | --- |
| Internal image | 640×480 | 1280×960 |
| Geometry edges | Original pixel raster | Contrast-directed subpixel edge antialiasing |
| Visibility limit | 2,600 engine units | 7,800 engine units |
| Textures | Indexed nearest sampling | 2× Catmull-Rom reconstruction, trilinear mipmaps, up to 4-tap anisotropic filtering |
| Mesh lighting | Original port's flat face lighting | Interpolated vertex-normal directional lighting |
| Shadows | None | Soft projected vehicle and selected scenery silhouettes |

Both modes preserve the original field of view, 4:3 framing and graphical HUD.
Window resizing letterboxes the image. The HUD is drawn after world antialiasing
at its original logical coordinates. Gameplay, sound, item pickups and portable
saves are independent of the graphics mode. Returning to original mode restores
its image exactly for the same camera and race state.

## Texture reconstruction

Original PCX files remain read-only. Texture reconstruction runs in memory when
a texture is first used and is cached for later frames. Textures up to 512×512
are enlarged twofold in each dimension; larger images keep their resolution.
This is deterministic bicubic reconstruction, not AI-generated detail. It
improves sampling and edge contrast but cannot recover missing source detail.

The terrain atlas is filtered cell by cell. Its mip chain stops before adjacent
cells would merge, and anisotropic footprints are clamped to their own cell.
Palette-index cutouts are premultiplied during filtering to avoid colored or
black outlines around trees and other masked meshes. Filtering and mipmaps
reduce the shimmer of distant markings and grass.

The filtered cache targets 96 MiB. Textures referenced by the current frame
remain pinned until rendering completes, so a particularly texture-heavy frame
may temporarily exceed that target. Switching modes keeps the cache warm;
loading a different CD clears it. The first enhanced frame can take longer
while the textures are reconstructed.

## Lighting, shadows and performance

Enhanced lighting uses a fixed sun direction and the original mesh normals.
Vehicle and selected solid scenery meshes cast convex silhouettes onto a local
ground plane with a feathered edge. These are approximate planar shadows; they
do not implement a general shadow map, self-shadowing, ray tracing or changing
time of day. Open/concave scenery can produce a broader silhouette than its
individual parts would cast. Distant shadows are omitted to bound their cost.

The renderer remains portable C++ and needs no new GPU API or graphics driver.
Enhanced world rendering and edge filtering share work across up to eight CPU
threads. Original mode retains its previous rendering path. Enhanced mode costs
more CPU time and memory, so its frame rate depends on the scene and computer;
F4/G is also the immediate fallback on slower systems. A local macOS arm64
measurement of one fixed race state, with warm texture caches and 20 render
iterations per camera, measured about 48–69 render frames/s in enhanced mode.
This excludes window presentation and is a scene-specific CPU measurement, not
a minimum frame rate. It is an optional visual
mode, rather than a claim of equivalence to the original renderer.

Reproducible captures use fixed simulation time:

```sh
opena2 --data /path/to/CD.iso --headless --demo --classic --camera 2 --frames 450 --screenshot classic.bmp
opena2 --data /path/to/CD.iso --headless --demo --enhanced --camera 2 --frames 450 --screenshot enhanced.bmp
```

The graphics regression test uses procedural assets and can optionally compare
the same race with the player's CD. Address/undefined-behavior and thread
sanitizers can check the filtered and parallel paths without a graphics device.

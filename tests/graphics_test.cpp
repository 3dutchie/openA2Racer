#include "renderer.hpp"
#include "scene.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void textures() {
  Image atlas;
  atlas.width = atlas.height = 8;
  atlas.tilesX = atlas.tilesY = 2;
  atlas.pixels.resize(64);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x)
      atlas.pixels[size_t(y) * 8 + x] = x < 4 ? 0xffff0000 : 0xff00ff00;
  FilteredTexture texture(atlas, false);
  check(texture.width() == 16 && texture.height() == 16, "Texture is reconstructed at 2x size");
  for (float lod : {0.f, .5f, 2.f, 100.f}) {
    check(texture.sample(.499f, .25f, lod) == 0xffff0000, "Atlas boundary bleeds at a mip level");
    check(texture.sample(.501f, .25f, lod) == 0xff00ff00, "Atlas adjacent tile loses its color");
    check(texture.sample(.8f, .25f, lod, {.49f, .25f}) == 0xffff0000,
          "Anisotropic footprint crosses an atlas cell");
  }
  check(texture.sample(std::numeric_limits<float>::infinity(), 0, 0) == 0,
        "Non-finite texture coordinates accepted");
  Image cutout;
  cutout.width = cutout.height = 4;
  cutout.pixels.assign(16, 0xffff0000);
  cutout.indices.assign(16, 0);
  for (int y = 0; y < 4; ++y) {
    cutout.pixels[size_t(y) * 4 + 2] = 0xff00ff00;
    cutout.pixels[size_t(y) * 4 + 3] = 0xff00ff00;
    cutout.indices[size_t(y) * 4 + 2] = cutout.indices[size_t(y) * 4 + 3] = 1;
  }
  FilteredTexture mask(cutout, true);
  uint32_t edge = mask.sample(.49f, .5f, .5f);
  check(((edge >> 16) & 255) == 0 && ((edge >> 8) & 255) > 0,
        "Transparent palette colors contaminate cutout edges");
  check(mixColor(0xff123456, 0xffabcdef, 0) == 0xff123456 &&
            mixColor(0xff123456, 0xffabcdef, 1) == 0xffabcdef,
        "Packed filtering changes interpolation endpoints");
}
void switchingAndAA() {
  Renderer renderer;
  Camera camera;
  camera.focal = 100;
  auto draw = [&] {
    renderer.clear(0xff000000);
    renderer.triangle(camera, {{-3, -2, 3}, {}}, {{3, -2, 3}, {}}, {{-.7f, 2.9f, 3}, {}}, nullptr);
  };
  draw();
  auto classic = renderer.pixels();
  for (int i = 0; i < 4; ++i) {
    renderer.setEnhanced(true);
    draw();
    check(renderer.pixelWidth() == 1280 && renderer.pixelHeight() == 960 &&
              renderer.pixels().size() == 1280 * 960,
          "Enhanced framebuffer size");
    renderer.flushWorld();
    auto aliased = renderer.pixels();
    renderer.finishWorld();
    size_t changed = 0;
    for (size_t n = 0; n < aliased.size(); ++n)
      changed += aliased[n] != renderer.pixels()[n];
    check(changed > 100, "Edge antialiasing leaves diagonal silhouette unchanged");
    renderer.rect(10, 10, 1, 1, 0xff12ab34);
    check(renderer.pixels()[size_t(20) * 1280 + 20] == 0xff12ab34 &&
              renderer.pixels()[size_t(21) * 1280 + 21] == 0xff12ab34,
          "Original HUD coordinates are not scaled independently of world AA");
    renderer.setEnhanced(false);
    draw();
    check(renderer.pixels() == classic, "Returning to classic mode changes its image");
  }
}
void shadows() {
  Renderer renderer;
  renderer.setEnhanced(true);
  Camera camera;
  camera.lookAt({0, 30, -40}, {0, 0, 20});
  renderer.clear(0xffa0a0a0);
  renderer.quad(camera, {-50, 0, -20}, {50, 0, -20}, {50, 0, 100}, {-50, 0, 100}, nullptr, 1, 1,
                0xffffffff);
  Mesh box;
  for (float x : {-4.f, 4.f})
    for (float y : {0.f, 10.f})
      for (float z : {-7.f, 7.f})
        box.vertices.push_back({{x, y, z}, {}, {}, 0});
  renderer.flushWorld();
  auto before = renderer.pixels();
  renderer.shadow(camera, box, {0, 0, 20}, 0, 1, {0, 0, 20});
  renderer.flushWorld();
  size_t darkened = 0;
  for (size_t p = 0; p < before.size(); ++p)
    if (before[p] != renderer.pixels()[p]) {
      check((renderer.pixels()[p] & 255) <= (before[p] & 255), "Shadow brightens the ground");
      ++darkened;
    }
  check(darkened > 100, "Projected mesh shadow is missing");
  // Overlay depth must remain the ground's, so nearer geometry still occludes it.
  renderer.quad(camera, {-50, 2, -20}, {50, 2, -20}, {50, 2, 100}, {-50, 2, 100}, nullptr, 1, 1,
                0xffff0000);
  renderer.flushWorld();
  before = renderer.pixels();
  renderer.shadow(camera, box, {0, 0, 20}, 0, 1, {0, 0, 20});
  renderer.flushWorld();
  check(renderer.pixels() == before, "Ground shadow paints over elevated geometry");
}
void original(const char *path) {
  GameData data(path);
  Assets assets(data);
  Renderer renderer;
  std::array<Track, 3> tracks{loadTrack(data, 1), loadTrack(data, 2), loadTrack(data, 3)};
  Race race(tracks);
  Scene scene(renderer, assets);
  std::ostringstream state;
  race.save(state);
  scene.draw(race, 1);
  auto classic = renderer.pixels();
  auto triangles = renderer.triangles();
  renderer.setEnhanced(true);
  scene.draw(race, 1);
  check(renderer.triangles() > triangles, "Extended scene geometry and shadows are absent");
  renderer.setEnhanced(false);
  scene.draw(race, 1);
  check(renderer.pixels() == classic, "Real-data classic image changes after toggle");
  std::ostringstream after;
  race.save(after);
  check(after.str() == state.str(), "Graphics toggle alters the simulation/save state");
}
} // namespace
int main(int argc, char **argv) {
  try {
    textures();
    switchingAndAA();
    shadows();
    if (argc > 1)
      original(argv[1]);
    std::cout << "Enhanced graphics: texture/atlas filtering, alpha, HD, AA, shadows and live "
                 "switching passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Enhanced graphics failed: " << e.what() << '\n';
    return 1;
  }
}

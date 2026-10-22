#include "guardrails.hpp"
#include <cstdlib>
#include <iostream>

namespace {
void require(bool ok, const char *message) {
  if (!ok) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main() {
  // A diagnostic atlas makes an incorrect terrain-row selection unmistakable.
  // The top row is split longitudinally to catch missing/repeated UV ranges.
  a2::Image atlas;
  atlas.width = atlas.height = 64;
  atlas.pixels.resize(64 * 64, 0xffff00ff);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 64; ++x)
      atlas.pixels[size_t(y) * 64 + x] = x < 32 ? 0xff00ff00 : 0xff0000ff;
  a2::RoadPose a{{0, 3, 0}, {0, 0, 1}, {1, 0, 0}}, b{{0, 3, 100}, {0, 0, 1}, {1, 0, 0}};
  for (bool reverse : {false, true}) {
    a2::Renderer renderer;
    a2::Camera camera;
    // View from either side at close range, with fog disabled by distance.
    camera.lookAt({reverse ? 150.0f : -150.0f, 10, 50}, {0, 5, 50});
    renderer.clear(0xff000000);
    a2::drawGuardrails(renderer, atlas, camera, a, b);
    bool green = false, blue = false;
    int top = 480, bottom = 0;
    for (size_t i = 0; i < renderer.pixels().size(); ++i) {
      uint32_t pixel = renderer.pixels()[i];
      require(pixel != 0xffff00ff, "Guardrail UVs sampled terrain below the atlas top row");
      if (pixel != 0xff000000) {
        top = std::min(top, int(i / 640));
        bottom = std::max(bottom, int(i / 640));
        green |= pixel == 0xff00ff00;
        blue |= pixel == 0xff0000ff;
      }
    }
    require(green && blue, "Rail must be visible from both sides and span the entire atlas width");
    require(bottom - top > 20, "Recovered five-unit rail must have visible height near the camera");
  }
  std::cout << "guardrails passed\n";
}

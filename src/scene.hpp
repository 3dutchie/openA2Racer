#pragma once
#include "assets.hpp"
#include "hud.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

namespace a2 {
class Scene {
public:
  Scene(Renderer &renderer, Assets &assets)
      : renderer_(renderer), assets_(assets), hud_(renderer, assets) {}
  void draw(const Race &, int camera = 1, bool radar = true, bool horizon = true,
            bool dashboard = true);
  void carPreview(int car, float angle);
  void drawPause() { hud_.drawPause(); }

private:
  Renderer &renderer_;
  Assets &assets_;
  Hud hud_;
  void scenery(const Race &, const Camera &);
  void roadProps(const Race &, const Camera &);
  void rankMarker(const Camera &, int rank, Vec3 position, float yaw);
  void hud(const Race &, int camera, bool radar);
};
} // namespace a2

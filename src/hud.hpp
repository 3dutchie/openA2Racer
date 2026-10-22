#pragma once
#include "assets.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

namespace a2 {
// Original game artwork is decoded from the user's CD at runtime.
class Hud {
public:
  Hud(Renderer &, Assets &);
  void draw(const Race &, int camera, bool radar);
  void drawPause();
  void drawFinish(float presentationSeconds, int camera);

private:
  Renderer &renderer_;
  Assets &assets_;
  const std::vector<Sprite> &sprites_;
  Image cockpit_;
  bool original_ = false;
  void at(size_t sprite, int x, int y);
  void anchor(size_t sprite, int x, int y);
  void instruments(const Player &, int camera);
  void counters(const Race &);
  void radar(const Race &, int camera);
  void ticket(const Race &, int camera);
  void fallback(const Race &, int camera);
};
} // namespace a2

#pragma once
#include "types.hpp"

namespace a2 {
// Runtime reconstruction of the player's textures; never writes CD assets.
class FilteredTexture {
public:
  FilteredTexture(const Image &, bool mask);
  uint32_t sample(float u, float v, float lod) const;
  uint32_t sample(float u, float v, float lod, Vec2 footprintCenter) const;
  size_t bytes() const;
  int levels() const { return int(levels_.size()); }
  int width() const { return levels_.front().width; }
  int height() const { return levels_.front().height; }

private:
  struct Level {
    int width, height;
    std::vector<uint32_t> pixels;
  };
  int tilesX_ = 1, tilesY_ = 1;
  std::vector<Level> levels_;
  uint32_t bilinear(const Level &, float u, float v) const;
};
uint32_t mixColor(uint32_t a, uint32_t b, float t);
} // namespace a2

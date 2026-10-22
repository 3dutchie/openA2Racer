#include "texture.hpp"
#include <stdexcept>

namespace a2 {
namespace {
float cubic(float x) {
  x = std::abs(x);
  if (x < 1)
    return (1.5f * x - 2.5f) * x * x + 1;
  if (x < 2)
    return ((-.5f * x + 2.5f) * x - 4) * x + 2;
  return 0;
}
uint32_t pack(const std::array<float, 4> &v) {
  uint32_t result = 0;
  for (int i = 0; i < 4; ++i)
    result |= uint32_t(std::clamp(int(std::round(v[size_t(i)])), 0, 255)) << (i * 8);
  return result;
}
float wrap(float x) {
  return x - std::floor(x);
}
} // namespace
uint32_t mixColor(uint32_t a, uint32_t b, float t) {
  const unsigned weight = unsigned(std::clamp(int(t * 256 + .5f), 0, 256));
  constexpr uint32_t lanes = 0x00ff00ff;
  const uint32_t rb = (((a & lanes) * (256 - weight) + (b & lanes) * weight) >> 8) & lanes;
  const uint32_t ag =
      ((((a >> 8) & lanes) * (256 - weight) + ((b >> 8) & lanes) * weight)) & 0xff00ff00;
  return rb | ag;
}

FilteredTexture::FilteredTexture(const Image &source, bool mask) {
  if (source.width <= 0 || source.height <= 0 ||
      source.pixels.size() != size_t(source.width) * source.height)
    throw std::runtime_error("Invalid filtered texture");
  tilesX_ = source.width % std::max(1, source.tilesX) == 0 ? std::max(1, source.tilesX) : 1;
  tilesY_ = source.height % std::max(1, source.tilesY) == 0 ? std::max(1, source.tilesY) : 1;
  // Alpha is premultiplied while reconstructing and filtering, so cutout trees
  // cannot pick up black halos from transparent palette entries.
  auto read = [&](int x, int y, int cellX, int cellY) {
    int cw = source.width / tilesX_, ch = source.height / tilesY_;
    if (tilesX_ > 1)
      x = std::clamp(x, cellX * cw, (cellX + 1) * cw - 1);
    else
      x = (x % source.width + source.width) % source.width;
    if (tilesY_ > 1)
      y = std::clamp(y, cellY * ch, (cellY + 1) * ch - 1);
    else
      y = (y % source.height + source.height) % source.height;
    size_t p = size_t(y) * source.width + x;
    uint32_t c = source.pixels[p];
    if (mask && source.indices.size() == source.pixels.size() && source.indices[p] == 0)
      return uint32_t(0);
    float alpha = float(c >> 24) / 255;
    return (c & 0xff000000u) | (uint32_t(((c >> 16) & 255) * alpha) << 16) |
           (uint32_t(((c >> 8) & 255) * alpha) << 8) | uint32_t((c & 255) * alpha);
  };
  // Two-times Catmull-Rom reconstruction retains more edge contrast than a
  // bilinear enlargement. It cannot invent missing photographic detail.
  const int scale = source.width <= 512 && source.height <= 512 ? 2 : 1;
  Level base{source.width * scale, source.height * scale, {}};
  base.pixels.resize(size_t(base.width) * base.height);
  for (int y = 0; y < base.height; ++y) {
    float sy = (y + .5f) / scale - .5f;
    int iy = int(std::floor(sy)), cy = y / (base.height / tilesY_);
    for (int x = 0; x < base.width; ++x) {
      float sx = (x + .5f) / scale - .5f;
      int ix = int(std::floor(sx)), cx = x / (base.width / tilesX_);
      std::array<float, 4> channels{};
      for (int j = -1; j <= 2; ++j)
        for (int i = -1; i <= 2; ++i) {
          float w = cubic(sx - ix - i) * cubic(sy - iy - j);
          uint32_t c = read(ix + i, iy + j, cx, cy);
          for (int k = 0; k < 4; ++k)
            channels[size_t(k)] += float((c >> (k * 8)) & 255) * w;
        }
      uint32_t c = pack(channels);
      // Keep premultiplied channels bounded by alpha after cubic overshoot.
      unsigned a = c >> 24;
      base.pixels[size_t(y) * base.width + x] = (a << 24) | (std::min(a, (c >> 16) & 255) << 16) |
                                                (std::min(a, (c >> 8) & 255) << 8) |
                                                std::min(a, c & 255);
    }
  }
  levels_.push_back(std::move(base));
  while (levels_.back().width / tilesX_ >= 2 && levels_.back().height / tilesY_ >= 2) {
    const auto &prev = levels_.back();
    // Stop before a level would join neighbouring atlas cells.
    if (prev.width % (2 * tilesX_) || prev.height % (2 * tilesY_))
      break;
    Level next{prev.width / 2, prev.height / 2, {}};
    next.pixels.resize(size_t(next.width) * next.height);
    for (int y = 0; y < next.height; ++y)
      for (int x = 0; x < next.width; ++x) {
        size_t p = size_t(y * 2) * prev.width + x * 2;
        std::array<float, 4> v{};
        for (size_t q : {p, p + 1, p + size_t(prev.width), p + size_t(prev.width) + 1})
          for (int k = 0; k < 4; ++k)
            v[size_t(k)] += float((prev.pixels[q] >> (k * 8)) & 255) * .25f;
        next.pixels[size_t(y) * next.width + x] = pack(v);
      }
    levels_.push_back(std::move(next));
  }
}
uint32_t FilteredTexture::bilinear(const Level &level, float u, float v) const {
  int cellX = std::min(tilesX_ - 1, int(u * tilesX_));
  int cellY = std::min(tilesY_ - 1, int(v * tilesY_));
  float x = u * level.width - .5f, y = v * level.height - .5f;
  int ix = int(std::floor(x)), iy = int(std::floor(y));
  auto read = [&](int px, int py) {
    if (tilesX_ > 1)
      px = std::clamp(px, cellX * (level.width / tilesX_),
                      (cellX + 1) * (level.width / tilesX_) - 1);
    else
      px = px < 0 ? px + level.width : (px >= level.width ? px - level.width : px);
    if (tilesY_ > 1)
      py = std::clamp(py, cellY * (level.height / tilesY_),
                      (cellY + 1) * (level.height / tilesY_) - 1);
    else
      py = py < 0 ? py + level.height : (py >= level.height ? py - level.height : py);
    return level.pixels[size_t(py) * level.width + px];
  };
  return mixColor(mixColor(read(ix, iy), read(ix + 1, iy), x - ix),
                  mixColor(read(ix, iy + 1), read(ix + 1, iy + 1), x - ix), y - iy);
}
uint32_t FilteredTexture::sample(float u, float v, float lod) const {
  return sample(u, v, lod, {u, v});
}
uint32_t FilteredTexture::sample(float u, float v, float lod, Vec2 center) const {
  if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(lod))
    return 0;
  if (!std::isfinite(center.x) || !std::isfinite(center.y))
    return 0;
  if (tilesX_ > 1) {
    float lo = std::floor(wrap(center.x) * tilesX_) / tilesX_;
    u = std::clamp(wrap(center.x) + (u - center.x), lo + .000001f, lo + 1.f / tilesX_ - .000001f);
  }
  if (tilesY_ > 1) {
    float lo = std::floor(wrap(center.y) * tilesY_) / tilesY_;
    v = std::clamp(wrap(center.y) + (v - center.y), lo + .000001f, lo + 1.f / tilesY_ - .000001f);
  }
  u = wrap(u);
  v = wrap(v);
  lod = std::clamp(lod, 0.f, float(levels_.size() - 1));
  int a = int(lod), b = std::min(a + 1, int(levels_.size() - 1));
  uint32_t color = bilinear(levels_[size_t(a)], u, v);
  return b == a || lod - a < .00390625f
             ? color
             : mixColor(color, bilinear(levels_[size_t(b)], u, v), lod - a);
}
size_t FilteredTexture::bytes() const {
  size_t size = 0;
  for (const auto &level : levels_)
    size += level.pixels.size() * sizeof(uint32_t);
  return size;
}
} // namespace a2

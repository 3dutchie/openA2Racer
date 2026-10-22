#include "render_workers.hpp"
#include "renderer.hpp"
#include <limits>

namespace a2 {
void Renderer::setEnhanced(bool enabled) {
  if (enhanced_ == enabled)
    return;
  const int scale = enabled ? 2 : 1;
  // Allocate before committing, so allocation failure leaves a usable renderer.
  std::vector<uint32_t> color(size_t(width * scale) * height * scale);
  std::vector<float> depth(color.size());
  color_.swap(color);
  depth_.swap(depth);
  commands_.clear();
  for (auto &band : bands_)
    band.clear();
  enhanced_ = enabled;
  scale_ = scale;
}
void Renderer::resetTextureCache() {
  textures_.clear();
  textureBytes_ = 0;
}
const FilteredTexture &Renderer::filtered(const Image &image, bool mask) {
  auto key = std::make_pair(&image, mask);
  auto it = textures_.find(key);
  if (it != textures_.end()) {
    it->second.used = frame_;
    return it->second.texture;
  }
  FilteredTexture texture(image, mask);
  constexpr size_t budget = 96 * 1024 * 1024;
  while (!textures_.empty() && textureBytes_ + texture.bytes() > budget) {
    auto oldest =
        std::min_element(textures_.begin(), textures_.end(), [](const auto &a, const auto &b) {
          return a.second.used < b.second.used;
        });
    // Queued triangles retain texture pointers until this frame is flushed.
    if (oldest->second.used == frame_)
      break;
    textureBytes_ -= oldest->second.texture.bytes();
    textures_.erase(oldest);
  }
  textureBytes_ += texture.bytes();
  return textures_.emplace(key, CachedTexture{std::move(texture), frame_}).first->second.texture;
}
void Renderer::finishWorld() {
  if (!enhanced_)
    return;
  flushWorld();
  // Contrast-directed subpixel edge filtering, applied before the original HUD.
  // The direction follows the edge, preserving detail across flat surfaces.
  scratch_ = color_;
  const int w = pixelWidth(), h = pixelHeight();
  auto read = [&](int x, int y) {
    return scratch_[size_t(std::clamp(y, 0, h - 1)) * w + std::clamp(x, 0, w - 1)];
  };
  auto sample = [&](float x, float y) {
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    return mixColor(mixColor(read(ix, iy), read(ix + 1, iy), x - ix),
                    mixColor(read(ix, iy + 1), read(ix + 1, iy + 1), x - ix), y - iy);
  };
  auto luma = [](uint32_t c) {
    return float((c >> 16) & 255) * .299f + float((c >> 8) & 255) * .587f + float(c & 255) * .114f;
  };
  parallelRenderRows(h, 32, [&](int begin, int end) {
    for (int y = std::max(1, begin); y < std::min(h - 1, end); ++y)
      for (int x = 1; x < w - 1; ++x) {
        float m = luma(read(x, y)), nw = luma(read(x - 1, y - 1)), ne = luma(read(x + 1, y - 1)),
              sw = luma(read(x - 1, y + 1)), se = luma(read(x + 1, y + 1));
        float lo = std::min({m, nw, ne, sw, se}), hi = std::max({m, nw, ne, sw, se});
        if (hi - lo < std::max(12.f, hi * .125f))
          continue;
        float dx = -((nw + ne) - (sw + se)), dy = (nw + sw) - (ne + se);
        float reduce = std::max((nw + ne + sw + se) * .03125f, 2.f);
        float inverse = 1 / (std::min(std::abs(dx), std::abs(dy)) + reduce);
        dx = std::clamp(dx * inverse, -6.f, 6.f);
        dy = std::clamp(dy * inverse, -6.f, 6.f);
        uint32_t a = mixColor(sample(x - dx / 6, y - dy / 6), sample(x + dx / 6, y + dy / 6), .5f);
        uint32_t b = mixColor(
            a,
            mixColor(sample(x - dx * .5f, y - dy * .5f), sample(x + dx * .5f, y + dy * .5f), .5f),
            .5f);
        float value = luma(b);
        color_[size_t(y) * w + x] = value < lo || value > hi ? a : b;
      }
  });
}

void Renderer::shadow(const Camera &cam, const Mesh &mesh, Vec3 position, float yaw, float scale,
                      Vec3 groundPoint, Vec3 normal) {
  if (!enhanced_ || mesh.vertices.size() < 3)
    return;
  const Vec3 sun = normalized(Vec3{-.45f, 1, -.3f});
  normal = normalized(normal);
  if (normal.y < 0)
    normal = normal * -1;
  const float denominator = dot(sun, normal);
  if (denominator < .1f)
    return;
  Vec3 horizontal = normalized(cross({0, 0, 1}, normal)), vertical = cross(normal, horizontal);
  float s = std::sin(yaw), c = std::cos(yaw);
  std::vector<Vec2> points;
  for (const auto &vertex : mesh.vertices) {
    Vec3 v = vertex.position * scale;
    Vec3 world = position + Vec3{v.x * c + v.z * s, v.y, v.z * c - v.x * s};
    float elevation = std::max(0.f, dot(world - groundPoint, normal));
    Vec3 p = world - sun * (elevation / denominator) - groundPoint;
    points.push_back({dot(p, horizontal), dot(p, vertical)});
  }
  std::sort(points.begin(), points.end(),
            [](Vec2 a, Vec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
  points.erase(std::unique(points.begin(), points.end(),
                           [](Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }),
               points.end());
  if (points.size() < 3)
    return;
  auto turn = [](Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  };
  std::vector<Vec2> hull(points.size() * 2);
  size_t count = 0;
  for (auto p : points) {
    while (count >= 2 && turn(hull[count - 2], hull[count - 1], p) <= 0)
      --count;
    hull[count++] = p;
  }
  size_t lower = count + 1;
  for (size_t i = points.size() - 1; i-- > 0;) {
    auto p = points[i];
    while (count >= lower && turn(hull[count - 2], hull[count - 1], p) <= 0)
      --count;
    hull[count++] = p;
  }
  if (count < 4)
    return;
  hull.resize(count - 1);
  Vec2 center{};
  for (auto p : hull)
    center = center + p * (1.f / hull.size());
  auto vertex = [&](Vec2 p, float alpha) {
    return DrawVertex{
        groundPoint + horizontal * p.x + vertical * p.y + normal * .08f, {}, 1, alpha};
  };
  for (size_t i = 0; i < hull.size(); ++i) {
    Vec2 a = hull[i], b = hull[(i + 1) % hull.size()];
    triangle(cam, vertex(center, .3f), vertex(a, .3f), vertex(b, .3f), nullptr, 0xff132135, false,
             1, true);
    Vec2 oa = a + normalized(a - center) * 3, ob = b + normalized(b - center) * 3;
    triangle(cam, vertex(a, .3f), vertex(oa, 0), vertex(ob, 0), nullptr, 0xff132135, false, 1,
             true);
    triangle(cam, vertex(a, .3f), vertex(ob, 0), vertex(b, .3f), nullptr, 0xff132135, false, 1,
             true);
  }
}
} // namespace a2

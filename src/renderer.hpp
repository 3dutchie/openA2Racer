#pragma once
#include "texture.hpp"
#include "types.hpp"
#include <atomic>
#include <filesystem>
#include <map>
#include <string_view>

namespace a2 {
struct Camera {
  Vec3 position, forward{0, 0, 1}, right{1, 0, 0}, up{0, 1, 0};
  float focal = 420, nearPlane = 1.5f, farPlane = 2600;
  void lookAt(Vec3 eye, Vec3 target);
};
struct DrawVertex {
  Vec3 position;
  Vec2 uv;
  float light = 1, alpha = 1;
};
class Renderer {
public:
  static constexpr int width = 640, height = 480;
  Renderer();
  void setEnhanced(bool);
  bool enhanced() const { return enhanced_; }
  int pixelWidth() const { return width * scale_; }
  int pixelHeight() const { return height * scale_; }
  void resetTextureCache();
  void finishWorld();
  void flushWorld();
  void clear(uint32_t color);
  void background(const Image &sky, float heading = 0, float horizon = 165);
  void triangle(const Camera &, DrawVertex, DrawVertex, DrawVertex, const Image *,
                uint32_t color = 0xffffffff, bool mask = false, float light = 1,
                bool blend = false);
  void quad(const Camera &, Vec3 a, Vec3 b, Vec3 c, Vec3 d, const Image *, float uScale = 1,
            float vScale = 1, uint32_t color = 0xffffffff);
  void mesh(const Camera &, const Mesh &, const Image &, Vec3 position, float yaw, float scale = 1,
            bool mask = false, bool cullBackFaces = false);
  void shadow(const Camera &, const Mesh &, Vec3 position, float yaw, float scale, Vec3 groundPoint,
              Vec3 groundNormal = {0, 1, 0});
  void rect(int x, int y, int w, int h, uint32_t color);
  void line(int x0, int y0, int x1, int y1, uint32_t color);
  void text(int x, int y, std::string_view text, uint32_t color = 0xffffffff, int scale = 1);
  void blit(const Image &, int x, int y, int w = 0, int h = 0, bool transparent = false);
  void blitKey(const Image &, int x, int y, uint8_t transparentIndex, int w = 0, int h = 0);
  void sprite(const Sprite &, int anchorX, int anchorY, uint8_t transparentIndex = 255);
  void dim(float amount = 0.5f);
  bool saveBMP(const std::filesystem::path &) const;
  const std::vector<uint32_t> &pixels() const { return color_; }
  uint64_t triangles() const { return triangles_.load(std::memory_order_relaxed); }

private:
  std::vector<uint32_t> color_;
  std::vector<float> depth_;
  std::vector<uint32_t> scratch_;
  bool enhanced_ = false;
  int scale_ = 1;
  struct CachedTexture {
    FilteredTexture texture;
    uint64_t used;
  };
  std::map<std::pair<const Image *, bool>, CachedTexture> textures_;
  size_t textureBytes_ = 0;
  uint64_t frame_ = 0;
  const FilteredTexture &filtered(const Image &, bool mask);
  std::atomic<uint64_t> triangles_{0};
  struct Projected {
    float x, y, q, u, v, light, alpha;
  };
  struct Command {
    Projected a, b, c;
    const Image *texture;
    const FilteredTexture *filter;
    uint32_t tint;
    bool mask, blend;
    float light, farPlane;
  };
  std::vector<Command> commands_;
  std::array<std::vector<size_t>, 30> bands_;
  void raster(Projected a, Projected b, Projected c, const Image *, uint32_t, bool, float,
              float farPlane, bool blend, int rowBegin, int rowEnd, const FilteredTexture *);
};
} // namespace a2

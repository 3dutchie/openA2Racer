#include "renderer.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace a2;
namespace {
constexpr uint32_t black = 0xff000000, red = 0xffff0000, green = 0xff00ff00, blue = 0xff0000ff;
void check(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
}
uint32_t pixel(const Renderer &renderer, int x, int y) {
  return renderer.pixels().at(size_t(y) * Renderer::width + x);
}
Camera camera() {
  Camera c;
  c.focal = 100;
  c.farPlane = 1000;
  return c;
}
DrawVertex projected(const Camera &cam, float x, float y, float z, Vec2 uv = {}) {
  return {{(x - Renderer::width * .5f) * z / cam.focal,
           (Renderer::height * .5f - y) * z / cam.focal, z},
          uv};
}
void screenTriangle(Renderer &r, const Camera &c, float z, uint32_t color) {
  r.triangle(c, projected(c, 100, 100, z), projected(c, 500, 100, z), projected(c, 100, 400, z),
             nullptr, color);
}
void depth() {
  Renderer r;
  auto c = camera();
  r.clear(black);
  screenTriangle(r, c, 8, blue);
  screenTriangle(r, c, 2, red);
  check(pixel(r, 200, 200) == red, "Near geometry did not occlude far geometry");
  r.clear(black);
  screenTriangle(r, c, 2, red);
  screenTriangle(r, c, 8, blue);
  check(pixel(r, 200, 200) == red, "Depth depends on submission order");
  r.clear(black);
  screenTriangle(r, c, 8, blue);
  check(pixel(r, 200, 200) == blue, "Clear did not reset depth");
  // Both windings must render because the original meshes are submitted without
  // relying on a recovered culling convention.
  r.clear(black);
  r.triangle(c, projected(c, 100, 100, 5), projected(c, 100, 400, 5), projected(c, 500, 100, 5),
             nullptr, green);
  check(pixel(r, 200, 200) == green, "Reversed winding was dropped");
  r.clear(black);
  r.triangle(c, projected(c, 100, 100, 5), projected(c, 200, 200, 5), projected(c, 300, 300, 5),
             nullptr, red);
  check(r.triangles() == 0, "Degenerate triangle submitted");
}
void oneSidedMeshes() {
  // Two separate surfaces with opposite winding. Back faces must not fill
  // the openings in a front-facing mesh, as with the original rank numerals.
  const auto cam = camera();
  Mesh mesh;
  for (auto point : {Vec2{100, 100}, Vec2{260, 100}, Vec2{100, 400}, Vec2{380, 100}, Vec2{540, 100},
                     Vec2{380, 400}}) {
    auto v = projected(cam, point.x, point.y, 5);
    mesh.vertices.push_back({v.position - Vec3{0, 0, 5}, {}, {}, 0});
  }
  mesh.faces = {{{0, 1, 2}, {}}, {{3, 5, 4}, {}}};
  Image texture;
  texture.width = texture.height = 1;
  texture.pixels = {red};
  for (bool enhanced : {false, true}) {
    Renderer renderer;
    renderer.setEnhanced(enhanced);
    auto sample = [&](int x, int y) {
      const int scale = renderer.pixelWidth() / Renderer::width;
      return renderer.pixels()[size_t(y * scale) * renderer.pixelWidth() + x * scale];
    };
    renderer.clear(black);
    renderer.mesh(cam, mesh, texture, {0, 0, 5}, 0);
    renderer.finishWorld();
    check(sample(130, 180) != black && sample(410, 180) != black,
          "Default mesh rendering must retain both windings");
    renderer.clear(black);
    renderer.mesh(cam, mesh, texture, {0, 0, 5}, 0, 1, false, true);
    renderer.finishWorld();
    check(sample(130, 180) != black && sample(410, 180) == black,
          "One-sided mesh back faces filled front-facing openings");
    renderer.clear(black);
    renderer.mesh(cam, mesh, texture, {0, 0, 5}, pi, 1, false, true);
    renderer.finishWorld();
    check(renderer.triangles() == 1, "Rotating a one-sided mesh must reverse its visible surface");
  }
}
void perspective() {
  Renderer r;
  auto c = camera();
  Image texture;
  texture.width = 2;
  texture.height = 1;
  texture.pixels = {red, blue};
  r.clear(black);
  r.triangle(c, projected(c, 100, 100, 2, {0, 0}), projected(c, 500, 100, 8, {1, 0}),
             projected(c, 100, 400, 8, {0, 0}), &texture);
  // At this pixel affine U is just above0.5 (blue), while perspective-correct U
  // is about0.336 (red). This discriminates the interpolation implementations.
  check(pixel(r, 300, 200) == red, "UV interpolation is not perspective correct");
  r.clear(black);
  r.triangle(c, projected(c, 100, 100, 5, {-0.25f, 0}), projected(c, 500, 100, 5, {-0.25f, 0}),
             projected(c, 100, 400, 5, {-0.25f, 0}), &texture);
  check(pixel(r, 200, 200) == blue, "Negative UV wrapping failed");
}
void extremeCoordinates() {
  Renderer r;
  auto c = camera();
  Image texture;
  texture.width = texture.height = 1;
  texture.pixels = {red};
  r.clear(black);
  r.triangle(c, projected(c, 100, 100, 5, {1e30f, -1e30f}),
             projected(c, 500, 100, 5, {1e30f, -1e30f}), projected(c, 100, 400, 5, {1e30f, -1e30f}),
             &texture);
  check(pixel(r, 200, 200) == red, "Large finite UVs failed bounded texture wrapping");
  // These finite vertices pass the disk decoder but project outside int range.
  // The visible section must be rasterized after floating-point viewport bounds.
  r.clear(black);
  r.triangle(c, {{1e8f, -1, 4}, {}}, {{0, -1, 4}, {}}, {{0, 1, 4}, {}}, nullptr, green);
  check(pixel(r, 321, 240) == green, "Large projected coordinates failed viewport clipping");
  r.clear(black);
  r.triangle(c, {{-1e8f, -1, 4}, {}}, {{0, -1, 4}, {}}, {{0, 1, 4}, {}}, nullptr, blue);
  check(pixel(r, 318, 240) == blue,
        "Large negative projected coordinates failed viewport clipping");
  r.clear(black);
  r.triangle(c, {{std::numeric_limits<float>::infinity(), -1, 4}, {}}, {{0, -1, 4}, {}},
             {{0, 1, 4}, {}}, nullptr, green);
  check(r.triangles() == 0, "Non-finite projected vertex reached rasterization");
}
void clipping() {
  Renderer clipped, reference;
  auto c = camera();
  c.nearPlane = 1.5f;
  DrawVertex a{{0, -1, .5f}, {}}, b{{-3, 2, 5}, {}}, d{{3, 2, 5}, {}};
  float t = (c.nearPlane - a.position.z) / (b.position.z - a.position.z);
  DrawVertex left{a.position + (b.position - a.position) * t, {}},
      right{a.position + (d.position - a.position) * t, {}};
  clipped.clear(black);
  reference.clear(black);
  clipped.triangle(c, a, b, d, nullptr, red);
  reference.triangle(c, left, b, d, nullptr, red);
  reference.triangle(c, left, d, right, nullptr, red);
  check(clipped.pixels() == reference.pixels(),
        "Near-plane clipping differs from explicit clipped polygon");
  check(pixel(clipped, 320, 220) == red, "Near-plane crossing vanished");
  clipped.clear(black);
  clipped.triangle(c, {{0, 0, .3f}, {}}, {{1, 1, .4f}, {}}, {{-1, 1, .5f}, {}}, nullptr, red);
  check(clipped.triangles() == 0, "Fully near-clipped triangle rendered");
  c.farPlane = 10;
  clipped.clear(black);
  clipped.triangle(c, {{0, 0, 11}, {}}, {{1, 1, 12}, {}}, {{-1, 1, 13}, {}}, nullptr, red);
  check(clipped.triangles() == 0, "Fully far-clipped triangle rendered");
  clipped.clear(black);
  clipped.triangle(c, {{-2, -1, 5}, {}}, {{2, -1, 5}, {}}, {{0, 2, 15}, {}}, nullptr, red);
  check(clipped.triangles() > 0, "Far-plane crossing vanished");
}
void mask() {
  Renderer r;
  auto c = camera();
  Image texture;
  texture.width = 2;
  texture.height = 1;
  texture.pixels = {green, blue};
  texture.indices = {0, 1};
  auto a = projected(c, 100, 100, 3, {0, 0}), b = projected(c, 500, 100, 3, {1, 0}),
       d = projected(c, 500, 400, 3, {1, 1}), e = projected(c, 100, 400, 3, {0, 1});
  r.clear(black);
  r.triangle(c, a, b, d, &texture, 0xffffffff, true);
  r.triangle(c, a, d, e, &texture, 0xffffffff, true);
  r.quad(c, projected(c, 100, 100, 6).position, projected(c, 500, 100, 6).position,
         projected(c, 500, 400, 6).position, projected(c, 100, 400, 6).position, nullptr, 1, 1,
         red);
  check(pixel(r, 160, 200) == red, "Masked pixel wrote colour or depth");
  check(pixel(r, 440, 200) == blue, "Opaque indexed texel failed mask rendering");
}
void blitting() {
  Image image;
  image.width = 2;
  image.height = 2;
  image.pixels = {red, green, blue, 0xffffff00};
  image.indices = {0, 1, 2, 3};
  Renderer r;
  r.clear(black);
  r.blit(image, -1, -1);
  check(pixel(r, 0, 0) == 0xffffff00, "Negative blit clipping lost source position");
  check(pixel(r, 1, 0) == black, "Clipped blit extended past edge");
  r.clear(black);
  r.blit(image, -2, -2, 4, 4);
  check(pixel(r, 0, 0) == 0xffffff00 && pixel(r, 1, 1) == 0xffffff00,
        "Scaled clipped blit samples wrong pixel");
  r.clear(black);
  r.blit(image, 639, 479);
  check(pixel(r, 639, 479) == red, "Bottom-right blit clipping failed");
  r.clear(black);
  r.blit(image, 0, 0, 0, 0, true);
  check(pixel(r, 0, 0) == black && pixel(r, 1, 0) == green, "Transparent blit mask failed");
  r.rect(-100, -100, 101, 101, blue);
  check(pixel(r, 0, 0) == blue && pixel(r, 1, 1) == 0xffffff00, "Rectangle clipping failed");
}
void bmp() {
  Renderer r;
  r.clear(black);
  r.rect(0, 0, 1, 1, red);
  r.rect(639, 479, 1, 1, blue);
  auto path =
      std::filesystem::temp_directory_path() /
      ("a2-renderer-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bmp");
  check(r.saveBMP(path), "BMP writing failed");
  std::ifstream file(path, std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(file)), {});
  std::error_code ec;
  std::filesystem::remove(path, ec);
  auto u32 = [&](size_t p) {
    return uint32_t(b.at(p)) | uint32_t(b.at(p + 1)) << 8 | uint32_t(b.at(p + 2)) << 16 |
           uint32_t(b.at(p + 3)) << 24;
  };
  check(b.size() == 54 + 640 * 480 * 3 && b[0] == 'B' && b[1] == 'M', "BMP file header/size");
  check(u32(10) == 54 && u32(18) == 640 && u32(22) == 480, "BMP dimensions/offset");
  check(b[54 + 639 * 3] == 255 && b[54 + 639 * 3 + 1] == 0 && b[54 + 639 * 3 + 2] == 0,
        "BMP bottom row is not BGR");
  size_t top = 54 + size_t(479) * 640 * 3;
  check(b[top] == 0 && b[top + 1] == 0 && b[top + 2] == 255, "BMP top row orientation failed");
}
void spriteTransparency() {
  Renderer renderer;
  Image image;
  image.width = 2;
  image.height = 1;
  image.indices = {0, 255};
  image.pixels = {black, 0xffff00ff};
  renderer.clear(green);
  renderer.blitKey(image, 5, 7, 255);
  check(pixel(renderer, 5, 7) == black, "Sprite black must remain opaque");
  check(pixel(renderer, 6, 7) == green, "Sprite index255 must leave background intact");
  Sprite sprite;
  sprite.image = image;
  sprite.hotspotX = -2;
  sprite.hotspotY = 3;
  renderer.sprite(sprite, 10, 10);
  check(pixel(renderer, 12, 7) == black, "Signed sprite hotspot placement");
  renderer.clear(blue);
  renderer.blitKey(image, -1, 0, 255, 4, 2);
  check(pixel(renderer, 0, 0) == black && pixel(renderer, 1, 0) == blue,
        "Scaled index255 sprite clipping");
}
} // namespace
int main() {
  try {
    depth();
    oneSidedMeshes();
    perspective();
    extremeCoordinates();
    clipping();
    mask();
    blitting();
    spriteTransparency();
    bmp();
    std::cout << "Renderer checks passed: depth, perspective, clip, mask, wrapping, large "
                 "coordinates, blit, BMP\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Renderer test failed: " << e.what() << '\n';
    return 1;
  }
}

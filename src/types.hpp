#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace a2 {
constexpr float pi = 3.14159265358979323846f;
struct Vec2 {
  float x = 0, y = 0;
};
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
inline Vec2 operator+(Vec2 a, Vec2 b) {
  return {a.x + b.x, a.y + b.y};
}
inline Vec2 operator-(Vec2 a, Vec2 b) {
  return {a.x - b.x, a.y - b.y};
}
inline Vec2 operator*(Vec2 a, float s) {
  return {a.x * s, a.y * s};
}
inline Vec3 operator+(Vec3 a, Vec3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 a, float s) {
  return {a.x * s, a.y * s, a.z * s};
}
inline float dot(Vec2 a, Vec2 b) {
  return a.x * b.x + a.y * b.y;
}
inline float dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline float length(Vec2 a) {
  return std::sqrt(dot(a, a));
}
inline float length(Vec3 a) {
  return std::sqrt(dot(a, a));
}
inline Vec2 normalized(Vec2 a) {
  float l = length(a);
  return l > 0.00001f ? a * (1 / l) : Vec2{0, 1};
}
inline Vec3 normalized(Vec3 a) {
  float l = length(a);
  return l > 0.00001f ? a * (1 / l) : Vec3{0, 0, 1};
}
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
struct Image {
  int width = 0, height = 0;
  // Independently filtered cells keep terrain atlas tiles from bleeding.
  int tilesX = 1, tilesY = 1;
  std::vector<uint32_t> pixels; // 0xAARRGGBB
  std::vector<uint8_t> indices;
  std::array<uint32_t, 256> palette{};
};
struct Vertex {
  Vec3 position, normal;
  Vec2 uv;
  uint8_t flags = 0;
};
struct Face {
  std::array<uint16_t, 3> indices{};
  std::array<uint8_t, 3> flags{};
};
struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
};
struct Sound {
  int sampleRate = 0;
  std::vector<float> samples;
};
struct Sprite {
  Image image;
  int hotspotX = 0, hotspotY = 0;
};
struct RoadRecord {
  Vec2 center;
  std::array<uint32_t, 25> fields{};
};
struct Placement {
  std::array<uint32_t, 4> fields{};
  float value = 0;
};
struct Track {
  std::vector<RoadRecord> road;
  std::vector<Placement> placements;
  std::vector<float> distances;
  float length() const { return distances.empty() ? 0 : distances.back(); }
};
} // namespace a2

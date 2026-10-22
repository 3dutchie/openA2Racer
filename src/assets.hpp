#pragma once
#include "types.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <span>

namespace a2 {
using Bytes = std::vector<uint8_t>;
// Accepts either an extracted CD directory or the original raw CD image.
class GameData {
public:
  explicit GameData(const std::filesystem::path &path);
  Bytes read(const std::string &name) const;
  bool exists(const std::string &name) const;
  std::string description() const;

private:
  std::filesystem::path path_;
  struct Extent {
    uint32_t lba = 0, size = 0;
  };
  std::map<std::string, Extent> extents_;
  std::map<std::string, std::filesystem::path> files_;
  int sectorSize_ = 0, payloadOffset_ = 0;
};
class Archive {
public:
  Archive(const GameData &data, const std::string &stem);
  std::span<const uint8_t> member(const std::string &name) const;
  std::vector<std::string> names() const;
  bool contains(const std::string &name) const;

private:
  Bytes blob_;
  std::map<std::string, std::pair<size_t, size_t>> members_;
};
Image decodePCX(std::span<const uint8_t> bytes);
Mesh decodeMesh(std::span<const uint8_t> bytes);
Sound decodeWAV(std::span<const uint8_t> bytes);
std::vector<Sprite> decodeSprites(const GameData &data, const std::string &stem,
                                  const std::array<uint32_t, 256> &palette);
Track loadTrack(const GameData &data, int number);
// Cached decoded original resources; lookup is case-insensitive.
class Assets {
public:
  explicit Assets(const GameData &data);
  const Image &image(const std::string &name);
  const Mesh &mesh(const std::string &name);
  const Sound &sound(const std::string &name);
  const std::vector<Sprite> &sprites(const std::string &stem,
                                     const std::string &paletteImage = "SURSCRN.pcx");
  bool has(const std::string &name) const;
  const Archive &race() const { return race_; }
  const Archive &menu() const { return menu_; }

private:
  const GameData &data_;
  Archive race_, menu_;
  std::map<std::string, Image> images_;
  std::map<std::string, Mesh> meshes_;
  std::map<std::string, Sound> sounds_;
  std::map<std::pair<std::string, std::string>, std::vector<Sprite>> sprites_;
};
} // namespace a2

#include "assets.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>

namespace a2 {
namespace {
constexpr size_t maxFileBytes = 512u * 1024u * 1024u;
constexpr size_t maxPixels = 16u * 1024u * 1024u;
[[noreturn]] void fail(const std::string &message) {
  throw std::runtime_error(message);
}
std::string key(std::string name) {
  for (auto &c : name) {
    if (c >= 'A' && c <= 'Z')
      c = char(c - 'A' + 'a');
    if (c == '\\')
      c = '/';
  }
  if (name.empty() || name.front() == '/' || name.find(':') != std::string::npos)
    fail("Invalid game data path");
  for (size_t p = 0; p < name.size();) {
    auto e = name.find('/', p);
    if (e == std::string::npos)
      e = name.size();
    auto part = name.substr(p, e - p);
    if (part.empty() || part == "." || part == "..")
      fail("Invalid game data path");
    p = e + 1;
  }
  return name;
}
struct Reader {
  std::span<const uint8_t> b;
  void need(size_t p, size_t n) const {
    if (p > b.size() || n > b.size() - p)
      fail("Truncated asset data");
  }
  uint8_t u8(size_t p) const {
    need(p, 1);
    return b[p];
  }
  uint16_t u16(size_t p) const {
    need(p, 2);
    return uint16_t(b[p]) | uint16_t(b[p + 1]) << 8;
  }
  uint32_t u32(size_t p) const {
    need(p, 4);
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
           uint32_t(b[p + 3]) << 24;
  }
  float f32(size_t p) const {
    auto v = std::bit_cast<float>(u32(p));
    if (!std::isfinite(v))
      fail("Non-finite asset coordinate");
    return v;
  }
  bool tag(size_t p, const char *t) const {
    return p <= b.size() && 4 <= b.size() - p && std::memcmp(b.data() + p, t, 4) == 0;
  }
};
Bytes readFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    fail("Cannot open game data: " + path.string());
  const auto end = in.tellg();
  if (end < 0 || uint64_t(end) > maxFileBytes)
    fail("Game data file exceeds size limit: " + path.string());
  Bytes b(static_cast<size_t>(end));
  in.seekg(0);
  if (!b.empty() && !in.read(reinterpret_cast<char *>(b.data()), std::streamsize(b.size())))
    fail("Cannot read game data: " + path.string());
  return b;
}
Bytes readExtent(const std::filesystem::path &path, uint32_t lba, uint32_t size, int sector,
                 int payload) {
  if (size > maxFileBytes)
    fail("Disc member exceeds size limit");
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    fail("Cannot open disc image: " + path.string());
  auto end = in.tellg();
  if (end < 0)
    fail("Cannot size disc image");
  uint64_t fileBytes = uint64_t(end);
  Bytes b(size);
  size_t done = 0;
  while (done < size) {
    size_t n = std::min<size_t>(2048, size - done);
    uint64_t offset = (uint64_t(lba) + done / 2048) * uint64_t(sector) + uint64_t(payload);
    if (offset > fileBytes || n > fileBytes - offset)
      fail("Disc extent lies outside image");
    in.seekg(std::streamoff(offset));
    if (!in.read(reinterpret_cast<char *>(b.data() + done), std::streamsize(n)))
      fail("Cannot read disc image extent");
    done += n;
  }
  return b;
}
size_t checkedPixels(unsigned w, unsigned h) {
  if (w == 0 || h == 0 || w > 8192 || h > 8192 || size_t(w) * h > maxPixels)
    fail("Unsupported image dimensions");
  return size_t(w) * h;
}
uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return 0xff000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
}
} // namespace

GameData::GameData(const std::filesystem::path &path) : path_(path) {
  if (std::filesystem::is_directory(path_)) {
    for (const auto &entry : std::filesystem::directory_iterator(path_))
      if (entry.is_regular_file()) {
        auto n = key(entry.path().filename().string());
        if (!files_.emplace(n, entry.path()).second)
          fail("Ambiguous case-insensitive game filename: " + n);
      }
    if (!exists("RCS.IND") && std::filesystem::is_directory(path_ / "CD")) {
      path_ /= "CD";
      files_.clear();
      for (const auto &entry : std::filesystem::directory_iterator(path_))
        if (entry.is_regular_file()) {
          auto n = key(entry.path().filename().string());
          if (!files_.emplace(n, entry.path()).second)
            fail("Ambiguous case-insensitive game filename: " + n);
        }
    }
  } else {
    // Probe the descriptor at logical sector 16; this release's CUE uses MODE2/2352.
    Bytes descriptor;
    for (auto layout : std::array<std::pair<int, int>, 3>{{{2352, 24}, {2048, 0}, {2352, 16}}}) {
      try {
        auto d = readExtent(path_, 16, 2048, layout.first, layout.second);
        if (d.size() >= 7 && std::memcmp(d.data() + 1, "CD001", 5) == 0 && d[6] == 1) {
          sectorSize_ = layout.first;
          payloadOffset_ = layout.second;
          descriptor = std::move(d);
          break;
        }
      } catch (const std::runtime_error &) {
      }
    }
    if (!sectorSize_)
      fail("Not a supported ISO9660 disc image (expected ISO or MODE2/2352)");
    bool joliet = false;
    Bytes primary;
    for (uint32_t sector = 16; sector < 80; ++sector) {
      auto d = readExtent(path_, sector, 2048, sectorSize_, payloadOffset_);
      if (std::memcmp(d.data() + 1, "CD001", 5) != 0 || d[6] != 1)
        fail("Invalid ISO9660 volume descriptor");
      if (d[0] == 1)
        primary = d;
      if (d[0] == 2 && d[88] == '%' && d[89] == '/' &&
          (d[90] == '@' || d[90] == 'C' || d[90] == 'E')) {
        descriptor = d;
        joliet = true;
      }
      if (d[0] == 255)
        break;
    }
    if (!joliet) {
      if (primary.empty())
        fail("Disc lacks primary ISO9660 descriptor");
      descriptor = std::move(primary);
    }
    Reader vr{descriptor};
    if (vr.u16(128) != 2048)
      fail("Unsupported ISO9660 logical sector size");
    if (vr.u8(156) < 34)
      fail("Invalid ISO9660 root record");
    auto root = readExtent(path_, vr.u32(158), vr.u32(166), sectorSize_, payloadOffset_);
    Reader rr{root};
    for (size_t p = 0; p < root.size();) {
      auto n = rr.u8(p);
      if (!n) {
        p = (p / 2048 + 1) * 2048;
        continue;
      }
      if (n < 34 || (p % 2048) + n > 2048)
        fail("Invalid ISO9660 directory record");
      rr.need(p, n);
      auto nameBytes = rr.u8(p + 32);
      if (size_t(33) + nameBytes > n)
        fail("Invalid ISO9660 filename");
      if ((rr.u8(p + 25) & 2) == 0) {
        std::string name;
        for (size_t i = 0; i < nameBytes; i += (joliet ? 2 : 1)) {
          if (joliet) {
            if (i + 1 >= nameBytes || rr.u8(p + 33 + i) != 0)
              fail("Unsupported non-ASCII root game filename");
            name.push_back(char(rr.u8(p + 34 + i)));
          } else
            name.push_back(char(rr.u8(p + 33 + i)));
        }
        auto semicolon = name.find(';');
        if (semicolon != std::string::npos)
          name.resize(semicolon);
        if (!name.empty() && name.back() == '.')
          name.pop_back();
        auto normalized = key(name);
        if (!extents_.emplace(normalized, Extent{rr.u32(p + 2), rr.u32(p + 10)}).second)
          fail("Duplicate disc filename: " + normalized);
      }
      p += n;
    }
  }
  if (!exists("RCS.IND") || !exists("RCS.IMG") || !exists("ETAPPE1.TRK"))
    fail("A2 Racer data not found in " + path_.string() +
         ". Select the original CD directory or image.");
}
Bytes GameData::read(const std::string &name) const {
  auto n = key(name);
  if (sectorSize_) {
    auto it = extents_.find(n);
    if (it == extents_.end())
      fail("Missing disc file: " + name);
    return readExtent(path_, it->second.lba, it->second.size, sectorSize_, payloadOffset_);
  }
  auto it = files_.find(n);
  if (it == files_.end())
    fail("Missing game file: " + name);
  return readFile(it->second);
}
bool GameData::exists(const std::string &name) const {
  auto n = key(name);
  return sectorSize_ ? extents_.contains(n) : files_.contains(n);
}
std::string GameData::description() const {
  return path_.string() + (sectorSize_ ? " (disc image)" : " (directory)");
}

Archive::Archive(const GameData &data, const std::string &stem) : blob_(data.read(stem + ".IMG")) {
  auto idx = data.read(stem + ".IND");
  Reader r{idx};
  auto count = r.u16(0);
  if (!count || idx.size() != 2 + size_t(count) * 24)
    fail("Invalid named archive index: " + stem);
  for (size_t i = 0; i < count; ++i) {
    size_t rec = 2 + i * 24, start = r.u32(rec + 20),
           end = i + 1 < count ? r.u32(rec + 44) : blob_.size();
    if (start >= end || end > blob_.size() || (i == 0 && start != 0))
      fail("Archive offsets out of bounds: " + stem);
    std::string name;
    for (size_t j = 0; j < 12 && r.u8(rec + j); ++j)
      name.push_back(char(r.u8(rec + j)));
    if (!members_.emplace(key(name), std::pair{start, end - start}).second)
      fail("Duplicate archive member: " + name);
  }
}
std::span<const uint8_t> Archive::member(const std::string &name) const {
  auto it = members_.find(key(name));
  if (it == members_.end())
    fail("Missing archive member: " + name);
  return std::span<const uint8_t>(blob_).subspan(it->second.first, it->second.second);
}
std::vector<std::string> Archive::names() const {
  std::vector<std::string> names;
  for (const auto &[n, unused] : members_)
    names.push_back(n);
  return names;
}
bool Archive::contains(const std::string &name) const {
  return members_.contains(key(name));
}

Image decodePCX(std::span<const uint8_t> bytes) {
  Reader r{bytes};
  r.need(0, 128 + 769);
  if (r.u8(0) != 10 || r.u8(2) != 1 || r.u8(3) != 8 || r.u8(65) != 1 ||
      r.u8(bytes.size() - 769) != 12)
    fail("Unsupported PCX format (expected 8-bit indexed RLE)");
  auto xmin = r.u16(4), ymin = r.u16(6), xmax = r.u16(8), ymax = r.u16(10);
  if (xmax < xmin || ymax < ymin)
    fail("Invalid PCX bounds");
  unsigned w = unsigned(xmax) - xmin + 1, h = unsigned(ymax) - ymin + 1, stride = r.u16(66);
  size_t pixels = checkedPixels(w, h);
  if (stride < w || stride > 8192 || size_t(stride) * h > maxPixels)
    fail("Invalid PCX scanline size");
  Image out;
  out.width = int(w);
  out.height = int(h);
  out.indices.resize(pixels);
  out.pixels.resize(pixels);
  size_t palette = bytes.size() - 768;
  for (size_t i = 0; i < 256; ++i)
    out.palette[i] =
        rgb(r.u8(palette + i * 3), r.u8(palette + i * 3 + 1), r.u8(palette + i * 3 + 2));
  size_t pos = 128, decoded = 0, expected = size_t(stride) * h;
  while (decoded < expected) {
    if (pos >= bytes.size() - 769)
      fail("Truncated PCX pixel stream");
    auto value = r.u8(pos++);
    size_t count = 1;
    if ((value & 0xc0) == 0xc0) {
      count = value & 0x3f;
      if (!count || pos >= bytes.size() - 769)
        fail("Invalid PCX run");
      value = r.u8(pos++);
    }
    if (count > expected - decoded)
      fail("PCX run exceeds raster");
    for (size_t j = 0; j < count; ++j, ++decoded) {
      auto x = decoded % stride, y = decoded / stride;
      if (x < w) {
        out.indices[y * w + x] = value;
        out.pixels[y * w + x] = out.palette[value];
      }
    }
  }
  return out;
}
Mesh decodeMesh(std::span<const uint8_t> bytes) {
  Reader r{bytes};
  uint32_t nv = r.u32(0), nf = r.u32(4);
  if (nv == 0 || nv > 65536 || nf > 1000000 ||
      uint64_t(8) + uint64_t(nv) * 33 + uint64_t(nf) * 9 != bytes.size())
    fail("Invalid D3D mesh counts or size");
  Mesh mesh;
  mesh.vertices.reserve(nv);
  mesh.faces.reserve(nf);
  for (size_t i = 0; i < nv; ++i) {
    size_t p = 8 + i * 33;
    mesh.vertices.push_back({{r.f32(p), r.f32(p + 4), r.f32(p + 8)},
                             {r.f32(p + 12), r.f32(p + 16), r.f32(p + 20)},
                             {r.f32(p + 24), r.f32(p + 28)},
                             r.u8(p + 32)});
  }
  for (size_t i = 0; i < nf; ++i) {
    size_t p = 8 + size_t(nv) * 33 + i * 9;
    Face f{{r.u16(p), r.u16(p + 2), r.u16(p + 4)}, {r.u8(p + 6), r.u8(p + 7), r.u8(p + 8)}};
    for (auto index : f.indices)
      if (index >= nv)
        fail("Mesh triangle index out of bounds");
    mesh.faces.push_back(f);
  }
  return mesh;
}
Sound decodeWAV(std::span<const uint8_t> bytes) {
  Reader r{bytes};
  r.need(0, 12);
  if (!r.tag(0, "RIFF") || !r.tag(8, "WAVE"))
    fail("Invalid WAV header");
  size_t end = size_t(r.u32(4)) + 8;
  if (end > bytes.size() || end < 12)
    fail("Invalid RIFF size");
  unsigned channels = 0, bits = 0, rate = 0, align = 0;
  bool haveFmt = false, haveData = false;
  std::span<const uint8_t> pcm;
  for (size_t p = 12; p < end;) {
    if (end - p < 8)
      fail("Truncated WAV chunk header");
    size_t n = r.u32(p + 4), start = p + 8;
    if (n > end - start)
      fail("WAV chunk exceeds RIFF");
    if (r.tag(p, "fmt ")) {
      if (n < 16)
        fail("Truncated WAV format");
      if (r.u16(start) != 1)
        fail("Unsupported WAV encoding (expected PCM)");
      channels = r.u16(start + 2);
      rate = r.u32(start + 4);
      align = r.u16(start + 12);
      bits = r.u16(start + 14);
      haveFmt = true;
    }
    if (r.tag(p, "data")) {
      if (haveData)
        fail("Multiple WAV data chunks unsupported");
      pcm = bytes.subspan(start, n);
      haveData = true;
    }
    p = start + n;
    if (n & 1) {
      if (p < end)
        ++p;
      else if (p != end)
        fail("Invalid WAV padding");
    }
  }
  if (!haveFmt || !haveData || !channels || channels > 2 || (bits != 8 && bits != 16) ||
      rate < 1000 || rate > 192000 || align != channels * (bits / 8) || pcm.size() % align)
    fail("Unsupported or invalid WAV PCM format");
  Sound out;
  out.sampleRate = int(rate);
  out.samples.resize(pcm.size() / align);
  Reader pr{pcm};
  for (size_t i = 0; i < out.samples.size(); ++i) {
    float sample = 0;
    for (size_t c = 0; c < channels; ++c) {
      size_t p = i * align + c * (bits / 8);
      sample += bits == 8 ? (float(pr.u8(p)) - 128.f) / 128.f
                          : float(std::bit_cast<int16_t>(pr.u16(p))) / 32768.f;
    }
    out.samples[i] = sample / float(channels);
  }
  return out;
}
std::vector<Sprite> decodeSprites(const GameData &data, const std::string &stem,
                                  const std::array<uint32_t, 256> &palette) {
  auto idx = data.read(stem + ".IND"), blob = data.read(stem + ".IMG");
  Reader ir{idx}, br{blob};
  auto count = ir.u16(0);
  if (idx.size() != 2 + size_t(count) * 4 || br.u16(0) != count)
    fail("Invalid sprite archive counts");
  std::vector<Sprite> sprites;
  sprites.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    size_t start = ir.u32(2 + i * 4), end = i + 1 < count ? ir.u32(6 + i * 4) : blob.size();
    if (start >= end || end > blob.size() || (i == 0 && start != 2))
      fail("Invalid sprite offsets");
    br.need(start, 8);
    auto w = br.u16(start), h = br.u16(start + 2);
    auto size = checkedPixels(w, h);
    if (end - start != 8 + size)
      fail("Invalid sprite raster size");
    Sprite s;
    s.hotspotX = std::bit_cast<int16_t>(br.u16(start + 4));
    s.hotspotY = std::bit_cast<int16_t>(br.u16(start + 6));
    s.image.width = w;
    s.image.height = h;
    s.image.palette = palette;
    s.image.indices.assign(blob.begin() + std::ptrdiff_t(start + 8),
                           blob.begin() + std::ptrdiff_t(end));
    s.image.pixels.reserve(size);
    for (auto index : s.image.indices)
      s.image.pixels.push_back(palette[index]);
    sprites.push_back(std::move(s));
  }
  return sprites;
}
Track loadTrack(const GameData &data, int number) {
  if (number < 1 || number > 3)
    fail("Invalid track number");
  auto stem = "ETAPPE" + std::to_string(number);
  auto tb = data.read(stem + ".TRK"), ob = data.read(stem + ".OMP");
  Reader tr{tb}, om{ob};
  auto n = tr.u32(0), m = om.u32(0);
  if (n < 2 || n > 1000000 || uint64_t(n) * 108 + 4 != tb.size() || m > 1000000 ||
      uint64_t(m) * 20 + 4 != ob.size())
    fail("Invalid track/placement record count");
  Track t;
  t.road.reserve(n);
  t.distances.reserve(n);
  t.placements.reserve(m);
  for (size_t i = 0; i < n; ++i) {
    size_t p = 4 + i * 108;
    RoadRecord rr;
    rr.center = {tr.f32(p), tr.f32(p + 4)};
    for (size_t j = 0; j < 25; ++j)
      rr.fields[j] = tr.u32(p + 8 + j * 4);
    float d = i ? a2::length(rr.center - t.road.back().center) : 0.f;
    if (i && (!std::isfinite(d) || d < .001f))
      fail("Degenerate track segment");
    t.distances.push_back(i ? t.distances.back() + d : 0);
    t.road.push_back(rr);
  }
  for (size_t i = 0; i < m; ++i) {
    size_t p = 4 + i * 20;
    Placement pl;
    for (size_t j = 0; j < 4; ++j)
      pl.fields[j] = om.u32(p + j * 4);
    pl.value = om.f32(p + 16);
    if (pl.fields[0] >= n)
      fail("Placement road index out of bounds");
    t.placements.push_back(pl);
  }
  return t;
}
Assets::Assets(const GameData &data) : data_(data), race_(data, "RCS"), menu_(data, "A2MRCS") {}
bool Assets::has(const std::string &name) const {
  return race_.contains(name) || menu_.contains(name);
}
const Image &Assets::image(const std::string &name) {
  auto n = key(name);
  auto it = images_.find(n);
  if (it == images_.end()) {
    it = images_.emplace(n, decodePCX((race_.contains(n) ? race_ : menu_).member(n))).first;
    if (n == "grass2.pcx") {
      it->second.tilesX = 8;
      it->second.tilesY = 8;
    }
  }
  return it->second;
}
const Mesh &Assets::mesh(const std::string &name) {
  auto n = key(name);
  auto it = meshes_.find(n);
  if (it == meshes_.end())
    it = meshes_.emplace(n, decodeMesh((race_.contains(n) ? race_ : menu_).member(n))).first;
  return it->second;
}
const Sound &Assets::sound(const std::string &name) {
  auto n = key(name);
  auto it = sounds_.find(n);
  if (it == sounds_.end())
    it = sounds_.emplace(n, decodeWAV((race_.contains(n) ? race_ : menu_).member(n))).first;
  return it->second;
}
const std::vector<Sprite> &Assets::sprites(const std::string &stem,
                                           const std::string &paletteImage) {
  auto id = std::make_pair(key(stem), key(paletteImage));
  auto it = sprites_.find(id);
  if (it == sprites_.end())
    it = sprites_.emplace(id, decodeSprites(data_, stem, image(paletteImage).palette)).first;
  return it->second;
}
} // namespace a2

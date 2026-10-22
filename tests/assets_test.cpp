#include "assets.hpp"
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error(message);
}
void rejects(const std::function<void()> &f, const std::string &message) {
  bool threw = false;
  try {
    f();
  } catch (const std::exception &) {
    threw = true;
  }
  check(threw, message);
}
void put16(Bytes &b, size_t p, uint16_t v) {
  b.at(p) = uint8_t(v);
  b.at(p + 1) = uint8_t(v >> 8);
}
void put32(Bytes &b, size_t p, uint32_t v) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(v >> (8 * i));
}
void putFloat(Bytes &b, size_t p, float v) {
  put32(b, p, std::bit_cast<uint32_t>(v));
}
void tag(Bytes &b, size_t p, const char *text) {
  for (size_t i = 0; i < 4; ++i)
    b.at(p + i) = uint8_t(text[i]);
}
Bytes pcxFixture() {
  Bytes b(128 + 6 + 769);
  b[0] = 10;
  b[1] = 5;
  b[2] = 1;
  b[3] = 8;
  b[65] = 1;
  put16(b, 8, 1);
  put16(b, 10, 1);
  put16(b, 66, 2);
  // Two pixels of index 3 on line 1, then two literal indices on line 2.
  b[128] = 0xc2;
  b[129] = 3;
  b[130] = 1;
  b[131] = 2;
  b[132] = 0;
  b[133] = 0;
  size_t pal = b.size() - 769;
  b[pal] = 12;
  b[pal + 1 + 3 * 3] = 255;
  b[pal + 1 + 1 * 3 + 1] = 255;
  b[pal + 1 + 2 * 3 + 2] = 255;
  return b;
}
Bytes meshFixture() {
  Bytes b(8 + 3 * 33 + 9);
  put32(b, 0, 3);
  put32(b, 4, 1);
  putFloat(b, 8, 1);
  putFloat(b, 8 + 33 + 4, 1);
  putFloat(b, 8 + 66 + 8, 1);
  put16(b, 8 + 99 + 2, 1);
  put16(b, 8 + 99 + 4, 2);
  return b;
}
Bytes waveFixture() {
  // fmt, odd-length unknown chunk with pad, data; three unsigned PCM samples.
  Bytes b(12 + 24 + 10 + 12);
  tag(b, 0, "RIFF");
  put32(b, 4, uint32_t(b.size() - 8));
  tag(b, 8, "WAVE");
  tag(b, 12, "fmt ");
  put32(b, 16, 16);
  put16(b, 20, 1);
  put16(b, 22, 1);
  put32(b, 24, 11025);
  put32(b, 28, 11025);
  put16(b, 32, 1);
  put16(b, 34, 8);
  tag(b, 36, "JUNK");
  put32(b, 40, 1);
  b[44] = 77;
  tag(b, 46, "data");
  put32(b, 50, 3);
  b[54] = 0;
  b[55] = 128;
  b[56] = 255;
  return b;
}
void write(const std::filesystem::path &p, const Bytes &b) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
  if (!f)
    throw std::runtime_error("Cannot write test fixture");
}
struct TempDir {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("a2-assets-test-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TempDir() { std::filesystem::create_directory(path); }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
void unitTests() {
  auto pcx = pcxFixture();
  auto image = decodePCX(pcx);
  check(image.width == 2 && image.height == 2, "PCX dimensions");
  check(image.indices == Bytes({3, 3, 1, 2}), "PCX RLE indices");
  check(image.pixels == std::vector<uint32_t>({0xffff0000u, 0xffff0000u, 0xff00ff00u, 0xff0000ffu}),
        "PCX palette conversion");
  auto bad = pcx;
  bad[128] = 0xc0;
  rejects([&] { decodePCX(bad); }, "Zero-length PCX run accepted");
  bad = pcx;
  bad[128] = 0xff;
  rejects([&] { decodePCX(bad); }, "PCX raster overflow accepted");
  bad = pcx;
  put16(bad, 66, 1);
  rejects([&] { decodePCX(bad); }, "Undersized PCX row accepted");
  bad = pcx;
  put16(bad, 8, 65535);
  rejects([&] { decodePCX(bad); }, "Excessive PCX allocation accepted");
  auto mesh = meshFixture();
  auto model = decodeMesh(mesh);
  check(model.vertices.size() == 3 && model.faces.size() == 1, "Mesh counts");
  check(model.vertices[0].position.x == 1 && model.faces[0].indices[2] == 2, "Mesh records");
  bad = mesh;
  put16(bad, 8 + 99, 3);
  rejects([&] { decodeMesh(bad); }, "Mesh invalid index accepted");
  bad = mesh;
  put32(bad, 8, 0x7fc00000);
  rejects([&] { decodeMesh(bad); }, "Mesh NaN accepted");
  bad = mesh;
  put32(bad, 0, 0xffffffff);
  rejects([&] { decodeMesh(bad); }, "Mesh count overflow accepted");
  auto wave = waveFixture();
  auto sound = decodeWAV(wave);
  check(sound.sampleRate == 11025 && sound.samples.size() == 3, "WAV metadata/chunk padding");
  check(sound.samples[0] == -1 && sound.samples[1] == 0 && sound.samples[2] > .99f,
        "Unsigned PCM conversion");
  bad = wave;
  put32(bad, 50, 0xffffffff);
  rejects([&] { decodeWAV(bad); }, "WAV chunk overflow accepted");
  bad = wave;
  put16(bad, 20, 6);
  rejects([&] { decodeWAV(bad); }, "Compressed WAV accepted");
  for (size_t n = 0; n < mesh.size(); ++n)
    rejects([&] { decodeMesh(std::span<const uint8_t>(mesh).first(n)); },
            "Truncated mesh accepted");
  for (size_t n = 0; n < 58; ++n)
    rejects([&] { decodeWAV(std::span<const uint8_t>(wave).first(n)); }, "Truncated WAV accepted");
  rejects([] { decodePCX({}); }, "Empty PCX accepted");
  rejects([] { GameData("path-that-does-not-exist-a2-tests"); }, "Missing data accepted");
  TempDir fixture;
  Bytes index(26);
  put16(index, 0, 1);
  const char *name = "test.pcx";
  for (size_t i = 0; name[i]; ++i)
    index[2 + i] = uint8_t(name[i]);
  write(fixture.path / "rCs.InD", index);
  write(fixture.path / "rCS.IMG", pcx);
  write(fixture.path / "ETAPPE1.TRK", {});
  GameData data(fixture.path);
  check(data.read("RCS.IMG") == pcx, "Case-insensitive directory lookup");
  Archive archive(data, "RCS");
  check(archive.member("TEST.PCX").size() == pcx.size(), "Named archive lookup");
  rejects([&] { data.read("../RCS.IMG"); }, "Parent traversal accepted");
  rejects([&] { archive.member("absent.pcx"); }, "Missing archive member accepted");
  put32(index, 22, 0xffffffff);
  write(fixture.path / "rCs.InD", index);
  rejects([&] { Archive broken(data, "RCS"); }, "Invalid archive offset accepted");
  // A small standards-shaped primary namespace fixture exercises plain ISO and
  // raw MODE2 sector mapping without needing proprietary game files in CI.
  put32(index, 22, 0);
  Bytes iso(22 * 2048);
  auto descriptor = [&](size_t lba, uint8_t type) {
    size_t p = lba * 2048;
    iso[p] = type;
    const char *id = "CD001";
    for (size_t i = 0; i < 5; ++i)
      iso[p + 1 + i] = uint8_t(id[i]);
    iso[p + 6] = 1;
  };
  descriptor(16, 1);
  descriptor(17, 255);
  put16(iso, 16 * 2048 + 128, 2048);
  iso[16 * 2048 + 156] = 34;
  put32(iso, 16 * 2048 + 158, 18);
  put32(iso, 16 * 2048 + 166, 2048);
  size_t record = 18 * 2048;
  auto rootFile = [&](const std::string &name, uint32_t lba, const Bytes &bytes) {
    size_t n = 33 + name.size() + (name.size() % 2 == 0 ? 1 : 0);
    iso[record] = uint8_t(n);
    put32(iso, record + 2, lba);
    put32(iso, record + 10, uint32_t(bytes.size()));
    iso[record + 32] = uint8_t(name.size());
    for (size_t i = 0; i < name.size(); ++i)
      iso[record + 33 + i] = uint8_t(name[i]);
    std::copy(bytes.begin(), bytes.end(), iso.begin() + std::ptrdiff_t(lba * 2048));
    record += n;
  };
  rootFile("RCS.IND;1", 19, index);
  rootFile("RCS.IMG;1", 20, pcx);
  rootFile("ETAPPE1.TRK;1", 21, {});
  write(fixture.path / "plain.iso", iso);
  GameData plain(fixture.path / "plain.iso");
  check(plain.read("rCs.ImG") == pcx, "Plain ISO extent mapping");
  check(Archive(plain, "RCS").member("TEST.PCX").size() == pcx.size(), "Archive from plain ISO");
  Bytes raw(22 * 2352);
  for (size_t lba = 0; lba < 22; ++lba)
    std::copy_n(iso.begin() + std::ptrdiff_t(lba * 2048), 2048,
                raw.begin() + std::ptrdiff_t(lba * 2352 + 24));
  write(fixture.path / "mode2.iso", raw);
  GameData mode2(fixture.path / "mode2.iso");
  check(mode2.read("RCS.IMG") == pcx, "MODE2 raw sector mapping");
  auto invalidIso = iso;
  put32(invalidIso, 16 * 2048 + 158, 9999);
  write(fixture.path / "extent.iso", invalidIso);
  rejects([&] { GameData malformed(fixture.path / "extent.iso"); },
          "Out-of-bounds ISO root accepted");
  // Tiny malformed file must be rejected as a disc, before allocating its extents.
  write(fixture.path / "bad.iso", Bytes(10));
  rejects([&] { GameData malformed(fixture.path / "bad.iso"); }, "Truncated image accepted");
  std::cout << "Synthetic asset checks passed\n";
}
void originalTests(const std::filesystem::path &path) {
  GameData data(path);
  Archive race(data, "RCS"), menu(data, "A2MRCS");
  check(race.names().size() == 222 && menu.names().size() == 60, "Original archive counts");
  size_t pcx = 0, wav = 0, meshes = 0, vertices = 0, faces = 0;
  for (const auto *archive : {&race, &menu})
    for (const auto &name : archive->names()) {
      if (name.ends_with(".pcx")) {
        auto im = decodePCX(archive->member(name));
        check(im.pixels.size() == size_t(im.width) * im.height, "Decoded PCX pixel count");
        ++pcx;
      } else if (name.ends_with(".wav")) {
        auto s = decodeWAV(archive->member(name));
        check(s.sampleRate == 11025 || s.sampleRate == 22050, "Original sample rate");
        ++wav;
      } else if (name.ends_with(".d3d")) {
        auto m = decodeMesh(archive->member(name));
        vertices += m.vertices.size();
        faces += m.faces.size();
        ++meshes;
      }
    }
  check(pcx == 121 && wav == 98 && meshes == 63 && vertices == 5340 && faces == 7377,
        "Complete original resource coverage");
  auto palette = decodePCX(menu.member("hoofd.pcx")).palette;
  check(decodeSprites(data, "A2MRC", palette).size() == 329, "Original menu sprite count");
  check(decodeSprites(data, "GAMESPR", palette).size() == 332, "Original game sprite count");
  for (int n = 1; n <= 3; ++n) {
    auto t = loadTrack(data, n);
    check(t.road.size() == std::array<size_t, 3>{636, 1020, 1233}[size_t(n - 1)],
          "Original road count");
    check(t.placements.size() == std::array<size_t, 3>{194, 233, 242}[size_t(n - 1)],
          "Original placement count");
    check(t.length() > 10000 && std::isfinite(t.length()), "Track accumulated distance");
  }
  std::filesystem::path iso =
      std::filesystem::is_directory(path) ? path.parent_path() / "CD.iso" : path;
  if (std::filesystem::is_regular_file(iso)) {
    GameData disc(iso);
    for (const auto &filename : {"RCS.IND", "RCS.IMG", "A2MRCS.IMG", "A2MRC.IMG", "GAMESPR.IMG",
                                 "ETAPPE1.TRK", "ETAPPE2.OMP", "A2INTRO.SMK"})
      check(data.read(filename) == disc.read(filename),
            "Image/directory comparison for " + std::string(filename));
    // Exercise full decoding from the raw image, not just byte comparison.
    Archive imageRace(disc, "RCS");
    check(decodeMesh(imageRace.member("porche.d3d")).vertices.size() == 162, "Mesh from image");
    check(loadTrack(disc, 3).road.size() == 1233, "Track from image");
  }
  std::cout << "Original assets passed: 121 PCX, 98 WAV, 63 meshes, 661 sprites, all 3 tracks and "
               "placements\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    unitTests();
    if (argc > 1)
      originalTests(argv[1]);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Asset test failed: " << e.what() << '\n';
    return 1;
  }
}

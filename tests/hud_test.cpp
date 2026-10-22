#include "hud.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void word(Bytes &b, uint16_t n) {
  b.push_back(uint8_t(n));
  b.push_back(uint8_t(n >> 8));
}
void dword(Bytes &b, uint32_t n) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(n >> (i * 8)));
}
void write(const std::filesystem::path &path, const Bytes &b) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
  check(bool(out), "Cannot write HUD fixture");
}
uint32_t color(int sprite) {
  int index = sprite % 253 + 1;
  return 0xff000000u | uint32_t(index << 16) | uint32_t((255 - index) << 8) | 64;
}
struct Fixture {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("a2-hud-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixture(int count = 332) {
    std::filesystem::create_directory(path);
    Bytes pcx(128);
    pcx[0] = 10;
    pcx[1] = 5;
    pcx[2] = 1;
    pcx[3] = 8;
    pcx[65] = 1;
    pcx[8] = 127;
    pcx[9] = 2;
    pcx[10] = 223;
    pcx[11] = 1;
    pcx[66] = 128;
    pcx[67] = 2;
    // Wholly procedural cockpit: open black sky, solid body and an enclosed
    // black instrument. It detects accidental transparency in the interior.
    for (int y = 0; y < 480; ++y)
      for (int x = 0; x < 640; ++x) {
        uint8_t index = y < 300 || (x >= 18 && x <= 22 && y >= 348 && y <= 352) ? 0 : 1;
        pcx.push_back(index);
      }
    pcx.push_back(12);
    for (int i = 0; i < 256; ++i) {
      pcx.push_back(i == 0 ? 0 : uint8_t(i));
      pcx.push_back(i == 0 ? 0 : uint8_t(255 - i));
      pcx.push_back(i == 0 ? 0 : 64);
    }
    Bytes named(26);
    named[0] = 1;
    const std::string name = "SURSCRN.pcx";
    std::copy(name.begin(), name.end(), named.begin() + 2);
    for (const auto *stem : {"RCS", "A2MRCS"}) {
      write(path / (std::string(stem) + ".IND"), named);
      write(path / (std::string(stem) + ".IMG"), pcx);
    }
    Bytes index, blob;
    word(index, uint16_t(count));
    word(blob, uint16_t(count));
    for (int sprite = 0; sprite < count; ++sprite) {
      dword(index, uint32_t(blob.size()));
      word(blob, 3);
      word(blob, 1);
      word(blob, 1);
      word(blob, 0);
      blob.push_back(uint8_t(sprite % 253 + 1));
      blob.push_back(0);
      blob.push_back(255);
    }
    write(path / "GAMESPR.IND", index);
    write(path / "GAMESPR.IMG", blob);
    write(path / "ETAPPE1.TRK", {});
  }
  ~Fixture() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
uint32_t pixel(const Renderer &r, int x, int y) {
  return r.pixels().at(size_t(y) * 640 + x);
}
std::array<Track, 3> tracks() {
  std::array<Track, 3> result;
  for (auto &track : result) {
    track.road.resize(2);
    track.road[1].center = {0, 20000};
    track.distances = {0, 20000};
  }
  return result;
}
void composition() {
  Fixture fixture;
  GameData data(fixture.path);
  Assets assets(data);
  Renderer renderer;
  Hud hud(renderer, assets);
  auto road = tracks();
  Race race(road);
  race.player.speed = 0;
  race.player.fuel = 0;
  race.player.timeLeft = 3723;
  race.player.money = 12345;
  renderer.clear(0xff123456);
  hud.draw(race, 0, false);
  check(pixel(renderer, 300, 100) == 0xff123456, "Windshield must show the world");
  check(pixel(renderer, 20, 350) == 0xff000000, "Enclosed black cockpit must stay opaque");
  check(pixel(renderer, 215, 320) == color(55), "Stopped speedometer frame missing");
  check(pixel(renderer, 216, 320) == 0xff000000, "Black gauge pixel must stay opaque");
  check(pixel(renderer, 217, 320) == color(0), "Sprite key 255 must reveal the cockpit");
  check(pixel(renderer, 292, 328) == color(248), "Empty fuel gauge missing");
  check(pixel(renderer, 65, 24) == color(30) && pixel(renderer, 85, 24) == color(21) &&
            pixel(renderer, 122, 24) == color(30) && pixel(renderer, 143, 24) == color(22) &&
            pixel(renderer, 178, 24) == color(30) && pixel(renderer, 199, 24) == color(23),
        "Remaining time must use original HH:MM:SS and per-glyph hotspots");
  check(pixel(renderer, 145, 54) == color(25), "Money above four digits must not be truncated");
  auto rpm = pixel(renderer, 135, 319);
  race.player.speed = 200;
  race.player.gear = 4;
  race.player.fuel = 100;
  race.player.heading = .5f;
  race.player.steering = 1;
  renderer.clear(0xff123456);
  hud.draw(race, 0, false);
  check(pixel(renderer, 215, 320) != color(55), "Speedometer must respond to speed");
  check(pixel(renderer, 135, 319) != rpm, "Tachometer must respond to engine speed");
  check(pixel(renderer, 292, 328) == color(267), "Fuel gauge must respond to fuel level");
  check(pixel(renderer, 44, 348) != color(82), "Steering wheel must respond to steering");
  race.player.steering = 0;
  renderer.clear(0xff123456);
  hud.draw(race, 0, false);
  check(pixel(renderer, 44, 348) == color(82),
        "Centered wheel must stay centered when the car faces across the road");
  for (int camera : {1, 2}) {
    renderer.clear(0xff123456);
    hud.draw(race, camera, true);
    check(pixel(renderer, 0, 380) == 0xff000000 && pixel(renderer, 119, 479) == 0xff000000,
          "Standalone radar sits at bottom left");
    check(pixel(renderer, 520, 100) == 0xff123456,
          "Standalone radar must leave the upper-right view clear");
  }
  renderer.clear(0xff123456);
  hud.draw(race, 1, false);
  check(pixel(renderer, 484, 358) != 0xff123456, "External camera instruments missing");
  hud.drawPause();
  check(pixel(renderer, 319, 240) == color(331), "Original pause panel missing");
  renderer.clear(0xff123456);
  hud.drawFinish(0, 1);
  check(pixel(renderer, 319, 200) == color(298), "Finish flag uses the original wrap frame and anchor");
  check(pixel(renderer, 320, 200) == 0xff000000 && pixel(renderer, 321, 200) == 0xff123456,
        "Finish flag preserves black pixels and masks palette index255");
  renderer.clear(0xff123456);
  hud.drawFinish(.11f, 1);
  check(pixel(renderer, 319, 200) == color(284), "Finish flag follows the recovered animation order");
  renderer.clear(0xff123456);
  hud.drawFinish(.11f, 0);
  check(pixel(renderer, 319, 160) == color(284), "Cockpit finish flag shifts up forty pixels");
  Race ticket(road);
  while (ticket.status() == RaceStatus::Countdown)
    ticket.update(.25f, {});
  ticket.pickups.clear();
  Vehicle cop;
  cop.kind = VehicleKind::Police;
  cop.policeMode = PoliceMode::Pursuing;
  cop.policeTarget = -1;
  cop.distance = ticket.player.distance + 35;
  cop.lateral = cop.targetLateral = ticket.player.lateral;
  ticket.vehicles = {cop};
  for (int step = 0; step < 60 && ticket.fineDelay() == 0; ++step)
    ticket.update(1.0f / 120, {});
  check(ticket.fineAmount() == 250, "HUD fine fixture must catch the player");
  renderer.clear(0xff123456);
  hud.draw(ticket, 1, false);
  check(pixel(renderer, 160, 100) == color(301), "Original ticket panel placement missing");
  check(pixel(renderer, 401, 230) == color(303) &&
            pixel(renderer, 413, 230) == color(306) && pixel(renderer, 425, 230) == color(53),
        "Fine amount uses original blue digit mapping and hotspots");
  renderer.clear(0xff123456);
  hud.draw(ticket, 0, false);
  check(pixel(renderer, 160, 60) == color(301), "Cockpit ticket shifts up by forty pixels");
}
void fallback() {
  Fixture fixture(2);
  GameData data(fixture.path);
  Assets assets(data);
  Renderer renderer;
  Hud hud(renderer, assets);
  auto road = tracks();
  Race race(road);
  hud.draw(race, 0, true);
  hud.draw(race, 1, false);
  hud.drawPause();
}
void original(const std::filesystem::path &path) {
  GameData data(path);
  Assets assets(data);
  Renderer renderer;
  Hud hud(renderer, assets);
  auto road = tracks();
  Race race(road);
  race.player.speed = race.player.fuel = 0;
  renderer.clear(0xff123456);
  hud.draw(race, 0, false);
  check(pixel(renderer, 300, 100) == 0xff123456, "Original windshield mask failed");
  auto stopped = renderer.pixels();
  race.player.speed = 200;
  race.player.gear = 4;
  race.player.fuel = 100;
  renderer.clear(0xff123456);
  hud.draw(race, 0, false);
  auto changed = [&](int left, int top, int width, int height) {
    int count = 0;
    for (int y = top; y < top + height; ++y)
      for (int x = left; x < left + width; ++x)
        count += stopped[size_t(y) * 640 + x] != pixel(renderer, x, y);
    return count;
  };
  check(changed(215, 320, 71, 69) > 20, "Original speedometer did not animate");
  check(changed(135, 319, 70, 68) > 20, "Original tachometer did not animate");
  check(changed(292, 328, 48, 49) > 10, "Original fuel gauge did not animate");
  std::cout << "Original CD HUD integration passed\n";
}
} // namespace
int main(int argc, char **argv) {
  try {
    composition();
    fallback();
    if (argc > 1)
      original(argv[1]);
    std::cout
        << "HUD checks passed: original layout, opacity, gauges, steering, counters, fallback\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "HUD test failed: " << e.what() << '\n';
    return 1;
  }
}

#include "hud.hpp"
#include <cstdio>

namespace a2 {
namespace {
constexpr std::array<size_t, 10> digits{30, 21, 22, 23, 24, 25, 26, 27, 28, 29};
constexpr std::array<size_t, 6> positions{247, 236, 237, 238, 239, 240};
size_t speedFrame(int frame, bool cockpit) {
  if (cockpit)
    return size_t(frame < 20 ? 55 + frame : 216 + frame - 20);
  return size_t(frame < 20 ? 115 + frame : 196 + frame - 20);
}
// Only the black region connected to the top of SURSCRN is the windshield.
// Gauge interiors, the radar screen and the lower cockpit must remain opaque.
Image cockpitMask(const Image &source) {
  Image result = source;
  if (result.indices.size() != result.pixels.size() || result.width <= 0)
    return result;
  std::vector<size_t> queue;
  auto visit = [&](size_t p) {
    if (result.indices[p] == 0) {
      result.indices[p] = 255;
      queue.push_back(p);
    }
  };
  for (int x = 0; x < result.width; ++x)
    visit(size_t(x));
  for (size_t i = 0; i < queue.size(); ++i) {
    size_t p = queue[i], x = p % size_t(result.width);
    if (x)
      visit(p - 1);
    if (x + 1 < size_t(result.width))
      visit(p + 1);
    if (p >= size_t(result.width))
      visit(p - size_t(result.width));
    if (p + size_t(result.width) < result.indices.size())
      visit(p + size_t(result.width));
  }
  return result;
}
} // namespace

Hud::Hud(Renderer &renderer, Assets &assets)
    : renderer_(renderer), assets_(assets), sprites_(assets.sprites("GAMESPR")),
      original_(sprites_.size() >= 332) {
  if (original_)
    cockpit_ = cockpitMask(assets.image("SURSCRN.pcx"));
}
void Hud::at(size_t sprite, int x, int y) {
  renderer_.blitKey(sprites_.at(sprite).image, x, y, 255);
}
void Hud::anchor(size_t sprite, int x, int y) {
  renderer_.sprite(sprites_.at(sprite), x, y, 255);
}
void Hud::instruments(const Player &p, int camera) {
  // Frames and positions are recovered from A2SPEL 0x409275..0x409c83.
  // Their face spans 0..250 km/h. The new simulation supplies km/h directly;
  // the original native velocity and engine RPM use different internal units.
  int speed = std::clamp(int(std::round(std::abs(p.speed) / 6.25f)), 0, 39);
  int fuel = std::clamp(int(p.fuel * 19 / 100), 0, 19);
  if (camera == 0) {
    renderer_.blitKey(cockpit_, 0, 0, 255);
    at(0, 0, 267);
    int steering = std::clamp(7 + int(std::round(p.steering * 7)), 0, 14);
    at(size_t(75 + steering), 44, 348);
    at(size_t(248 + fuel), 292, 328);
    at(speedFrame(speed, true), 215, 320);
    // An idle and gear-dependent engine speed drives the recovered RPM frames.
    // This is a reconstruction until original engine dynamics are recovered.
    float rpm = 1000 + 6000 * p.speed / (48 * std::max(1, p.gear));
    int tachometer = std::clamp(int(rpm * 19 / 7000), 0, 19);
    at(size_t(90 + tachometer), 135, 319);
  } else {
    at(speedFrame(speed, false), 484, 358);
    at(size_t(176 + fuel), 432, 403);
  }
}
void Hud::counters(const Race &race) {
  // Original 640x480 label coordinates and digit anchors, 0x409383..0x4099be.
  at(20, 16, 16);
  int time = std::max(0, int(std::ceil(race.player.timeLeft)));
  int hours = time / 3600 % 100, minutes = time / 60 % 60, seconds = time % 60;
  const int values[]{hours / 10,   hours % 10,   minutes / 10,
                     minutes % 10, seconds / 10, seconds % 10};
  const int x[]{66, 86, 123, 144, 179, 200};
  for (int i = 0; i < 6; ++i)
    anchor(digits[size_t(values[i])], x[i], 24);
  at(325, 16, 46);
  auto money = std::to_string(std::max(0, race.player.money));
  if (money.size() < 4)
    money.insert(0, 4 - money.size(), '0');
  for (size_t i = 0; i < money.size(); ++i)
    anchor(digits[size_t(money[i] - '0')], 66 + int(i) * 20, 54);
  at(241, 508, 15);
  anchor(positions[size_t(std::clamp(race.position(), 1, 6) - 1)], 590, 25);
}
void Hud::radar(const Race &race, int camera) {
  // The original cockpit already includes the VDsys screen and its housing.
  // Without the cockpit housing, the standalone radar belongs at bottom left.
  int left = camera == 0 ? 459 : 0, top = camera == 0 ? 377 : 380;
  int width = 120, height = camera == 0 ? 60 : 100;
  renderer_.rect(left, top, width, height, 0xff000000);
  int cx = left + width / 2, cy = top + height * 3 / 4;
  for (const auto &vehicle : race.vehicles) {
    float delta = vehicle.distance - race.player.distance;
    int x = cx + int((vehicle.lateral - race.player.lateral) * .2f);
    int y = cy - int(delta * .08f);
    if (x < left + 2 || x >= left + width - 2 || y < top + 2 || y >= top + height - 2)
      continue;
    uint32_t color = vehicle.kind == VehicleKind::Police ? 0xffff4040 : 0xffdddddd;
    renderer_.rect(x - 1, y - 1, 3, 3, color);
  }
  renderer_.rect(cx - 1, cy - 1, 3, 3, 0xffffffff);
}
void Hud::fallback(const Race &race, int camera) {
  // The public procedural fixtures deliberately contain only two sprites.
  if (camera == 0)
    renderer_.blit(assets_.image("SURSCRN.pcx"), 0, 0, 640, 480, true);
  char text[120];
  std::snprintf(text, sizeof(text), "TIJD %02d:%02d  GELD %d  POSITIE %d/6",
                std::max(0, int(std::ceil(race.player.timeLeft))) / 60,
                std::max(0, int(std::ceil(race.player.timeLeft))) % 60, race.player.money, race.position());
  renderer_.text(16, 16, text, 0xffffdd55);
  std::snprintf(text, sizeof(text), "%03d KM/H  BENZINE %d%%", int(race.player.speed),
                int(race.player.fuel));
  renderer_.text(16, 455, text, 0xffffdd55, 2);
}
void Hud::ticket(const Race &race, int camera) {
  if (original_) {
    // A2SPEL 0x4123a3..0x412423: large ticket (animation22), blue
    // amount glyphs (animation28), 12-unit spacing. Cockpit shifts it up40.
    const int top = camera == 0 ? 60 : 100;
    at(301, 160, top);
    constexpr size_t blueDigits[]{53, 300, 303, 304, 305, 306, 307, 308, 309, 310};
    int amount = std::clamp(race.fineAmount(), 0, 9999);
    const int values[]{amount / 1000, amount / 100 % 10, amount / 10 % 10, amount % 10};
    for (int i = values[0] == 0 ? 1 : 0; i < 4; ++i)
      anchor(blueDigits[values[i]], 390 + i * 12, top + 130);
  } else {
    renderer_.rect(160, 100, 320, 194, 0xffd7c66c);
    renderer_.text(188, 120, "PROCES VERBAAL", 0xff55518b, 2);
    renderer_.text(188, 196, "BOETE IN FL: " + std::to_string(race.fineAmount()), 0xff55518b);
    renderer_.text(180, 270, "ENTER=TERUG", 0xffff3020, 2);
  }
}
void Hud::draw(const Race &race, int camera, bool showRadar) {
  if (original_) {
    instruments(race.player, camera);
    counters(race);
  } else {
    fallback(race, camera);
  }
  if (showRadar)
    radar(race, camera);
  if (race.status() == RaceStatus::Countdown && race.introCountdown() == 0) {
    if (original_) {
      // Original animation 18: unlit, then three red stages, then green.
      constexpr size_t lights[]{51, 43, 44, 45, 46};
      int step = std::clamp(4 - int(std::ceil(race.countdown())), 1, 3);
      anchor(lights[size_t(step)], 320, 160);
    } else {
      renderer_.text(302, 165, std::to_string(std::max(1, int(std::ceil(race.countdown())))),
                     0xffffdb4d, 8);
    }
  } else if (original_ && race.status() == RaceStatus::Running && race.player.elapsed < .8f &&
             race.fineDelay() == 0) {
    anchor(46, 320, 160);
  }
  if (race.fineDelay() > 0) {
    ticket(race, camera);
  }
}
void Hud::drawFinish(float presentationSeconds, int camera) {
  if (!original_)
    return;
  // A2SPEL 0x41166b: animation19 in 640x480 mode, with its native
  // hotspots at (320,200), or y160 over the cockpit. Table0x458788
  // contains one-based sprite IDs in this order, including the wrap frame.
  constexpr size_t frames[]{298, 284, 285, 286, 287, 288, 289, 290,
                            291, 292, 293, 294, 295, 296, 297, 283};
  // The original increments once per main loop; ten frames per second is
  // a stable presentation choice rather than a recovered wall-clock rate.
  const float time = std::isfinite(presentationSeconds) ? std::max(0.0f, presentationSeconds) : 0;
  const size_t frame = size_t(std::fmod(time, float(std::size(frames)) / 10) * 10);
  anchor(frames[frame], 320, camera == 0 ? 160 : 200);
}
void Hud::drawPause() {
  renderer_.dim(.45f);
  if (original_)
    anchor(331, 320, 240);
  else
    renderer_.text(232, 200, "PAUZE", 0xffffd35a, 5);
}
} // namespace a2

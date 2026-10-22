#include "simulation.hpp"
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
Track road(int points, float spacing) {
  Track result;
  for (int i = 0; i < points; ++i) {
    RoadRecord r;
    r.center = {0, i * spacing};
    result.road.push_back(r);
    result.distances.push_back(r.center.y);
  }
  return result;
}
void finishLines() {
  const std::array<Track, 3> tracks{road(101, 80), road(121, 90), road(141, 100)};
  for (int stage = 0; stage < 6; ++stage) {
    Race race(tracks);
    race.start(stage);
    const auto &t = race.track();
    const size_t sourceChunk = race.reverse() ? 15 : t.road.size() - 15;
    const float expected = race.reverse() ? t.length() - t.distances[sourceChunk]
                                          : t.distances[sourceChunk];
    check(std::abs(race.finishDistance() - expected) < .001f,
          "Stage finish must match the original gantry source chunk");
    const auto finishPose = race.pose(race.finishDistance());
    const auto sourcePose = sampleTrack(t, t.distances[sourceChunk]);
    check(length(finishPose.position - sourcePose.position) < .001f,
          "Finish distance maps to the gantry on forward and return stages");
    while (race.status() == RaceStatus::Countdown)
      race.update(.25f, {});
    race.vehicles.clear();
    race.pickups.clear();
    race.roadProps.clear();
    race.takeEvents();
    race.player.distance = expected - 1;
    race.player.speed = 0;
    race.update(1.f / 120, {});
    check(race.status() == RaceStatus::Running, "Race must continue immediately before FINISH");
    race.player.speed = 120;
    race.update(1.f / 120, {});
    check(race.status() == RaceStatus::Finished, "Crossing FINISH must finish the stage");
    const int money = race.player.money;
    const float distance = race.player.distance;
    auto events = race.takeEvents();
    int finishSounds = 0;
    for (const auto &event : events)
      finishSounds += event.sound == "finish.wav";
    check(finishSounds == 1, "Finishing emits exactly one finish sound");
    for (int frame = 0; frame < 120; ++frame)
      race.update(1.f / 120, {1, 0, 0});
    check(race.player.distance == distance && race.player.money == money &&
              race.takeEvents().empty(),
          "Presentation updates cannot move a finished race or award it again");
  }
  const std::array<Track, 3> small{road(2, 20000), road(2, 20000), road(2, 20000)};
  Race fixture(small);
  for (int stage = 0; stage < 6; ++stage) {
    fixture.start(stage);
    check(fixture.finishDistance() == 19920, "Short procedural fixtures preserve their finish");
  }
}
} // namespace
int main() {
  try {
    finishLines();
    std::cout << "Finish gantry crossings and single award passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

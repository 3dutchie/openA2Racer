#include "scene.hpp"
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
Track straight(float distance) {
  Track t;
  for (int i = 0; i <= 100; ++i) {
    RoadRecord r;
    r.center = {0, distance * i / 100};
    t.road.push_back(r);
    t.distances.push_back(r.center.y);
  }
  return t;
}
std::string snapshot(const Race &race) {
  std::ostringstream out;
  race.save(out);
  return out.str();
}
void running(Race &race) {
  for (int n = 0; n < 721; ++n)
    race.update(1.0f / 120, {});
  check(race.status() == RaceStatus::Running, "Countdown must complete");
  race.vehicles.clear();
  race.pickups.clear();
  race.takeEvents();
}
void generation(const std::array<Track, 3> &tracks) {
  for (int stage = 0; stage < 6; ++stage) {
    Race race(tracks);
    race.start(stage);
    check(race.roadProps.size() == 40 || race.roadProps.size() == 50,
          "Original roadworks contain four/five ten-post zones");
    for (size_t i = 0; i < race.roadProps.size(); ++i) {
      const auto &post = race.roadProps[i];
      const int span = int(race.track().road.size() / 20);
      const int center = int(span * (1 + 1.5f * float(i / 10)));
      const int chunk = center + (race.reverse() ? -int(i % 10) : int(i % 10));
      const float sourceDistance = race.track().distances[size_t(std::clamp(chunk, 0, 100))] + 40;
      const float expected = race.reverse() ? race.track().length() - sourceDistance : sourceDistance;
      check(std::abs(post.distance - expected) < .001f,
            "Roadworks follow recovered zone spacing and reversed source-chunk direction");
      check((post.lateral == 85 || post.lateral == 108) && post.height == 0 &&
                post.state == RoadPropState::Standing,
            "Delineators stand on the original shoulder lanes");
      const auto source = sampleTrack(race.track(), sourceDistance,
                                      race.reverse() ? -post.lateral : post.lateral);
      check(length(race.pose(post.distance, post.lateral).position - source.position) < .001f,
            "Return-stage props sit on the opposite source carriageway");
    }
    const auto first = snapshot(race);
    race.start(stage);
    check(snapshot(race) == first, "Stage restart restores the same pristine roadworks");
  }
  check(std::string(stageName(0)) == "AMSTERDAM - VIANEN" &&
            std::string(stageName(1)) == "VIANEN - EINDHOVEN" &&
            std::string(stageName(5)) == "VIANEN - AMSTERDAM",
        "Stage names match the original route cards");
}
void ranking(const std::array<Track, 3> &tracks) {
  Race race(tracks);
  race.player.distance = 1000;
  race.player.car = 3;
  race.vehicles.clear();
  for (int car = 0; car < 6; ++car) {
    if (car == race.player.car)
      continue;
    Vehicle v;
    v.car = car;
    v.kind = VehicleKind::Rival;
    v.distance = 1000;
    race.vehicles.push_back(v);
  }
  Vehicle traffic;
  traffic.distance = 9000;
  race.vehicles.push_back(traffic);
  traffic.kind = VehicleKind::Police;
  race.vehicles.push_back(traffic);
  std::set<int> ranks{race.position()};
  check(race.position() == 4, "Equal-distance rivals use stable car-order ranking");
  for (size_t i = 0; i < 5; ++i) {
    check(race.rivalPosition(i) == race.vehicles[i].car + 1,
          "Tied position markers agree with the player's HUD rank");
    ranks.insert(race.rivalPosition(i));
  }
  check(ranks == std::set<int>({1, 2, 3, 4, 5, 6}), "Six racers have six distinct ranks");
  check(race.rivalPosition(5) == 0 && race.rivalPosition(6) == 0 &&
            race.rivalPosition(999) == 0,
        "Traffic and police have no race position markers");
  race.vehicles[4].distance = 2000;
  check(race.rivalPosition(4) == 1 && race.position() == 5,
        "Overtaking changes marker and player rank immediately");
}
void impactsAndSaves(const std::array<Track, 3> &tracks) {
  for (int stage : {0, 3}) {
    Race race(tracks);
    race.start(stage);
    running(race);
    race.roadProps = {RoadProp{}};
    race.player.distance = 1000;
    race.player.lateral = 85;
    race.player.speed = 200;
    race.roadProps[0].distance = 1030;
    race.roadProps[0].lateral = 85;
    for (int i = 0; i < 6; ++i)
      race.update(1.0f / 120, {});
    check(race.roadProps[0].state == RoadPropState::Flying && race.roadProps[0].height > 0 &&
              race.roadProps[0].forwardVelocity > 0 && race.roadProps[0].pitch != 0,
          "Hitting a delineator launches and tumbles it on both route directions");
    auto sounds = race.takeEvents();
    size_t bumps = 0;
    for (const auto &e : sounds)
      bumps += e.sound == "bump.wav";
    check(bumps == 1, "A post produces one impact effect");
    const auto save = snapshot(race);
    Race restored(tracks);
    std::istringstream in(save);
    restored.load(in);
    check(snapshot(restored) == save, "An airborne post round-trips all motion and rotation");
    for (int i = 0; i < 240; ++i) {
      race.update(1.0f / 120, {});
      restored.update(1.0f / 120, {});
    }
    check(snapshot(race) == snapshot(restored), "Airborne save continuation is deterministic");
    check(race.roadProps[0].state == RoadPropState::Hidden && race.roadProps[0].height == 0,
          "The original delineator disappears after landing");
    race.player.distance = race.roadProps[0].distance;
    race.player.lateral = race.roadProps[0].lateral;
    race.player.speed = 100;
    race.takeEvents();
    race.update(1.0f / 120, {});
    for (const auto &e : race.takeEvents())
      check(e.sound != "bump.wav", "A removed delineator cannot be hit repeatedly");
  }
  Race npc(tracks);
  running(npc);
  npc.roadProps = {RoadProp{}};
  npc.roadProps[0].distance = 3000;
  npc.roadProps[0].lateral = -85;
  Vehicle v;
  v.distance = 3020;
  v.lateral = v.targetLateral = -85;
  v.speed = v.targetSpeed = 180;
  v.oncoming = true;
  npc.vehicles.push_back(v);
  for (int i = 0; i < 5; ++i)
    npc.update(1.0f / 120, {});
  check(npc.roadProps[0].state == RoadPropState::Flying &&
            npc.roadProps[0].forwardVelocity < 0,
        "Oncoming vehicles knock posts in their own direction");

  // Strip only the extension added after the original v1 event list. The rest
  // of the save and its route fingerprints remain the original v1 schema.
  Race original(tracks);
  original.start(4);
  running(original);
  original.player.money = 12345;
  original.player.distance = 1500;
  original.roadProps.clear();
  std::string legacy = snapshot(original);
  legacy.replace(13, 1, "1");
  check(legacy.ends_with("0\n0 0\n0\n0\nEND\n"), "Migration fixture ends in empty v2/v3/v4 extensions");
  legacy.erase(legacy.size() - 14, 10);
  Race restored(tracks);
  std::istringstream old(legacy);
  restored.load(old);
  check(restored.stage() == 4 && restored.player.money == 12345 &&
            restored.player.distance == 1500 && restored.vehicles.empty() &&
            restored.pickups.empty() && !restored.roadProps.empty(),
        "Version1 saves migrate to standing work zones without resetting the race");
  original.roadProps = restored.roadProps;
  check(snapshot(original) == snapshot(restored), "Migration preserves every prior field and RNG");

  std::string corrupt = snapshot(restored);
  const auto extension = corrupt.rfind("\n0 0\n0\n0\nEND\n");
  check(extension != std::string::npos, "Locate the v3 extension after the last serialized prop");
  const auto last = corrupt.rfind('\n', extension - 1);
  check(last != std::string::npos, "Locate the last serialized prop");
  corrupt.replace(last + 1, extension - last - 1, "nan 85 0 0 0 0 0 0 0 0");
  const auto before = snapshot(restored);
  bool rejected = false;
  try {
    std::istringstream bad(corrupt);
    restored.load(bad);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && snapshot(restored) == before, "Invalid prop states reject the whole save atomically");
}
void originalAssets(const char *path) {
  GameData data(path);
  Assets assets(data);
  std::array<Track, 3> tracks{loadTrack(data, 1), loadTrack(data, 2), loadTrack(data, 3)};
  // Remove fixed bridges/gantries so these visibility assertions measure the
  // dynamic props and ranks rather than their correct occlusion by scenery.
  for (auto &track : tracks)
    track.placements.clear();
  const auto &post = assets.mesh("werkweg2.d3d");
  check(!post.vertices.empty() && !post.faces.empty(), "Original delineator model decodes");
  for (int rank = 1; rank <= 6; ++rank) {
    const auto stem = "number" + std::to_string(rank);
    check(!assets.mesh(stem + ".d3d").vertices.empty() &&
              !assets.image(stem + ".pcx").pixels.empty(),
          "Each original red numeral model and texture decodes");
  }
  Renderer renderer;
  Scene scene(renderer, assets);
  Race race(tracks);
  for (bool enhanced : {false, true}) {
    race.start();
    running(race);
    race.vehicles.clear();
    race.pickups.clear();
    race.player.distance = race.roadProps[0].distance - 150;
    renderer.setEnhanced(enhanced);
    for (int view : {0, 1, 2}) {
      race.roadProps[0].state = RoadPropState::Standing;
      scene.draw(race, view, false);
      const auto standing = renderer.pixels();
      race.roadProps[0].state = RoadPropState::Hidden;
      scene.draw(race, view, false);
      check(renderer.pixels() != standing, "Original delineators render in camera " +
                                              std::to_string(view) + " enhanced " + std::to_string(enhanced));
    }
    // Change the nearby rival's rank using an offscreen racer. The primary
    // car and all visible geometry stay fixed, isolating the numeral change.
    race.roadProps.clear();
    race.vehicles.clear();
    Vehicle rival;
    rival.kind = VehicleKind::Rival;
    rival.car = 0;
    rival.distance = race.player.distance + 180;
    rival.lateral = rival.targetLateral = 30;
    race.vehicles.push_back(rival);
    for (int view : {0, 1, 2}) {
      scene.draw(race, view, false);
      const auto visible = renderer.pixels();
      race.vehicles.push_back(rival);
      race.vehicles[1].distance = race.track().length() - 100;
      scene.draw(race, view, false);
      // Exclude the top HUD and lower cockpit/gauges, including the player's
      // own external-view marker. A HUD position change alone must not pass.
      const int scale = renderer.pixelWidth() / 640;
      size_t numeralPixels = 0;
      for (int y = 80 * scale; y < 260 * scale; ++y)
        for (int x = 0; x < renderer.pixelWidth(); ++x) {
          const size_t pixel = size_t(y) * renderer.pixelWidth() + x;
          numeralPixels += renderer.pixels()[pixel] != visible[pixel];
        }
      check(numeralPixels > 0, "Rival rank markers update in each camera and graphics mode");
      race.vehicles.pop_back();
    }
    // Camera1 without dashboard uses the standalone HUD rather than the
    // cockpit housing. Radar changes must remain within the bottom-left panel.
    scene.draw(race, 0, false, true, false);
    const auto noRadar = renderer.pixels();
    scene.draw(race, 0, true, true, false);
    const int scale = renderer.pixelWidth() / 640;
    size_t radarPixels = 0;
    for (int y = 0; y < renderer.pixelHeight(); ++y)
      for (int x = 0; x < renderer.pixelWidth(); ++x) {
        const auto pixel = size_t(y) * renderer.pixelWidth() + x;
        if (renderer.pixels()[pixel] == noRadar[pixel])
          continue;
        check(x < 120 * scale && y >= 380 * scale,
              "Dashboard-off cockpit radar must stay at bottom left in both graphics modes");
        ++radarPixels;
      }
    check(radarPixels > 100, "Dashboard-off radar must actually render");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::array<Track, 3> tracks{straight(20000), straight(25000), straight(30000)};
    generation(tracks);
    ranking(tracks);
    impactsAndSaves(tracks);
    if (argc > 1)
      originalAssets(argv[1]);
    std::cout << "Roadworks placement, impacts, ranks and save migration passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Race details failed: " << e.what() << '\n';
    return 1;
  }
}

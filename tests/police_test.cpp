#include "simulation.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
Track straight() {
  Track track;
  for (int i = 0; i <= 500; ++i) {
    RoadRecord road;
    road.center = {0, float(i * 200)};
    track.road.push_back(road);
    track.distances.push_back(float(i * 200));
  }
  return track;
}
void run(Race &race, float seconds) {
  for (int i = 0; i < int(seconds * 120); ++i)
    race.update(1.0f / 120, {});
}
Vehicle police(float distance, float lateral = 108) {
  Vehicle result;
  result.kind = VehicleKind::Police;
  result.distance = distance;
  result.lateral = result.targetLateral = lateral;
  result.targetSpeed = 255;
  return result;
}
} // namespace
int main() {
  try {
    std::array<Track, 3> tracks{straight(), straight(), straight()};
    constexpr int counts[]{10, 14, 20, 25, 17, 15};
    for (int stage = 0; stage < 6; ++stage) {
      Race race(tracks);
      race.start(stage);
      int count = 0;
      for (const auto &v : race.vehicles)
        if (v.kind == VehicleKind::Police) {
          ++count;
          check(v.speed == 0 && (v.lateral == 85 || v.lateral == 108) &&
                    v.policeMode == PoliceMode::Parked,
                "police start at rest in recovered verge/shoulder lane slots");
        }
      check(count == counts[stage], "original INI police count for each stage");
    }
    Race race(tracks);
    run(race, 6.1f);
    race.pickups.clear();
    race.vehicles = {police(5000)};
    race.player.distance = 4500;
    race.player.speed = 160;
    run(race, 0.1f);
    check(race.vehicles[0].policeMode == PoliceMode::Parked &&
              race.vehicles[0].distance == 5000,
          "stake-out waits until a speeding racer passes it");
    race.player.distance = 5100;
    run(race, 0.1f);
    check(race.vehicles[0].policeMode == PoliceMode::Intercepting &&
              race.vehicles[0].policeTarget == -1 && race.vehicles[0].lateral < 108 &&
              race.vehicles[0].targetLateral == 60 && race.vehicles[0].speed > 0,
          "police pull out from shoulder behind a speeding player");
    race.vehicles = {police(5000)};
    Vehicle rival;
    rival.kind = VehicleKind::Rival;
    rival.distance = 5200;
    rival.lateral = rival.targetLateral = 60;
    rival.speed = rival.targetSpeed = 180;
    race.vehicles.push_back(rival);
    race.player.distance = 4500;
    race.player.speed = 0;
    run(race, 0.1f);
    check(race.vehicles[0].policeTarget == 1 &&
              race.vehicles[0].policeMode == PoliceMode::Intercepting,
          "roadside police can intercept a rival ahead of the player");
    std::ostringstream snapshot;
    race.save(snapshot);
    Race restored(tracks);
    std::istringstream saved(snapshot.str());
    restored.load(saved);
    check(restored.vehicles[0].policeTarget == 1 &&
              restored.vehicles[0].policeMode == PoliceMode::Intercepting,
          "save preserves police target and pull-out state");
    std::vector<std::string> legacyLines;
    std::istringstream lines(snapshot.str());
    std::string line;
    while (std::getline(lines, line))
      legacyLines.push_back(line);
    legacyLines.front() = "A2RACER_SAVE 2";
    legacyLines.erase(legacyLines.end() - std::ptrdiff_t(race.vehicles.size()) - 4,
                      legacyLines.end() - 1);
    std::string legacy;
    for (const auto &row : legacyLines)
      legacy += row + '\n';
    std::istringstream oldSave(legacy);
    restored.load(oldSave);
    check(restored.vehicles[0].distance == race.vehicles[0].distance &&
              restored.vehicles[0].speed == race.vehicles[0].speed &&
              restored.vehicles[0].policeMode == PoliceMode::Pursuing &&
              restored.vehicles[0].policeTarget == -1,
          "version2 saves preserve cruising police and migrate to an active pursuit");
    constexpr int fines[]{250, 500, 800, 1200, 1500, 2000, 2000};
    race.player.money = 10000;
    for (int fine : fines) {
      race.player.distance = 5000;
      race.player.lateral = 60;
      race.player.speed = 160;
      auto cop = police(5030, 60);
      cop.policeMode = PoliceMode::Pursuing;
      cop.policeTarget = -1;
      cop.speed = 160;
      race.vehicles = {cop};
      int before = race.player.money;
      run(race, 0.4f);
      check(race.fineAmount() == fine && race.player.money == before - fine &&
                race.fineDelay() > 0 && race.player.speed == 0,
            "caught player receives the original escalating fine table");
      float distance = race.player.distance, time = race.player.timeLeft;
      std::ostringstream pending;
      race.save(pending);
      Race loadedTicket(tracks);
      std::istringstream pendingInput(pending.str());
      loadedTicket.load(pendingInput);
      std::ostringstream reserialized;
      loadedTicket.save(reserialized);
      check(pending.str() == reserialized.str() && loadedTicket.fineAmount() == fine,
            "pending ticket, escalation and police return state round-trip exactly");
      run(race, 5);
      check(race.fineDelay() > 0 && race.player.distance == distance && race.player.timeLeft == time,
            "police ticket holds the race until explicitly dismissed");
      race.dismissFine();
      check(race.fineDelay() == 0, "ticket dismissal releases race");
    }
    race.vehicles = {police(5000, 60)};
    race.vehicles[0].policeMode = PoliceMode::Returning;
    race.vehicles[0].policeTimer = 7;
    race.vehicles[0].speed = 50;
    race.player.distance = 10000;
    race.player.speed = 0;
    run(race, 9);
    check(race.vehicles[0].policeMode == PoliceMode::Parked &&
              race.vehicles[0].speed == 0 && race.vehicles[0].lateral == 108,
          "completed interception returns police to a stationary shoulder stake-out");
    std::cout << "Police checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

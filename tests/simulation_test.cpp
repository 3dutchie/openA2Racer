#include "simulation.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace a2;
static void check(bool v, const char *text) {
  if (!v)
    throw std::runtime_error(text);
}
static Track straight(float size) {
  Track t;
  for (int i = 0; i <= 100; ++i) {
    RoadRecord r;
    r.center = {0, size * i / 100};
    t.road.push_back(r);
    t.distances.push_back(size * i / 100);
  }
  return t;
}
static void running(Race &r) {
  for (int n = 0; n < 721; ++n)
    r.update(1.0f / 120, {});
  check(r.status() == RaceStatus::Running, "countdown transition");
}
int main() {
  try {
    std::array<Track, 3> tracks{straight(20000), straight(25000), straight(30000)};
    Race sequence(tracks);
    check(sequence.introCountdown() == 3 && sequence.player.timeLeft == 180,
          "race begins with the camera phase and original three-minute timer");
    for (int i = 0; i < 361; ++i)
      sequence.update(1.0f / 120, {1, 0, 1, false});
    check(sequence.status() == RaceStatus::Countdown && sequence.introCountdown() == 0 &&
              sequence.player.speed == 0 && sequence.player.timeLeft == 180,
          "camera descent ends before start lights and racing remain frozen");
    constexpr float initialTimes[]{180, 300, 540, 540, 300, 180};
    for (int stage = 0; stage < 6; ++stage) {
      sequence.start(stage);
      check(sequence.player.timeLeft == initialTimes[stage] && sequence.position() == 6,
            "six stages use original time limits and six-car starting grid");
    }
    auto p = sampleTrack(tracks[0], 1000, 30);
    check(std::abs(p.position.x - 30) < 0.01f && std::abs(p.position.z - 1000) < 0.01f,
          "road sampling");
    check(std::abs(dot(p.forward, p.right)) < 0.0001f, "orthogonal road frame");
    auto hill = tracks[0];
    hill.road[5].fields[0] = 1234; // Decoy: the first word is not elevation.
    hill.road[6].fields[0] = 5678;
    hill.road[5].fields[1] = 4;
    hill.road[6].fields[1] = 12;
    check(std::abs(sampleTrack(hill, 1100).position.y - 8) < 0.0001f,
          "second TRK integer supplies interpolated road elevation");
    hill.road[5].fields[1] = uint32_t(-4);
    hill.road[6].fields[1] = uint32_t(-12);
    check(std::abs(sampleTrack(hill, 1100).position.y + 8) < 0.0001f,
          "TRK elevation remains signed");
    for (int stage : {0, 3})
      for (float side : {-1.0f, 1.0f}) {
        Race barrier(tracks);
        barrier.start(stage);
        running(barrier);
        barrier.vehicles.clear();
        barrier.pickups.clear();
        barrier.player.lateral = side * 10.1f;
        barrier.player.speed = 160;
        barrier.player.heading = -side * 0.6f;
        barrier.update(1.0f / 120, {0, 0, -side, false});
        check(std::abs(barrier.player.lateral - side * 10) < 0.001f,
              "median rail stops the car with half-car clearance");
        check(side * barrier.player.heading > 0 && barrier.player.speed < 130,
              "median impact reflects heading and slows the car");
        for (int step = 0; step < 240; ++step) {
          barrier.update(1.0f / 120, {1, 0, -side, false});
          check(side * barrier.player.lateral >= 10,
                "continued steering cannot cross either median rail");
        }
        barrier.start(stage);
        running(barrier);
        barrier.vehicles.clear();
        barrier.pickups.clear();
        barrier.player.lateral = side * 30;
        barrier.player.speed = 160;
        const float before = barrier.player.distance;
        barrier.update(1.0f / 120, {});
        check(barrier.player.lateral == side * 30 && barrier.player.speed > 159 &&
                  barrier.player.distance > before,
              "median collision leaves ordinary driving unaffected");
      }
    // Steering centers the wheels in the original, not the world direction.
    // Releasing a turn must retain that direction, including through bends.
    for (int stage : {0, 3}) {
      Race steering(tracks);
      steering.start(stage);
      running(steering);
      steering.vehicles.clear();
      steering.pickups.clear();
      steering.roadProps.clear();
      steering.player.speed = 100;
      steering.player.heading = .05f;
      const float lateral = steering.player.lateral;
      steering.update(.1f, {});
      check(std::abs(steering.player.heading - .05f) < .00001f &&
                steering.player.lateral > lateral,
            "releasing steering preserves direction and lateral travel");
      steering.player.speed = 0;
      steering.update(.1f, {0, 0, 1, false});
      check(std::abs(steering.player.heading - .05f) < .00001f,
            "steering cannot rotate a stationary car");
    }
    for (int stage : {0, 3})
      for (float direction : {-1.0f, 1.0f}) {
        Race wheel(tracks);
        wheel.start(stage);
        running(wheel);
        wheel.vehicles.clear();
        wheel.pickups.clear();
        wheel.roadProps.clear();
        wheel.player.lateral = 45;
        wheel.player.speed = 80;
        wheel.update(.15f, {0, 0, direction, false});
        check(direction * wheel.player.steering > .99f && direction * wheel.player.heading > 0,
              "wheel input ramps toward the requested turn in both directions");
        wheel.update(.08f, {});
        check(direction * wheel.player.steering > 0 && direction * wheel.player.steering < .6f,
              "released steering returns smoothly toward center");
        std::ostringstream savedWheel;
        wheel.save(savedWheel);
        Race restoredWheel(tracks);
        std::istringstream wheelInput(savedWheel.str());
        restoredWheel.load(wheelInput);
        check(restoredWheel.player.steering == wheel.player.steering,
              "snapshot preserves a partially returning wheel");
        wheel.update(.2f, {});
        restoredWheel.update(.2f, {});
        check(wheel.player.steering == 0 && restoredWheel.player.heading == wheel.player.heading,
              "return to exact center continues deterministically after loading");
        const float heading = wheel.player.heading;
        wheel.update(.1f, {});
        check(wheel.player.heading == heading && std::abs(heading) > .01f,
              "centered wheel stops turning without snapping the car back to the road");
        std::string legacy = savedWheel.str();
        const auto end = legacy.rfind("\nEND\n");
        const auto steeringLine = legacy.rfind('\n', end - 1);
        legacy.erase(steeringLine + 1, end - steeringLine);
        legacy.replace(13, 1, "3");
        std::istringstream oldWheel(legacy);
        restoredWheel.load(oldWheel);
        check(restoredWheel.player.steering == 0 &&
                  restoredWheel.player.heading != 0,
              "version3 saves retain car direction and migrate to centered wheels");
      }
    Track bend;
    for (Vec2 center : {Vec2{0, 0}, Vec2{0, 5000},
                        Vec2{5000 * std::sin(.1f), 5000 + 5000 * std::cos(.1f)}}) {
      RoadRecord record;
      record.center = center;
      if (!bend.road.empty())
        bend.distances.push_back(bend.distances.back() + length(center - bend.road.back().center));
      else
        bend.distances.push_back(0);
      bend.road.push_back(record);
    }
    const std::array<Track, 3> bends{bend, bend, bend};
    for (int stage : {0, 3}) {
      Race steering(bends);
      steering.start(stage);
      running(steering);
      steering.vehicles.clear();
      steering.pickups.clear();
      steering.roadProps.clear();
      steering.player.distance = 4999;
      steering.player.speed = 100;
      auto before = steering.pose(steering.player.distance);
      steering.update(1.0f / 120, {});
      auto after = steering.pose(steering.player.distance);
      const Vec3 direction = after.forward * std::cos(steering.player.heading) +
                             after.right * std::sin(steering.player.heading);
      check(length(direction - before.forward) < .00001f &&
                std::abs(steering.player.heading) > .09f,
            "a road bend does not automatically rotate the car on either route direction");
    }
    Race r(tracks);
    running(r);
    r.vehicles.clear();
    r.pickups.clear();
    float start = r.player.distance;
    for (int i = 0; i < 1200; ++i)
      r.update(1.0f / 120, {1, 0, 0, false});
    check(r.player.distance > start + 1000 && r.player.speed > 100, "accelerates and advances");
    float speed = r.player.speed;
    for (int i = 0; i < 120; ++i)
      r.update(1.0f / 120, {0, 1, 0, false});
    check(r.player.speed < speed - 50, "brakes");
    r.player.speed = 120;
    float x = r.player.lateral;
    for (int i = 0; i < 120; ++i)
      r.update(1.0f / 120, {0, 0, 1, false});
    check(r.player.lateral > x, "steering right");
    r.player.lateral = 60;
    r.player.speed = 0;
    r.player.distance = 2000;
    r.pickups.push_back({2000, 60, PickupKind::Money, false});
    int money = r.player.money;
    r.update(1.0f / 120, {});
    check(r.player.money == money + 500 && r.pickups[0].collected, "pickup collected");
    r.update(1.0f / 120, {});
    check(r.player.money == money + 500, "pickup one shot");
    // Each original pickup has both an effect and a separate speech clip.
    const PickupKind kinds[]{PickupKind::Money, PickupKind::Fuel, PickupKind::Time};
    const char *effects[]{"bonusg.wav", "bonusb.wav", "bonust.wav"};
    const char *speech[]{"sp_b3.wav", "sp_b2.wav", "sp_b1.wav"};
    for (int i = 0; i < 3; ++i) {
      Race pickup(tracks);
      running(pickup);
      pickup.vehicles.clear();
      pickup.pickups = {{pickup.player.distance, pickup.player.lateral, kinds[i], false}};
      pickup.takeEvents();
      const float priorTime = pickup.player.timeLeft;
      pickup.update(1.0f / 120, {});
      auto events = pickup.takeEvents();
      check(events.size() == 2 && events[0].sound == effects[i] && events[1].sound == speech[i],
            "pickup plays its effect and correctly mapped original speech");
      if (kinds[i] == PickupKind::Time)
        check(std::abs(pickup.player.timeLeft - priorTime - 120) < .01f,
              "original INI time bonus adds two minutes");
      pickup.update(1.0f / 120, {});
      check(pickup.takeEvents().empty(), "collected pickup must not repeat speech or effect");
    }
    r.player.timeLeft = 0.001f;
    r.update(1.0f / 120, {});
    check(r.status() == RaceStatus::Failed, "time failure");
    for (int stage = 0; stage < 6; ++stage) {
      r.start(stage);
      running(r);
      r.vehicles.clear();
      r.pickups.clear();
      auto a = r.pose(100, 30), b = r.pose(101, 30);
      check(dot(b.position - a.position, a.forward) > 0, "reverse route advances forward");
      check(r.reverse() == (stage >= 3), "stage reversal");
      r.player.distance = r.track().length() - 81;
      r.player.speed = 200;
      r.update(1.0f / 120, {});
      check(r.status() == RaceStatus::Finished, "stage completion");
      if (stage < 5)
        check(r.nextStage() && r.stage() == stage + 1, "campaign progression");
      else
        check(!r.nextStage(), "campaign ends");
    }
    r.start();
    running(r);
    r.vehicles.clear();
    r.pickups.clear();
    r.player.distance = 1000;
    r.player.lateral = 30;
    r.player.speed = 160;
    Vehicle v;
    v.distance = 1000;
    v.lateral = v.targetLateral = 30;
    v.speed = 0;
    r.vehicles.push_back(v);
    r.update(1.0f / 120, {});
    check(r.player.speed < 100, "vehicle collision slows player");
    r.start();
    running(r);
    r.vehicles.clear();
    r.pickups.clear();
    r.player.fuel = 0;
    r.update(1.0f / 120, {});
    check(r.status() == RaceStatus::Failed, "fuel failure");
    Race original(tracks), restored(tracks);
    running(original);
    for (int i = 0; i < 600; ++i)
      original.update(1.0f / 120, {1, 0, 0, false});
    std::ostringstream save;
    original.save(save);
    std::istringstream load(save.str());
    restored.load(load);
    std::ostringstream identical;
    restored.save(identical);
    check(save.str() == identical.str(), "complete snapshot roundtrip");
    for (int i = 0; i < 600; ++i) {
      Input controls{1, 0, (i % 240 < 120 ? 0.1f : -0.1f), false};
      original.update(1.0f / 120, controls);
      restored.update(1.0f / 120, controls);
    }
    std::ostringstream a, b;
    original.save(a);
    restored.save(b);
    check(a.str() == b.str(), "snapshot deterministic continuation");
    auto reject = [&](std::string text) {
      std::ostringstream before;
      restored.save(before);
      bool threw = false;
      try {
        std::istringstream in(text);
        restored.load(in);
      } catch (const std::runtime_error &) {
        threw = true;
      }
      check(threw, "corrupt snapshot rejected");
      std::ostringstream after;
      restored.save(after);
      check(before.str() == after.str(), "corrupt restore is atomic");
    };
    reject("bogus");
    reject(save.str().substr(0, save.str().size() / 2));
    reject(save.str() + "TRAILING");
    for (const char *invalidSteering : {"nan", "2"}) {
      std::string badWheel = save.str();
      const auto end = badWheel.rfind("\nEND\n");
      const auto wheel = badWheel.rfind('\n', end - 1);
      badWheel.replace(wheel + 1, end - wheel - 1, invalidSteering);
      reject(badWheel);
    }
    std::string wrongVersion = save.str();
    wrongVersion.replace(13, 1, "9");
    reject(wrongVersion);
    std::vector<std::string> lines;
    std::istringstream source(save.str());
    std::string line;
    while (std::getline(source, line))
      lines.push_back(line);
    auto changedLine = [&](size_t index, const std::string &replacement) {
      std::string out;
      for (size_t i = 0; i < lines.size(); ++i)
        out += (i == index ? replacement : lines[i]) + "\n";
      return out;
    };
    reject(changedLine(1, "0 0 0"));
    reject(changedLine(4, "999999999"));
    reject(changedLine(3, "nan 45 0 0 100 240 5000 0 1 0"));
    reject(std::string(2 * 1024 * 1024 + 1, 'x'));
    auto otherTracks = tracks;
    otherTracks[0].road[0].center.x = 2;
    Race differentData(otherTracks);
    bool mismatch = false;
    try {
      std::istringstream in(save.str());
      differentData.load(in);
    } catch (const std::runtime_error &) {
      mismatch = true;
    }
    check(mismatch, "different original routes rejected");
    std::cout << "Simulation: sampling, controls, collision, pickups, failure, reverse stages and "
                 "progression passed\n";
    std::cout << "Saves: full state roundtrip, deterministic continuation, corruption bounds and "
                 "atomic restore passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

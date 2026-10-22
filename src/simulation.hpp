#pragma once
#include "types.hpp"
#include <array>
#include <iosfwd>
#include <string>

namespace a2 {
struct Input {
  float throttle = 0, brake = 0, steer = 0;
  bool handbrake = false;
};
enum class RaceStatus { Countdown, Running, Finished, Failed };
enum class VehicleKind { Traffic, Rival, Police };
enum class PoliceMode { Parked, Intercepting, Pursuing, Returning };
enum class PickupKind { Money, Fuel, Time };
struct RoadPose {
  Vec3 position, forward, right;
  size_t segment = 0;
  float fraction = 0;
};
struct Player {
  float distance = 0, lateral = 45, speed = 0, heading = 0, fuel = 100, timeLeft = 240;
  int money = 5000, car = 0, gear = 1;
  float elapsed = 0;
  // Wheel deflection is independent of the car's road-relative heading.
  float steering = 0;
};
struct Vehicle {
  float distance = 0, lateral = 45, speed = 0, targetSpeed = 0, targetLateral = 45;
  VehicleKind kind = VehicleKind::Traffic;
  int car = 0;
  bool oncoming = false;
  float cooldown = 0;
  PoliceMode policeMode = PoliceMode::Parked;
  float policeTimer = 0;
  // -2 is unassigned, -1 is the player; other values index a rival in vehicles.
  int policeTarget = -2;
};
struct Pickup {
  float distance = 0, lateral = 45;
  PickupKind kind = PickupKind::Money;
  bool collected = false;
};
enum class RoadPropState { Standing, Flying, Hidden };
// Dynamic werkweg2 delineators are generated separately from OMP scenery.
// Motion is relative to the direction of the current stage, including returns.
struct RoadProp {
  float distance = 0, lateral = 85, height = 0;
  float forwardVelocity = 0, lateralVelocity = 0, verticalVelocity = 0;
  float pitch = 0, roll = 0, yaw = 0;
  RoadPropState state = RoadPropState::Standing;
};
struct RaceEvent {
  std::string sound, message;
};
struct CarInfo {
  const char *name;
  const char *mesh;
  const char *texture;
  float maxSpeed, acceleration;
};
const std::array<CarInfo, 6> &cars();
const char *stageName(int stage);
int stageTrackNumber(int stage);
// Signed road frame in increasing source-file order, independent of the current race stage.
RoadPose sampleTrack(const Track &track, float sourceDistance, float lateral = 0);

class Race {
public:
  explicit Race(const std::array<Track, 3> &tracks);
  void start(int stage = 0, int car = 0);
  bool nextStage();
  void update(float dt, const Input &input);
  void save(std::ostream &output) const;
  // Invalid or incompatible snapshots throw without changing the running race.
  void load(std::istream &input);
  RaceStatus status() const { return status_; }
  int stage() const { return stage_; }
  bool reverse() const { return stage_ >= 3; }
  const Track &track() const;
  RoadPose pose(float distance, float lateral = 0) const;
  // Stage-relative distance of the dynamically generated FINISH gantry.
  float finishDistance() const;
  float countdown() const { return countdown_; }
  float introCountdown() const {
    return status_ == RaceStatus::Countdown ? std::max(0.0f, countdown_ - 3) : 0;
  }
  float fineDelay() const { return fineDelay_; }
  int fineAmount() const { return fineAmount_; }
  int position() const;
  // Returns 0 for traffic/police or an invalid index; rivals rank with the player.
  int rivalPosition(size_t vehicleIndex) const;
  const std::string &failureReason() const { return failureReason_; }
  void dismissFine() { fineDelay_ = 0; }
  std::vector<RaceEvent> takeEvents();
  Player player;
  std::vector<Vehicle> vehicles;
  std::vector<Pickup> pickups;
  std::vector<RoadProp> roadProps;

private:
  const std::array<Track, 3> &tracks_;
  RaceStatus status_ = RaceStatus::Countdown;
  int stage_ = 0;
  float countdown_ = 3, fineDelay_ = 0, collisionCooldown_ = 0, policePressure_ = 0;
  int fineAmount_ = 0;
  int finesIssued_ = 0;
  unsigned random_ = 1;
  std::string failureReason_;
  std::vector<RaceEvent> events_;
  float random();
  void emit(const char *sound, const std::string &message);
  void fail(const char *reason);
  void populate();
  void populateRoadProps();
  void strikeRoadProps(float fromDistance, float fromLateral, float toDistance,
                       float toLateral, float speed, float heading, bool playerImpact);
  void tick(float dt, const Input &input);
};
} // namespace a2

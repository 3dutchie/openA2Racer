#include "simulation.hpp"
#include <bit>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace a2 {
namespace {
constexpr std::array<int, 6> stageTracks{1, 2, 3, 3, 2, 1};
constexpr std::array<const char *, 6> stages{"AMSTERDAM - VIANEN", "VIANEN - EINDHOVEN",
                                             "EINDHOVEN - MAASTRICHT", "MAASTRICHT - EINDHOVEN",
                                             "EINDHOVEN - VIANEN", "VIANEN - AMSTERDAM"};
// The original mesh/texture names are recovered from the game. Handling values
// are reconstruction tuning, not an assertion of original physics equivalence.
const std::array<CarInfo, 6> carTable{{{"Porsche", "porche.d3d", "porche.pcx", 265, 31},
                                       {"Camaro", "camaro.d3d", "camaro.pcx", 250, 34},
                                       {"BMW", "bmw.d3d", "bmw.pcx", 240, 30},
                                       {"Jaguar", "jaguar.d3d", "jaguar.pcx", 255, 29},
                                       {"Lotus", "lotus.d3d", "lotus.pcx", 260, 33},
                                       {"Viper", "viper.d3d", "viper.pcx", 280, 32}}};
float clamp01(float v) {
  return std::clamp(v, 0.0f, 1.0f);
}
float moveTowards(float a, float b, float maxDelta) {
  return a + std::clamp(b - a, -maxDelta, maxDelta);
}
uint64_t trackFingerprint(const Track &t) {
  uint64_t value = 14695981039346656037ull;
  auto word = [&](uint32_t n) {
    for (int i = 0; i < 4; ++i) {
      value ^= uint8_t(n >> (i * 8));
      value *= 1099511628211ull;
    }
  };
  word(uint32_t(t.road.size()));
  word(uint32_t(t.placements.size()));
  for (const auto &r : t.road) {
    word(std::bit_cast<uint32_t>(r.center.x));
    word(std::bit_cast<uint32_t>(r.center.y));
    for (auto f : r.fields)
      word(f);
  }
  for (const auto &p : t.placements) {
    for (auto f : p.fields)
      word(f);
    word(std::bit_cast<uint32_t>(p.value));
  }
  return value;
}
void requireSave(bool condition) {
  if (!condition)
    throw std::runtime_error("Invalid or incompatible A2 Racer save file");
}
bool finiteRange(float value, float low, float high) {
  return std::isfinite(value) && value >= low && value <= high;
}
} // namespace

const std::array<CarInfo, 6> &cars() {
  return carTable;
}
const char *stageName(int stage) {
  return stages[std::clamp(stage, 0, 5)];
}
int stageTrackNumber(int stage) {
  return stageTracks[std::clamp(stage, 0, 5)];
}

RoadPose sampleTrack(const Track &t, float distance, float lateral) {
  if (t.road.size() < 2 || t.distances.size() != t.road.size())
    throw std::runtime_error("A road needs at least two points and a distance table");
  distance = std::clamp(distance, 0.0f, t.length());
  auto next = std::upper_bound(t.distances.begin(), t.distances.end(), distance);
  size_t i = next == t.distances.begin() ? 0 : size_t(next - t.distances.begin() - 1);
  i = std::min(i, t.road.size() - 2);
  const float span = t.distances[i + 1] - t.distances[i];
  const float f = span > 0.00001f ? clamp01((distance - t.distances[i]) / span) : 0;
  const auto a = t.road[i].center, b = t.road[i + 1].center;
  Vec2 tangent = normalized(b - a);
  const Vec2 p = a + (b - a) * f;
  RoadPose result;
  result.forward = {tangent.x, 0, tangent.y};
  result.right = {tangent.y, 0, -tangent.x};
  // The second integer after x/z is elevation: 0x41e335 and 0x41e4a2
  // FILD the loader's stack+0xf8, populated by the second word read.
  float h = lerp(float(int32_t(t.road[i].fields[1])), float(int32_t(t.road[i + 1].fields[1])), f);
  result.position = Vec3{p.x, h, p.y} + result.right * lateral;
  result.segment = i;
  result.fraction = f;
  return result;
}

Race::Race(const std::array<Track, 3> &tracks) : tracks_(tracks) {
  start();
}
const Track &Race::track() const {
  return tracks_[size_t(stageTrackNumber(stage_) - 1)];
}
RoadPose Race::pose(float distance, float lateral) const {
  auto result = sampleTrack(track(), reverse() ? track().length() - distance : distance,
                            reverse() ? -lateral : lateral);
  if (reverse()) {
    result.forward = result.forward * -1;
    result.right = result.right * -1;
  }
  return result;
}
float Race::random() {
  random_ = random_ * 1664525u + 1013904223u;
  return float(random_ >> 8) * (1.0f / 16777216.0f);
}
void Race::emit(const char *sound, const std::string &message) {
  events_.push_back({sound, message});
}
std::vector<RaceEvent> Race::takeEvents() {
  auto e = std::move(events_);
  events_.clear();
  return e;
}
void Race::fail(const char *why) {
  status_ = RaceStatus::Failed;
  failureReason_ = why;
  player.speed = 0;
  emit("loser.wav", why);
}

void Race::save(std::ostream &output) const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  out << "A2RACER_SAVE 2\n";
  for (const auto &t : tracks_)
    out << trackFingerprint(t) << ' ';
  out << '\n'
      << stage_ << ' ' << int(status_) << ' ' << countdown_ << ' ' << fineDelay_ << ' '
      << collisionCooldown_ << ' ' << policePressure_ << ' ' << random_ << ' '
      << std::quoted(failureReason_) << '\n';
  out << player.distance << ' ' << player.lateral << ' ' << player.speed << ' ' << player.heading
      << ' ' << player.fuel << ' ' << player.timeLeft << ' ' << player.money << ' ' << player.car
      << ' ' << player.gear << ' ' << player.elapsed << '\n';
  out << vehicles.size() << '\n';
  for (const auto &v : vehicles)
    out << v.distance << ' ' << v.lateral << ' ' << v.speed << ' ' << v.targetSpeed << ' '
        << v.targetLateral << ' ' << int(v.kind) << ' ' << v.car << ' ' << int(v.oncoming) << ' '
        << v.cooldown << '\n';
  out << pickups.size() << '\n';
  for (const auto &p : pickups)
    out << p.distance << ' ' << p.lateral << ' ' << int(p.kind) << ' ' << int(p.collected) << '\n';
  out << events_.size() << '\n';
  for (const auto &e : events_)
    out << std::quoted(e.sound) << ' ' << std::quoted(e.message) << '\n';
  out << roadProps.size() << '\n';
  for (const auto &p : roadProps)
    out << p.distance << ' ' << p.lateral << ' ' << p.height << ' ' << p.forwardVelocity << ' '
        << p.lateralVelocity << ' ' << p.verticalVelocity << ' ' << p.pitch << ' ' << p.roll << ' '
        << p.yaw << ' ' << int(p.state) << '\n';
  out << "END\n";
  requireSave(bool(out));
  // Use the same validator for writes: public player data may have been changed
  // by a frontend. Invalid state must not overwrite a previous good save.
  std::string text = out.str();
  std::istringstream checkStream(text);
  Race checkRace(tracks_);
  checkRace.load(checkStream);
  output.write(text.data(), std::streamsize(text.size()));
  if (!output)
    throw std::runtime_error("Could not write A2 Racer save file");
}

void Race::load(std::istream &input) {
  // Bound the entire document before parsing quoted strings or allocating any
  // entity lists. The largest supplied stage uses far less than this budget.
  std::string text;
  char c;
  while (input.get(c)) {
    requireSave(text.size() < 2 * 1024 * 1024);
    text.push_back(c);
  }
  requireSave(!input.bad());
  std::istringstream in(text);
  in.imbue(std::locale::classic());
  std::string magic;
  int version = 0;
  requireSave(bool(in >> magic >> version));
  requireSave(magic == "A2RACER_SAVE" && (version == 1 || version == 2));
  for (const auto &t : tracks_) {
    uint64_t fingerprint = 0;
    requireSave(bool(in >> fingerprint));
    requireSave(fingerprint == trackFingerprint(t));
  }
  Race restored(tracks_);
  int state = 0;
  requireSave(bool(in >> restored.stage_ >> state >> restored.countdown_ >> restored.fineDelay_ >>
                   restored.collisionCooldown_ >> restored.policePressure_ >> restored.random_ >>
                   std::quoted(restored.failureReason_)));
  requireSave(restored.stage_ >= 0 && restored.stage_ < 6 && state >= 0 && state <= 3);
  restored.status_ = RaceStatus(state);
  requireSave(finiteRange(restored.countdown_, 0, 3) && finiteRange(restored.fineDelay_, 0, 10) &&
              finiteRange(restored.collisionCooldown_, 0, 5) &&
              finiteRange(restored.policePressure_, 0, 3) && restored.failureReason_.size() <= 512);
  auto &p = restored.player;
  requireSave(bool(in >> p.distance >> p.lateral >> p.speed >> p.heading >> p.fuel >> p.timeLeft >>
                   p.money >> p.car >> p.gear >> p.elapsed));
  const float total = restored.track().length();
  requireSave(finiteRange(p.distance, 0, total) && finiteRange(p.lateral, -120, 120) &&
              finiteRange(p.speed, 0, 400) && finiteRange(p.heading, -1, 1) &&
              finiteRange(p.fuel, 0, 100) && finiteRange(p.timeLeft, 0, 86400) && p.money >= 0 &&
              p.money <= 1000000000 && p.car >= 0 && p.car < 6 && p.gear >= 1 && p.gear <= 5 &&
              finiteRange(p.elapsed, 0, 86400));
  size_t count = 0;
  requireSave(bool(in >> count));
  requireSave(count <= 5000);
  restored.vehicles.clear();
  restored.vehicles.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    Vehicle v;
    int kind = 0, oncoming = 0;
    requireSave(bool(in >> v.distance >> v.lateral >> v.speed >> v.targetSpeed >> v.targetLateral >>
                     kind >> v.car >> oncoming >> v.cooldown));
    requireSave(kind >= 0 && kind <= 2 && (oncoming == 0 || oncoming == 1) && v.car >= 0 &&
                v.car < 10);
    v.kind = VehicleKind(kind);
    v.oncoming = bool(oncoming);
    requireSave((v.kind != VehicleKind::Rival || v.car < 6) &&
                finiteRange(v.distance, -1000, total + 2000) && finiteRange(v.lateral, -120, 120) &&
                finiteRange(v.targetLateral, -120, 120) && finiteRange(v.speed, 0, 400) &&
                finiteRange(v.targetSpeed, 0, 400) && finiteRange(v.cooldown, 0, 5));
    restored.vehicles.push_back(v);
  }
  requireSave(bool(in >> count));
  requireSave(count <= 10000);
  restored.pickups.clear();
  restored.pickups.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    Pickup q;
    int kind = 0, collected = 0;
    requireSave(bool(in >> q.distance >> q.lateral >> kind >> collected));
    requireSave(kind >= 0 && kind <= 2 && (collected == 0 || collected == 1) &&
                finiteRange(q.distance, 0, total) && finiteRange(q.lateral, -120, 120));
    q.kind = PickupKind(kind);
    q.collected = bool(collected);
    restored.pickups.push_back(q);
  }
  requireSave(bool(in >> count));
  requireSave(count <= 1024);
  restored.events_.clear();
  restored.events_.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    RaceEvent e;
    requireSave(bool(in >> std::quoted(e.sound) >> std::quoted(e.message)));
    requireSave(e.sound.size() <= 128 && e.message.size() <= 512);
    restored.events_.push_back(std::move(e));
  }
  if (version == 1) {
    // Old snapshots predate dynamic roadworks. Recreate their initial layout
    // without consuming the saved race RNG or resetting any existing entity.
    restored.populateRoadProps();
  } else {
    requireSave(bool(in >> count));
    requireSave(count <= 10000);
    restored.roadProps.clear();
    restored.roadProps.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      RoadProp q;
      int state = 0;
      requireSave(bool(in >> q.distance >> q.lateral >> q.height >> q.forwardVelocity >>
                       q.lateralVelocity >> q.verticalVelocity >> q.pitch >> q.roll >> q.yaw >>
                       state));
      requireSave(state >= 0 && state <= 2 && finiteRange(q.distance, 0, total) &&
                  finiteRange(q.lateral, -1000, 1000) && finiteRange(q.height, 0, 1000) &&
                  finiteRange(q.forwardVelocity, -2000, 2000) &&
                  finiteRange(q.lateralVelocity, -2000, 2000) &&
                  finiteRange(q.verticalVelocity, -2000, 2000) &&
                  finiteRange(q.pitch, -pi, pi) && finiteRange(q.roll, -pi, pi) &&
                  finiteRange(q.yaw, -pi, pi));
      q.state = RoadPropState(state);
      restored.roadProps.push_back(q);
    }
  }
  std::string end;
  requireSave(bool(in >> end));
  requireSave(end == "END");
  in >> std::ws;
  requireSave(in.eof());
  // Commit only after the complete document passed validation. No original game
  // files are touched, and a failed read leaves every live field unchanged.
  stage_ = restored.stage_;
  status_ = restored.status_;
  countdown_ = restored.countdown_;
  fineDelay_ = restored.fineDelay_;
  collisionCooldown_ = restored.collisionCooldown_;
  policePressure_ = restored.policePressure_;
  random_ = restored.random_;
  failureReason_ = std::move(restored.failureReason_);
  player = restored.player;
  vehicles = std::move(restored.vehicles);
  pickups = std::move(restored.pickups);
  roadProps = std::move(restored.roadProps);
  events_ = std::move(restored.events_);
}

void Race::start(int stage, int car) {
  if (stage < 0 || stage >= 6 || car < 0 || car >= 6)
    throw std::invalid_argument("Invalid race stage or car");
  stage_ = stage;
  const int money = stage == 0 ? 5000 : player.money;
  player = Player{};
  player.car = car;
  player.money = money;
  const auto &t = track();
  if (t.length() <= 0 || t.road.size() < 2)
    throw std::runtime_error("Empty race track");
  player.distance = std::min(500.0f, t.length() * 0.01f);
  player.timeLeft = std::max(90.0f, t.length() / 420.0f + 65.0f);
  status_ = RaceStatus::Countdown;
  countdown_ = 3;
  fineDelay_ = collisionCooldown_ = policePressure_ = 0;
  failureReason_.clear();
  events_.clear();
  random_ = 0xa2u + unsigned(stage) * 1009;
  populate();
  emit("mstart.wav", stageName(stage_));
}
bool Race::nextStage() {
  if (status_ != RaceStatus::Finished || stage_ >= 5)
    return false;
  start(stage_ + 1, player.car);
  return true;
}

void Race::populate() {
  vehicles.clear();
  pickups.clear();
  const float total = track().length();
  // Five rivals plus the player = the original six-racer field.
  for (int c = 0; c < 6; ++c) {
    if (c == player.car)
      continue;
    Vehicle v;
    v.kind = VehicleKind::Rival;
    v.car = c;
    v.distance = player.distance + 120.0f + float(vehicles.size()) * 100;
    v.lateral = (vehicles.size() % 2) ? 30.0f : 60.0f;
    v.targetLateral = v.lateral;
    v.speed = 0;
    v.targetSpeed = 174 + random() * 23;
    vehicles.push_back(v);
  }
  for (float d = 950; d < total - 300; d += 300 + random() * 650) {
    Vehicle v;
    v.kind = VehicleKind::Traffic;
    v.car = int(random() * 10);
    v.distance = d;
    v.oncoming = random() < 0.45f;
    v.lateral = (random() < 0.5f ? 30.0f : 60.0f) * (v.oncoming ? -1 : 1);
    v.targetLateral = v.lateral;
    v.speed = 75 + random() * 55;
    v.targetSpeed = v.speed;
    vehicles.push_back(v);
  }
  for (float d = 2600; d < total - 600; d += 4500 + random() * 2500) {
    Vehicle v;
    v.kind = VehicleKind::Police;
    v.car = 0;
    v.distance = d;
    v.lateral = v.targetLateral = 60;
    v.speed = v.targetSpeed = 95 + random() * 20;
    vehicles.push_back(v);
  }
  // The original generates bonus objects independently of OMP scenery. Lane
  // choices30/60 are grounded in the recovered lane-offset table. Locations
  // and amounts here are deterministic reconstruction rules.
  int index = 0;
  for (float d = 1700; d < total - 400; d += 1100 + random() * 1200) {
    Pickup p;
    p.distance = d;
    p.lateral = (index % 2) ? 30.0f : 60.0f;
    p.kind = static_cast<PickupKind>(index++ % 3);
    pickups.push_back(p);
  }
  populateRoadProps();
}

void Race::populateRoadProps() {
  roadProps.clear();
  const auto &t = track();
  // 0x41158d chooses 4/5 work zones; 0x413dca emits ten werkweg2
  // posts per zone in consecutive source chunks, lanes 0/1 or 9/10.
  // Use a separate deterministic generator so migrating v1 saves leaves the
  // prior vehicle/pickup RNG sequence intact. It is not the original CRT rand.
  unsigned seed = 0x726f6164u + unsigned(stage_) * 1009u;
  auto choice = [&]() {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 16;
  };
  const int zones = 4 + int(choice() % 2);
  const int span = int(t.road.size() / 20);
  for (int zone = 0; zone < zones; ++zone) {
    const int center = int(float(span) * (1 + 1.5f * zone));
    for (int post = 0; post < 10; ++post) {
      int chunk = std::clamp(center + (reverse() ? -post : post), 0, int(t.road.size()) - 1);
      float sourceDistance = std::clamp(t.distances[size_t(chunk)] + 40, 0.0f, t.length());
      RoadProp p;
      p.distance = reverse() ? t.length() - sourceDistance : sourceDistance;
      p.lateral = choice() % 2 ? 85.0f : 108.0f;
      roadProps.push_back(p);
    }
  }
}

int Race::position() const {
  int pos = 1;
  for (const auto &v : vehicles)
    if (v.kind == VehicleKind::Rival &&
        (v.distance > player.distance || (v.distance == player.distance && v.car < player.car)))
      ++pos;
  return pos;
}

int Race::rivalPosition(size_t index) const {
  if (index >= vehicles.size() || vehicles[index].kind != VehicleKind::Rival)
    return 0;
  const auto &rival = vehicles[index];
  int rank = 1;
  if (player.distance > rival.distance ||
      (player.distance == rival.distance && player.car <= rival.car))
    ++rank;
  for (size_t i = 0; i < vehicles.size(); ++i) {
    const auto &other = vehicles[i];
    if (i != index && other.kind == VehicleKind::Rival &&
        (other.distance > rival.distance ||
         (other.distance == rival.distance &&
          (other.car < rival.car || (other.car == rival.car && i < index)))))
      ++rank;
  }
  return rank;
}

void Race::strikeRoadProps(float fromDistance, float fromLateral, float toDistance,
                          float toLateral, float speed, float heading, bool playerImpact) {
  if (speed < 2)
    return;
  for (auto &p : roadProps) {
    if (p.state != RoadPropState::Standing)
      continue;
    // Closest point on this tick's swept path prevents skipping a slender post.
    const float travel = toDistance - fromDistance;
    const float f = std::abs(travel) > 0.00001f ? clamp01((p.distance - fromDistance) / travel) : 0;
    if (std::abs(p.distance - lerp(fromDistance, toDistance, f)) > 14 ||
        std::abs(p.lateral - lerp(fromLateral, toLateral, f)) > 6.5f)
      continue;
    p.state = RoadPropState::Flying;
    const float velocity = speed * (10.0f / 3.6f);
    const float direction = travel < 0 ? -1.0f : 1.0f;
    // Original 0x413a5c transfers vehicle motion, lifts the post, tumbles it,
    // damps horizontal motion and removes it on landing. Impulse/gravity are
    // tuned for this port's seconds-based movement, rather than frame-identical.
    p.forwardVelocity = direction * std::cos(heading) * velocity * 0.6f;
    p.lateralVelocity = std::sin(heading) * velocity * 0.6f;
    p.verticalVelocity = std::min(100.0f, 8 + velocity * 0.18f);
    if (playerImpact || std::abs(p.distance - player.distance) < 300)
      emit("bump.wav", "");
  }
}

void Race::update(float dt, const Input &input) {
  if (!std::isfinite(dt) || dt <= 0)
    return;
  // All callers, including headless replays, receive the same bounded stepping.
  // This avoids tunnelling after a debugger pause or a dragged window.
  dt = std::min(dt, 0.25f);
  while (dt > 0) {
    float step = std::min(dt, 1.0f / 120.0f);
    tick(step, input);
    dt -= step;
  }
}

void Race::tick(float dt, const Input &input) {
  if (status_ == RaceStatus::Finished || status_ == RaceStatus::Failed)
    return;
  if (status_ == RaceStatus::Countdown) {
    float old = countdown_;
    countdown_ = std::max(0.0f, countdown_ - dt);
    if (old > 2 && countdown_ <= 2)
      emit("set.wav", "READY");
    if (old > 1 && countdown_ <= 1)
      emit("set.wav", "SET");
    if (countdown_ <= 0) {
      status_ = RaceStatus::Running;
      emit("go.wav", "GO!");
    }
    return;
  }
  collisionCooldown_ = std::max(0.0f, collisionCooldown_ - dt);
  if (fineDelay_ > 0) {
    fineDelay_ = std::max(0.0f, fineDelay_ - dt);
    return;
  }
  for (size_t i = 0; i < roadProps.size(); ++i) {
    auto &p = roadProps[i];
    if (p.state != RoadPropState::Flying)
      continue;
    p.distance = std::clamp(p.distance + p.forwardVelocity * dt, 0.0f, track().length());
    p.lateral = std::clamp(p.lateral + p.lateralVelocity * dt, -1000.0f, 1000.0f);
    p.height = std::max(0.0f, p.height + p.verticalVelocity * dt);
    p.verticalVelocity -= 220 * dt;
    const float damping = std::exp(-3.0776f * dt); // Original horizontal damping: .95/frame.
    p.forwardVelocity *= damping;
    p.lateralVelocity *= damping;
    p.pitch = std::remainder(p.pitch + dt * (5 + float(i % 3)), 2 * pi);
    p.roll = std::remainder(p.roll + dt * (i % 2 ? -4 : 4), 2 * pi);
    p.yaw = std::remainder(p.yaw + dt * (3 + float(i % 5)), 2 * pi);
    if (p.height == 0 && p.verticalVelocity < 0) {
      p.state = RoadPropState::Hidden;
      p.forwardVelocity = p.lateralVelocity = p.verticalVelocity = 0;
    }
  }
  player.elapsed += dt;
  player.timeLeft -= dt;
  if (player.timeLeft <= 0) {
    player.timeLeft = 0;
    fail("TIME IS UP");
    return;
  }
  if (player.fuel <= 0) {
    player.fuel = 0;
    fail("OUT OF FUEL");
    return;
  }
  const auto &car = carTable[size_t(player.car)];
  float throttle = std::isfinite(input.throttle) ? clamp01(input.throttle) : 0;
  float brake = std::isfinite(input.brake) ? clamp01(input.brake) : 0;
  float steer = std::isfinite(input.steer) ? std::clamp(input.steer, -1.0f, 1.0f) : 0;
  const float drag = 1.0f + 0.00012f * player.speed * player.speed;
  const float acceleration = car.acceleration * (1.0f - 0.45f * player.speed / car.maxSpeed);
  player.speed +=
      dt * (throttle * acceleration - drag - brake * 90.0f - (input.handbrake ? 110.0f : 0));
  player.speed = std::clamp(player.speed, 0.0f, car.maxSpeed);
  const int oldGear = player.gear;
  player.gear = std::clamp(int(player.speed / 48) + 1, 1, 5);
  if (player.gear > oldGear)
    emit("gearup.wav", "");
  else if (player.gear < oldGear)
    emit("geardwn.wav", "");
  player.fuel =
      std::max(0.0f, player.fuel - dt * (0.025f + player.speed * 0.00125f + throttle * 0.055f));
  const float velocity = player.speed * (10.0f / 3.6f);
  const float previousDistance = player.distance, previousLateral = player.lateral;
  player.heading +=
      steer * dt * (0.95f / (1 + player.speed / 200.0f)) * (input.handbrake ? 1.7f : 1.0f);
  player.heading *= std::exp(-2.8f * dt);
  player.heading = std::clamp(player.heading, -0.65f, 0.65f);
  const float approachSide = player.lateral < 0 ? -1.0f : 1.0f;
  player.lateral += std::sin(player.heading) * velocity * dt;
  if (std::abs(player.lateral) > 85) {
    player.speed =
        std::max(0.0f, player.speed - dt * (std::abs(player.lateral) > 120 ? 90.0f : 30.0f));
  }
  // Recovered median rail faces are at +/-5. Keep a five-unit half-car
  // clearance on the side from which the car approached, including reverse
  // stages. Bounce/slowdown follows this engine's approximate outer response.
  const bool medianHit = approachSide * player.lateral < 10;
  if (medianHit)
    player.lateral = approachSide * 10;
  const bool outerHit = std::abs(player.lateral) > 118;
  if (outerHit)
    player.lateral = std::clamp(player.lateral, -118.0f, 118.0f);
  if (medianHit || outerHit) {
    player.heading *= -0.35f;
    if (collisionCooldown_ <= 0 && player.speed > 30) {
      player.speed *= 0.7f;
      collisionCooldown_ = 1;
      emit("bump2.wav", "BARRIER");
    }
  }
  player.distance =
      std::min(track().length(), player.distance + std::cos(player.heading) * velocity * dt);
  strikeRoadProps(previousDistance, previousLateral, player.distance, player.lateral,
                  player.speed, player.heading, true);

  for (auto &v : vehicles) {
    const float previousVehicleDistance = v.distance, previousVehicleLateral = v.lateral;
    v.cooldown = std::max(0.0f, v.cooldown - dt);
    const float relative = v.distance - player.distance;
    float desired = v.targetSpeed;
    if (v.kind == VehicleKind::Police) {
      const bool pursuit = std::abs(relative) < 1600 && player.speed > 145;
      desired = pursuit ? std::max(170.0f, player.speed + 15) : 105.0f;
      if (pursuit)
        v.targetLateral = std::clamp(player.lateral, 20.0f, 70.0f);
    }
    if (!v.oncoming) {
      // Local lane occupancy reproduces the intended overtaking behaviour without
      // purporting to translate the original driver tactic state machines.
      const Vehicle *blocking = nullptr;
      for (const auto &other : vehicles) {
        if (&other == &v || other.oncoming)
          continue;
        float gap = other.distance - v.distance;
        if (gap > 0 && gap < 160 && std::abs(other.lateral - v.targetLateral) < 14) {
          blocking = &other;
          break;
        }
      }
      if (blocking) {
        float alternate = v.targetLateral < 45 ? 60.0f : 30.0f;
        bool clear = std::abs(player.distance - v.distance) > 90 ||
                     std::abs(player.lateral - alternate) > 18;
        for (const auto &other : vehicles)
          if (&other != &v && std::abs(other.distance - v.distance) < 150 &&
              std::abs(other.lateral - alternate) < 18)
            clear = false;
        if (clear)
          v.targetLateral = alternate;
        else
          desired = std::min(desired, std::max(0.0f, blocking->speed - 5));
      }
    }
    v.lateral = moveTowards(v.lateral, v.targetLateral, 18 * dt);
    v.speed = moveTowards(v.speed, desired, dt * 22);
    v.distance += v.speed * (10.0f / 3.6f) * dt * (v.oncoming ? -1 : 1);
    if (v.kind != VehicleKind::Rival) {
      if (v.distance < 0)
        v.distance = track().length() - 100;
      if (v.distance > track().length())
        v.distance = 100;
    } else
      v.distance = std::min(v.distance, track().length());
    if (std::abs(v.distance - previousVehicleDistance) < track().length() * 0.5f)
      strikeRoadProps(previousVehicleDistance, previousVehicleLateral, v.distance, v.lateral,
                      v.speed, 0, false);
    if (std::abs(v.distance - player.distance) < 25 && std::abs(v.lateral - player.lateral) < 12 &&
        collisionCooldown_ <= 0) {
      collisionCooldown_ = 1.2f;
      v.cooldown = 1.2f;
      const float impact = std::abs(player.speed - (v.oncoming ? -v.speed : v.speed));
      player.speed *= impact > 80 ? 0.35f : 0.7f;
      v.speed *= 0.65f;
      player.heading = (player.lateral >= v.lateral ? 1 : -1) * 0.14f;
      emit("bump.wav", "COLLISION");
    }
  }

  bool policeNear = false;
  for (const auto &v : vehicles) {
    if (v.kind != VehicleKind::Police)
      continue;
    if (std::abs(v.distance - player.distance) < 150 && std::abs(v.lateral - player.lateral) < 55 &&
        player.speed > 145)
      policeNear = true;
  }
  policePressure_ = policeNear ? policePressure_ + dt : std::max(0.0f, policePressure_ - dt);
  if (policePressure_ > 2.5f) {
    const int fine = std::max(100, int((player.speed - 120) * 5));
    player.money -= fine;
    player.speed = 0;
    policePressure_ = 0;
    fineDelay_ = 2.5f;
    emit("sp_p1.wav", "POLICE FINE: FL " + std::to_string(fine));
    if (player.money < 0) {
      player.money = 0;
      fail("NO MONEY FOR THE FINE");
      return;
    }
  }

  for (auto &p : pickups) {
    if (p.collected || std::abs(p.distance - player.distance) > 30 ||
        std::abs(p.lateral - player.lateral) > 13)
      continue;
    p.collected = true;
    switch (p.kind) {
    case PickupKind::Money:
      player.money += 500;
      emit("bonusg.wav", "FL 500 BONUS");
      emit("sp_b3.wav", "");
      break;
    case PickupKind::Fuel:
      player.fuel = std::min(100.0f, player.fuel + 25);
      emit("bonusb.wav", "FUEL BONUS");
      emit("sp_b2.wav", "");
      break;
    case PickupKind::Time:
      player.timeLeft += 15;
      emit("bonust.wav", "15 SECOND BONUS");
      emit("sp_b1.wav", "");
      break;
    }
  }
  if (player.distance >= track().length() - 80) {
    status_ = RaceStatus::Finished;
    player.speed = 0;
    const int award = std::max(0, 7 - position()) * 500;
    player.money += award;
    emit("finish.wav", stage_ == 5 ? "A2 RACE COMPLETE" : "STAGE COMPLETE");
  }
}
} // namespace a2

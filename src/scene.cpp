#include "scene.hpp"
#include "guardrails.hpp"
#include "legacy_geometry.hpp"
#include <cstdio>

namespace a2 {
namespace {
struct ObjectDef {
  const char *model;
  const char *texture;
};
constexpr ObjectDef objects[] = {
    {"sky", "Sky"},           {"bovw", "50"},           {"arena", "Arena"},
    {"viaduct", "VIADUCT"},   {"molent", "MOLEN"},      {"shell", "Shell"},
    {"roadsgn", "ROADSGN"},   {"uit", "UIT"},           {"zaltboml", "zaltboml"},
    {"lekbrug", "Lekbrug"},   {"gebouwg", "GEBOUW"},    {"gebouw2g", "TILE"},
    {"gebouw3g", "TILE2"},    {"trees", "Trees"},       {"wegresg2", "wegresg2"},
    {"elek4", "elek4"},       {"huisje", "huisje"},     {"rijtjes", "rijtjes"},
    {"werkweg", "werkweg"},   {"werkweg2", "werkweg2"}, {"flits", "flits"},
    {"praatp", "Praatp"},     {"start", "Start"},       {"finish", "finish"},
    {"geld", "Geld"},         {"benzine", "BENZine"},   {"tijd", "Tijd"},
    {"lantaarn", "LANTAARN"}, {"eindh", "eindh"},       {"amsterd", "Amsterd"},
    {"denbosch", "Denbosch"}, {"maastr", "maastr"},     {"utrecht", "Utrecht"},
    {"s_maastr", "s_maastr"}, {"s_utr", "s_utr"},       {"s_denb", "s_denb"},
    {"s_adam", "s_adam"},     {"s_eindho", "s_eindho"}, {"number1", "number1"},
    {"number2", "number2"},   {"number3", "number3"},   {"number4", "number4"},
    {"number5", "number5"},   {"number6", "number6"},   {"hek", "hek"},
    {"koe", "koe"},           {"100", "100"},           {"babe", "babe"}};
float yawOf(Vec3 f) {
  return std::atan2(f.x, f.z);
}
std::string file(const char *name, const char *ext) {
  return std::string(name) + ext;
}
} // namespace
void Scene::draw(const Race &race, int view, bool radar, bool horizon, bool dashboard) {
  auto p = race.pose(race.player.distance, race.player.lateral);
  Vec3 f = p.forward, r = p.right;
  float h = race.player.heading;
  f = normalized(f * std::cos(h) + r * std::sin(h));
  Camera cam;
  if (view == 0)
    cam.lookAt(p.position + Vec3{0, 8.3f, 0} + f * 5, p.position + f * 160 + Vec3{0, 6.5f, 0});
  else if (view == 2)
    cam.lookAt(p.position - f * 105 + Vec3{0, 155, 0}, p.position + f * 150);
  else
    cam.lookAt(p.position - f * 52 + Vec3{0, 21, 0}, p.position + f * 85 + Vec3{0, 5, 0});
  cam.farPlane = renderer_.enhanced() ? 7800 : 2600;
  renderer_.clear(0xff628194);
  if (horizon)
    renderer_.background(assets_.image("Sky.pcx"), yawOf(f), 240 + cam.forward.y * cam.focal);
  const auto &track = race.track();
  const auto &grass = assets_.image("grass2.pcx");
  const auto &asphalt = assets_.image("2baans.pcx");
  for (size_t i = 0; i + 1 < track.road.size(); ++i) {
    auto a = sampleTrack(track, track.distances[i]), b = sampleTrack(track, track.distances[i + 1]);
    Vec3 mid = (a.position + b.position) * 0.5f;
    if (length(mid - cam.position) > cam.farPlane + 200 ||
        dot(mid - cam.position, cam.forward) < -220)
      continue;
    auto edge = [&](RoadPose pose, float lateral, float y) {
      return pose.position + pose.right * lateral + Vec3{0, y, 0};
    };
    // The two original 11-code arrays select the ten terrain strips on each
    // side of a segment from an 8x8 atlas. The final code is a boundary entry.
    renderer_.quad(cam, edge(a, -1800, -0.6f), edge(a, 1800, -0.6f), edge(b, 1800, -0.6f),
                   edge(b, -1800, -0.6f), nullptr, 1, 1, 0xff4b7034);
    for (int side = 0; side < 2; ++side)
      for (int strip = 0; strip < 10; ++strip) {
        float sign = side ? 1.0f : -1.0f, inner = sign * (120 + strip * 60.0f),
              outer = inner + sign * 60;
        unsigned code = track.road[i].fields[size_t((side ? 14 : 3) + strip)] & 63u;
        float u0 = float(code % 8) / 8, u1 = u0 + 0.125f, v0 = float(code / 8) / 8,
              v1 = v0 + 0.125f;
        DrawVertex vA{edge(a, inner, -0.1f), {u0, v1}}, vB{edge(b, inner, -0.1f), {u0, v0}},
            vC{edge(b, outer, -0.1f), {u1, v0}}, vD{edge(a, outer, -0.1f), {u1, v1}};
        renderer_.triangle(cam, vA, vB, vC, &grass);
        renderer_.triangle(cam, vA, vC, vD, &grass);
      }
    // Each half of the original highway maps the entire two-lane texture,
    // mirrored at the centre. Its outer grass shoulder begins around85 units.
    renderer_.quad(cam, edge(a, 0, 0), edge(a, 120, 0), edge(b, 120, 0), edge(b, 0, 0), &asphalt, 1,
                   1);
    renderer_.quad(cam, edge(a, 0, 0), edge(a, -120, 0), edge(b, -120, 0), edge(b, 0, 0), &asphalt,
                   1, 1);
    drawGuardrails(renderer_, grass, cam, a, b);
  }
  scenery(race, cam);
  roadProps(race, cam);
  for (const auto &pickup : race.pickups) {
    if (pickup.collected)
      continue;
    auto pos = race.pose(pickup.distance, pickup.lateral);
    if (length(pos.position - cam.position) > cam.farPlane ||
        dot(pos.position - cam.position, cam.forward) < -25)
      continue;
    int id = pickup.kind == PickupKind::Money ? 24 : pickup.kind == PickupKind::Fuel ? 25 : 26;
    renderer_.mesh(cam, assets_.mesh(file(objects[id].model, ".d3d")),
                   assets_.image(file(objects[id].texture, ".pcx")),
                   pos.position + Vec3{0, 6 + std::sin(race.player.elapsed * 3) * 2, 0},
                   race.player.elapsed * 1.5f, 1.8f);
  }
  for (size_t i = 0; i < race.vehicles.size(); ++i) {
    const auto &v = race.vehicles[i];
    auto pos = race.pose(v.distance, v.lateral);
    if (length(pos.position - cam.position) > cam.farPlane ||
        dot(pos.position - cam.position, cam.forward) < -40)
      continue;
    const char *model;
    const char *texture;
    if (v.kind == VehicleKind::Police) {
      model = "police.d3d";
      texture = "police.pcx";
    } else if (v.kind == VehicleKind::Rival) {
      const auto &c = cars()[size_t(v.car) % cars().size()];
      model = c.mesh;
      texture = c.texture;
    } else {
      static const char *models[] = {"golf.d3d", "panda.d3d",  "cx.d3d",     "merceds.d3d",
                                     "van.d3d",  "vracht.d3d", "interl.d3d", "volvo.d3d"};
      static const char *textures[] = {"golf.pcx",   "panda.pcx",  "cx.pcx", "mercedes.pcx",
                                       "vanmap.pcx", "vracht.pcx", "il.pcx", "volvo.pcx"};
      model = models[size_t(v.car) % 8];
      texture = textures[size_t(v.car) % 8];
    }
    if (renderer_.enhanced() && length(pos.position - cam.position) < 1800)
      renderer_.shadow(cam, assets_.mesh(model), pos.position,
                       yawOf(pos.forward) + (v.oncoming ? pi : 0), 1, pos.position);
    renderer_.mesh(cam, assets_.mesh(model), assets_.image(texture), pos.position,
                   yawOf(pos.forward) + (v.oncoming ? pi : 0));
    if (v.kind == VehicleKind::Rival)
      rankMarker(cam, race.rivalPosition(i), pos.position, yawOf(pos.forward));
    if (v.kind == VehicleKind::Police && std::fmod(race.player.elapsed * 8, 2) < 1) {
      auto light = pos.position + Vec3{0, 9, 0};
      renderer_.quad(cam, light - pos.right * 2, light + pos.right * 2,
                     light + pos.right * 2 + Vec3{0, 1, 0}, light - pos.right * 2 + Vec3{0, 1, 0},
                     nullptr, 1, 1, 0xff5277ff);
    }
  }
  if (view != 0) {
    const auto &c = cars()[size_t(race.player.car) % 6];
    renderer_.shadow(cam, assets_.mesh(c.mesh), p.position, yawOf(f), 1, p.position);
    renderer_.mesh(cam, assets_.mesh(c.mesh), assets_.image(c.texture), p.position, yawOf(f));
    rankMarker(cam, race.position(), p.position, yawOf(f));
  }
  renderer_.finishWorld();
  hud(race, view == 0 && !dashboard ? 1 : view, radar);
}

void Scene::rankMarker(const Camera &cam, int rank, Vec3 position, float yaw) {
  if (rank < 1 || rank > 6)
    return;
  const auto &def = objects[37 + rank];
  // Recovered number callbacks (0x408481..0x408b40) use the original number
  // meshes, vehicle heading and a ten-unit vertical offset. Ordinary world
  // depth testing lets cars/scenery hide the markers naturally. Cull back
  // faces so mirrored rear glyphs cannot fill the front numeral openings.
  renderer_.mesh(cam, assets_.mesh(file(def.model, ".d3d")),
                 assets_.image(file(def.texture, ".pcx")), position + Vec3{0, 10, 0}, yaw, 1, true,
                 true);
}

void Scene::roadProps(const Race &race, const Camera &cam) {
  if (race.roadProps.empty())
    return;
  const auto &model = assets_.mesh("werkweg2.d3d");
  const auto &texture = assets_.image("werkweg2.pcx");
  for (const auto &prop : race.roadProps) {
    if (prop.state == RoadPropState::Hidden)
      continue;
    auto pose = race.pose(prop.distance, prop.lateral);
    Vec3 position = pose.position + Vec3{0, prop.height, 0};
    if (length(position - cam.position) > cam.farPlane ||
        dot(position - cam.position, cam.forward) < -15)
      continue;
    const float yaw = yawOf(pose.forward) + prop.yaw;
    if (prop.state == RoadPropState::Standing) {
      renderer_.mesh(cam, model, texture, position, yaw);
      continue;
    }
    Mesh tumbling = model;
    auto rotate = [&](Vec3 p) {
      Vec3 x{p.x, p.y * std::cos(prop.pitch) - p.z * std::sin(prop.pitch),
             p.y * std::sin(prop.pitch) + p.z * std::cos(prop.pitch)};
      return Vec3{x.x * std::cos(prop.roll) - x.y * std::sin(prop.roll),
                  x.x * std::sin(prop.roll) + x.y * std::cos(prop.roll), x.z};
    };
    for (auto &v : tumbling.vertices) {
      v.position = rotate(v.position);
      v.normal = rotate(v.normal);
    }
    renderer_.mesh(cam, tumbling, texture, position, yaw);
  }
}
void Scene::scenery(const Race &race, const Camera &cam) {
  const auto &track = race.track();
  for (const auto &placement : track.placements) {
    unsigned id = placement.fields[3], chunk = placement.fields[0];
    if (id == 0 || id >= std::size(objects) || chunk >= track.road.size())
      continue;
    auto def = objects[id];
    auto meshName = file(def.model, ".d3d"), textureName = file(def.texture, ".pcx");
    if (!assets_.has(meshName) || !assets_.has(textureName))
      continue;
    auto frame = sampleTrack(track, track.distances[chunk]);
    float radius = 0;
    const Mesh &mesh = assets_.mesh(meshName);
    for (const auto &v : mesh.vertices)
      radius =
          std::max(radius, std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z));
    float offset = placement.value;
    if (placement.fields[2] >= 3)
      offset = radius + 2 * offset;
    if (placement.fields[2] == 1 || placement.fields[2] == 3)
      offset = -offset;
    if (placement.fields[2] == 0)
      offset = 0;
    Vec3 pos = frame.position + frame.right * offset;
    if (length(pos - cam.position) > cam.farPlane + radius ||
        dot(pos - cam.position, cam.forward) < -radius)
      continue;
    if (placement.fields[2] >= 3)
      pos.y = 0;
    float yaw = sceneryYaw(frame, placement.fields[2], placement.fields[1]);
    renderer_.mesh(cam, mesh, assets_.image(textureName), pos, yaw, 1,
                   id == 13 || id == 44 || id == 45 || id >= 33);
    // Convex footprints are suitable for solid buildings, bridges and stations;
    // alpha-cutout trees and small signs would cast rectangular false shadows.
    if (renderer_.enhanced() && length(pos - cam.position) < 1800 &&
        (id == 2 || id == 3 || id == 5 || (id >= 8 && id <= 12) || (id >= 16 && id <= 17)))
      renderer_.shadow(cam, mesh, pos, yaw, 1, frame.position);
  }
}
void Scene::hud(const Race &race, int camera, bool radar) {
  hud_.draw(race, camera, radar);
}
void Scene::carPreview(int car, float angle) {
  const auto &c = cars()[size_t(car) % 6];
  Camera cam;
  cam.lookAt({27, 17, 37}, {0, 4, 0});
  cam.farPlane = 150;
  renderer_.mesh(cam, assets_.mesh(c.mesh), assets_.image(c.texture), {0, 0, 0}, angle, 1);
  renderer_.finishWorld();
}
} // namespace a2

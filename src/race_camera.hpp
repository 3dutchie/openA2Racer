#pragma once
#include "renderer.hpp"
#include "simulation.hpp"

namespace a2 {
// The opening overhead view is visible in original gameplay footage. The
// interpolation and its duration are reconstruction choices, not recovered
// camera constants. Deriving it from the saved countdown also makes restart,
// pause and loading a countdown deterministic.
inline Camera raceCamera(const RoadPose &pose, Vec3 forward, int view,
                         float countdown = 0) {
  Vec3 eye, target;
  if (view == 0) {
    eye = pose.position + Vec3{0, 8.3f, 0} + forward * 5;
    target = pose.position + forward * 160 + Vec3{0, 6.5f, 0};
  } else if (view == 2) {
    eye = pose.position - forward * 105 + Vec3{0, 155, 0};
    target = pose.position + forward * 150;
  } else {
    eye = pose.position - forward * 52 + Vec3{0, 21, 0};
    target = pose.position + forward * 85 + Vec3{0, 5, 0};
  }
  float intro = std::clamp(countdown / 3.0f, 0.0f, 1.0f);
  intro = intro * intro * (3 - 2 * intro);
  // A small forward component avoids a singular lookAt basis at straight down.
  const Vec3 overheadEye = pose.position - forward * 8 + Vec3{0, 285, 0};
  const Vec3 overheadTarget = pose.position + forward * 1;
  Camera camera;
  camera.lookAt(eye * (1 - intro) + overheadEye * intro,
                target * (1 - intro) + overheadTarget * intro);
  return camera;
}
} // namespace a2

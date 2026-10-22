#include "race_camera.hpp"
#include <iostream>
#include <stdexcept>

using namespace a2;
namespace {
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void close(Vec3 actual, Vec3 expected, const char *message) {
  check(length(actual - expected) < 0.001f, message);
}
void cameraDirections(Vec3 forward) {
  RoadPose pose{{50, 4, 200}, forward, {forward.z, 0, -forward.x}};
  Camera overhead = raceCamera(pose, forward, 0, 3);
  check(overhead.forward.y < -.99f, "Race starts looking down at the grid");
  check(dot(overhead.right, pose.right) > .99f, "Overhead camera preserves road direction");
  for (int view : {0, 1, 2}) {
    auto opening = raceCamera(pose, forward, view, 3);
    close(opening.position, overhead.position, "Intro begins from the same overhead position");
    auto final = raceCamera(pose, forward, view);
    auto beforeGo = raceCamera(pose, forward, view, .0001f);
    close(beforeGo.position, final.position, "Camera reaches selected view without a jump");
    close(beforeGo.forward, final.forward, "Camera direction reaches selected view smoothly");
    Camera previous = opening;
    for (int frame = 1; frame <= 360; ++frame) {
      auto current = raceCamera(pose, forward, view, 3 - frame / 120.f);
      check(std::isfinite(current.forward.y) && std::abs(length(current.forward) - 1) < .001f &&
                std::abs(length(current.right) - 1) < .001f,
            "Descent has a finite orthonormal camera frame");
      check(length(current.position - previous.position) < 2,
            "Descent advances continuously at the simulation step");
      previous = current;
    }
  }
  close(raceCamera(pose, forward, 0).position,
        pose.position + forward * 5 + Vec3{0, 8.3f, 0}, "Cockpit endpoint is preserved");
  close(raceCamera(pose, forward, 1).position,
        pose.position - forward * 52 + Vec3{0, 21, 0}, "Chase endpoint is preserved");
  close(raceCamera(pose, forward, 2).position,
        pose.position - forward * 105 + Vec3{0, 155, 0}, "High-camera endpoint is preserved");
}
} // namespace
int main() {
  try {
    cameraDirections({0, 0, 1});
    cameraDirections({0, 0, -1});
    cameraDirections(normalized(Vec3{1, 0, 1}));
    std::cout << "Opening camera descent and selected-view endpoints passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

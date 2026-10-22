#include "legacy_geometry.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace a2;
static void check(bool value, const char *what) {
  if (!value)
    throw std::runtime_error(what);
}
static int unit(float yaw) {
  return int(std::lround(double(yaw) * 1024.0 / (2.0 * 3.141596)));
}
int main() {
  try {
    RoadPose north;
    north.forward = {0, 0, 1};
    north.right = {1, 0, 0};
    check(unit(sceneryYaw(north, 1, 0)) == 255, "negative-side perpendicular heading");
    check(unit(sceneryYaw(north, 3, 0)) == 255, "negative extended side");
    check(unit(sceneryYaw(north, 2, 0)) == 767, "positive-side perpendicular heading");
    check(unit(sceneryYaw(north, 1, 1)) == 493, "original 0x312 zero-component branch");
    check(unit(sceneryYaw(north, 2, 1)) == 1023, "original 0x100 zero-component branch");
    check(unit(sceneryYaw(north, 0, 1)) == 1023, "centre uses positive-side orientation branch");
    RoadPose east;
    east.forward = {1, 0, 0};
    east.right = {0, 0, -1};
    check(unit(sceneryYaw(east, 1, 0)) == 255, "zero-x negative branch");
    check(unit(sceneryYaw(east, 2, 0)) == 767, "zero-x positive branch");
    RoadPose diagonal;
    diagonal.forward = normalized(Vec3{1, 0, 1});
    diagonal.right = {diagonal.forward.z, 0, -diagonal.forward.x};
    check(unit(sceneryYaw(diagonal, 1, 0)) == 383, "diagonal negative perpendicular");
    check(unit(sceneryYaw(diagonal, 2, 0)) == 895, "diagonal positive perpendicular");
    check(unit(sceneryYaw(diagonal, 1, 1)) == 127, "diagonal negative parallel");
    check(unit(sceneryYaw(diagonal, 2, 1)) == 639, "diagonal positive parallel");
    std::cout << "Legacy OMP yaw: side, orientation, wrap, truncation and original zero-component "
                 "branches passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

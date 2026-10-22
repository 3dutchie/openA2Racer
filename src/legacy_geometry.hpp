#pragma once
#include "simulation.hpp"
#include <cmath>
#include <cstdint>

namespace a2 {
// OMP orientation reconstructed from A2SPEL.DAT 0x407f67..0x4080b8,
// followed by the stored-angle conversion at 0x4082f2..0x40830f.
// Preserve its truncation and zero-component branches, including the unusual
// literal 0x312. See docs/RECONSTRUCTION.md for evidence and precision limits.
inline float sceneryYaw(const RoadPose &frame, uint32_t position, uint32_t angle) {
  const double x = frame.right.x, z = frame.right.z;
  const bool negativeSide = position == 1 || position == 3;
  int stored = 0;
  if (angle == 0) {
    if (x == 0)
      stored = negativeSide ? 0 : 0x200;
    else
      stored = int(std::atan2(negativeSide ? z : -z, negativeSide ? x : -x) * 1024.0 / 6.283185307);
  } else if (angle == 1) {
    if (z == 0)
      stored = negativeSide ? 0x312 : 0x100;
    else
      stored = int(std::atan2(negativeSide ? x : -x, negativeSide ? -z : z) * 1024.0 / 6.283185307);
  }
  const uint32_t drawAngle = (1023u - (uint32_t(stored) & 1023u) + 256u) & 1023u;
  // The original lookup-table initializer uses this approximation of pi.
  return float(double(drawAngle) * (2.0 * 3.141596 / 1024.0));
}
} // namespace a2

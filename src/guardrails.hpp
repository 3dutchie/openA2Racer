#pragma once
#include "renderer.hpp"
#include "simulation.hpp"

namespace a2 {
// Recovered from A2SPEL.DAT 0x41f358 (vertices), 0x4207fb..0x420ef4
// (UVs) and 0x4217b3..0x42196f (faces). See analysis/rails/FINDINGS.md.
// The original grass2 atlas includes the beam, its posts and the grass below.
void drawGuardrails(Renderer &, const Image &grassAtlas, const Camera &, const RoadPose &start,
                    const RoadPose &end);
} // namespace a2

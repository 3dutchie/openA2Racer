#include "guardrails.hpp"

namespace a2 {
void drawGuardrails(Renderer &renderer, const Image &grassAtlas, const Camera &camera,
                    const RoadPose &start, const RoadPose &end) {
  // All four strips are present in the original execute-buffer builder. The
  // two median faces are separated by ten units; outer faces sit at +/-120.
  // There is no TRK flag test in that builder that removes individual rails.
  for (float lateral : {-120.0f, -5.0f, 120.0f, 5.0f}) {
    const Vec3 a = start.position + start.right * lateral;
    const Vec3 b = end.position + end.right * lateral;
    const Vec3 up{0, 5, 0};
    const DrawVertex lowerStart{a, {0, 0.125f}}, upperStart{a + up, {0, 0}},
        lowerEnd{b, {1, 0.125f}}, upperEnd{b + up, {1, 0}};
    // Original faces include both windings. Renderer::triangle already draws
    // both sides, so duplicate reverse faces would only repeat the same work.
    renderer.triangle(camera, lowerStart, lowerEnd, upperEnd, &grassAtlas);
    renderer.triangle(camera, lowerStart, upperEnd, upperStart, &grassAtlas);
  }
}
} // namespace a2

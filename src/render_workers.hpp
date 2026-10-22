#pragma once
#include <functional>
namespace a2 {
// Each task owns disjoint screen rows. Completion is a barrier before overlays.
void parallelRenderRows(int rows, int band, const std::function<void(int, int)> &);
} // namespace a2

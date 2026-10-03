#pragma once
//! Bounded search for a ledge top to
//! climb onto.



#include <cmath>
#include <initializer_list>
#include <optional>

namespace fc {
/// `World` wrapper that allows at most
/// 4096 rays.
///
/// Past the limit, rays report a
/// blocking, non-climbable hit and
/// `exhausted` is set, so the search
/// fails safely.
template <class BaseWorld, class Vector, class CollisionHit>
class TopCandidateBudgetWorld final : public BaseWorld {
public:
  static constexpr unsigned limit = 4096;
  BaseWorld &source;
  unsigned count{};
  bool exhausted{};
  /// Wrap `value`.
  explicit TopCandidateBudgetWorld(BaseWorld &value) : source(value) {}
  /// Forward one ray within the budget.
  std::optional<CollisionHit> ray(Vector from, Vector to) override {
    if (count >= limit) {
      exhausted = true;
      return CollisionHit{from, (from - to).unit(), false};
    }
    ++count;
    return source.ray(from, to);
  }
};

/// Probe the center lane, then lanes at
/// ±75% of `radius`.
///
/// # Returns
/// The first found result, or empty.
template <class Probe>
auto searchTopCandidateLanes(float radius, Probe &&probe) {
  using Result = decltype(probe(0.f));
  if (auto found = probe(0.f))
    return found;
  if (!std::isfinite(radius) || radius <= 0)
    return Result{};
  const float side = radius * .75f;
  for (float offset : {side, -side})
    if (auto found = probe(offset))
      return found;
  return Result{};
}
} // namespace fc

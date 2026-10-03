#pragma once
//! Back-off for expensive wall searches
//! that just failed.


#include <algorithm>
#include <cmath>

namespace fc {
/// Remembers a failed search so it is
/// not repeated every frame while
/// nothing changed.
///
/// A new search is allowed once the
/// delay expires, or the position (0.25
/// units), normal, input or mode
/// changes.
template <class Vector> struct SearchRetry {
  float remaining{};
  Vector position{}, normal{}, input{};
  unsigned mode{};
  bool valid{}, standingPath{};
  /// Count the delay down by `dt` (max
  /// 50 ms).
  void tick(float dt) {
    if (std::isfinite(dt) && dt > 0)
      remaining = std::max(0.f, remaining - std::min(dt, .05f));
  }
  /// Whether a new search may run for
  /// these inputs.
  bool ready(Vector p, Vector n, Vector intent, unsigned nextMode) const {
    return !valid || !std::isfinite(remaining) || remaining <= 0 ||
           !p.finite() || !n.finite() || !intent.finite() ||
           !position.finite() || !normal.finite() || !input.finite() ||
           (p - position).length() > .25f || (n - normal).length() > .01f ||
           (intent - input).length() > .01f || mode != nextMode;
  }
  /// Record a failed search and wait
  /// `seconds` (0-0.25).
  ///
  /// `standing` marks a search started
  /// from the ground.
  void defer(Vector p, Vector n, Vector intent, unsigned nextMode,
             float seconds, bool standing = false) {
    position = p;
    normal = n;
    input = intent;
    mode = nextMode;
    standingPath = standing;
    valid =
        p.finite() && n.finite() && intent.finite() && std::isfinite(seconds);
    remaining = valid ? std::clamp(seconds, 0.f, .25f) : 0.f;
  }
  /// Allow the next search at once.
  void reset() { *this = {}; }
};
} // namespace fc

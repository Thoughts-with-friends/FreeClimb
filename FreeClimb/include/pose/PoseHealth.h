#pragma once
//! Watchdog for the engine's pose
//! callback: detects when FreeClimb's
//! pose is no longer being applied.


#include <algorithm>
#include <cmath>
#include <cstdint>
namespace fc {
/// Whether the last callback (ms tick
/// `last`) was at most 250 ms before
/// `now`.
inline bool recentPoseCallback(std::uint64_t now, std::uint64_t last) {
  return last != 0 && now >= last && now - last <= 250;
}

/// Tracks how long the applied-pose
/// counter has stopped changing.
class PoseHealth {
  std::uint32_t last{};
  float silence{};
  bool seen{};

public:
  /// Start over, as if never seen.
  void reset() {
    last = 0;
    silence = 0;
    seen = false;
  }
  /// Feed the applied-pose counter once
  /// per frame. Silence grows by `dt`
  /// (capped at 50 ms) while it does
  /// not change.
  void sample(std::uint32_t count, float dt) {
    if (count != last) {
      seen = true;
      silence = 0;
      last = count;
    } else if (std::isfinite(dt) && dt > 0)
      silence += std::clamp(dt, 0.f, .05f);
  }
  /// Seen at least once and silent for
  /// at most 120 ms.
  bool ready() const { return seen && silence <= .12f; }
  /// Force `ready()` to `false` until
  /// the counter changes again.
  void invalidate() { silence = std::max(silence, .13f); }
  /// Silent too long: 1.5 s after it
  /// was seen, 0.75 s if never seen.
  bool failed() const { return silence >= (seen ? 1.5f : .75f); }
  /// Current silence in seconds.
  float stale() const { return silence; }
};
} // namespace fc

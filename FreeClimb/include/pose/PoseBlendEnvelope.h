#pragma once
//! Weight curve for blending
//! FreeClimb's pose in and out over the
//! game's own animation.



#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fc {

/// Entry and exit blend weight.
///
/// Progress only advances on frames
/// where a new pose was applied, so the
/// blend never runs ahead of what is
/// visible.
class PoseBlendEnvelope {
  enum class Mode { idle, entering, exiting };
  Mode mode = Mode::idle;
  float phase{}, duration = .28f;
  std::uint32_t observedOutputs{};
  /// Smootherstep of `x`, clamped.
  static float ease(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * x * (10 + x * (-15 + 6 * x));
  }
  /// Usable frame time: 0 if invalid,
  /// at most 50 ms.
  static float step(float dt) {
    return std::isfinite(dt) && dt > 0 ? std::min(dt, .05f) : 0.f;
  }

public:
  /// Blend durations: entry, normal
  /// exit, and exit into a fall.
  static constexpr float entrySeconds = .18f, exitSeconds = .28f,
                         fallExitSeconds = .16f;
  /// Back to idle (weight 0).
  void clear() { *this = {}; }
  /// Start blending in.
  void beginEntry() {
    mode = Mode::entering;
    phase = 0;
    observedOutputs = 0;
  }
  /// Current weight: 0.01-1 while
  /// entering, 1-0 while exiting.
  float weight() const {
    if (mode == Mode::idle)
      return 0;

    return mode == Mode::entering ? .01f + .99f * ease(phase) : 1 - ease(phase);
  }
  /// Seconds into the exit, or 0.
  float elapsedExitSeconds() const {
    return mode == Mode::exiting ? phase * duration : 0.f;
  }
  /// Advance the entry by `dt` if the
  /// applied-pose counter changed.
  void advanceEntry(std::uint32_t applied, float dt) {
    const float elapsed = step(dt);
    if (mode != Mode::entering || applied == observedOutputs || elapsed <= 0)
      return;
    observedOutputs = applied;
    phase = std::min(1.f, phase + elapsed / entrySeconds);
  }

  /// Start blending out over `seconds`
  /// (clamped to 0.05-1).
  void beginExit(std::uint32_t applied = 0, float seconds = exitSeconds) {
    mode = Mode::exiting;
    phase = 0;
    observedOutputs = applied;
    duration = std::isfinite(seconds) && seconds > 0
                   ? std::clamp(seconds, .05f, 1.f)
                   : exitSeconds;
  }
  /// Advance the exit by `dt` if the
  /// applied-pose counter changed.
  void advanceExit(std::uint32_t applied, float dt) {
    const float elapsed = step(dt);
    if (mode != Mode::exiting || applied == observedOutputs || elapsed <= 0)
      return;
    observedOutputs = applied;
    phase = std::min(1.f, phase + elapsed / duration);
  }
};
} // namespace fc

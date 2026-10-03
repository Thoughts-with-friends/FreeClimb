#pragma once
//! Detects the player being stuck while
//! walking into something, for the
//! diagnostics log.


#include "traversal/Core.h"

namespace fc {

/// Reports when the player pushes in
/// one direction but moves slower than
/// 8 units/s for 0.65 s.
///
/// At most one report per 5 s and 64 in
/// total.
class GroundMotionProbe {
  Vec previous{}, heading{};
  float stalled{}, cooldown{};
  bool initialized{};
  unsigned reports{};

public:
  /// Stop tracking until the next
  /// eligible sample.
  void suspend() {
    initialized = false;
    stalled = 0;
  }
  /// Feed one frame.
  ///
  /// # Params
  /// - `intent`: Movement input.
  /// - `eligible`: Tracking is allowed
  ///   (on the ground, not climbing).
  ///
  /// # Returns
  /// `true` when a stall should be
  /// reported.
  bool sample(Vec position, Vec intent, float dt, bool eligible) {
    if (!std::isfinite(dt) || dt <= 0)
      return false;
    dt = std::min(dt, .05f);
    cooldown = std::max(0.f, cooldown - dt);
    intent.z = 0;
    if (!eligible || !position.finite() || !intent.finite() ||
        intent.length() < .1f) {
      suspend();
      return false;
    }
    intent = intent.unit();
    const Vec delta = position - previous;
    if (!initialized || intent.dot(heading) < .7f || delta.length() > 50) {
      initialized = true;
      previous = position;
      heading = intent;
      stalled = 0;
      return false;
    }
    previous = position;
    heading = intent;

    const float speed = delta.length() / dt;
    if (speed > 8)
      stalled = 0;
    else
      stalled += dt;
    if (stalled < .65f || cooldown > 0 || reports >= 64)
      return false;
    ++reports;
    cooldown = 5;
    stalled = 0;
    return true;
  }
};
} // namespace fc

#pragma once
//! Smoothed view heading used while
//! climbing, so the camera turns with
//! the wall without snapping.





#include "pose/YawFrame.h"

namespace fc {

/// Critically damped yaw follower.
///
/// Moves toward a target heading with a
/// second-order response, never faster
/// than `maxSpeed`, and never
/// overshoots the target.
class ViewHeading {
  float heading{}, velocity{};
  bool initialized{};

public:
  /// Turn speed limit (radians/s).
  static constexpr float maxSpeed = 6.f;
  /// Spring stiffness (1/s); higher
  /// follows the target faster.
  static constexpr float response = 14.f;
  /// Forget the heading; `ready()`
  /// becomes `false`.
  void reset() {
    initialized = false;
    velocity = 0;
  }
  /// Whether `begin` has set a heading.
  bool ready() const { return initialized; }
  /// Current heading (radians).
  float value() const { return heading; }
  /// Current turn speed (radians/s).
  float speed() const { return velocity; }
  /// Start following from `actual` with
  /// zero speed. A non-finite value
  /// resets instead.
  void begin(float actual) {
    if (!std::isfinite(actual)) {
      reset();
      return;
    }
    heading = normalizeYaw(actual);
    velocity = 0;
    initialized = true;
  }
  /// Step toward `target` by `dt`
  /// seconds.
  ///
  /// `dt` is capped at 50 ms so a long
  /// frame cannot jump the camera.
  ///
  /// # Returns
  /// The new heading, or the old one if
  /// not ready or the input is invalid.
  float advance(float target, float dt) {
    if (!initialized || !std::isfinite(target) || !std::isfinite(dt) || dt <= 0)
      return heading;
    dt = std::min(dt, .05f);
    const float error = yawDifference(target, heading);

    const float offset = -error, c = velocity + response * offset;
    const float decay = std::exp(-response * dt);
    const float nextOffset = (offset + c * dt) * decay;
    float step = std::clamp(nextOffset - offset, -maxSpeed * dt, maxSpeed * dt);
    velocity =
        std::clamp((velocity - response * c * dt) * decay, -maxSpeed, maxSpeed);

    if (step * error > 0 && std::abs(step) > std::abs(error)) {
      step = error;
      velocity = 0;
    }
    heading = normalizeYaw(heading + step);
    return heading;
  }
};
} // namespace fc

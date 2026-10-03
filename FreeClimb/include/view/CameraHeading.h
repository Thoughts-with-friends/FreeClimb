#pragma once
//! Camera yaw captured in world space,
//! so it survives the player turning.






#include "pose/YawFrame.h"

namespace fc {
/// World-space camera yaw in radians,
/// `[0, 2π)`.
///
/// Captured from the actor yaw plus the
/// camera's yaw relative to the actor.
struct CameraHeading {
  float worldYaw{};
  /// Capture the camera heading.
  ///
  /// # Params
  /// - `actorYaw`: Actor yaw (radians).
  /// - `relativeYaw`: Camera yaw
  ///   relative to the actor.
  ///
  /// # Returns
  /// `None` if either angle is not
  /// finite.
  static std::optional<CameraHeading> capture(float actorYaw,
                                              float relativeYaw) {
    if (!std::isfinite(actorYaw) || !std::isfinite(relativeYaw))
      return {};
    return CameraHeading{
        normalizeYaw(normalizeYaw(actorYaw) + normalizeYaw(relativeYaw))};
  }
  /// Signed yaw from `actorYaw` to this
  /// heading, in `(-π, π]`.
  ///
  /// # Returns
  /// `None` if either angle is not
  /// finite.
  std::optional<float> relativeTo(float actorYaw) const {
    if (!std::isfinite(actorYaw) || !std::isfinite(worldYaw))
      return {};
    return yawDifference(worldYaw, normalizeYaw(actorYaw));
  }
};
} // namespace fc

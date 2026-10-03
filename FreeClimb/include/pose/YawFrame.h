#pragma once
//! Yaw (heading) helpers and the
//! wall-aligned frame poses are
//! authored in.
//!
//! Yaw is measured in radians from +Y
//! toward +X, like the game's heading.


#include "pose/Pose.h"

namespace fc {
/// Wrap `yaw` into `[0, 2π)`.
inline float normalizeYaw(float yaw) {
  constexpr float circle = 6.28318530718f;
  yaw = std::fmod(yaw, circle);
  if (yaw < 0)
    yaw += circle;

  return yaw >= circle ? 0.f : yaw;
}
/// Shortest signed turn from `actual`
/// to `target`, in `[-π, π]`.
inline float yawDifference(float target, float actual) {
  return std::remainder(target - actual, 6.28318530718f);
}
/// Yaw of a horizontal direction.
///
/// # Returns
/// `None` if `forward` is not finite or
/// nearly vertical.
inline std::optional<float> headingYaw(Vec forward) {
  if (!forward.finite() ||
      forward.x * forward.x + forward.y * forward.y < 1e-8f)
    return {};
  return normalizeYaw(std::atan2(forward.x, forward.y));
}
/// Yaw that faces a wall whose outward
/// normal is `outward`.
inline std::optional<float> facingWallYaw(Vec outward) {
  return headingYaw(outward * -1);
}
/// Yaw of the +Y axis of `rotation`.
inline std::optional<float> frameYaw(Quat rotation) {
  return headingYaw(rotation.rotate({0, 1, 0}));
}

/// Converts transforms between a bone's
/// parent space and a frame that faces
/// the wall.
struct WallYawFrame {
  Quat parentFromWall;
  /// # Params
  /// - `parentWorld`: World rotation of
  ///   the parent bone.
  /// - `wallYaw`: Yaw facing the wall.
  WallYawFrame(Quat parentWorld, float wallYaw)
      : parentFromWall(
            (parentWorld.inverse() * Quat::axis({0, 0, 1}, -wallYaw)).unit()) {}
  /// Wall frame to parent space.
  Transform toParent(Transform value) const {
    value.t = parentFromWall.rotate(value.t);
    value.q = (parentFromWall * value.q).unit();
    return value;
  }
  /// Parent space to wall frame.
  Transform toWall(Transform value) const {
    const auto inverse = parentFromWall.inverse();
    value.t = inverse.rotate(value.t);
    value.q = (inverse * value.q).unit();
    return value;
  }
};
} // namespace fc

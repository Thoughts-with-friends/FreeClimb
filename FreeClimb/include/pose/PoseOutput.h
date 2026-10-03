#pragma once
//! Writes FreeClimb's pose into the
//! engine's pose buffer.



#include "pose/Pose.h"
#include "pose/TrackView.h"
#include <span>

namespace fc {
/// Outcome of a pose write.
///
/// - `ignored`: Not our character, or
///   zero weight.
/// - `invalid`: The buffer failed
///   validation; nothing written.
/// - `applied`: Written.
enum class PoseWrite { ignored, invalid, applied };

/// Blend `authored` into the first 99
/// tracks of `tracks`.
///
/// # Params
/// - `character`: Owner of the buffer;
///   must be in `owners`.
/// - `weight`: Blend weight, 0-1.
///
/// All tracks are validated before any
/// is written.
inline PoseWrite applyCharacterPose(const void *character,
                                    std::span<const void *const> owners,
                                    void *tracks, const Pose &authored,
                                    float weight) {
  if (!character || weight <= 0 || !std::isfinite(weight) ||
      authored.size() != 99 ||
      std::find(owners.begin(), owners.end(), character) == owners.end())
    return PoseWrite::ignored;
  auto target = densePose(tracks);
  if (!target.data || target.count < 99)
    return PoseWrite::invalid;

  for (std::size_t i = 0; i < 99; ++i) {
    const auto *p = target.data + i * 48;
    const auto q = field<Quat>(p, 16);
    if (!field<Vec>(p, 0).finite() || !field<Vec>(p, 32).finite() ||
        !std::isfinite(q.dot(q)) || q.dot(q) < .5f || q.dot(q) > 1.5f)
      return PoseWrite::invalid;
  }
  weight = std::clamp(weight, 0.f, 1.f);
  for (std::size_t i = 0; i < 99; ++i) {
    auto *p = target.data + i * 48;
    const Transform base{field<Vec>(p, 0), field<Quat>(p, 16),
                         field<Vec>(p, 32)};
    const auto result = blend(base, authored[i], weight);
    std::memcpy(p, &result.t, sizeof(Vec));
    std::memcpy(p + 16, &result.q, sizeof(Quat));
    std::memcpy(p + 32, &result.s, sizeof(Vec));
  }
  return PoseWrite::applied;
}
} // namespace fc

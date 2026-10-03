#pragma once
//! Wall running driven by the game's
//! own sprint animation, tilted onto
//! the wall.


#include "pose/Pose.h"
#include <mutex>
namespace fc {

/// Keeps the vanilla sprint animation
/// playing while wall running.
///
/// Sends `moveStart`/`SprintStart` on
/// start and the stop events on end; if
/// the animation goes stale it retries
/// up to three times. Thread safe.
class NativeRunDrive {
  std::mutex mutex;
  bool active{};
  std::uint64_t lastKick{}, staleSince{}, healthySince{};
  unsigned retries{};

public:
  /// Forget the drive state (sends
  /// nothing).
  void reset() {
    std::scoped_lock lock(mutex);
    active = false;
    lastKick = staleSince = healthySince = 0;
    retries = 0;
  }
  /// Drive the graph toward
  /// `requested`.
  ///
  /// # Params
  /// - `fresh`: The animation is
  ///   currently moving.
  /// - `now`: Time in ms.
  /// - `dispatch`: Sends one graph
  ///   event.
  ///
  /// # Returns
  /// Whether any event was sent.
  template <class Dispatch>
  bool update(bool requested, bool fresh, std::uint64_t now,
              Dispatch dispatch) {
    int action = 0;
    {
      std::scoped_lock lock(mutex);
      if (requested != active) {
        active = requested;
        action = requested ? 1 : -1;
        lastKick = now;
        staleSince = healthySince = 0;
        retries = 0;
      } else if (active) {
        if (fresh) {
          staleSince = 0;
          if (!healthySince)
            healthySince = now;
          if (now - healthySince >= 750)
            retries = 0;
        } else {
          healthySince = 0;
          if (!staleSince)
            staleSince = now;
          if (now - staleSince >= 180 && now - lastKick >= 650 && retries < 3) {
            action = 2;
            lastKick = now;
            ++retries;
          }
        }
      }
    }
    if (!action)
      return false;

    if (action > 0) {
      if (action == 1)
        dispatch("IdleForceDefaultState");
      dispatch("moveStart");
      dispatch("SprintStart");
    } else {
      dispatch("SprintStop");
      dispatch("moveStop");
    }
    return true;
  }
};
/// Smooth switch between climbing and
/// wall running (0.46 s in, 0.40 s
/// out).
class WallModeTransition {
  bool target{};
  float phase = 1, from{};

public:
  /// Settle on climbing.
  void reset() {
    target = false;
    phase = 1;
    from = 0;
  }
  /// Eased progress of the current
  /// switch.
  float fraction() const { return smooth(phase); }
  /// Wall run weight, 0-1.
  float weight() const {
    return from + ((target ? 1.f : 0.f) - from) * fraction();
  }
  /// Switch to running (`true`) or
  /// climbing; restarts from the
  /// current weight.
  ///
  /// # Returns
  /// Whether the target changed.
  bool select(bool sprint) {
    if (sprint == target)
      return false;
    from = weight();
    target = sprint;
    phase = 0;
    return true;
  }
  /// Advance by `dt` (max 50 ms).
  void advance(float dt) {
    phase = std::min(1.f, phase + std::clamp(dt, 0.f, .05f) /
                                      (target ? .46f : .40f));
  }
};
/// Whether the native pose's legs and
/// arms are clearly away from rest (the
/// sprint animation is playing).
inline bool animatedNativePose(const Pose &native, const Pose &rest) {
  if (native.size() != 99 || rest.size() != 99)
    return false;
  float difference = 0;
  for (int i : {6, 7, 9, 10, 28, 29, 31, 32}) {
    if (!native[i].t.finite() || !std::isfinite(native[i].q.dot(native[i].q)))
      return false;
    difference += angleBetween(native[i].q, rest[i].q);
  }
  return difference > .2f;
}
/// Confirms the sprint animation is
/// really moving: at least three leg
/// changes over 60 ms, the last one
/// within 100 ms.
class NativeRunEvidence {
  std::array<Quat, 4> previous{};
  std::uint64_t lastMotion{};
  std::uint64_t motionStart{};
  unsigned changes{};
  bool initialized{};

public:
  /// Forget the evidence.
  void reset() { *this = {}; }
  /// Feed one native pose.
  ///
  /// # Returns
  /// Whether the run looks animated.
  bool sample(const Pose &native, const Pose &rest, bool requested,
              std::uint64_t now) {
    if (!requested || !animatedNativePose(native, rest)) {
      reset();
      return false;
    }
    constexpr int legs[] = {6, 7, 9, 10};
    float change = 0;
    for (int i = 0; i < 4; ++i) {
      if (initialized)
        change += angleBetween(previous[i], native[legs[i]].q);
      previous[i] = native[legs[i]].q;
    }
    if (initialized && change > .015f) {
      if (!lastMotion || now - lastMotion > 100) {
        motionStart = now;
        changes = 0;
      }
      lastMotion = now;
      ++changes;
    }
    initialized = true;
    return changes >= 3 && now >= motionStart && now - motionStart >= 60 &&
           lastMotion != 0 && now >= lastMotion && now - lastMotion <= 100;
  }
};

/// Tilt the native run pose onto the
/// wall and offset it by the gap.
///
/// # Params
/// - `slope`: Wall normal Z.
/// - `gap`: Distance from the wall.
/// - `scale`: Actor scale.
/// - `direction`: Run direction.
inline Pose nativeWallRun(const Pose &native, float slope, float gap,
                          float scale, Vec direction) {
  if (native.size() != 99)
    return {};
  Pose result = native;
  const float heading = std::atan2(-direction.x, direction.y);
  const Quat rotation =
      Quat::axis({1, 0, 0}, std::acos(std::clamp(slope, -.15f, .75f))) *
      Quat::axis({0, 0, 1}, heading);
  result[0].q = rotation * native[0].q;
  result[0].t = rotation.rotate(native[0].t) +
                Vec{0, (gap - 5) / std::clamp(scale, .5f, 2.f), 0};
  return result;
}
} // namespace fc

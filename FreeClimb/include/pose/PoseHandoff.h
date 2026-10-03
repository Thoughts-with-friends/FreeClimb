#pragma once
//! Hands the body between the game's
//! animation and FreeClimb's pose
//! without visible pops.
//!
//! Covers the blend in, the blend out
//! (optionally continuing the motion),
//! and the final recovery at the top of
//! a mantle.


#include "pose/Pose.h"

namespace fc {
/// Mantle progress at which control
/// starts returning to the game: the
/// last 0.24 s, but not before 72%.
inline float topRecoveryBegin(float seconds) {
  return std::isfinite(seconds) && seconds > 0
             ? std::max(.72f, 1.f - .24f / seconds)
             : 1.f;
}
/// How far control has returned to the
/// game at `progress` of a mantle, 0-1
/// (0 for other motions).
inline float topRecovery(Motion motion, float progress, float seconds) {
  const float begin = topRecoveryBegin(seconds);
  if (motion != Motion::contextMantle || !std::isfinite(progress) ||
      begin >= 1.f || progress <= begin)
    return 0.f;
  if (progress >= 1.f)
    return 1.f;
  return std::clamp(smooth((progress - begin) / (1.f - begin)), 0.f, 1.f);
}

/// One-shot permission for the top
/// recovery of a mantle.
class TopRecoveryGate {
  bool attempted{}, accepted{};
  float seconds{};

public:
  /// Forget the attempt.
  void clear() { *this = {}; }
  /// Ask once, when a mantle reaches
  /// the recovery point.
  ///
  /// # Returns
  /// `true` on the single call that
  /// should start the recovery.
  bool request(State state, float progress, float duration) {
    const float begin = topRecoveryBegin(duration);
    if (attempted || state != State::mantle || !std::isfinite(progress) ||
        begin >= 1.f || progress < begin)
      return false;
    seconds = duration;
    attempted = true;
    return true;
  }
  /// Record whether the game accepted
  /// the recovery.
  void resolve(bool success) {
    if (attempted)
      accepted = success;
  }
  /// Whether the recovery was accepted.
  bool ready() const { return accepted; }
  /// Recovery weight once accepted,
  /// else 0.
  float weight(Motion motion, float progress) const {
    return accepted ? topRecovery(motion, progress, seconds) : 0.f;
  }
};

/// Mixes the authored pose with the
/// game's native pose.
///
/// Keeps the last two displayed poses
/// so an exit can continue their
/// motion, and a revision number so
/// stale results are dropped.
class PoseHandoff {
  Pose entry, displayed, olderDisplayed, displayedSource, olderSource,
      exitSource, nativeExitSource;
  PoseContinuation exitContinuation, nativeExitContinuation;
  float displayedTime{}, olderTime{};
  float displayedRecovery{}, exitWeight = 1;
  std::uint64_t revision{};
  bool exiting{}, movingExit{}, nativeExit{};
  float nativeExitAge{};

public:
  /// Length of the smooth takeover by
  /// the game's own animation.
  static constexpr float nativeExitSeconds = .12f;
  /// One evaluated frame.
  ///
  /// - `pose`: What to display.
  /// - `source`: FreeClimb's part
  ///   before mixing with the native
  ///   pose.
  /// - `recovery`: Native share, 0-1.
  /// - `revision`: Handoff revision it
  ///   belongs to.
  /// - `sampleTime`: Time it samples.
  struct Output {
    Pose pose, source;
    float recovery{};
    std::uint64_t revision{};
    float sampleTime{};
  };
  /// Forget everything and bump the
  /// revision.
  void clear() {
    const auto next = revision + 1;
    *this = {};
    revision = next;
  }
  /// Whether a pose was displayed.
  bool hasOutput() const { return !displayed.empty(); }
  /// FreeClimb's share when the exit
  /// began.
  float exitContribution() const { return exitWeight; }
  /// Whether the smooth native takeover
  /// is still running.
  bool nativeExitActive() const {
    return nativeExit && nativeExitAge < nativeExitSeconds;
  }
  /// Start handing the body back.
  ///
  /// # Params
  /// - `continueMotion`: Keep the last
  ///   motion going (e.g. falling).
  /// - `smoothNativeTakeover`: Fade
  ///   into the game's animation over
  ///   `nativeExitSeconds`.
  ///
  /// # Returns
  /// `false` if nothing was displayed.
  bool beginExit(bool continueMotion = false,
                 bool smoothNativeTakeover = false) {
    if (!hasOutput())
      return false;

    exitSource = displayedSource;
    exitWeight = 1 - displayedRecovery;
    movingExit = continueMotion;
    if (movingExit)
      exitContinuation.begin(displayedSource, olderSource,
                             displayedTime - olderTime);

    nativeExit = smoothNativeTakeover && !continueMotion;
    nativeExitAge = 0;
    if (nativeExit) {
      nativeExitSource = displayed;
      nativeExitContinuation.begin(displayed, olderDisplayed,
                                   displayedTime - olderTime);
    }

    ++revision;
    exiting = true;
    return true;
  }
  /// Advance the exit pose to `elapsed`
  /// seconds, keeping arm bends valid.
  ///
  /// `acknowledgedNativeElapsed`, when
  /// set, drives the takeover instead.
  void advanceExitSource(float elapsed, const Library &library,
                         float acknowledgedNativeElapsed = -1) {
    if (!exiting)
      return;
    if (movingExit) {
      exitSource = exitContinuation.sample(elapsed);
      library.guardArmBends(exitSource);
    }
    const float nativeElapsed =
        acknowledgedNativeElapsed >= 0 ? acknowledgedNativeElapsed : elapsed;
    if (nativeExit && std::isfinite(nativeElapsed)) {
      nativeExitAge = std::max(nativeExitAge, std::max(0.f, nativeElapsed));
      if (nativeExitAge < nativeExitSeconds) {
        nativeExitSource = nativeExitContinuation.sample(nativeExitAge);
        if (nativeExitAge > 0)
          library.guardArmBends(nativeExitSource);
      }
    }
  }
  /// Mix one frame without committing
  /// it.
  ///
  /// While entering, the authored pose
  /// is blended over the entry pose by
  /// `weight`; while exiting, the exit
  /// pose fades out.
  ///
  /// # Returns
  /// Empty output when the pose sizes
  /// differ.
  Output evaluate(const Pose &native, const Pose &authored, float weight,
                  float recovery = 0, float sampleTime = 0) {
    if (native.size() != authored.size())
      return {};
    weight = std::clamp(weight, 0.f, 1.f);
    recovery = std::clamp(recovery, 0.f, 1.f);
    if (entry.empty())
      entry = native;
    Output result;
    result.source = native;
    result.pose = native;
    result.revision = revision;
    result.sampleTime = sampleTime;
    result.recovery = exiting ? 1 - weight * exitWeight : recovery;
    for (std::size_t i = 0; i < native.size(); ++i) {
      result.source[i] =
          exiting ? exitSource[i] : blend(entry[i], authored[i], weight);
      result.pose[i] = blend(result.source[i], native[i], result.recovery);
      if (nativeExitActive())
        result.pose[i] = blend(nativeExitSource[i], result.pose[i],
                               smooth(nativeExitAge / nativeExitSeconds));
    }
    return result;
  }

  /// Commit a result that was actually
  /// displayed.
  ///
  /// # Returns
  /// `false` for stale revisions or
  /// out-of-order sample times.
  bool consumed(const Output &result) {
    if (result.revision != revision || result.pose.empty() ||
        result.pose.size() != result.source.size())
      return false;
    if (!exiting) {

      if (result.sampleTime < displayedTime)
        return false;
      if (result.sampleTime > displayedTime) {
        olderSource = displayedSource;
        olderTime = displayedTime;
        olderDisplayed = displayed;
        displayedTime = result.sampleTime;
      }
    }
    displayed = result.pose;
    displayedSource = result.source;
    displayedRecovery = result.recovery;
    return true;
  }

  /// `evaluate` and `consumed` in one
  /// step; returns `native` on failure.
  Pose compose(const Pose &native, const Pose &authored, float weight,
               float recovery = 0) {
    auto result = evaluate(native, authored, weight, recovery);
    consumed(result);
    return result.pose.empty() ? native : result.pose;
  }
};
} // namespace fc

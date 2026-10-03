#pragma once
//! When to play traversal sounds.
//!
//! Pure timing logic with no game
//! calls: it watches the motion, its
//! phase and limb contact weights, and
//! emits sound cues. Playback lives in
//! `TraversalAudioRuntime.h`.





#include "traversal/Core.h"

namespace fc {
/// Kind of traversal sound.
enum class SoundCue { grip, step, push, top };
/// One cue to play, with a gain
/// multiplier (0-1).
struct TraversalSound {
  SoundCue cue{};
  float gain = 1;
};
/// Cues emitted in one frame (at most
/// two).
struct TraversalSounds {
  std::array<TraversalSound, 2> items{};
  unsigned count{};
};

/// Decides which cues a frame emits.
///
/// # Rules
/// - Climbing cycles: a grip or step
///   when a hand or foot newly takes
///   weight (>= 0.65).
/// - Wall runs: steps at fixed points
///   of the run cycle.
/// - One-shot moves: push-off, catch
///   and top cues at fixed phases.
/// - At most one cue every 85 ms.
class TraversalAudio {
  Motion previous = Motion::none;
  float lastPhase{}, quiet{};
  Vec lastPosition{};
  std::array<bool, 4> loaded{};
  bool started{}, completed{};

public:
  /// Forget all timing state.
  void reset() { *this = {}; }
  /// Advance one frame.
  ///
  /// # Params
  /// - `motion`: Current motion.
  /// - `phase`: Motion phase, 0-1.
  /// - `position`: Player position;
  ///   cycles only sound while moving.
  /// - `weights`: Contact weights of
  ///   left hand, right hand, left
  ///   foot, right foot.
  /// - `dt`: Frame time (capped at 50
  ///   ms).
  /// - `outputReady`: Pose output is
  ///   live; otherwise state resets.
  /// - `topCompleted`: The mantle has
  ///   reached the top.
  ///
  /// # Returns
  /// Cues to play this frame.
  TraversalSounds update(Motion motion, float phase, Vec position,
                         std::array<float, 4> weights, float dt,
                         bool outputReady, bool topCompleted = false) {
    TraversalSounds out;
    if (!outputReady || !isActiveMotion(motion)) {
      reset();
      return out;
    }
    if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(phase) ||
        !position.finite())
      return out;
    dt = std::min(dt, .05f);
    phase = std::clamp(phase, 0.f, 1.f);
    quiet = std::max(0.f, quiet - dt);
    const bool changed = !started || motion != previous;
    const bool moving = started && (position - lastPosition).length() > .015f;
    const bool cycle =
        runMotion(motion) || (motion >= Motion::up && motion <= Motion::right);
    const float before = changed ? 0.f : lastPhase;
    auto crossed = [&](float at) {
      return cycle && phase < before ? (at > before || at <= phase)
                                     : (before < at && phase >= at);
    };
    auto emit = [&](SoundCue cue, float gain) {
      if (quiet > 0 || out.count >= out.items.size())
        return;
      out.items[out.count++] = {cue, gain};
      quiet = .085f;
    };

    if (cycle && !changed && moving) {
      if (runMotion(motion)) {
        if (crossed(.3421053f) || crossed(.8421053f))
          emit(SoundCue::step, .85f);
      } else {
        bool grip = false, foot = false;
        for (unsigned limb = 0; limb < 4; ++limb) {
          if (weights[limb] >= .65f && !loaded[limb]) {
            (limb < 2 ? grip : foot) = true;
            loaded[limb] = true;
          } else if (weights[limb] < .20f)
            loaded[limb] = false;
        }
        if (grip)
          emit(SoundCue::grip, .70f);
        else if (foot)
          emit(SoundCue::step, .65f);
      }
    } else if (!cycle) {
      const bool entry =
          motion == Motion::reach || motion == Motion::jumpCatch ||
          motion == Motion::sprintCatch || motion == Motion::ledgeCatch;
      const bool hop = hopMotion(motion);
      const bool wallKick =
          motion >= Motion::kickUp && motion <= Motion::kickRight;
      const bool departing =
          motion == Motion::dropBack || motion == Motion::backFlipOut;
      if ((hop || departing) && crossed(.10f))
        emit(SoundCue::push, .75f);
      if (entry && crossed(.76f))
        emit(SoundCue::grip, .8f);
      if (motion == Motion::runLaunch && crossed(.94f))
        emit(SoundCue::step, .85f);
      if (wallKick && crossed(.95f))
        emit(SoundCue::step, .85f);
      if (hop && !departing && !wallKick) {
        const float catchAt = motion == Motion::contextHopLeft    ? .52f
                              : motion == Motion::contextHopRight ? .57f
                                                                  : .70f;
        if (crossed(catchAt))
          emit(SoundCue::grip, .85f);
      }
      const bool top = motion == Motion::contextMantle;
      if (top && crossed(.375f))
        emit(SoundCue::grip, .55f);
      if (top && topCompleted && !completed)
        emit(SoundCue::top, .75f);
    }

    if (changed || !moving)
      for (unsigned i = 0; i < 4; ++i)
        loaded[i] = weights[i] >= .65f;
    previous = motion;
    lastPhase = phase;
    lastPosition = position;
    started = true;
    completed = topCompleted;
    return out;
  }
};
} // namespace fc

#pragma once
//! Plays the cues of `TraversalAudio`
//! in the game.
//!
//! Each cue picks a bundled wav, routes
//! it through the vanilla output model
//! and is scaled by the footsteps
//! category volume. Playback results
//! are sampled for the log, without
//! blocking the frame.





#include "audio/SoundFiles.h"
#include "audio/TraversalAudio.h"
#include "runtime/RuntimeVersion.h"

namespace fc {

/// Game-side traversal audio player.
///
/// Uses a ring of six voices; a new cue
/// stops the oldest voice.
class TraversalAudioRuntime {
public:
  /// Per-group counters for the log.
  ///
  /// - `queued`/`rejected`: Requests
  ///   accepted or refused.
  /// - `sampled`: Requests whose state
  ///   is polled afterwards.
  /// - `playing`: Seen playing.
  /// - `durationKnown`/`unobserved`:
  ///   Never seen playing, with or
  ///   without a known duration.
  /// - `cancelled`: Stopped while
  ///   polled.
  struct GroupStats {
    unsigned queued{}, rejected{}, sampled{}, playing{}, durationKnown{},
        unobserved{}, cancelled{};
  };

private:
  /// One playing sound and its pending
  /// state poll.
  struct Voice {
    RE::BSSoundHandle sound{};
    float age{};
    unsigned group{}, probe{};
    std::uint64_t duration{};
    bool pending{};
  };
  /// Vanilla forms the bundled sounds
  /// play through (see `sounds`).
  RE::BGSSoundCategory *category{};
  RE::BGSSoundOutput *output{};
  std::array<unsigned, 4> lastFile{};
  unsigned picks{};
  std::array<Voice, 6> voices{};
  std::array<GroupStats, 4> stats{};
  TraversalAudio timing;
  unsigned nextVoice{};
  bool installed{}, dirty{};
  /// Sound group index of a cue.
  static unsigned slot(SoundCue cue) {
    switch (cue) {
    case SoundCue::step:
      return 0;
    case SoundCue::grip:
      return 1;
    case SoundCue::push:
      return 2;
    case SoundCue::top:
      return 3;
    }
    return 0;
  }
  /// Group name for log messages.
  static const char *name(unsigned group) {
    constexpr std::array names{"step", "grip", "push", "top"};
    return names[group];
  }
  /// Move a sound to `point`.
  ///
  /// SE calls the engine setter
  /// directly; AE queues a
  /// `SetPosition` audio message.
  static bool position(RE::BSSoundHandle &sound, Vec point) {
    if (!runtime::isSE()) {
      auto *manager = RE::BSAudioManager::GetSingleton();
      if (!manager || sound.soundID == RE::BSSoundHandle::kInvalidID)
        return false;
      manager->ComposeMessage(RE::SOUND_MSG::SetPosition, sound.soundID, 0,
                              nullptr, {},
                              RE::NiPoint3{point.x, point.y, point.z});
      return true;
    }
    using Function = bool (*)(RE::BSSoundHandle *, float, float, float);
    REL::Relocation<Function> function{REL::ID(66370)};
    return function(&sound, point.x, point.y, point.z);
  }
  /// Log the per-group totals if they
  /// changed since the last summary.
  void summarize() {
    if (!dirty)
      return;
    for (unsigned i = 0; i < stats.size(); ++i) {
      const auto &s = stats[i];
      if (!s.queued && !s.rejected)
        continue;
      SKSE::log::info("Traversal audio totals: group={}, queued={}, "
                      "rejected={}, sampled={}, nativePlaying={}, "
                      "durationOnly={}, unobserved={}, cancelled={}",
                      name(i), s.queued, s.rejected, s.sampled, s.playing,
                      s.durationKnown, s.unobserved, s.cancelled);
    }
    dirty = false;
  }
  /// Count a voice whose state poll was
  /// cut short.
  void cancelProbe(Voice &voice) {
    if (voice.pending) {
      ++stats[voice.group].cancelled;
      voice.pending = false;
      dirty = true;
    }
  }
  /// Count a refused request; log the
  /// first three and then every 32nd.
  void reject(unsigned group, const char *stage) {
    auto &s = stats[group];
    ++s.rejected;
    dirty = true;
    if (s.rejected <= 3 || s.rejected % 32 == 0)
      SKSE::log::warn(
          "Traversal sound request rejected: group={}, stage={}, rejected={}",
          name(group), stage, s.rejected);
  }
  /// Get a sound handle for one random
  /// wav of `group`, routed through the
  /// vanilla output model.
  ///
  /// # Returns
  /// `false` if the engine refused the
  /// file.
  bool acquire(RE::BSAudioManager &manager, RE::BSSoundHandle &sound,
               unsigned group) {
    const char *file = sounds::pick(group, lastFile[group], ++picks);
    if (!file)
      return false;
    RE::BSResource::ID id;
    id.GenerateFromPath(file);
    manager.GetSoundHandleByFile(sound, id, 0x10, sounds::priority);
    if (sound.soundID == RE::BSSoundHandle::kInvalidID)
      return false;
    sound.SetOutputModel(output);
    return true;
  }
  /// Footsteps category volume (the
  /// in-game slider), 0-1.
  float level() const {
    return category ? std::clamp(category->GetCategoryVolume(), 0.f, 1.f) : 1.f;
  }
  /// Start one cue at `point`.
  ///
  /// Steps: handle, position, volume,
  /// play. A failed step is logged with
  /// its stage name and the voice is
  /// stopped.
  void play(TraversalSound cue, Vec point) {
    const auto group = slot(cue.cue);
    auto *manager = RE::BSAudioManager::GetSingleton();
    if (!manager) {
      reject(group, "manager");
      return;
    }
    auto &voice = voices[nextVoice++ % voices.size()];
    cancelProbe(voice);
    if (voice.sound.soundID != RE::BSSoundHandle::kInvalidID)
      voice.sound.Stop();
    voice = {};
    voice.group = group;

    const char *failed = "file";
    bool accepted = acquire(*manager, voice.sound, group);
    const float gain = std::clamp(volume * cue.gain * level(), 0.f, 1.f);
    if (accepted) {
      failed = "position";
      accepted = position(voice.sound, point);
    }
    if (accepted) {
      failed = "volume";
      accepted = voice.sound.SetVolume(gain);
    }
    if (accepted) {
      failed = "play";
      accepted = voice.sound.Play();
    }
    if (!accepted) {
      if (voice.sound.soundID != RE::BSSoundHandle::kInvalidID)
        voice.sound.Stop();
      reject(group, failed);
      return;
    }
    auto &s = stats[group];
    ++s.queued;
    dirty = true;

    voice.pending = s.queued <= 8 || s.queued % 32 == 0;
    if (voice.pending)
      ++s.sampled;
    if (s.queued == 1 || s.queued % 32 == 0)
      SKSE::log::info("Traversal sound queued: group={}, queued={}, handle={}, "
                      "gain={:.3f}, category=Footsteps",
                      name(group), s.queued, voice.sound.soundID, gain);
  }

public:
  /// Audio switch and master volume
  /// from the settings.
  bool enabled = true;
  float volume = .75f;
  const auto &statistics() const { return stats; }
  /// Resolve the vanilla forms the
  /// sounds need.
  ///
  /// # Returns
  /// `false` on unsupported runtimes or
  /// when the forms are missing.
  bool install() {
    if (!runtime::supported())
      return false;
    auto *data = RE::TESDataHandler::GetSingleton();
    category = data ? data->LookupForm<RE::BGSSoundCategory>(sounds::category,
                                                             "Skyrim.esm")
                    : nullptr;
    output = data ? data->LookupForm<RE::BGSSoundOutput>(sounds::output,
                                                         "Skyrim.esm")
                  : nullptr;
    installed = category && output;
    SKSE::log::info("Bundled traversal audio: category={}, output={}, "
                    "enabled={}, volume={:.2f}",
                    category != nullptr, output != nullptr, enabled, volume);
    if (!installed)
      SKSE::log::warn("Vanilla sound category or output model missing; audio "
                      "disabled, traversal remains available");
    return installed;
  }

  /// Poll pending voices at 12, 35, 70,
  /// 150, 350 and 750 ms after start,
  /// and record whether the engine
  /// reports them playing.
  void observe(float dt) {
    if (!std::isfinite(dt) || dt <= 0)
      return;
    constexpr std::array times{.012f, .035f, .070f, .150f, .350f, .750f};
    for (auto &voice : voices) {
      if (!voice.pending)
        continue;
      voice.age += std::min(dt, .1f);
      if (voice.age < times[voice.probe])
        continue;

      auto observed = voice.sound;
      observed.assumeSuccess = false;
      const bool playing = observed.IsPlaying();
      voice.duration = std::max(voice.duration, observed.GetDuration());
      ++voice.probe;
      if (playing) {
        auto &s = stats[voice.group];
        ++s.playing;
        voice.pending = false;
        dirty = true;
        if (s.playing == 1 || s.playing % 32 == 0)
          SKSE::log::info(
              "Traversal native audio active: group={}, handle={}, "
              "duration={}, delay={:.3f}s (not an audibility measurement)",
              name(voice.group), voice.sound.soundID, voice.duration,
              voice.age);
      } else if (voice.probe == times.size()) {
        auto &s = stats[voice.group];
        voice.pending = false;
        dirty = true;
        if (voice.duration)
          ++s.durationKnown;
        else
          ++s.unobserved;

        if (s.unobserved + s.durationKnown <= 3 ||
            (s.unobserved + s.durationKnown) % 32 == 0)
          SKSE::log::warn(
              "Traversal audio observation inconclusive: group={}, handle={}, "
              "duration={}, probes={}; no native playing state sampled",
              name(voice.group), voice.sound.soundID, voice.duration,
              voice.probe);
      }
    }
  }
  /// Restart cue timing and log totals,
  /// letting sounds finish.
  void resetTiming() {
    timing.reset();
    summarize();
  }
  /// Stop every voice and log totals.
  void stop() {
    timing.reset();
    for (auto &voice : voices) {
      cancelProbe(voice);
      if (voice.sound.soundID != RE::BSSoundHandle::kInvalidID)
        voice.sound.Stop();
      voice = {};
    }
    summarize();
  }
  /// Run one frame: emit cues for the
  /// current motion and play them at
  /// the matching body height.
  ///
  /// Does nothing while disabled, muted
  /// or not installed.
  template <class Lib>
  void update(const Lib &library, const Traversal &traversal,
              const Result &result, float phase, float dt, bool outputReady) {
    if (!enabled || !installed || volume <= 0) {
      timing.reset();
      return;
    }
    const auto cues =
        timing.update(result.motion, phase, traversal.position,
                      library.contactWeights(result.motion, phase), dt,
                      outputReady, result.completed);
    for (unsigned i = 0; i < cues.count; ++i) {
      const auto cue = cues.items[i];
      const float height = cue.cue == SoundCue::grip   ? traversal.cfg.grip
                           : cue.cue == SoundCue::push ? 30.f
                                                       : 12.f;
      play(cue, traversal.position + Vec{0, 0, height});
    }
  }
};
} // namespace fc

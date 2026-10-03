#pragma once
//! Loading an animation pack: a
//! `pack.json` manifest that lists one
//! HKX clip and config per motion slot.


#include "animation/AnimationOverrides.h"

namespace fc
{
  /// Result of one slot, including the
  /// clip length in seconds.
  struct AnimationPackSlotResult
  {
    Motion motion{};
    std::string file, reason;
    OverrideStatus status = OverrideStatus::missing;
    std::size_t samples{};
    float seconds{};
  };
  /// Result of a pack load.
  ///
  /// `committed` is `true` only if
  /// every required slot loaded;
  /// otherwise `library` is left
  /// unchanged and `error` says why.
  struct AnimationPackReport
  {
    bool committed{};
    std::string error;
    std::vector<AnimationPackSlotResult> slots;
    std::size_t loaded{}, rejected{}, missing{}, inputBytes{}, outputBytes{};
  };
  /// Load the pack described by
  /// `manifest` into `library`, all or
  /// nothing.
  AnimationPackReport loadAnimationPack(Library &library,
                                        const std::filesystem::path &manifest,
                                        AnimationOverrideLimits limits = {});
} // namespace fc

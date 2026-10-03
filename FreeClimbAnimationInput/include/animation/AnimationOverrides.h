#pragma once
//! Loading loose HKX files that replace
//! built-in motions, with limits on
//! size and decode time.


#include "animation/MotionSlots.h"
#include "pose/Pose.h"
#include <filesystem>

namespace fc
{
  /// Resource limits for loading.
  ///
  /// - `fileBytes`/`totalBytes`: Input
  ///   size per file / in total.
  /// - `frames`: Frames per clip.
  /// - `totalOutputBytes`: Decoded pose
  ///   memory.
  /// - `fileMilliseconds`/
  ///   `totalMilliseconds`: Decode time
  ///   budgets.
  struct AnimationOverrideLimits
  {
    std::size_t fileBytes = 64 * 1024 * 1024, totalBytes = 256 * 1024 * 1024,
                frames = 1201, totalOutputBytes = 128 * 1024 * 1024;
    unsigned fileMilliseconds = 3000, totalMilliseconds = 30000;
  };
  /// Outcome for one motion slot.
  enum class OverrideStatus
  {
    missing,
    loaded,
    rejected
  };
  /// Result of one slot: its file, the
  /// status, the rejection `reason` and
  /// the decoded sample count.
  struct AnimationOverrideResult
  {
    Motion motion{};
    std::string file, reason;
    OverrideStatus status = OverrideStatus::missing;
    std::size_t samples{};
  };
  /// Results of all slots and totals.
  struct AnimationOverrideReport
  {
    std::vector<AnimationOverrideResult> slots;
    std::size_t loaded{}, rejected{}, missing{}, inputBytes{}, outputBytes{};
  };
  /// Load `<slot>.hkx` files from
  /// `directory` into `library`.
  ///
  /// A rejected file keeps the built-in
  /// motion for that slot.
  AnimationOverrideReport loadHkxOverrides(Library &library,
                                           const std::filesystem::path &directory,
                                           AnimationOverrideLimits limits = {});
} // namespace fc

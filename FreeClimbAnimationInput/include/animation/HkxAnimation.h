#pragma once
//! Decoder for Skyrim SE HKX animation
//! files (Havok 2010 packfiles).



#include "pose/Pose.h"
#include <span>

namespace fc
{
  /// A decoded clip.
  ///
  /// - `duration`: Length in seconds.
  /// - `identityMapping`: Tracks map
  ///   1:1 to skeleton bones.
  /// - `boneIndices`/`trackNames`:
  ///   Track to bone mapping.
  /// - `rotations`: Per frame, per
  ///   track rotations.
  /// - `frames`: Full poses per frame.
  struct HkxClip
  {
    float duration{};
    bool identityMapping{};
    std::string skeletonName;
    std::vector<int> boneIndices;
    std::vector<std::string> trackNames;
    std::vector<std::vector<Quat>> rotations;
    std::vector<Pose> frames;
  };
  /// Decode an HKX file.
  ///
  /// # Errors
  /// Returns `false` and sets `error`
  /// for unsupported or malformed data.
  bool decodeHkxAnimation(std::span<const std::uint8_t> bytes, HkxClip &clip,
                          std::string &error);
} // namespace fc

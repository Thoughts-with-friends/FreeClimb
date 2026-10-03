#pragma once
//! Decoder for Havok spline-compressed
//! animation data inside HKX files.



#include "pose/Pose.h"
#include <string>

namespace fc {
/// Raw spline-compressed animation:
/// block layout, timing and the packed
/// data bytes.
struct HkxSplineData {
  std::uint32_t tracks{}, numFrames{}, numBlocks{}, maxFramesPerBlock{},
      maskAndQuantizationSize{};
  float blockDuration{}, blockInverseDuration{}, frameDuration{};
  std::vector<std::uint32_t> blockOffsets, floatBlockOffsets, transformOffsets,
      floatOffsets;
  std::vector<std::uint8_t> data;
};
/// Decode full transforms (position,
/// rotation, scale) for every frame.
///
/// # Errors
/// Returns `false` and sets `error` for
/// malformed blocks.
bool decodeHkxSplineTransforms(const HkxSplineData &source,
                               std::vector<Pose> &frames, std::string &error);
/// Decode only rotations, per frame and
/// track.
bool decodeHkxSpline(const HkxSplineData &source,
                     std::vector<std::vector<Quat>> &rotations,
                     std::string &error);
} // namespace fc

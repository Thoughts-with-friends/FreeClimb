#pragma once
//! Read-only view of the engine's dense
//! animation pose buffer
//! (`hkaPose`-like track array).


#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fc {

/// Unaligned read of a `T` at `base +
/// offset`.
template <class T> T field(const void *base, std::size_t offset) {
  T value{};
  std::memcpy(&value, static_cast<const std::byte *>(base) + offset, sizeof(T));
  return value;
}
/// Pose transforms of one buffer:
/// `count` entries of 48 bytes
/// (translation, rotation, scale).
struct PoseTrack {
  std::byte *data{};
  int count{};
};
/// Locate the transforms in an engine
/// pose buffer.
///
/// Every header field is range checked
/// first (3-128 tracks, 48-byte stride,
/// aligned, positive weight).
///
/// # Returns
/// Empty when the buffer does not look
/// like a dense pose.
inline PoseTrack densePose(void *tracks) {
  if (!tracks)
    return {};
  const auto bytes = field<std::int32_t>(tracks, 0),
             count = field<std::int32_t>(tracks, 4);
  if (count < 3 || count > 128 || bytes < 16 + count * 16 || bytes > 1048576)
    return {};
  constexpr int header = 16 + 2 * 16;
  const int capacity = field<std::int16_t>(tracks, header),
            size = field<std::int16_t>(tracks, header + 2);
  const int offset = field<std::int16_t>(tracks, header + 4),
            stride = field<std::int16_t>(tracks, header + 6);
  const float weight = field<float>(tracks, header + 8);
  if (capacity < 1 || capacity > 1024 || size < 1 || size > capacity ||
      offset < 16 + count * 16 || stride != 48 ||
      offset + capacity * 48 > bytes || offset % 16 || !std::isfinite(weight) ||
      weight <= 0 || field<std::uint8_t>(tracks, header + 12) != 0 ||
      field<std::uint8_t>(tracks, header + 13) != 1)
    return {};
  return {static_cast<std::byte *>(tracks) + offset, size};
}
} // namespace fc

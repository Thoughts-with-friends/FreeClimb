#pragma once
//! Bundled traversal sounds and the
//! vanilla forms they play through.
//!
//! This replaces the `FC_*` sound
//! descriptors of the former
//! `FreeClimb.esp` (see
//! `FreeClimb.xml`), so no plugin file
//! is needed.






#include <array>
#include <cstdint>
#include <span>

namespace fc {

/// Sound groups `step`, `grip`, `push`
/// and `top`; paths are relative to
/// `Data`.
namespace sounds {

/// Footfalls on the wall or ground.
inline constexpr std::array step{
    "Sound\\fx\\FreeClimb\\step01.wav",
    "Sound\\fx\\FreeClimb\\step02.wav",
    "Sound\\fx\\FreeClimb\\step03.wav",
    "Sound\\fx\\FreeClimb\\step04.wav",
};

/// A hand catching a hold.
inline constexpr std::array grip{
    "Sound\\fx\\FreeClimb\\grip01.wav",
    "Sound\\fx\\FreeClimb\\grip02.wav",
    "Sound\\fx\\FreeClimb\\grip03.wav",
};

/// Pushing off for a hop or drop.
inline constexpr std::array push{
    "Sound\\fx\\FreeClimb\\push01.wav",
    "Sound\\fx\\FreeClimb\\push02.wav",
};

/// Finishing a climb over the top.
inline constexpr std::array top{
    "Sound\\fx\\FreeClimb\\top01.wav",
    "Sound\\fx\\FreeClimb\\top02.wav",
    "Sound\\fx\\FreeClimb\\top03.wav",
};

/// Playback priority (`BNAM` of the
/// former descriptors).
inline constexpr std::uint32_t priority = 128;

/// `Skyrim.esm` sound category the
/// former descriptors used (`GNAM`,
/// footsteps).
inline constexpr std::uint32_t category = 0x0F5FFC;

/// `Skyrim.esm` output model the former
/// descriptors used (`ONAM`, 3D
/// attenuated).
inline constexpr std::uint32_t output = 0x0428B6;

/// Files of one group.
///
/// # Params
/// - `group`: Audio slot index (0 step,
///   1 grip, 2 push, 3 top).
///
/// # Returns
/// Empty for an unknown group.
[[nodiscard]] inline std::span<const char *const> files(unsigned group) {
  switch (group) {
  case 0:
    return step;
  case 1:
    return grip;
  case 2:
    return push;
  case 3:
    return top;
  }
  return {};
}

/// Pick one file of a group, avoiding
/// the previous pick when possible.
///
/// # Params
/// - `group`: Audio slot index.
/// - `last`: Index picked last time for
///   this group; updated.
/// - `seed`: Any changing number, e.g.
///   a play counter.
///
/// # Returns
/// `nullptr` for an unknown group.
[[nodiscard]] inline const char *pick(unsigned group, unsigned &last,
                                      unsigned seed) {
  const auto list = files(group);
  if (list.empty()) {
    return nullptr;
  }

  auto index = static_cast<unsigned>((seed * 2654435761u >> 16) % list.size());
  if (list.size() > 1 && index == last) {
    index = (index + 1) % list.size();
  }
  last = index;
  return list[index];
}

} // namespace sounds
} // namespace fc

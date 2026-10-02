#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace fc {

/// Bundled traversal sound files, grouped as `step`, `grip`, `push`, `top`.
///
/// Same paths as the `ANAM` entries of the `FC_*` sound descriptors in
/// `FreeClimb.esp` (see `FreeClimb.xml`). Used when the ESP is not loaded.
namespace sounds {

inline constexpr std::array step{
    "Data\\Sound\\fx\\FreeClimb\\step01.wav",
    "Data\\Sound\\fx\\FreeClimb\\step02.wav",
    "Data\\Sound\\fx\\FreeClimb\\step03.wav",
    "Data\\Sound\\fx\\FreeClimb\\step04.wav",
};

inline constexpr std::array grip{
    "Data\\Sound\\fx\\FreeClimb\\grip01.wav",
    "Data\\Sound\\fx\\FreeClimb\\grip02.wav",
    "Data\\Sound\\fx\\FreeClimb\\grip03.wav",
};

inline constexpr std::array push{
    "Data\\Sound\\fx\\FreeClimb\\push01.wav",
    "Data\\Sound\\fx\\FreeClimb\\push02.wav",
};

inline constexpr std::array top{
    "Data\\Sound\\fx\\FreeClimb\\top01.wav",
    "Data\\Sound\\fx\\FreeClimb\\top02.wav",
    "Data\\Sound\\fx\\FreeClimb\\top03.wav",
};

/// Playback priority from the ESP descriptors (`BNAM priority`).
inline constexpr std::uint32_t priority = 128;

/// Files of one group. `group` is the audio slot index (0..3).
[[nodiscard]] inline std::span<const char* const> files(unsigned group) {
    switch (group) {
    case 0: return step;
    case 1: return grip;
    case 2: return push;
    case 3: return top;
    }
    return {};
}

/// Pick one file of a group, avoiding the previous pick when possible.
///
/// # Params
/// - `group` : Audio slot index (0..3).
/// - `last`  : Index returned last time for this group. Updated in place.
/// - `seed`  : Any changing number, e.g. a play counter.
[[nodiscard]] inline const char* pick(unsigned group, unsigned& last, unsigned seed) {
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

}
}

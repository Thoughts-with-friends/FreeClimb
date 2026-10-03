#pragma once
//! Remembers whether the engine ray
//! collector functions were verified
//! for the current physics world.




#include <cstdint>

namespace fc {
/// Verified engine functions.
///
/// - `add`: Native closest-hit add;
///   empty when verification failed.
/// - `pick`: Address of the world's
///   pick function it was checked
///   against.
template <class NativeAdd> struct RayCollectorValidation {
  NativeAdd add{};
  std::uintptr_t pick{};
};
/// Caches one validation per world.
///
/// Re-verifies when the world, its
/// vtable or its pick slot changes.
///
/// # Type params
/// - `WorldHandle`: Owning smart
///   pointer to the world.
/// - `NativeAdd`: Function pointer type
///   being verified.
template <class WorldHandle, class NativeAdd>
class RayCollectorValidationCache {
  WorldHandle world;
  std::uintptr_t table{}, pick{};
  NativeAdd add{};
  bool checked{};
  /// Drop the cached result.
  void invalidate() {
    table = pick = 0;
    add = {};
    checked = false;
  }

public:
  using World = typename WorldHandle::element_type;
  RayCollectorValidationCache() = default;
  RayCollectorValidationCache(const RayCollectorValidationCache &) = delete;
  RayCollectorValidationCache &
  operator=(const RayCollectorValidationCache &) = delete;
  /// Use world `value`; invalidates
  /// when it differs from the current
  /// one.
  void bind(World *value) {
    if (world.get() == value)
      return;
    invalidate();
    world = WorldHandle(value);
  }
  /// Forget the world and the result.
  void reset() {
    invalidate();
    world.reset();
  }
  /// Verified add function for the
  /// bound world.
  ///
  /// # Params
  /// - `currentTable`: World vtable
  ///   address now.
  /// - `readPick`: Reads the pick slot
  ///   of a vtable.
  /// - `verify`: Full verification, run
  ///   on a miss.
  ///
  /// # Returns
  /// Empty when no world is bound or
  /// verification failed.
  template <class ReadPick, class Verify>
  NativeAdd resolve(std::uintptr_t currentTable, ReadPick readPick,
                    Verify verify) {
    if (!world)
      return {};
    if (checked && table == currentTable && (!add || readPick(table) == pick))
      return add;
    invalidate();
    table = currentTable;
    const auto validated = verify(world.get(), table);
    add = validated.add;
    pick = validated.pick;
    checked = true;
    return add;
  }
};
} // namespace fc

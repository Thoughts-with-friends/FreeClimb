#pragma once
//! Tiny per-frame cache for values
//! derived from a collidable pointer.



#include <array>
#include <cstdint>

namespace fc {
/// Direct-mapped cache with 16 entries
/// keyed by address.
///
/// A colliding key simply replaces the
/// old entry, so lookups stay O(1) and
/// allocation free.
template <class Value> class RayCandidateCache {
  struct Entry {
    const void *key{};
    Value value{};
  };
  std::array<Entry, 16> entries{};

public:
  /// Cached value for `key`, computed
  /// with `compute()` on a miss.
  template <class Compute>
  const Value &resolve(const void *key, Compute compute) {
    const auto address = reinterpret_cast<std::uintptr_t>(key);
    auto &entry =
        entries[((address >> 4) ^ (address >> 11)) & (entries.size() - 1)];
    if (entry.key != key) {
      entry.value = compute();
      entry.key = key;
    }
    return entry.value;
  }
};
} // namespace fc

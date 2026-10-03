#pragma once
//! Memory checks that run before
//! touching game memory or installing a
//! vtable hook.
//!
//! They query page protection with
//! `VirtualQuery`, so a bad pointer
//! fails the check instead of crashing
//! the game.





#include "runtime/RuntimeVersion.h"
#include <cstring>
#include <limits>

namespace fc::runtime {
/// Whether `bytes` bytes starting at
/// `address` are committed and
/// readable.
///
/// Walks every memory region in the
/// range, so ranges that cross a page
/// boundary are fully checked.
inline bool readable(std::uintptr_t address, std::size_t bytes) {
  if (!address || !bytes ||
      bytes > std::numeric_limits<std::uintptr_t>::max() - address)
    return false;
  const auto end = address + bytes;
  while (address < end) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<const void *>(address), &info,
                      sizeof(info)) ||
        info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
      return false;
    const auto protection = info.Protect & 0xFF;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY)
      return false;
    const auto start = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - start)
      return false;
    const auto next = start + info.RegionSize;
    if (next <= address)
      return false;
    address = next;
  }
  return true;
}
/// Whether `address` points into
/// committed, executable memory.
inline bool callable(std::uintptr_t address) {
  MEMORY_BASIC_INFORMATION info{};
  if (!address ||
      !VirtualQuery(reinterpret_cast<const void *>(address), &info,
                    sizeof(info)) ||
      info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
    return false;
  const auto protection = info.Protect & 0xFF;
  return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
}
/// Whether `address` is callable code
/// inside the game executable's own
/// `.text` segments.
inline bool executable(std::uintptr_t address) {
  for (const auto name : {REL::Segment::textx, REL::Segment::textw}) {
    const auto segment = REL::Module::get().segment(name);
    if (address >= segment.address() &&
        address - segment.address() < segment.size())
      return callable(address);
  }
  return false;
}
/// Whether vtable `table` has a
/// readable entry at `slot` that points
/// to callable code.
///
/// Used before every vtable hook
/// install.
///
/// # Params
/// - `table`: Vtable address.
/// - `slot`: Entry index (`<= 0x1000`).
inline bool hookSite(std::uintptr_t table, std::size_t slot) {
  if (slot > 0x1000 || !readable(table, (slot + 1) * sizeof(std::uintptr_t)))
    return false;
  std::uintptr_t target{};
  std::memcpy(&target,
              reinterpret_cast<const void *>(table + slot * sizeof(target)),
              sizeof(target));
  return callable(target);
}
} // namespace fc::runtime

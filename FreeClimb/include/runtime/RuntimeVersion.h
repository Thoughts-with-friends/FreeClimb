#pragma once
//! Queries about the running game
//! executable, built on
//! `RuntimePolicy.h`.






#include "runtime/RuntimePolicy.h"

namespace fc::runtime {
/// Whether the loaded executable is a
/// supported runtime.
inline bool supported() {
  return supported(REL::Module::get().version().pack());
}
/// Whether the game is Skyrim SE 1.5.97
/// (the only pre-AE runtime).
inline bool isSE() {
  return REL::Module::get().version() == REL::Version(1, 5, 97, 0);
}
} // namespace fc::runtime

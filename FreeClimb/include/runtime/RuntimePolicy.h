#pragma once
//! Game runtime versions FreeClimb
//! supports, and the facts that differ
//! between them.
//!
//! Everything here is `constexpr` and
//! free of game headers, so tests can
//! check the version tables directly.
//!
//! # Packed versions
//! A version is packed into 32 bits as
//! `major.minor.patch.build` with 8, 8,
//! 12 and 4 bits, matching SKSE.






#include <array>
#include <cstddef>
#include <cstdint>

namespace fc::runtime {
/// Pack a game version into the SKSE
/// 32-bit layout.
///
/// # Example
/// ```cpp
/// pack(1, 6, 1170); // 1.6.1170.0
/// ```
constexpr std::uint32_t pack(unsigned major, unsigned minor, unsigned patch,
                             unsigned build = 0) {
  return (major << 24) | (minor << 16) | (patch << 4) | build;
}
/// Executable versions the plugin is
/// built and tested for (SE 1.5.97, AE
/// 1.6.x and 1.7.x).
///
/// Keep in sync with `RUNTIMES` in
/// `xmake.lua`.
inline constexpr std::array supportedVersions{
    pack(1, 5, 97),  pack(1, 6, 317),  pack(1, 6, 318),  pack(1, 6, 323),
    pack(1, 6, 342), pack(1, 6, 353),  pack(1, 6, 629),  pack(1, 6, 640),
    pack(1, 6, 659), pack(1, 6, 1130), pack(1, 6, 1170), pack(1, 6, 1179),
    pack(1, 7, 99),  pack(1, 7, 104)};
/// Version SKSE reports for a game
/// executable.
///
/// GOG builds 1.6.659 and 1.6.1179 are
/// reported with build 1; every other
/// version is unchanged.
constexpr std::uint32_t skseVersion(std::uint32_t version) {
  return version == pack(1, 6, 659) || version == pack(1, 6, 1179)
             ? version | 1u
             : version;
}
/// Inverse of `skseVersion`: map an
/// SKSE-reported version back to the
/// executable version.
constexpr std::uint32_t gameVersionFromSKSE(std::uint32_t version) {
  return version == pack(1, 6, 659, 1) || version == pack(1, 6, 1179, 1)
             ? version & ~15u
             : version;
}
/// `supportedVersions` as SKSE reports
/// them (GOG builds use build 1).
inline constexpr auto supportedSKSEVersions = [] {
  auto versions = supportedVersions;
  for (auto &version : versions)
    version = skseVersion(version);
  return versions;
}();
/// Whether SKSE reports one of the
/// supported runtimes.
constexpr bool supportedSKSE(std::uint32_t version) {
  for (const auto candidate : supportedSKSEVersions)
    if (version == candidate)
      return true;
  return false;
}
/// Runtime families with different
/// memory layouts or Address Library
/// formats.
///
/// - `se`: 1.5.97.
/// - `ae`: 1.6.317 to 1.6.353.
/// - `ae629`: 1.6.629 to 1.6.1179.
/// - `ae17`: 1.7.x.
enum class Family { unsupported, se, ae, ae629, ae17 };
/// Whether `version` is an exact match
/// for a supported executable.
constexpr bool supported(std::uint32_t version) {
  for (const auto candidate : supportedVersions)
    if (version == candidate)
      return true;
  return false;
}
/// Family of a packed executable
/// version, or `unsupported`.
constexpr Family family(std::uint32_t version) {
  if (!supported(version))
    return Family::unsupported;
  if (version == pack(1, 5, 97))
    return Family::se;
  if (version >= pack(1, 7, 99))
    return Family::ae17;
  return version >= pack(1, 6, 629) ? Family::ae629 : Family::ae;
}
/// `PlayerCharacter` vtable slot of the
/// per-frame actor update hook.
inline constexpr std::size_t actorUpdateSlot = 0xAD;
/// Input handler vtable slot that
/// decides whether an event is handled
/// (guarded while climbing).
inline constexpr std::size_t inputFilterSlot = 1;
/// Input handler vtable slot that
/// processes a button event.
inline constexpr std::size_t keyboardProcessSlot = 2;
/// Gamepad device vtable slot that
/// polls the controller state.
inline constexpr std::size_t gamepadPollSlot = 2;
/// Address Library file format for a
/// runtime.
///
/// # Returns
/// - `1` for SE (`version-*.bin`).
/// - `2` for AE 1.6
///   (`versionlib-*.bin`).
/// - `5` for AE 1.7.
/// - `0` when unsupported.
constexpr unsigned addressFormat(std::uint32_t version) {
  const auto kind = family(version);
  return kind == Family::se            ? 1
         : kind == Family::ae17        ? 5
         : kind == Family::unsupported ? 0
                                       : 2;
}
} // namespace fc::runtime

#pragma once
//! User settings stored in
//! `Data/SKSE/Plugins/FreeClimb.ini`.






#include "input/GamepadInput.h"
#include "input/InputBindings.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace fc {
/// Every user setting with its default.
///
/// Speeds are in game units per second;
/// times in seconds. Stamina values are
/// points per second
/// (`movingPerSecond`,
/// `hangingPerSecond`) or points
/// (`requiredToGrab`).
struct UserSettings {
  std::string language = "english";
  bool enabled = true, notifications = true, lowStaminaNotifications = true,
       jumpToAttach = true, autoMantle = true;
  bool contextActions = true, threepeatAnimations = true,
       automaticClimbActions = true, legacyAutomaticHops = false;
  bool surfaceActionVariants = true, wallRunObstacleJumps = true,
       contextualMantleEnabled = true, diagnostics = false, fancyJumps = true;
  bool audioEnabled = true, staminaEnabled = true;
  float upSpeed = 100, downSpeed = 78, sideSpeed = 82, wallRunSpeed = 379.5f,
        diagonalRunMultiplier = 1.15f;
  float autoActionMinSeconds = .8f, autoActionMaxSeconds = 1.25f,
        audioVolume = .75f;
  float hopOut = 32, kickOut = 52, reach = 110, grabMaxSnap = 60,
        groundJumpHeight = 88, maxNormalZ = .7f;
  float movingPerSecond = 10, hangingPerSecond = 0, requiredToGrab = 12;
  std::array<float, 2> automaticSideWeights{1, 1};
  InputBindings bindings;
  GamepadSettings gamepad;
};
/// Pages of the settings menu; each can
/// be reset on its own.
enum class SettingsPage {
  general,
  movement,
  automatic,
  stamina,
  audio,
  keys,
  diagnostics
};
/// Settings read from the ini.
///
/// `found` is `false` when the file was
/// missing; `warnings` lists ignored or
/// repaired values.
struct SettingsLoadResult {
  UserSettings settings;
  bool found = false;
  std::vector<std::string> warnings;
};
/// Clamp every value to its valid range
/// and replace invalid bindings with
/// the defaults.
UserSettings sanitizeUserSettings(UserSettings settings);
/// Copy of `current` with the fields of
/// one menu page reset to defaults.
UserSettings restoreSettingsPage(SettingsPage page,
                                 const UserSettings &current);
/// Read and sanitize the ini at `path`.
/// A missing file yields the defaults
/// and a warning.
SettingsLoadResult loadUserSettings(const std::filesystem::path &path);
/// Write `settings` to `path`.
///
/// # Errors
/// Returns `false` and sets `error` for
/// invalid bindings or a failed write.
bool saveUserSettings(const std::filesystem::path &path,
                      const UserSettings &settings, std::string &error);
/// Ini text for `settings`.
std::string userSettingsIni(const UserSettings &settings);
} // namespace fc

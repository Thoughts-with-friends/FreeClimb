#pragma once
//! Optional in-game settings menu,
//! shown through SKSE Menu Framework
//! when that mod is installed.
//!
//! The menu runs on the UI thread and
//! talks to the plugin only through
//! `SettingsMenuCallbacks`.





#include "settings/UserSettings.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace RE {
class InputEvent;
}

namespace fc {
/// Status of one animation slot shown
/// in the diagnostics page.
///
/// - `status`: Validation state code.
/// - `triggers`/`observed`: Times the
///   motion was requested / seen
///   playing.
struct MenuAnimationSlot {
  std::string name, file, reason;
  int status = 0;
  std::uint64_t triggers = 0;
  std::uint64_t observed = 0;
  std::size_t samples = 0;
  float seconds = 0;
};
/// Read-only plugin state the menu
/// displays.
struct SettingsMenuSnapshot {
  bool ready = false, traversalActive = false, movementPending = false,
       reloadPending = false;
  bool audioReady = false, diagnosticsEnabled = false;
  std::string status, error, packName;
  std::uint64_t automaticAttempts = 0, automaticActions = 0,
                wallRunObstacleJumps = 0;
  std::vector<MenuAnimationSlot> slots;
};
/// Plugin hooks the menu calls.
///
/// - `getSettings`: Current settings.
/// - `snapshot`: Current state.
/// - `requestSave`: Apply and save.
/// - `requestAudio`: Live audio switch
///   and volume.
/// - `requestReloadAnimations`: Reload
///   the animation pack.
struct SettingsMenuCallbacks {
  std::function<UserSettings()> getSettings;
  std::function<SettingsMenuSnapshot()> snapshot;
  std::function<void(UserSettings)> requestSave;
  std::function<void(bool, float)> requestAudio;
  std::function<void()> requestReloadAnimations;
};
/// Register the menu with SKSE Menu
/// Framework.
///
/// # Returns
/// `false` if the framework or one of
/// its functions is missing, or
/// `getSettings`/`requestSave` is
/// empty. Repeated calls succeed.
bool registerSettingsMenu(SettingsMenuCallbacks callbacks);
/// Whether a framework window that
/// blocks game input is open.
bool settingsMenuBlocking();
/// Feed the DirectInput keyboard state
/// (256 bytes) to a running key
/// capture.
void settingsMenuKeyboardSample(const std::uint8_t *keys);
/// Feed the XInput state to a running
/// button capture.
void settingsMenuGamepadSample(bool available, std::uint16_t buttons = 0,
                               std::uint8_t leftTrigger = 0,
                               std::uint8_t rightTrigger = 0);
/// Whether a key or button event must
/// be hidden from the game because a
/// capture is using it.
bool settingsMenuFilterInput(RE::InputEvent *event);
} // namespace fc

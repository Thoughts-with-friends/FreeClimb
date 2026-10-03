#pragma once
//! Experimental controller support:
//! button chords, stick to direction
//! mapping, and which button events are
//! hidden from the game while climbing.
//!
//! Buttons are indexed 0-15 in the
//! order of `gamepadButtonNames`
//! (D-pad, Start, Back, sticks,
//! bumpers, face buttons, triggers).





#include "input/InputBindings.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fc {
/// Bit set of held buttons; bit `i` is
/// button index `i`.
using GamepadChord = std::uint16_t;
/// Ini name of a button index.
struct GamepadButtonName {
  std::string_view name;
  unsigned index;
};
/// Names of all 16 buttons, by index.
inline constexpr std::array<GamepadButtonName, 16> gamepadButtonNames{
    {{"DPadUp", 0},
     {"DPadDown", 1},
     {"DPadLeft", 2},
     {"DPadRight", 3},
     {"Start", 4},
     {"Back", 5},
     {"LS", 6},
     {"RS", 7},
     {"LB", 8},
     {"RB", 9},
     {"A", 10},
     {"B", 11},
     {"X", 12},
     {"Y", 13},
     {"LT", 14},
     {"RT", 15}}};
/// Bit for button `index`, or 0 when
/// out of range.
inline constexpr GamepadChord gamepadButtonMask(unsigned index) {
  return index < 16 ? static_cast<GamepadChord>(1u << index) : GamepadChord{};
}
/// Whether a chord can be bound: 1 to 4
/// buttons, without Start or Back
/// (reserved for menus).
inline bool validGamepadChord(GamepadChord chord) {
  return chord && !(chord & (gamepadButtonMask(4) | gamepadButtonMask(5))) &&
         std::popcount(chord) <= 4;
}
/// Parse `"LB+Y"`-style text
/// (case-insensitive).
///
/// # Returns
/// `None` for unknown or repeated
/// buttons, invalid chords, or text
/// longer than 128 chars.
inline std::optional<GamepadChord> parseGamepadChord(std::string_view text) {
  GamepadChord result{};
  if (text.size() > 128)
    return {};
  while (!text.empty()) {
    const auto plus = text.find('+');
    const auto name = trimKeyName(text.substr(0, plus));
    if (name.empty())
      return {};
    GamepadChord button{};
    for (const auto &named : gamepadButtonNames)
      if (keyNameEqual(name, named.name)) {
        button = gamepadButtonMask(named.index);
        break;
      }
    if (!button || (result & button))
      return {};
    result = static_cast<GamepadChord>(result | button);
    if (plus == std::string_view::npos)
      break;
    text.remove_prefix(plus + 1);
    if (text.empty())
      return {};
  }
  if (!validGamepadChord(result))
    return {};
  return result;
}
/// Text form of a chord in button
/// order, or empty if invalid.
inline std::string serializeGamepadChord(GamepadChord chord) {
  if (!validGamepadChord(chord))
    return {};
  std::string result;
  for (const auto &named : gamepadButtonNames)
    if (chord & gamepadButtonMask(named.index)) {
      if (!result.empty())
        result += '+';
      result += named.name;
    }
  return result;
}
/// Controller actions and their
/// defaults.
///
/// - `entry`: LB+Y.
/// - `runModifier`: LB.
/// - `hop`: Y.
/// - `drop`: B (let go of the wall).
struct GamepadBindings {
  GamepadChord entry = gamepadButtonMask(8) | gamepadButtonMask(13);
  GamepadChord runModifier = gamepadButtonMask(8), hop = gamepadButtonMask(13),
               drop = gamepadButtonMask(11);
  static GamepadBindings defaults() { return {}; }
  bool operator==(const GamepadBindings &) const = default;
};
/// Ini name of a gamepad binding and
/// its member pointer.
struct GamepadBindingField {
  std::string_view name;
  GamepadChord GamepadBindings::*member;
};
/// Gamepad bindings in ini order.
inline constexpr std::array<GamepadBindingField, 4> gamepadBindingFields{
    {{"entry", &GamepadBindings::entry},
     {"runModifier", &GamepadBindings::runModifier},
     {"hop", &GamepadBindings::hop},
     {"drop", &GamepadBindings::drop}}};
/// Whether holding `a` holds every
/// button of non-empty `b`.
inline bool gamepadChordImplies(GamepadChord a, GamepadChord b) {
  return b && (a & b) == b;
}
/// Check gamepad bindings: all valid,
/// no action (entry excepted) implying
/// another, and entry not containing
/// drop.
inline BindingValidation
validateGamepadBindings(const GamepadBindings &bindings) {
  for (const auto &field : gamepadBindingFields)
    if (!validGamepadChord(bindings.*field.member))
      return {
          false, "Invalid gamepad combination", std::string(field.name), {}};
  for (unsigned i = 1; i < gamepadBindingFields.size(); ++i)
    for (unsigned j = i + 1; j < gamepadBindingFields.size(); ++j) {
      const auto a = bindings.*gamepadBindingFields[i].member,
                 b = bindings.*gamepadBindingFields[j].member;
      if (gamepadChordImplies(a, b) || gamepadChordImplies(b, a))
        return {false, "Conflicting gamepad combinations",
                std::string(gamepadBindingFields[i].name),
                std::string(gamepadBindingFields[j].name)};
    }
  if (gamepadChordImplies(bindings.entry, bindings.drop))
    return {false, "Entry cannot include the complete drop binding", "entry",
            "drop"};
  return {true, {}, {}, {}};
}
/// Controller settings from the ini.
///
/// - `deadzone`: Stick radius ignored
///   (0.1-0.8).
/// - `triggerThreshold`: Trigger travel
///   that counts as pressed (0.1-0.95).
struct GamepadSettings {
  bool enabled = true;
  float deadzone = .25f, triggerThreshold = .5f;
  GamepadBindings bindings;
  bool operator==(const GamepadSettings &) const = default;
};
/// Clamp ranges and fall back to
/// defaults for invalid values or
/// bindings.
inline GamepadSettings sanitizeGamepadSettings(GamepadSettings settings) {
  settings.deadzone = std::isfinite(settings.deadzone)
                          ? std::clamp(settings.deadzone, .1f, .8f)
                          : .25f;
  settings.triggerThreshold =
      std::isfinite(settings.triggerThreshold)
          ? std::clamp(settings.triggerThreshold, .1f, .95f)
          : .5f;
  if (!validateGamepadBindings(settings.bindings).valid)
    settings.bindings = GamepadBindings{};
  return settings;
}
/// Current controller state: held
/// buttons and the left stick as
/// -1/0/+1 per axis.
///
/// After `reset` or
/// `blockUntilButtonsReleased`, no keys
/// are reported until every button is
/// up.
class GamepadState {
  GamepadChord buttons{};
  int horizontal{}, vertical{};
  bool stickActive{}, blocked{};
  /// Quantize one normalized stick axis
  /// with hysteresis: engage at 0.45,
  /// hold until below 0.35.
  static int axis(float value, int previous) {
    const float magnitude = std::abs(value);
    const int direction = value > 0 ? 1 : value < 0 ? -1 : 0;
    if (direction == previous && magnitude >= .35f)
      return previous;
    return magnitude >= .45f ? direction : 0;
  }

public:
  /// Set one button from an analog
  /// value. Triggers (14, 15) must
  /// reach `triggerThreshold`.
  void setButton(unsigned index, float value, float triggerThreshold) {
    if (index >= gamepadButtonNames.size())
      return;
    const float threshold = std::isfinite(triggerThreshold)
                                ? std::clamp(triggerThreshold, .1f, .95f)
                                : .5f;
    const bool down =
        std::isfinite(value) && value > 0 && (index < 14 || value >= threshold);
    const auto button = gamepadButtonMask(index);
    buttons = static_cast<GamepadChord>(down ? (buttons | button)
                                             : (buttons & ~button));
  }
  /// Set the left stick. Inside the
  /// deadzone both axes are neutral;
  /// outside, the direction is
  /// normalized before quantizing.
  void setStick(float x, float y, float deadzone) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
      horizontal = vertical = 0;
      stickActive = false;
      return;
    }
    x = std::clamp(x, -1.f, 1.f);
    y = std::clamp(y, -1.f, 1.f);
    const float threshold =
        std::isfinite(deadzone) ? std::clamp(deadzone, .1f, .8f) : .25f;
    const float magnitude = std::hypot(x, y);
    stickActive = magnitude > threshold;
    if (!stickActive) {
      horizontal = vertical = 0;
      return;
    }
    horizontal = axis(x / magnitude, horizontal);
    vertical = axis(y / magnitude, vertical);
  }
  /// Load a raw XInput sample.
  ///
  /// Maps XInput button bits (skipping
  /// the two unused bits 10-11) and
  /// scales triggers and stick to `[0,
  /// 1]` and `[-1, 1]`.
  void sampleXInput(std::uint16_t buttonBits, std::uint8_t leftTrigger,
                    std::uint8_t rightTrigger, std::int16_t lx, std::int16_t ly,
                    const GamepadSettings &settings) {
    for (unsigned index = 0; index < 14; ++index) {
      const unsigned bit = index < 10 ? index : index + 2;
      setButton(index, (buttonBits & (1u << bit)) ? 1.f : 0.f,
                settings.triggerThreshold);
    }
    setButton(14, static_cast<float>(leftTrigger) / 255.f,
              settings.triggerThreshold);
    setButton(15, static_cast<float>(rightTrigger) / 255.f,
              settings.triggerThreshold);
    const auto normalized = [](std::int16_t value) {
      return static_cast<float>(value) / (value < 0 ? 32768.f : 32767.f);
    };
    setStick(normalized(lx), normalized(ly), settings.deadzone);
  }
  /// Clear the state and block until
  /// all buttons are released.
  void reset() {
    buttons = 0;
    horizontal = vertical = 0;
    stickActive = false;
    blocked = true;
  }
  /// Report no keys until every button
  /// is released.
  void blockUntilButtonsReleased() { blocked = true; }
  /// Unblock once no button is held.
  ///
  /// # Returns
  /// `true` when not blocked.
  bool resumeIfButtonsReleased() {
    if (!buttons)
      blocked = false;
    return !blocked;
  }
  /// Whether input is blocked.
  bool waitingForButtonsRelease() const { return blocked; }
  /// Whether button `index` is held.
  bool held(unsigned index) const {
    return (buttons & gamepadButtonMask(index)) != 0;
  }
  /// No button held and the stick at
  /// rest.
  bool neutral() const { return !buttons && !stickActive; }
  /// Whether a valid chord is fully
  /// held.
  bool heldChord(GamepadChord chord) const {
    return validGamepadChord(chord) && gamepadChordImplies(buttons, chord);
  }
  /// Climbing `Keys` from the stick and
  /// bound chords (empty while
  /// blocked).
  Keys keys(const GamepadBindings &bindings) const {
    Keys result;
    result.bindingsMapped = true;
    if (blocked)
      return result;
    result.w = vertical > 0;
    result.a = horizontal < 0;
    result.s = vertical < 0;
    result.d = horizontal > 0;
    result.shift = heldChord(bindings.runModifier);
    result.space = heldChord(bindings.hop);
    result.entry = heldChord(bindings.entry);
    result.letGo = heldChord(bindings.drop);
    return result;
  }
};
/// Whether button `index` is part of a
/// fully held binding while attached
/// and not blocked.
inline bool ownsGamepadButton(const GamepadState &state,
                              const GamepadBindings &bindings, unsigned index,
                              bool attached) {
  if (!attached || index >= gamepadButtonNames.size() ||
      state.waitingForButtonsRelease())
    return false;
  for (const auto &field : gamepadBindingFields) {
    const auto chord = bindings.*field.member;
    if ((chord & gamepadButtonMask(index)) && state.heldChord(chord))
      return true;
  }
  return false;
}
/// Gamepad counterpart of
/// `InputOwnership`: hides button
/// events that belong to a climbing
/// binding until they are released.
class GamepadOwnership {
  std::array<bool, 16> owned{}, native{};

public:
  /// Decide whether one button event is
  /// hidden from the game.
  ///
  /// # Returns
  /// `true` to swallow the event.
  bool filter(unsigned index, bool isDown, bool isUp, bool attached,
              const GamepadState &before, const GamepadState &after,
              const GamepadBindings &bindings) {
    if (index >= owned.size())
      return false;
    if (isUp) {
      const bool consume = owned[index] && !native[index];
      owned[index] = native[index] = false;
      return consume;
    }
    if (isDown)
      owned[index] = native[index] = false;
    const bool active = ownsGamepadButton(before, bindings, index, attached) ||
                        ownsGamepadButton(after, bindings, index, attached);
    if (active)
      owned[index] = true;
    const bool consume = owned[index];
    if (isDown && !consume)
      native[index] = true;
    return consume;
  }
  /// Whether `index` was pressed while
  /// owned by the game.
  bool nativeDown(unsigned index) const {
    return index < native.size() && native[index];
  }
  /// Whether the game saw this button
  /// go down (used to detect a vanilla
  /// jump press).
  bool startedNativeJump(unsigned index) const { return nativeDown(index); }
  /// Forget all ownership.
  void reset() {
    owned.fill(false);
    native.fill(false);
  }
};
} // namespace fc

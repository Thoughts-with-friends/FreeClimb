#pragma once
//! Records a new key or button
//! combination for the settings menu
//! ("press the keys to bind").
//!
//! # Flow
//! 1. `begin` waits until all keys are
//! released (`waiting`). 2. Then it is
//! `armed`; pressed keys grow the
//! chord. 3. Releasing everything
//! finishes it as `captured`.
//!
//! Esc or the gamepad Back button
//! cancels.




#include "input/GamepadInput.h"
#include <bitset>

namespace fc {
/// Device a capture listens to.
enum class BindingCaptureDevice { keyboard, gamepad };
/// Progress of a capture.
///
/// - `idle`: Not started.
/// - `waiting`: Waiting for all keys to
///   be released.
/// - `armed`: Recording presses.
/// - `captured`: Chord is ready.
/// - `error`: See
///   `BindingCaptureError`.
/// - `cancelled`: User cancelled.
enum class BindingCaptureStatus {
  idle,
  waiting,
  armed,
  captured,
  error,
  cancelled
};
/// Why a capture failed.
///
/// - `invalid`: Chord cannot be bound.
/// - `tooMany`: More than 4 keys.
/// - `notSimultaneous`: A key was
///   released before the chord was
///   complete.
enum class BindingCaptureError { none, invalid, tooMany, notSimultaneous };
/// Tracks which keys and buttons the
/// capture swallowed, so their release
/// is swallowed too.
///
/// Slots 0-255 are keyboard scan codes,
/// 256-271 gamepad buttons.
class BindingCaptureOwnership {
  std::bitset<272> owned;
  /// Bit index of a key or button, or
  /// 272 when out of range.
  static unsigned slot(BindingCaptureDevice device, unsigned code) {
    const bool keyboard = device == BindingCaptureDevice::keyboard;
    return code < (keyboard ? 256u : 16u) ? code + (keyboard ? 0u : 256u)
                                          : 272u;
  }

public:
  /// Decide whether an input event must
  /// be hidden from the game.
  ///
  /// A press is owned while capturing;
  /// its release is hidden even after
  /// the capture ended.
  ///
  /// # Returns
  /// `true` to swallow the event.
  bool filter(BindingCaptureDevice device, unsigned code, bool isDown,
              bool isUp, bool capturing) {
    const auto index = slot(device, code);
    if (index >= owned.size())
      return capturing;
    const bool previous = owned[index];
    if (isUp) {
      owned[index] = false;
      return previous;
    }
    if (isDown) {
      owned[index] = capturing;
      return capturing;
    }
    return capturing || previous;
  }
  /// Whether any owned key is still
  /// held.
  bool pending() const { return owned.any(); }
  /// Whether `code` is currently owned.
  bool owns(BindingCaptureDevice device, unsigned code) const {
    const auto index = slot(device, code);
    return index < owned.size() && owned[index];
  }
  /// Release ownership of every key.
  void reset() { owned.reset(); }
};
/// Input state sampled once per frame
/// for `BindingCapture`.
///
/// `activationHeld` is the menu button
/// that started the capture; it must be
/// released first.
struct BindingCaptureSnapshot {
  InputState keyboard{};
  GamepadChord gamepad{};
  bool keyboardValid{}, gamepadValid{}, activationHeld{};
};
/// State and outcome of a capture.
///
/// `preview` is the chord as text, e.g.
/// `"W+A+D+Space"`.
struct BindingCaptureResult {
  BindingCaptureStatus status = BindingCaptureStatus::idle;
  BindingCaptureDevice device = BindingCaptureDevice::keyboard;
  BindingCaptureError error = BindingCaptureError::none;
  KeyChord keyboard{};
  GamepadChord gamepad{};
  std::string preview;
};
/// State machine that turns pressed
/// keys into a `KeyChord` or a
/// `GamepadChord`.
class BindingCapture {
  BindingCaptureResult value;
  std::bitset<256> candidate;
  /// End the capture with `status`,
  /// keeping only the device.
  const BindingCaptureResult &
  finish(BindingCaptureStatus status,
         BindingCaptureError error = BindingCaptureError::none) {
    const auto device = value.device;
    value = {};
    value.device = device;
    value.status = status;
    value.error = error;
    candidate.reset();
    return value;
  }

public:
  /// Start capturing on `device`.
  const BindingCaptureResult &begin(BindingCaptureDevice device) {
    value = {};
    value.device = device;
    value.status = BindingCaptureStatus::waiting;
    candidate.reset();
    return value;
  }
  /// Feed one frame of input.
  ///
  /// Grows the chord while keys are
  /// added, fails if a key is dropped
  /// early or the chord is invalid, and
  /// completes when all are released.
  const BindingCaptureResult &sample(const BindingCaptureSnapshot &snapshot) {
    if (!active())
      return value;
    if (snapshot.keyboardValid && snapshot.keyboard.held(0x01))
      return cancel();
    const bool keyboard = value.device == BindingCaptureDevice::keyboard;
    if (snapshot.gamepadValid && (snapshot.gamepad & gamepadButtonMask(5)))
      return cancel();
    if (keyboard ? !snapshot.keyboardValid : !snapshot.gamepadValid)
      return value;
    std::bitset<256> pressed;
    if (keyboard) {
      for (std::size_t i = 0; i < snapshot.keyboard.down.size(); ++i)
        pressed[i] = snapshot.keyboard.down[i];
    } else {
      for (unsigned i = 0; i < 16; ++i)
        pressed[i] = (snapshot.gamepad & gamepadButtonMask(i)) != 0;
    }
    if (value.status == BindingCaptureStatus::waiting) {
      if (pressed.none() && !snapshot.activationHeld)
        value.status = BindingCaptureStatus::armed;
      return value;
    }
    if (pressed.none()) {
      if (candidate.any())
        value.status = BindingCaptureStatus::captured;
      return value;
    }
    if (pressed.count() > 4)
      return finish(BindingCaptureStatus::error, BindingCaptureError::tooMany);
    KeyChord chord;
    if (keyboard) {
      for (unsigned i = 0; i < 256; ++i)
        if (pressed[i])
          chord.keys[chord.count++] = static_cast<KeyCode>(i);
      if (!validKeyChord(chord))
        return finish(BindingCaptureStatus::error,
                      BindingCaptureError::invalid);
    } else if (!validGamepadChord(snapshot.gamepad))
      return finish(BindingCaptureStatus::error, BindingCaptureError::invalid);
    if ((pressed & ~candidate).none())
      return value;
    if ((pressed & candidate) != candidate)
      return finish(BindingCaptureStatus::error,
                    BindingCaptureError::notSimultaneous);
    candidate = pressed;
    if (keyboard) {
      std::sort(chord.keys.begin(), chord.keys.begin() + chord.count,
                [](KeyCode a, KeyCode b) {
                  const bool am = keyGroup(a) >= anyShift,
                             bm = keyGroup(b) >= anyShift;
                  return am != bm ? am : a < b;
                });
      value.keyboard = chord;
      value.preview = serializeKeyChord(chord);
    } else {
      value.gamepad = snapshot.gamepad;
      value.preview = serializeGamepadChord(snapshot.gamepad);
    }
    return value;
  }
  /// Cancel the capture.
  const BindingCaptureResult &cancel() {
    return finish(BindingCaptureStatus::cancelled);
  }
  /// Latest result.
  const BindingCaptureResult &result() const { return value; }
  /// Whether the capture is still
  /// `waiting` or `armed`.
  bool active() const {
    return value.status == BindingCaptureStatus::waiting ||
           value.status == BindingCaptureStatus::armed;
  }
  /// Return to `idle`.
  void reset() {
    value = {};
    candidate.reset();
  }
};
} // namespace fc

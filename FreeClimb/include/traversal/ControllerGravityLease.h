#pragma once
//! Turns off gravity on the player's
//! character controller while climbing,
//! and always gives it back.



namespace fc {

/// Saves a controller's gravity and
/// sets it to 0 while held.
///
/// If the game swaps the controller
/// mid-climb, the saved value moves to
/// the new one.
template <class Controller> class ControllerGravityLease {
  Controller *owner{};
  float baseline{};
  bool suppressed{};
  /// Whether gravity is still the 0 we
  /// set.
  bool ownsZero() const { return owner && suppressed && owner->gravity == 0.f; }

public:
  /// What `release` restored.
  ///
  /// - `owned`: The held controller.
  /// - `replacement`: A new controller
  ///   that inherited our 0.
  struct Release {
    bool owned{}, replacement{};
  };
  /// Gravity to restore.
  float savedGravity() const { return baseline; }
  /// Whether a controller is held.
  bool active() const { return owner != nullptr; }
  /// Hold controller `next`, restoring
  /// the previous one first.
  void take(Controller *next) {
    if (next == owner)
      return;
    const bool inherited = ownsZero() && next && next->gravity == 0.f;
    const float nextBaseline =
        inherited ? baseline : (next ? next->gravity : 0.f);
    if (ownsZero())
      owner->gravity = baseline;
    owner = next;
    baseline = nextBaseline;
    suppressed = false;
  }
  /// Set gravity to 0.
  void suppress() {
    if (owner) {
      owner->gravity = 0.f;
      suppressed = true;
    }
  }
  /// Restore gravity and let go.
  ///
  /// # Params
  /// - `current`: The controller the
  ///   player uses now (may differ).
  Release release(Controller *current) {
    Release result;

    if (ownsZero()) {
      owner->gravity = baseline;
      result.owned = true;
      if (current && current != owner && current->gravity == 0.f) {
        current->gravity = baseline;
        result.replacement = true;
      }
    }
    owner = nullptr;
    baseline = 0;
    suppressed = false;
    return result;
  }
};
} // namespace fc

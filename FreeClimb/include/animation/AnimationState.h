#pragma once
//! Marker for "FreeClimb owns the
//! player's animation graph".




namespace fc
{

  /// Set while FreeClimb drives the
  /// player's animation graph.
  ///
  /// Replaces the `FC_AnimationState`
  /// global of the former
  /// `FreeClimb.esp`; only this plugin
  /// used it, so it lives in memory.
  ///
  /// # Persistence
  /// Not saved. `release()` runs on
  /// save and before load and already
  /// resets `bIsSynced`.
  class AnimationState
  {
  public:
    /// Mark the player as climbing.
    void set() { on = true; }

    /// Mark the player as released.
    void clear() { on = false; }

    /// Whether the player is marked as
    /// climbing.
    [[nodiscard]] bool active() const { return on; }

  private:
    bool on{};
  };

} // namespace fc

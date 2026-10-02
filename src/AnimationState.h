#pragma once

namespace fc {

/// Marker that is non-zero while FreeClimb drives the player's animation graph.
///
/// This used to live only in `FreeClimb.esp` (`FC_AnimationState`, `0x800`).
/// The DLL now owns the value, so climbing works without the ESP.
/// If the ESP is loaded, the value is mirrored into its global for compatibility.
///
/// # Persistence
/// With the ESP, the global is stored in the save, so `active()` reads it back
/// after `kPostLoadGame` (old behaviour). Without the ESP there is no saved
/// marker; `release()` on `kSaveGame` already resets `bIsSynced` before saving.
template<class Global>
class AnimationState {
public:
    /// Attach the optional ESP global. `nullptr` means DLL-only mode.
    void bind(Global* form) {
        global = form;
        sync();
    }

    /// Mark the player as climbing.
    void set() {
        value = active_value;
        sync();
    }

    /// Mark the player as released.
    void clear() {
        value = 0.f;
        sync();
    }

    /// Whether the player is marked as climbing.
    /// Reads the ESP global when bound, since a loaded save may have restored it.
    [[nodiscard]] bool active() const { return global ? global->value != 0.f : value != 0.f; }

    /// Whether the ESP global was found.
    [[nodiscard]] bool bound() const { return global != nullptr; }

private:
    static constexpr float active_value = 100.f;

    /// Copy the in-memory value to the ESP global, if any.
    void sync() {
        if (global) {
            global->value = value;
        }
    }

    float value{};
    Global* global{};
};

}

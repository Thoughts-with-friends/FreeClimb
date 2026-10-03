#pragma once
//! Turns held keys into climbing input,
//! and decides when a climb entry or a
//! jump grab is requested.


#include "traversal/Core.h"
namespace fc {
/// Abstract climbing keys of one frame
/// (keyboard or gamepad).
///
/// - `w`/`a`/`s`/`d`: Directions.
/// - `shift`: Run modifier.
/// - `space`: Hop.
/// - `entry`: Entry chord held (when
///   `bindingsMapped`).
/// - `letGo`: Gamepad drop button.
struct Keys {
  bool w{}, a{}, s{}, d{}, shift{}, space{}, entry{}, bindingsMapped{}, letGo{};
};

/// Climbing `Input` from keys.
///
/// Let go, or A+S+D+Space, releases the
/// wall. Otherwise W/S/A/D give the
/// direction, S+Space drops off, Space
/// hops, W mantles (if `autoMantle`)
/// and Shift runs.
///
/// # Params
/// - `spacePressed`: Space went down
///   this frame.
/// - `justAttached`: Ignore a Space
///   press that started the climb.
/// - `wasWallRunning`: Space does not
///   hop right after a wall run.
inline Input wallInput(Keys k, bool spacePressed, bool autoMantle = true,
                       bool justAttached = false, bool wasWallRunning = false) {
  if (k.letGo || (k.a && k.s && k.d && (spacePressed || k.space)))
    return {0, 0, true, false, false, false, false};
  spacePressed = spacePressed && !justAttached;
  return {float(k.d) - float(k.a),
          float(k.w) - float(k.s),
          spacePressed && k.s,
          autoMantle && k.w,
          spacePressed && !k.s && !k.shift && !wasWallRunning,
          k.s,
          k.shift && !k.s};
}
/// Whether the entry chord is held
/// (W+A+D+Space when unmapped).
inline bool entryChord(Keys k) {
  return k.bindingsMapped ? k.entry : k.w && k.a && k.d && k.space;
}
/// Entry chord held without S.
inline bool approachIntent(Keys k) { return entryChord(k) && !k.s; }
/// Ignores Shift that was already held
/// when the climb began, until it is
/// released.
class WallRunEntryGate {
  bool heldAtEntry{};

public:
  /// Remember whether Shift is held at
  /// entry.
  void begin(Keys keys) { heldAtEntry = keys.shift; }
  /// Stop ignoring Shift.
  void reset() { heldAtEntry = false; }
  /// `keys` with the entry Shift
  /// removed.
  Keys filter(Keys keys) {
    heldAtEntry = heldAtEntry && keys.shift;
    keys.shift = keys.shift && !heldAtEntry;
    return keys;
  }
};
/// Set the default W/A/S/D/Space key
/// for scan code `scan`.
inline void keyboardKey(Keys &k, unsigned scan, bool pressed) {
  switch (scan) {
  case 0x11:
    k.w = pressed;
    break;
  case 0x1e:
    k.a = pressed;
    break;
  case 0x1f:
    k.s = pressed;
    break;
  case 0x20:
    k.d = pressed;
    break;
  case 0x39:
    k.space = pressed;
    break;
  }
}
/// Entry chord state of one frame.
///
/// - `requested`: Chord held.
/// - `fresh`/`began`: First frame of
///   the gesture.
/// - `airborneAtBegin`: The gesture
///   started in the air.
struct ClimbEntryRequest {
  bool requested{}, fresh{}, began{}, airborneAtBegin{};
};
/// Tracks the entry gesture.
///
/// After a climb, or while S is held,
/// the chord must be released before it
/// counts again.
class ClimbEntryIntent {
  bool blocked{}, gesture{}, originAirborne{};

public:
  /// Ignore the chord until released.
  void blockUntilRelease() {
    blocked = true;
    gesture = originAirborne = false;
  }
  /// Feed one frame.
  ///
  /// # Params
  /// - `attached`/`suspended`: Already
  ///   climbing, or input is blocked;
  ///   both block until release.
  /// - `confirmedAirborne`: The player
  ///   is really in the air.
  ClimbEntryRequest sample(Keys k, bool attached = false,
                           bool suspended = false, float = 1.f / 60,
                           bool confirmedAirborne = false) {
    if (attached || suspended) {
      blockUntilRelease();
      return {};
    }
    const bool chord = entryChord(k);
    if (blocked) {
      if (!chord)
        blocked = false;
      return {};
    }
    if (k.s) {
      blockUntilRelease();
      return {};
    }
    if (!chord) {
      gesture = originAirborne = false;
      return {};
    }
    const bool began = !gesture;
    if (began) {
      gesture = true;
      originAirborne = confirmedAirborne;
    }
    return {true, began, began, originAirborne};
  }
  /// Whether the chord is blocked.
  bool waitingForRelease() const { return blocked; }
};
/// Keeps an entry alive for 150 ms
/// while the pose binding prepares,
/// unless the device changes or S / let
/// go is pressed.
class EntryPreparationGrace {
  float remaining{};
  bool gamepad{};

public:
  /// Start the grace period.
  void arm(bool fromGamepad) {
    remaining = .15f;
    gamepad = fromGamepad;
  }
  /// End the grace period.
  void cancel() { remaining = 0; }
  /// Advance by `dt`.
  ///
  /// # Returns
  /// Whether the grace is still active.
  bool sample(float dt, bool fromGamepad, Keys keys) {
    if (!std::isfinite(dt) || dt <= 0 || fromGamepad != gamepad || keys.s ||
        keys.letGo) {
      cancel();
      return false;
    }
    remaining = std::max(0.f, remaining - dt);
    return remaining > 0;
  }
};
/// Treats a vanilla jump press as fresh
/// for 150 ms.
class NativeJumpIntent {
  bool down{};
  float remaining{};

public:
  /// Feed the jump key.
  ///
  /// # Returns
  /// Whether the press is still fresh.
  bool sample(bool held, float dt) {
    if (!held) {
      down = false;
      remaining = 0;
      return false;
    }
    if (!down) {
      down = true;
      remaining = .15f;
    } else if (std::isfinite(dt) && dt > 0)
      remaining = std::max(0.f, remaining - std::min(dt, .1f));
    return remaining > 0;
  }
};

/// Player flight state for a jump grab.
///
/// - `airborne`: In the air or jumping
///   by any signal.
/// - `descending`: Falling (or not
///   rising) in the air.
/// - `confirmedAirborne`: The engine
///   says so (not just the graph).
struct GrabFlight {
  bool airborne{}, descending{};
  float verticalSpeed{};
  bool confirmedAirborne{};
};
/// Combine engine and graph signals
/// into a `GrabFlight`.
inline GrabFlight grabFlight(bool inAir, bool jumping, bool jumpGraph,
                             bool nativeJump, float verticalSpeed) {
  const float speed = std::isfinite(verticalSpeed) ? verticalSpeed : 0.f;
  const bool air = inAir || jumping || jumpGraph || nativeJump;
  return {air, air && (speed < -.5f || (inAir && !jumping && speed <= .5f)),
          speed, (inAir || jumping) && std::isfinite(verticalSpeed)};
}
/// Ledge catch when falling, jump catch
/// otherwise.
inline Motion grabEntryMotion(GrabFlight flight) {
  return flight.descending ? Motion::ledgeCatch : Motion::jumpCatch;
}
/// Window of 0.8 s in which a jump may
/// grab a wall in `facing` direction.
class JumpGrabGate {
  float remaining{};
  Vec direction{0, 1, 0};
  bool nativeJump{}, airRequest{};

public:
  /// Open the window.
  ///
  /// # Params
  /// - `facing`: Horizontal direction;
  ///   invalid input cancels.
  /// - `alreadyJumped`: The game has
  ///   started its own jump.
  /// - `airborneRequest`: Requested
  ///   while already in the air.
  void request(Vec facing = {0, 1, 0}, bool alreadyJumped = false,
               bool airborneRequest = false) {
    facing.z = 0;
    if (!facing.finite() || facing.length() < .9f) {
      cancel();
      return;
    }
    remaining = .8f;
    direction = facing.unit();
    nativeJump = alreadyJumped;
    airRequest = airborneRequest;
  }
  /// Keep the window open while the
  /// chord is held; `fresh` refreshes
  /// the air request.
  void hold(Vec facing, bool alreadyJumped, bool airborneRequest, bool fresh) {

    facing.z = 0;
    if (!facing.finite() || facing.length() < .9f) {
      cancel();
      return;
    }
    nativeJump = alreadyJumped;
    if (fresh)
      airRequest = airborneRequest;
    else if (!pending())
      airRequest = false;
    remaining = .8f;
    direction = facing.unit();
  }
  /// Close the window.
  void cancel() {
    remaining = 0;
    nativeJump = airRequest = false;
  }
  /// Count down; `suspended` cancels.
  void tick(float dt, bool suspended = false) {
    if (suspended) {
      cancel();
      return;
    }
    if (std::isfinite(dt) && dt > 0) {
      remaining = std::max(0.f, remaining - std::clamp(dt, 0.f, .05f));
      if (remaining <= 0)
        cancel();
    }
  }
  /// Whether the window is open.
  bool pending() const { return remaining > 0; }
  /// Grab direction.
  Vec facing() const { return direction; }
  /// Whether the game jumped too.
  bool startedNativeJump() const { return nativeJump; }
  /// Requested in the air and still
  /// airborne: allowed despite the
  /// cooldown.
  bool explicitAirCatch(GrabFlight current) const {
    return pending() && airRequest && current.confirmedAirborne;
  }
  /// Whether a grab may happen now.
  bool permitted(float cooldown, GrabFlight current) const {
    return pending() && (cooldown <= 0 || explicitAirCatch(current));
  }
};

/// Decides whether Space events reach
/// the game (so it does not also jump).
class SpacePressOwnership {
  bool owned{}, native{};

public:
  /// Claim the current Space press.
  void reserve() { owned = true; }
  /// Whether the game got the press.
  bool startedNativeJump() const { return native; }
  /// Decide one Space event.
  ///
  /// # Returns
  /// `true` to hide it from the game.
  bool filter(bool isDown, bool isUp, bool attached) {
    const bool consume = (owned || attached) && !(isUp && native);
    if (isDown && !consume)
      native = true;
    if (isUp) {
      owned = native = false;
    }
    return consume;
  }
  /// Forget ownership once Space is up.
  void synchronize(bool held) {
    if (!held)
      owned = native = false;
  }
};

/// Unlink every event for which
/// `remove` is true from an intrusive
/// list, fixing `head` and `tail`.
template <class Event, class Predicate>
void removeInputEvents(Event *&head, Event *&tail, Predicate remove) {
  Event *previous = nullptr;
  for (Event *event = head; event;) {
    Event *next = event->next;
    if (remove(event)) {
      if (previous)
        previous->next = next;
      else
        head = next;
      if (tail == event)
        tail = previous;
    } else
      previous = event;
    event = next;
  }
}
} // namespace fc

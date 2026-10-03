#pragma once
//! Which ray hits count as solid
//! geometry when probing walls.
//!
//! The engine reports the player's own
//! body and non-solid trigger volumes
//! too; those must not become climbing
//! surfaces.



#include <cstdint>
#include <utility>

namespace fc {
/// Havok collision response of the hit
/// body.
enum class RayResponse : std::uint8_t {
  unknown,
  simpleContact,
  reporting,
  none
};
/// What to do with a hit.
enum class RayDecision : std::uint8_t { keep, ignorePlayer, ignoreTrigger };
/// Facts about one hit, gathered from
/// the engine.
///
/// - `exactPlayer`: The player's own
///   collidable.
/// - `actorZone`: Hit is on the
///   ActorZone layer.
/// - `entity`/`phantom`: Body kind.
/// - `actor`: Belongs to an actor.
/// - `primitiveActivatorWithoutModel`:
///   Invisible activator volume.
struct RayHitFacts {
  bool exactPlayer{}, actorZone{}, entity{}, phantom{}, actor{};
  bool primitiveActivatorWithoutModel{};
  RayResponse response = RayResponse::unknown;
};
/// Classify a hit.
///
/// # Rules
/// - The player is always ignored.
/// - Outside ActorZone, or on an actor,
///   the hit is kept.
/// - ActorZone entities that only
///   report contacts, and invisible
///   activator phantoms, are triggers
///   and ignored.
constexpr RayDecision rayHitDecision(const RayHitFacts &hit) {
  if (hit.exactPlayer)
    return RayDecision::ignorePlayer;

  if (!hit.actorZone || hit.actor)
    return RayDecision::keep;
  if (hit.entity && (hit.response == RayResponse::reporting ||
                     hit.response == RayResponse::none))
    return RayDecision::ignoreTrigger;
  if (hit.phantom && hit.primitiveActivatorWithoutModel)
    return RayDecision::ignoreTrigger;
  return RayDecision::keep;
}

/// Classify a hit and call `accept`
/// when it is kept.
template <class Accept>
RayDecision dispatchRayHit(const RayHitFacts &hit, Accept &&accept) {
  const auto decision = rayHitDecision(hit);
  if (decision == RayDecision::keep)
    std::forward<Accept>(accept)();
  return decision;
}
} // namespace fc

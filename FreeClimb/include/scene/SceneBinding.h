#pragma once
//! Binds the 99 tracks of the canonical
//! skeleton to scene graph nodes of the
//! loaded character.
//!
//! Some tracks are owned by the engine
//! (cameras, equipment) or exist only
//! in animations; they are counted but
//! never bound.


#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace fc {

/// Whether track `i` is one of the two
/// camera leaves the engine drives.
///
/// # Params
/// - `names`/`parents`: The 99 bones of
///   the canonical skeleton.
inline bool engineOwnedCameraTrack(std::size_t i,
                                   std::span<const std::string> names,
                                   std::span<const int> parents) {
  if (names.size() != 99 || parents.size() != 99 || i >= 99 ||
      std::find(parents.begin(), parents.end(), int(i)) != parents.end())
    return false;
  return (i == 97 && names[i] == "Camera3rd [Cam3]" && parents[i] == -1) ||
         (i == 98 && names[i] == "Camera Control" && parents[i] == 0);
}

/// Whether track `i` is a weapon,
/// shield or quiver attachment leaf the
/// engine drives.
///
/// # Params
/// - `names`/`parents`: The 99 bones of
///   the canonical skeleton.
inline bool engineOwnedEquipmentTrack(std::size_t i,
                                      std::span<const std::string> names,
                                      std::span<const int> parents) {
  struct Leaf {
    std::size_t index;
    std::string_view name;
    int parent;
  };
  constexpr std::array<Leaf, 9> equipment = {
      Leaf{42, "Shield", 38},  {43, "Weapon", 39},     {60, "Quiver", 26},
      {61, "WeaponAxe", 5},    {62, "WeaponBack", 26}, {63, "WeaponBow", 26},
      {64, "WeaponDagger", 5}, {65, "WeaponMace", 5},  {66, "WeaponSword", 5}};
  if (names.size() != 99 || parents.size() != 99 || i >= 99 ||
      std::find(parents.begin(), parents.end(), int(i)) != parents.end())
    return false;
  return std::any_of(equipment.begin(), equipment.end(), [&](const Leaf &leaf) {
    return i == leaf.index && names[i] == leaf.name &&
           parents[i] == leaf.parent;
  });
}

/// Whether bone `name` is an
/// engine-owned equipment leaf.
inline bool engineOwnedEquipmentName(std::string_view name,
                                     std::span<const std::string> names,
                                     std::span<const int> parents) {
  if (names.size() != 99 || parents.size() != 99)
    return false;
  for (std::size_t i = 0; i < names.size(); ++i)
    if (name == names[i] && engineOwnedEquipmentTrack(i, names, parents))
      return true;
  return false;
}

/// Whether the engine, not FreeClimb,
/// drives track `i`.
inline bool engineOwnedTrack(std::size_t i, std::span<const std::string> names,
                             std::span<const int> parents) {
  return engineOwnedCameraTrack(i, names, parents) ||
         engineOwnedEquipmentTrack(i, names, parents);
}

/// Whether track `i` is a helper leaf
/// (look, translate, rotate) that may
/// have no scene node.
inline bool animationOnlyLeaf(std::size_t i, std::span<const std::string> names,
                              std::span<const int> parents) {
  struct Leaf {
    std::size_t index;
    std::string_view name;
    int parent;
  };
  constexpr std::array<Leaf, 3> optional = {Leaf{1, "x_NPC LookNode [Look]", 0},
                                            {2, "x_NPC Translate [Pos ]", 0},
                                            {3, "x_NPC Rotate [Rot ]", 0}};
  if (names.size() != 99 || parents.size() != 99 || i >= 99 ||
      std::find(parents.begin(), parents.end(), int(i)) != parents.end())
    return false;
  return std::any_of(optional.begin(), optional.end(), [&](const Leaf &leaf) {
    return i == leaf.index && names[i] == leaf.name &&
           parents[i] == leaf.parent;
  });
}
/// Scene nodes bound to the canonical
/// tracks.
///
/// - `nodes`: Node per track, or empty.
/// - `missing`: First track that failed
///   to bind, or -1.
/// - `count`/`virtualLeaves`/
///   `unownedTracks`: How each track
///   was resolved; they sum to 99 when
///   complete.
template <class Node> struct SceneBinding {
  std::array<Node, 99> nodes{};
  int missing = -1;
  std::size_t count{}, virtualLeaves{}, unownedTracks{};
  /// Whether every track was resolved
  /// and the root is bound.
  explicit operator bool() const {
    return missing < 0 && nodes[0] &&
           count + virtualLeaves + unownedTracks == nodes.size();
  }
  /// Call `function(index, node)` for
  /// every bound track.
  template <class Function> void each(Function function) const {
    for (std::size_t i = 0; i < nodes.size(); ++i)
      if (nodes[i])
        function(i, nodes[i]);
  }
};
/// Bind each track with `lookup(name)`.
///
/// Stops at the first required track
/// that cannot be found and records it
/// in `missing`.
template <class Node, class Lookup>
SceneBinding<Node> bindScene(std::span<const std::string> names,
                             std::span<const int> parents, Lookup lookup) {
  SceneBinding<Node> result;
  if (names.size() != 99 || parents.size() != 99) {
    result.missing = 0;
    return result;
  }
  for (std::size_t i = 0; i < 99; ++i) {
    if (engineOwnedTrack(i, names, parents)) {
      ++result.unownedTracks;
      continue;
    }
    result.nodes[i] = lookup(names[i]);
    if (result.nodes[i])
      ++result.count;
    else if (animationOnlyLeaf(i, names, parents))
      ++result.virtualLeaves;
    else {
      result.missing = int(i);
      return result;
    }
  }
  return result;
}
} // namespace fc

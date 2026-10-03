#pragma once
//! Rules for keeping the written pose
//! alive through the engine's scene
//! graph update passes.
//!
//! The engine recomputes world
//! transforms in several virtual
//! passes; FreeClimb hooks them so its
//! pose is applied before children are
//! updated.


#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace fc {
/// Engine update passes that are
/// hooked.
enum class ScenePass : unsigned { downward, selected, rigid, transformOnly };
/// `NiAVObject` vtable slot of a pass
/// (`UpdateDownwardPass` 0x2C to
/// `UpdateTransformAndBounds` 0x31).
constexpr std::size_t scenePassSlot(ScenePass pass) {
  return pass == ScenePass::transformOnly ? 0x31
                                          : 0x2C + static_cast<unsigned>(pass);
}
/// `NiAVObject` vtable slot of
/// `UpdateWorldData`.
constexpr std::size_t sceneWorldSlot() { return 0x30; }
/// Whether a flattened bone list is
/// usable: at most 4096 entries, and
/// every node-less entry has a parent
/// earlier in the list.
template <class Entry, std::size_t Extent>
bool validFlatParents(std::span<Entry, Extent> entries) {
  if (entries.size() > 4096)
    return false;
  for (std::size_t i = 0; i < entries.size(); ++i)
    if (!entries[i].node && entries[i].parentIndex >= 0 &&
        std::size_t(entries[i].parentIndex) >= i)
      return false;
  return true;
}
/// Recompute world transforms of a
/// flattened bone list in order.
///
/// Real nodes copy their world
/// transform; virtual entries compose
/// their parent's world with their
/// local transform.
///
/// # Returns
/// `false` if the parents are invalid.
template <class Entry, std::size_t Extent, class Transform, class Compose>
bool synchronizeFlatEntries(std::span<Entry, Extent> entries,
                            const Transform &rootWorld, Compose compose) {
  if (!validFlatParents(entries))
    return false;
  for (auto &entry : entries) {
    if (entry.node)
      entry.world = entry.node->world;
    else
      entry.world = compose(
          entry.parentIndex >= 0 ? entries[entry.parentIndex].world : rootWorld,
          entry.local);
  }
  return true;
}
/// Whether a pass must reapply the pose
/// on owned nodes.
constexpr bool overlaysPose(ScenePass pass, bool owned) {
  return owned &&
         (pass == ScenePass::downward || pass == ScenePass::selected ||
          pass == ScenePass::rigid || pass == ScenePass::transformOnly);
}
/// Whether the flattened tree must be
/// refreshed after the pass.
constexpr bool refreshFlatAfterPass(ScenePass pass, bool owned) {
  return owned && pass == ScenePass::transformOnly;
}
/// Node flags while FreeClimb owns a
/// node: selective update and
/// transforms on, rigid update off.
constexpr std::uint32_t ownedSceneNodeFlags(std::uint32_t flags) {
  return (flags | std::uint32_t{6}) & ~std::uint32_t{16};
}

/// Unmapped nodes between the mapped
/// bones and `root` (e.g. extra parents
/// added by other mods).
///
/// # Returns
/// `None` if a mapped node does not
/// lead to `root` within 256 steps.
template <class Node, class Parent>
std::optional<std::vector<Node>> sceneBridgeNodes(std::span<const Node> mapped,
                                                  Node root, Parent parent) {
  if (!root)
    return std::nullopt;
  std::vector<Node> bridges;
  for (auto node : mapped) {
    if (!node)
      continue;
    unsigned depth = 0;
    while (node && node != root && depth++ < 256) {
      node = parent(node);
      if (node && node != root &&
          std::find(mapped.begin(), mapped.end(), node) == mapped.end() &&
          std::find(bridges.begin(), bridges.end(), node) == bridges.end())
        bridges.push_back(node);
    }
    if (node != root)
      return std::nullopt;
  }
  return bridges;
}
/// Update flags passed on by an owned
/// selected pass, with bit 1 cleared.
constexpr std::uint32_t
effectiveUpdateDataFlags(ScenePass pass, std::uint32_t flags, bool owned) {

  return owned && pass == ScenePass::selected ? flags & ~std::uint32_t{2}
                                              : flags;
}
} // namespace fc

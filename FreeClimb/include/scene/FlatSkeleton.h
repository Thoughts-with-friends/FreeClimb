#pragma once
//! Access to `BSFlattenedBoneTree`, the
//! engine's flat bone array used by
//! character skeletons, and binding of
//! the canonical tracks to it.


#include "runtime/RuntimeSupport.h"
#include "scene/SceneBinding.h"
#include "scene/ScenePropagation.h"
#include <cstring>
namespace fc {
using FlatBoneEntry = RE::BSFlattenedBoneTree::BoneEntry;
/// `root` as a flattened bone tree, or
/// `nullptr` if it is not one.
inline RE::BSFlattenedBoneTree *flatTree(RE::NiAVObject *root) {
  const auto *rtti = root ? root->GetRTTI() : nullptr;
  return rtti && std::string_view(rtti->GetName()) == "BSFlattenedBoneTree"
             ? static_cast<RE::BSFlattenedBoneTree *>(root)
             : nullptr;
}
/// Bone entries of a flattened tree.
///
/// # Returns
/// - Empty span when `root` is not
///   flattened.
/// - `None` when the entries are
///   unreadable or invalid.
inline std::optional<std::span<FlatBoneEntry>>
flatEntries(RE::NiAVObject *root) {
  auto *tree = flatTree(root);
  if (!tree)
    return std::span<FlatBoneEntry>{};
  auto &data = tree->GetRuntimeData();
  if (data.numBones > 4096 || (!data.boneEntries && data.numBones))
    return std::nullopt;
  if (data.numBones &&
      !runtime::readable(reinterpret_cast<std::uintptr_t>(data.boneEntries),
                         data.numBones * sizeof(FlatBoneEntry)))
    return std::nullopt;
  std::span<FlatBoneEntry> entries(data.boneEntries, data.numBones);
  return validFlatParents(entries) ? std::optional(entries) : std::nullopt;
}
/// Engine function that recomputes a
/// flattened tree's world transforms.
using FlatWorldRefresh = void (*)(RE::NiAVObject *);
/// Verified engine refresh (SE only);
/// `nullptr` uses the portable
/// fallback.
inline FlatWorldRefresh flatWorldRefresh{};
/// Find and verify the engine refresh
/// function.
///
/// On SE the address and code bytes
/// must match; AE uses the fallback.
///
/// # Returns
/// `false` on unsupported runtimes or
/// an SE mismatch.
inline bool initializeFlatWorldRefresh() {
  flatWorldRefresh = nullptr;
  if (!runtime::supported())
    return false;
  if (!runtime::isSE())
    return true;
  const auto address = REL::ID(69363).address();
  constexpr std::string_view signature =
      "\x40\x56\x48\x83\xec\x60\x8b\x81\x28\x01\x00\x00\x48\x8b\xf1\x85\xc0\x0f\x84\xe8\x00\x00\x00"sv;
  if (address != REL::Module::get().base() + 0xC6A010 ||
      !runtime::executable(address) ||
      !runtime::readable(address, signature.size()) ||
      std::memcmp(reinterpret_cast<const void *>(address), signature.data(),
                  signature.size()) != 0) {
    SKSE::log::error(
        "SE flattened world refresh ABI mismatch; pose hooks not installed");
    return false;
  }
  flatWorldRefresh = reinterpret_cast<FlatWorldRefresh>(address);
  return true;
}
/// Recompute world transforms of a
/// flattened tree.
///
/// # Returns
/// `true` for non-flattened roots;
/// `false` if the entries are invalid.
inline bool refreshFlatWorld(RE::NiAVObject *root) {
  if (!flatTree(root))
    return true;
  const auto entries = flatEntries(root);
  if (!entries)
    return false;
  if (flatWorldRefresh) {
    flatWorldRefresh(root);
    return true;
  }
  return synchronizeFlatEntries(
      *entries, root->world,
      [](const auto &parent, const auto &local) { return parent * local; });
}
static_assert(sizeof(FlatBoneEntry) == 0x80);
static_assert(offsetof(FlatBoneEntry, node) == 0x70 &&
              offsetof(FlatBoneEntry, nodeName) == 0x78);
/// One bound track: a real node, a
/// virtual flattened entry, or both.
struct SceneSlot {
  RE::NiPointer<RE::NiAVObject> node;
  RE::NiTransform *flat{};
  RE::NiTransform *flatWorld{};
  RE::NiTransform *flatParentWorld{};
  /// Whether anything is bound.
  explicit operator bool() const {
    return node.get() != nullptr || flat != nullptr;
  }
  /// Writable local transform.
  RE::NiTransform &local() const { return flat ? *flat : node->local; }
  /// Current world transform.
  const RE::NiTransform &world() const {
    return node ? node->world : *flatWorld;
  }
  /// World transform the slot gets with
  /// local transform `authored`.
  RE::NiTransform expectedWorld(const RE::NiTransform &authored) const {
    if (node)
      return node->parent ? node->parent->world * authored : authored;
    return flatParentWorld ? *flatParentWorld * authored : authored;
  }
  /// Whether the flattened cache agrees
  /// with the node's world transform.
  bool cacheMatches() const {
    if (!node || !flatWorld)
      return true;

    return *flatWorld == node->world;
  }
};
/// Depth-first search for a node by
/// name (at most 128 levels).
inline RE::NiAVObject *existingNode(RE::NiAVObject *root, std::string_view name,
                                    int depth = 0) {
  if (!root || depth > 128)
    return nullptr;
  if (root->name.c_str() && name == root->name.c_str())
    return root;
  if (auto n = root->AsNode())
    for (auto &child : n->GetChildren())
      if (auto found = existingNode(child.get(), name, depth + 1))
        return found;
  return nullptr;
}
/// Bind the canonical tracks to the
/// character under `root`.
///
/// Lookup order: the root itself,
/// flattened entries, then any node in
/// the subtree.
inline SceneBinding<SceneSlot>
bindRuntimeScene(RE::NiAVObject *root, std::span<const std::string> names,
                 std::span<const int> parents) {
  const auto entries = flatEntries(root);
  if (!entries) {
    SceneBinding<SceneSlot> invalid;
    invalid.missing = 0;
    return invalid;
  }
  return bindScene<SceneSlot>(names, parents, [&](const std::string &name) {
    if (root && name == root->name.c_str())
      return SceneSlot{RE::NiPointer<RE::NiAVObject>(root), nullptr};
    for (auto &entry : *entries)
      if (entry.nodeName.c_str() && name == entry.nodeName.c_str()) {
        auto *parentWorld =
            entry.parentIndex >= 0 &&
                    std::size_t(entry.parentIndex) < entries->size()
                ? &(*entries)[entry.parentIndex].world
                : &root->world;
        return entry.node
                   ? SceneSlot{RE::NiPointer<RE::NiAVObject>(entry.node),
                               nullptr, &entry.world, parentWorld}
                   : SceneSlot{{}, &entry.local, &entry.world, parentWorld};
      }
    return SceneSlot{RE::NiPointer<RE::NiAVObject>(existingNode(root, name)),
                     nullptr};
  });
}
} // namespace fc

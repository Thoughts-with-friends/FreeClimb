#pragma once
//! Adapts poses authored on the
//! canonical skeleton to the actual
//! character, whose bone lengths,
//! scales or helper bases may differ
//! (e.g. body mods).


#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace fc {
/// Per-bone retargeting from the
/// authored skeleton to the live one.
///
/// Translations keep the authored
/// offset from rest (except the root
/// and COM, which move freely), scales
/// keep their ratio, and bones with a
/// non-identity basis are composed with
/// it.
template <class PoseType> class PoseRig {
public:
  /// Canonical bone count.
  static constexpr std::size_t count = 99;
  using Local = typename PoseType::value_type;

private:
  PoseType original, native;
  std::array<Local, count> bases{};
  std::array<bool, count> mapped{};
  bool enabled{};
  /// Exact component equality.
  template <class Vector> static bool same(Vector a, Vector b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }
  /// Whether a transform is exactly the
  /// identity.
  static bool identity(const Local &value) {
    return same(value.t, decltype(value.t){}) &&
           same(value.s, decltype(value.s){1, 1, 1}) && value.q.x == 0 &&
           value.q.y == 0 && value.q.z == 0 && std::abs(value.q.w) == 1;
  }

public:
  /// Whether three axes form a
  /// right-handed orthonormal basis
  /// (within 0.02).
  template <class Vector> static bool validAxes(Vector x, Vector y, Vector z) {
    return x.finite() && y.finite() && z.finite() &&
           std::abs(x.dot(x) - 1) < .02f && std::abs(y.dot(y) - 1) < .02f &&
           std::abs(z.dot(z) - 1) < .02f && std::abs(x.dot(y)) < .02f &&
           std::abs(x.dot(z)) < .02f && std::abs(y.dot(z)) < .02f &&
           x.cross(y).dot(z) > .98f;
  }
  /// Whether a local transform is sane:
  /// finite, offset below 1000 units,
  /// unit rotation, scale 0.1-5.
  static bool validLocal(const Local &value) {
    const auto norm = value.q.dot(value.q);
    return value.t.finite() && value.t.length() < 1000 && std::isfinite(norm) &&
           std::abs(norm - 1) < .01f && value.s.finite() && value.s.x >= .1f &&
           value.s.y >= .1f && value.s.z >= .1f && value.s.x <= 5 &&
           value.s.y <= 5 && value.s.z <= 5;
  }
  /// Whether `reference` still matches
  /// the structure `captured` when the
  /// rig was configured.
  static bool currentStructure(const Local &live, const Local &reference,
                               const Local &captured) {
    return validLocal(live) && validLocal(reference) &&
           same(reference.t, captured.t) && same(reference.s, captured.s) &&
           reference.q.x == captured.q.x && reference.q.y == captured.q.y &&
           reference.q.z == captured.q.z && reference.q.w == captured.q.w;
  }
  /// `child` expressed in the space of
  /// `parent` (inverse of compose).
  static Local relative(const Local &parent, const Local &child) {
    const auto delta = parent.q.inverse().rotate(child.t - parent.t);
    return {{delta.x / parent.s.x, delta.y / parent.s.y, delta.z / parent.s.z},
            (parent.q.inverse() * child.q).unit(),
            {child.s.x / parent.s.x, child.s.y / parent.s.y,
             child.s.z / parent.s.z}};
  }
  /// Live transform with the reference
  /// translation where the rig does not
  /// follow it (root, COM, unlocked).
  static Local structuralReference(std::size_t bone, Local live,
                                   const Local &reference,
                                   bool translationLocked) {
    if (bone == 0 || bone == 4 || !translationLocked)
      live.t = reference.t;
    return live;
  }
  /// Capture the authored rest pose,
  /// the live rest pose and per-bone
  /// bases.
  ///
  /// The rig is only `active()` when a
  /// selected bone actually differs.
  ///
  /// # Returns
  /// `false` (rig unchanged) if any
  /// transform is invalid.
  bool configure(const PoseType &source, const PoseType &actual,
                 const std::array<Local, count> &basis,
                 const std::array<bool, count> &selected) {
    if (source.size() != count || actual.size() != count)
      return false;
    bool changed = false;
    for (std::size_t i = 0; i < count; ++i) {
      if (!validLocal(source[i]))
        return false;
      if (!selected[i])
        continue;
      if (!validLocal(actual[i]) || !validLocal(basis[i]))
        return false;
      changed |= (i != 0 && i != 4 && !same(actual[i].t, source[i].t)) ||
                 !same(actual[i].s, source[i].s) || !identity(basis[i]);
    }
    original = source;
    native = actual;
    bases = basis;
    mapped = selected;
    enabled = changed;
    return true;
  }
  /// Whether adaptation is needed.
  bool active() const { return enabled; }
  /// Authored rest pose.
  const PoseType &source() const { return original; }
  /// Authored rest pose adapted to the
  /// live skeleton.
  PoseType reference() const {
    auto result = original;
    adapt(result);
    return result;
  }
  /// Apply the bone's basis to a local
  /// transform.
  Local toEffective(std::size_t bone, const Local &local) const {
    return enabled && bone < count && mapped[bone] && !identity(bases[bone])
               ? compose(bases[bone], local)
               : local;
  }
  /// Remove the bone's basis (inverse
  /// of `toEffective`).
  Local toLocal(std::size_t bone, const Local &effective) const {
    return enabled && bone < count && mapped[bone] && !identity(bases[bone])
               ? relative(bases[bone], effective)
               : effective;
  }
  /// Apply the bone's basis to a
  /// rotation only.
  template <class Rotation>
  Rotation rotation(std::size_t bone, Rotation value) const {
    return enabled && bone < count && mapped[bone] && !identity(bases[bone])
               ? (bases[bone].q * value).unit()
               : value;
  }
  /// Adapt a whole authored pose to the
  /// live skeleton in place.
  void adapt(PoseType &pose) const {
    if (!enabled || pose.size() != count)
      return;
    for (std::size_t i = 0; i < count; ++i)
      if (mapped[i]) {
        if (i != 0 && i != 4)
          pose[i].t = native[i].t + (pose[i].t - original[i].t);
        pose[i].s = {native[i].s.x * (pose[i].s.x / original[i].s.x),
                     native[i].s.y * (pose[i].s.y / original[i].s.y),
                     native[i].s.z * (pose[i].s.z / original[i].s.z)};
        pose[i] = toEffective(i, pose[i]);
      }
  }
  /// Reset to an inactive rig.
  void clear() { *this = {}; }
};
} // namespace fc

#pragma once
//! Finding hand holds: a horizontal lip
//! both hands can grip, or a flat wall
//! patch for the Threepeat hang.
//!
//!  Part of `Core.h` (included inside
//! `namespace fc`).



/// A hold for both hands.
///
/// - `center`: Middle of the hold.
/// - `normal`: Horizontal outward
///   normal of the face.
/// - `hands`: Left / right palm points.
/// - `wallPatch`: A flat wall, not a
///   lip.
struct GripEdge {
  Vec center{}, normal{};
  std::array<Vec, 2> hands{};
  bool wallPatch{};
};

/// Probe helpers for edge search.
namespace gripEdgeDetail {
/// Default half distance between the
/// hands.
inline constexpr float handHalfSpacing = 16.f;
/// How far the palm sits behind the
/// lip.
inline constexpr float palmInset = 3.f;
/// Depth of top surface that must
/// support the palm.
inline constexpr float stripDepth = 6.f;
/// Ray budget of one search.
inline constexpr unsigned queryLimit = 96;
/// Counts rays against `queryLimit`;
/// rays past it return no hit.
struct Queries {
  World &world;
  unsigned count{};
  std::optional<Hit> ray(Vec from, Vec to) {
    if (count >= queryLimit)
      return {};
    ++count;
    return world.ray(from, to);
  }
  bool exhausted() const { return count >= queryLimit; }
};
/// Climbable hit with sane values.
inline bool valid(const Hit &hit) {
  return hit.climbable && hit.point.finite() && hit.normal.finite() &&
         hit.normal.length() > .5f;
}
/// `value` flattened and normalized.
inline Vec horizontal(Vec value) {
  value.z = 0;
  return value.unit();
}
/// Hit on a near-horizontal top.
inline bool top(const Hit &hit) {
  return valid(hit) && hit.normal.unit().z >= .95f;
}
/// Hit on a vertical face pointing
/// along `outward`.
inline bool front(const Hit &hit, Vec outward) {
  return valid(hit) && std::abs(hit.normal.unit().z) <= .12f &&
         horizontal(hit.normal).dot(outward) > .95f;
}
/// A probed lip: edge point, palm point
/// on the top, face normal.
struct Row {
  Vec lip{}, palm{}, normal{};
};

/// Probe a lip near `guess`: a vertical
/// face just below, a flat top at the
/// same height deep enough for a palm,
/// and free space above.
inline std::optional<Row> row(Queries &queries, Vec guess, Vec outward) {
  const Vec below = guess - Vec{0, 0, 2};
  const auto face = queries.ray(below + outward * 8, below - outward * 4);
  if (!face || !front(*face, outward) || (face->point - below).length() > 1.25f)
    return {};
  const auto n = horizontal(face->normal);
  const Vec lip{face->point.x, face->point.y, guess.z};
  const auto probe = lip - n * palmInset;
  const auto palm = queries.ray(probe + Vec{0, 0, 4}, probe - Vec{0, 0, 4});
  if (!palm || !top(*palm) || std::abs(palm->point.z - guess.z) > 1.f)
    return {};
  const Vec actualLip{lip.x, lip.y, palm->point.z};
  for (float inset : {1.f, stripDepth}) {
    const auto point = actualLip - n * inset;
    const auto support =
        queries.ray(point + Vec{0, 0, 3}, point - Vec{0, 0, 3});
    if (!support || !top(*support) ||
        std::abs(support->point.z - palm->point.z) > .75f)
      return {};
  }
  const auto above = actualLip + Vec{0, 0, 2};

  if (queries.exhausted())
    return {};
  if (queries.ray(above + n * 3, above - n * .75f) || queries.exhausted())
    return {};
  return Row{actualLip, palm->point, n};
}
/// Whether a re-probed row is the same
/// lip (1.25 units, ~11°).
inline bool matchingRow(const Row &row, Vec expectedLip, Vec expectedNormal) {
  return (row.lip - expectedLip).length() <= 1.25f &&
         row.normal.dot(expectedNormal) > .98f;
}
} // namespace gripEdgeDetail

/// Whether a wall patch is still a flat
/// vertical face at all three points.
inline bool wallGripStillValid(World &world, const GripEdge &patch) {
  using namespace gripEdgeDetail;
  if (!patch.wallPatch || !patch.center.finite() || !patch.normal.finite() ||
      std::abs(patch.normal.z) > .001f ||
      std::abs(patch.normal.length() - 1) > .01f)
    return false;
  const Vec side{-patch.normal.y, patch.normal.x, 0};
  const float left = -(patch.hands[0] - patch.center).dot(side),
              right = (patch.hands[1] - patch.center).dot(side);
  if (!patch.hands[0].finite() || !patch.hands[1].finite() || left < 12 ||
      left > 52 || right < 12 || right > 52 || std::abs(left - right) > 1.25f)
    return false;
  for (const auto point : {patch.center, patch.hands[0], patch.hands[1]}) {
    if (std::abs(point.z - patch.center.z) > 1.25f ||
        std::abs((point - patch.center).dot(patch.normal)) > 1.25f)
      return false;
    for (const auto shift :
         {Vec{}, side * 2, side * -2, Vec{0, 0, 2}, Vec{0, 0, -2}}) {
      const auto expected = point + shift;
      const auto hit =
          world.ray(expected + patch.normal * 4, expected - patch.normal * 4);
      if (!hit || !front(*hit, patch.normal) ||
          (hit->point - expected).length() > 1.25f)
        return false;
    }
  }
  return true;
}
/// Find a flat wall patch at `height`
/// above `feet`, `halfSpan` to each
/// side, exactly `cfg.gap` away.
inline std::optional<GripEdge> findWallGrip(World &world, Vec feet, Vec outward,
                                            const Settings &cfg, float height,
                                            float halfSpan) {
  using namespace gripEdgeDetail;
  outward = horizontal(outward);
  if (!feet.finite() || outward.length() < .9f || !std::isfinite(height) ||
      height < 50 || height > 320 || !std::isfinite(halfSpan) ||
      halfSpan < 12 || halfSpan > 52 || !std::isfinite(cfg.gap) ||
      cfg.gap < 8 || cfg.gap > 100)
    return {};
  const Vec side{-outward.y, outward.x, 0};
  GripEdge patch;
  patch.normal = outward;
  patch.wallPatch = true;
  for (int sample = 0; sample < 3; ++sample) {
    const float offset = sample == 0 ? 0.f : sample == 1 ? -halfSpan : halfSpan;
    const auto from = feet + Vec{0, 0, height} + side * offset;
    const auto hit = world.ray(from, from - outward * (cfg.gap + 8));
    if (!hit || !front(*hit, outward) ||
        std::abs((from - hit->point).dot(outward) - cfg.gap) > 2)
      return {};
    if (sample == 0)
      patch.center = hit->point;
    else
      patch.hands[sample - 1] = hit->point;
  }
  return wallGripStillValid(world, patch) ? std::optional<GripEdge>(patch)
                                          : std::nullopt;
}

/// Search upward for a lip both hands
/// can hold between `minHandHeight` and
/// `maxHandHeight` above `feet`.
///
/// # Returns
/// The lowest valid edge, or `None`
/// (also when the ray budget ran out).
inline std::optional<GripEdge>
findGripEdge(World &world, Vec feet, Vec outward, const Settings &cfg,
             float minHandHeight, float maxHandHeight,
             float handHalfSpan = gripEdgeDetail::handHalfSpacing) {
  using namespace gripEdgeDetail;
  outward = horizontal(outward);
  if (!feet.finite() || outward.length() < .9f ||
      !std::isfinite(minHandHeight) || !std::isfinite(maxHandHeight) ||
      maxHandHeight < minHandHeight || maxHandHeight - minHandHeight > 80 ||
      !std::isfinite(cfg.gap) || cfg.gap < 8 || cfg.gap > 100 ||
      !std::isfinite(handHalfSpan) || handHalfSpan < 12 || handHalfSpan > 52)
    return {};
  Queries queries{world};

  for (unsigned band = 0; band < 2; ++band)
    for (float offset : {3.f, -3.f, 9.f, -9.f, 15.f, -15.f}) {
      if (queries.exhausted())
        return {};
      if (band && maxHandHeight - minHandHeight <= 8)
        continue;
      const float distance = cfg.gap + offset;
      if (distance < 6)
        continue;
      const auto lane = feet - outward * distance;
      const float seedHeight =
          (band ? (minHandHeight + maxHandHeight) * .5f : maxHandHeight) + 4;
      const Vec origin = lane + Vec{0, 0, seedHeight};
      if (band &&
          (queries.ray(feet + outward * 8 + Vec{0, 0, seedHeight}, origin) ||
           queries.exhausted()))
        continue;
      const auto seed =
          queries.ray(origin, lane + Vec{0, 0, minHandHeight - 4});
      if (!seed || !top(*seed) || seed->point.z < feet.z + minHandHeight ||
          seed->point.z > feet.z + maxHandHeight)
        continue;

      const auto from =
          feet + outward * 8 + Vec{0, 0, seed->point.z - feet.z - 2};
      const auto to = seed->point - outward * 2 - Vec{0, 0, 2};
      const auto face = queries.ray(from, to);
      if (!face || !front(*face, outward))
        continue;
      const Vec lip{face->point.x, face->point.y, seed->point.z};
      const float faceDistance = (feet - lip).dot(outward);
      if (faceDistance < 6 || faceDistance > cfg.gap + 20 ||
          faceDistance < cfg.gap - 20)
        continue;
      const auto middle = row(queries, lip, horizontal(face->normal));
      if (!middle)
        continue;
      GripEdge edge{middle->lip, middle->normal};
      const Vec side{-edge.normal.y, edge.normal.x, 0};
      bool pair = true;
      for (int hand = 0; hand < 2; ++hand) {
        const auto expected =
            edge.center + side * (hand == 0 ? -handHalfSpan : handHalfSpan);
        const auto contact = row(queries, expected, edge.normal);
        if (!contact || !matchingRow(*contact, expected, edge.normal)) {
          pair = false;
          break;
        }
        edge.hands[hand] = contact->palm;
      }
      if (pair && !queries.exhausted())
        return edge;
    }
  return {};
}

/// Whether a found edge (or wall patch)
/// still exists unchanged.
inline bool gripEdgeStillValid(World &world, const GripEdge &edge) {
  using namespace gripEdgeDetail;
  if (edge.wallPatch)
    return wallGripStillValid(world, edge);
  if (!edge.center.finite() || !edge.normal.finite() ||
      std::abs(edge.normal.z) > .001f ||
      std::abs(edge.normal.length() - 1) > .01f)
    return false;
  const Vec side{-edge.normal.y, edge.normal.x, 0};
  if (!edge.hands[0].finite() || !edge.hands[1].finite())
    return false;
  const float leftSpan = -(edge.hands[0] - edge.center).dot(side);
  const float rightSpan = (edge.hands[1] - edge.center).dot(side);
  if (leftSpan < 12 || leftSpan > 52 || rightSpan < 12 || rightSpan > 52 ||
      std::abs(leftSpan - rightSpan) > 1.25f)
    return false;
  const float halfSpan = (leftSpan + rightSpan) * .5f;
  for (int hand = 0; hand < 2; ++hand) {
    const auto expectedPalm = edge.center +
                              side * (hand == 0 ? -halfSpan : halfSpan) -
                              edge.normal * palmInset;
    if ((edge.hands[hand] - expectedPalm).length() > 1.25f)
      return false;
  }
  Queries queries{world};
  const auto middle = row(queries, edge.center, edge.normal);
  if (!middle || !matchingRow(*middle, edge.center, edge.normal))
    return false;
  for (int hand = 0; hand < 2; ++hand) {
    const auto expected =
        edge.center + side * (hand == 0 ? -halfSpan : halfSpan);
    const auto contact = row(queries, expected, edge.normal);
    if (!contact || !matchingRow(*contact, expected, edge.normal) ||
        (contact->palm - edge.hands[hand]).length() > .75f)
      return false;
  }
  return !queries.exhausted();
}

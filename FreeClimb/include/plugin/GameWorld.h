//! `fc::World` backed by the live game:
//! Havok ray casts that ignore the
//! player and trigger volumes.
//!
//!  Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.


/// Game collision for one frame.
///
/// Counts rays and records the first
/// hit and first ignored trigger for
/// the log.
struct GameWorld final : fc::World {
  RE::PlayerCharacter *player;
  RE::hkRefPtr<RE::hkpShapePhantom> playerPhantom;
  fc::RayCollectorValidationCache<RE::NiPointer<RE::bhkWorld>,
                                  fc::FilteredRayCollector::NativeAdd>
      rayCollectorValidation;
  std::uint64_t rayCandidates{}, rayClassifications{};
  int casts{}, hits{}, selfHits{}, controllerSelfHits{}, firstLayer = -1;
  std::uint32_t firstReference{}, firstBaseReference{}, firstFilterInfo{};
  int firstFormType = -1, firstBroadphase = -1, firstMotionType = -1,
      firstResponseType = -1;
  fc::Vec firstNormal{}, firstHitPoint{};
  std::string firstStaticModel;
  float firstDistance{};
  int ignoredTriggerHits{}, firstTriggerLayer = -1, firstTriggerBroadphase = -1;
  int firstTriggerMotionType = -1, firstTriggerResponseType = -1,
      firstTriggerFormType = -1;
  std::uint32_t firstTriggerReference{}, firstTriggerBaseReference{},
      firstTriggerFilterInfo{};
  fc::Vec firstTriggerHitPoint{};
  float firstTriggerDistance{};
  bool firstTriggerPrimitive{};
  /// Bind to the player's controller
  /// phantom.
  explicit GameWorld(RE::PlayerCharacter *p) : player(p) {

    if (auto *controller =
            skyrim_cast<RE::bhkCharProxyController *>(p->GetCharController()))
      if (auto *proxy = controller->GetCharacterProxy())
        playerPhantom = RE::hkRefPtr<RE::hkpShapePhantom>(proxy->shapePhantom);
  }
  /// Classification of one hit body.
  struct Candidate {
    const RE::TESObjectREFR *reference{};
    bool ownController{};
    fc::RayDecision decision = fc::RayDecision::keep;
  };
  /// Decide whether a hit body is the
  /// player, a trigger or solid.
  Candidate classify(const RE::hkpCollidable &body) {
    ++rayClassifications;
    const auto *ref = RE::TESHavokUtilities::FindCollidableRef(body);
    const bool ownController =
        playerPhantom && playerPhantom->GetCollidable() == &body;
    fc::RayHitFacts facts;
    facts.exactPlayer = ref == player || ownController;
    facts.actorZone = body.GetCollisionLayer() == RE::COL_LAYER::kActorZone;
    facts.actor = ref && ref->Is(RE::FormType::ActorCharacter);
    if (facts.actorZone && !facts.actor && !facts.exactPlayer) {
      facts.entity =
          body.broadPhaseHandle.type ==
          static_cast<int>(RE::hkpWorldObject::BroadPhaseType::kEntity);
      facts.phantom =
          body.broadPhaseHandle.type ==
          static_cast<int>(RE::hkpWorldObject::BroadPhaseType::kPhantom);
      if (facts.entity) {
        const auto *entity = body.GetOwner<RE::hkpEntity>();
        if (entity)
          switch (*entity->material.responseType) {
          case RE::hkpMaterial::ResponseType::kSimpleContact:
            facts.response = fc::RayResponse::simpleContact;
            break;
          case RE::hkpMaterial::ResponseType::kReporting:
            facts.response = fc::RayResponse::reporting;
            break;
          case RE::hkpMaterial::ResponseType::kNone:
            facts.response = fc::RayResponse::none;
            break;
          default:
            break;
          }
      } else if (facts.phantom && ref) {

        const auto *base = ref->GetBaseObject();
        const auto *acti = base ? base->As<RE::TESObjectACTI>() : nullptr;
        const auto *primitive = ref->extraList.GetByType<RE::ExtraPrimitive>();
        const char *model = acti ? acti->GetModel() : nullptr;
        facts.primitiveActivatorWithoutModel =
            acti && primitive && primitive->primitive && (!model || !*model);
      }
    }
    return {ref, ownController, fc::rayHitDecision(facts)};
  }
  struct RayContext {
    GameWorld &owner;
    const RE::hkpCollidable *firstTrigger{};
    float firstTriggerFraction{};
    std::optional<fc::RayCandidateCache<Candidate>> candidates;
    const Candidate &classify(const RE::hkpCollidable &body) {
      if (!candidates)
        candidates.emplace();
      return candidates->resolve(&body, [&] { return owner.classify(body); });
    }
  };
  /// Filter callback of the ray
  /// collector.
  static bool keepRayHit(void *opaque, const RE::hkpCollidable &body,
                         float fraction) {
    auto &context = *static_cast<RayContext *>(opaque);
    auto &owner = context.owner;
    ++owner.rayCandidates;
    const auto &candidate = context.classify(body);
    const auto decision = candidate.decision;
    const bool accepted = decision == fc::RayDecision::keep;
    if (decision == fc::RayDecision::ignorePlayer) {
      ++owner.selfHits;
      owner.controllerSelfHits += candidate.ownController;
    } else if (decision == fc::RayDecision::ignoreTrigger) {
      ++owner.ignoredTriggerHits;
      if (owner.firstTriggerLayer < 0 && !context.firstTrigger) {
        context.firstTrigger = &body;
        context.firstTriggerFraction = fraction;
      }
    }
    return accepted;
  }
  /// Remember the first ignored trigger
  /// for the log.
  void recordIgnoredTrigger(RayContext &context, fc::Vec from, fc::Vec to) {
    const auto *body = context.firstTrigger;
    if (!body || firstTriggerLayer >= 0)
      return;
    firstTriggerLayer = static_cast<int>(body->GetCollisionLayer());
    firstTriggerFilterInfo = body->broadPhaseHandle.collisionFilterInfo.filter;
    firstTriggerBroadphase = static_cast<int>(body->broadPhaseHandle.type);
    firstTriggerHitPoint = from + (to - from) * context.firstTriggerFraction;
    firstTriggerDistance = (firstTriggerHitPoint - from).length();
    const auto *ref = context.classify(*body).reference;
    if (ref) {
      firstTriggerReference = ref->GetFormID();
      if (const auto *base = ref->GetBaseObject()) {
        firstTriggerBaseReference = base->GetFormID();
        firstTriggerFormType = static_cast<int>(base->GetFormType());
      }
      const auto *primitive = ref->extraList.GetByType<RE::ExtraPrimitive>();
      firstTriggerPrimitive = primitive && primitive->primitive;
    }
    if (firstTriggerBroadphase ==
        static_cast<int>(RE::hkpWorldObject::BroadPhaseType::kEntity)) {
      if (const auto *entity = body->GetOwner<RE::hkpEntity>()) {
        firstTriggerMotionType = static_cast<int>(*entity->motion.type);
        firstTriggerResponseType =
            static_cast<int>(*entity->material.responseType);
      }
    }
  }
  /// Body sweep for back flips; other
  /// motions are not supported.
  bool actionBodyClear(fc::Motion motion, fc::Vec from, fc::Vec to,
                       float fromPhase, float toPhase,
                       fc::Vec outward) override {
    return motion == fc::Motion::backFlipOut &&
           fc::backFlipBodyClear(*this, poses.library, from, to, fromPhase,
                                 toPhase, outward, traversal.surfaceNormal.z,
                                 traversal.cfg.gap, player->GetScale());
  }
  std::optional<fc::Hit> ray(fc::Vec from, fc::Vec to) override {
    auto cell = player->GetParentCell();
    auto world = cell ? cell->GetbhkWorld() : nullptr;
    rayCollectorValidation.bind(world);
    if (!world)
      return fc::Hit{from, {0, 0, 1}, false};
    const auto scale = RE::bhkWorld::GetWorldScale();
    RE::CFilter filter{};
    player->GetCollisionFilterInfo(filter);
    RE::BSReadLockGuard lock(world->worldLock);
    {
      RE::bhkPickData pick{};
      pick.rayOutput.Reset();
      pick.rayInput.from = RE::hkVector4(ni(from) * scale);
      pick.rayInput.to = RE::hkVector4(ni(to) * scale);
      pick.rayInput.filterInfo.filter =
          (filter.filter & 0xFFFF0000) |
          static_cast<std::uint32_t>(RE::COL_LAYER::kCharController);
      RayContext context{*this};
      const auto table = *reinterpret_cast<const std::uintptr_t *>(world);
      const auto nativeAdd = rayCollectorValidation.resolve(
          table,
          [](std::uintptr_t value) {
            return reinterpret_cast<const std::uintptr_t *>(value)[0x33];
          },
          fc::verifiedRayCollectorAdd);
      fc::FilteredRayCollector collector(&context, keepRayHit, nativeAdd);
      if (nativeAdd)
        pick.closestRayHitCollector = collector.enginePrefix();
      ++casts;
      const bool hit = world->PickObject(pick);

      recordIgnoredTrigger(context, from, to);
      if (collector.malformed)
        return fc::Hit{from, {0, 0, 1}, false};
      if (!hit || !pick.rayOutput.HasHit())
        return {};
      const auto &r = pick.rayOutput;
      const auto point = from + (to - from) * r.hitFraction;
      const auto *ref =
          nativeAdd
              ? context.classify(*r.rootCollidable).reference
              : RE::TESHavokUtilities::FindCollidableRef(*r.rootCollidable);

      const bool ownController =
          playerPhantom && playerPhantom->GetCollidable() == r.rootCollidable;
      if (ref == player || ownController) {
        ++selfHits;
        controllerSelfHits += ownController;
        return fc::Hit{point, {0, 0, 1}, false};
      }
      const auto layer = r.rootCollidable->GetCollisionLayer();
      bool stable = layer == RE::COL_LAYER::kStatic ||
                    layer == RE::COL_LAYER::kGround ||
                    layer == RE::COL_LAYER::kTerrain;

      if (!stable &&
          (layer == RE::COL_LAYER::kClutter || layer == RE::COL_LAYER::kWard ||
           layer == RE::COL_LAYER::kProps) &&
          ref && ref->GetBaseObject() &&
          ref->GetBaseObject()->Is(RE::FormType::Static) &&
          r.rootCollidable->broadPhaseHandle.type ==
              static_cast<int>(RE::hkpWorldObject::BroadPhaseType::kEntity)) {
        const auto entity = r.rootCollidable->GetOwner<RE::hkpEntity>();
        stable =
            entity && entity->motion.type == RE::hkpMotion::MotionType::kFixed;
      }
      const auto &n = r.normal.quad;
      const fc::Vec normal{n.m128_f32[0], n.m128_f32[1], n.m128_f32[2]};
      if (hits++ == 0) {
        firstLayer = static_cast<int>(layer);
        firstNormal = normal;
        firstHitPoint = point;
        firstDistance = (point - from).length();
        firstReference = ref ? ref->GetFormID() : 0;
        firstFilterInfo =
            r.rootCollidable->broadPhaseHandle.collisionFilterInfo.filter;
        firstBroadphase =
            static_cast<int>(r.rootCollidable->broadPhaseHandle.type);
        const auto *base = ref ? ref->GetBaseObject() : nullptr;
        if (base) {
          firstBaseReference = base->GetFormID();
          firstFormType = static_cast<int>(base->GetFormType());
        }
        if (firstBroadphase ==
            static_cast<int>(RE::hkpWorldObject::BroadPhaseType::kEntity)) {
          if (const auto *entity =
                  r.rootCollidable->GetOwner<RE::hkpEntity>()) {
            firstMotionType = static_cast<int>(*entity->motion.type);
            firstResponseType =
                static_cast<int>(*entity->material.responseType);

            if (base &&
                entity->motion.type == RE::hkpMotion::MotionType::kFixed)
              if (const auto *object = base->As<RE::TESObjectSTAT>())
                if (const auto *model = object->GetModel())
                  firstStaticModel = model;
          }
        }
      }
      return fc::Hit{point, normal, stable};
    }
  }
};

/// Log the first ignored trigger
/// (diagnostics).
void reportIgnoredTrigger(const GameWorld &world) {
  if (!diagnostics || world.ignoredTriggerHits <= 0)
    return;
  SKSE::log::info(
      "Ignored non-solid trigger: hits={} reference={:08X} base={:08X} "
      "layer={} formType={} broadphase={} motion={} response={} primitive={} "
      "filter={:08X}; point=({:.2f},{:.2f},{:.2f}) distance={:.2f}",
      world.ignoredTriggerHits, world.firstTriggerReference,
      world.firstTriggerBaseReference, world.firstTriggerLayer,
      world.firstTriggerFormType, world.firstTriggerBroadphase,
      world.firstTriggerMotionType, world.firstTriggerResponseType,
      world.firstTriggerPrimitive, world.firstTriggerFilterInfo,
      world.firstTriggerHitPoint.x, world.firstTriggerHitPoint.y,
      world.firstTriggerHitPoint.z, world.firstTriggerDistance);
}

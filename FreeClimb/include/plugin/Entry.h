//! Native movement observation and the
//! climb entry (`acquire`).
//!
//! Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.



/// Whether the player may climb now
/// (loaded, not mounted, swimming, in a
/// menu, etc.).
bool allowed(RE::PlayerCharacter *p) {
  if (!p || !p->Is3DLoaded() || !p->GetCharController() ||
      !p->GetParentCell() || p->IsDead() || p->IsInKillMove() || p->IsOnMount())
    return false;
  auto state = p->AsActorState();
  return !state->IsSwimming() && !state->IsWeaponDrawn() &&
         state->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
         state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
}

/// Log the character controller's
/// contacts and shape (diagnostics).
bool observeNativeContacts(RE::PlayerCharacter *p,
                           const char *reason = "near-stop",
                           bool contacts = true) {
  const RE::NiPointer<RE::bhkCharacterController> controller(
      p ? p->GetCharController() : nullptr);
  auto *cell = p ? p->GetParentCell() : nullptr;
  auto *world = cell ? cell->GetbhkWorld() : nullptr;
  if (!controller || !world)
    return false;
  auto *proxyController =
      skyrim_cast<RE::bhkCharProxyController *>(controller.get());
  if (!proxyController)
    return false;
  const float scale = RE::bhkWorld::GetWorldScale();
  if (!std::isfinite(scale) || scale <= 0)
    return false;
  RE::BSReadLockGuard lock(world->worldLock);
  const auto *proxy = proxyController->GetCharacterProxy();
  if (!proxy || !proxy->shapePhantom ||
      proxy->shapePhantom->world != world->referencedObject.get())
    return false;
  RE::hkVector4 feet{}, centre{}, velocity{};
  controller->GetPosition(feet, true);
  controller->GetPosition(centre, false);
  controller->GetLinearVelocityImpl(velocity);
  const auto point = [&](const RE::hkVector4 &v) {
    return fc::Vec{v.quad.m128_f32[0], v.quad.m128_f32[1], v.quad.m128_f32[2]} /
           scale;
  };
  const auto actualFeet = point(feet), actualCentre = point(centre),
             actualVelocity = point(velocity),
             delta = actualFeet - vec(p->GetPosition());
  const auto bounds = controller->collisionBound.extents,
             bumper = controller->bumperCollisionBound.extents;
  const auto *ownBody = proxy->shapePhantom->GetCollidable();
  const bool shapeRecorded = fc::NativeShapeWitness::LogLocked(
      ownBody->shape, 1.f / scale, reason, attachmentsSinceLoad,
      &proxy->shapePhantom->motionState.transform);
  SKSE::log::info(
      "Native physics witness: feetDelta=({:.2f},{:.2f},{:.2f}) "
      "centre=({:.2f},{:.2f},{:.2f}) resolvedVelocity=({:.2f},{:.2f},{:.2f}) "
      "bounds=({:.2f},{:.2f},{:.2f}) bumper=({:.2f},{:.2f},{:.2f}) "
      "flags={:08X} filter={:08X} contacts={} slopeCos={:.3f} "
      "keepDistance={:.2f}; read-only",
      delta.x, delta.y, delta.z, actualCentre.x, actualCentre.y, actualCentre.z,
      actualVelocity.x, actualVelocity.y, actualVelocity.z, bounds.x, bounds.y,
      bounds.z, bumper.x, bumper.y, bumper.z, controller->flags.underlying(),
      ownBody->broadPhaseHandle.collisionFilterInfo.filter,
      proxy->manifold.size(), proxy->maxSlopeCosine,
      proxy->keepDistance / scale);

  if (!contacts || proxy->manifold.size() > 256)
    return shapeRecorded;
  for (std::uint32_t index = 0;
       index < std::min<std::uint32_t>(proxy->manifold.size(), 12); ++index) {
    const auto &contact = proxy->manifold[index];
    const auto *a = contact.rootCollidableA;
    const auto *b = contact.rootCollidableB;
    const auto *refA =
        a ? RE::TESHavokUtilities::FindCollidableRef(*a) : nullptr;
    const auto *refB =
        b ? RE::TESHavokUtilities::FindCollidableRef(*b) : nullptr;
    const bool ownA = a == ownBody || refA == p,
               ownB = b == ownBody || refB == p;
    const auto *other = ownA ? b : ownB ? a : nullptr;
    const auto *ref = ownA ? refB : ownB ? refA : nullptr;
    const auto *base = ref ? ref->GetBaseObject() : nullptr;
    const auto position = point(contact.contact.position);
    const auto &n = contact.contact.separatingNormal.quad;
    const char *model = "";
    if (base)
      if (const auto *object = base->As<RE::TESObjectSTAT>())
        if (const auto *path = object->GetModel())
          model = path;
    SKSE::log::info(
        "Native contact: index={} playerSide={} exactPhantom={}/{} "
        "samePlayer={}/{} reference={:08X} base={:08X} layer={} formType={} "
        "point=({:.2f},{:.2f},{:.2f}) separatingNormal=({:.3f},{:.3f},{:.3f}) "
        "distance={:.2f} shapeA={} shapeB={} model='{}'",
        index,
        ownA   ? "A"
        : ownB ? "B"
               : "unknown",
        a == ownBody, b == ownBody, refA == p, refB == p,
        ref ? ref->GetFormID() : 0, base ? base->GetFormID() : 0,
        other ? int(other->GetCollisionLayer()) : -1,
        base ? int(base->GetFormType()) : -1, position.x, position.y,
        position.z, n.m128_f32[0], n.m128_f32[1], n.m128_f32[2],
        n.m128_f32[3] / scale, contact.shapeKeyA, contact.shapeKeyB, model);
  }
  return shapeRecorded;
}

/// Log the controller shape once after
/// load (diagnostics).
void observeNativeShapeBaseline(RE::PlayerCharacter *p, float dt) {
  if (!diagnostics || traversal.active() || attachmentsSinceLoad ||
      nativeShapeBaselineSamples >= 3 || nativeShapeBaselineAttempts >= 16)
    return;
  nativeShapeBaselineAge += std::min(dt, .05f);
  constexpr std::array times{0.f, .5f, 2.f};
  if (nativeShapeBaselineAge < times[nativeShapeBaselineSamples] ||
      nativeShapeBaselineAge < nativeShapeBaselineRetry)
    return;
  nativeShapeBaselineRetry = nativeShapeBaselineAge + .25f;
  ++nativeShapeBaselineAttempts;

  if (observeNativeContacts(p, "pre-climb-baseline", false))
    ++nativeShapeBaselineSamples;
  else if (nativeShapeBaselineAttempts == 16)
    SKSE::log::info(
        "Native shape baseline unavailable after bounded retries; samples={}",
        nativeShapeBaselineSamples);
}

/// Log stalled walking (diagnostics).
void observeNativeMovement(RE::PlayerCharacter *p, const fc::Keys &held,
                           float dt) {
  if (!p || !p->Is3DLoaded() || !p->GetCharController() ||
      !p->GetParentCell() || p->IsDead() || p->IsOnMount()) {
    groundMotionProbe.suspend();
    return;
  }
  const auto *controls = RE::ControlMap::GetSingleton();
  const float yaw = p->GetAngleZ();
  const fc::Vec forward{std::sin(yaw), std::cos(yaw), 0},
      right{forward.y, -forward.x, 0};
  const auto intent = forward * float(int(held.w) - int(held.s)) +
                      right * float(int(held.d) - int(held.a));
  if (!groundMotionProbe.sample(vec(p->GetPosition()), intent, dt,
                                diagnostics && !traversal.active() &&
                                    controls &&
                                    controls->IsMovementControlsEnabled()))
    return;
  const auto *controller = p->GetCharController();
  const auto &velocity = controller->outVelocity.quad;
  const float worldScale = RE::bhkWorld::GetWorldScale();
  const auto position = vec(p->GetPosition());
  SKSE::log::info(
      "Native movement near-stop: pos=({:.2f},{:.2f},{:.2f}) gravity={:.3f} "
      "current={} wanted={} supported={} synced={} animationDriven={} "
      "SkyParkour={}/{} controllerOwned={} Shift={} W={} A={} S={} D={}; "
      "velocity=({:.2f},{:.2f},{:.2f}); diagnostic only",
      position.x, position.y, position.z, controller->gravity,
      int(controller->context.currentState), int(controller->wantState),
      int(controller->surfaceInfo.supportedState.get()), graph(p, "bIsSynced"),
      p->IsAnimationDriven(), graph(p, "SkyParkourOngoing"),
      graph(p, "SkyParkourSliding"), ownedController.get() != nullptr,
      held.shift, held.w, held.a, held.s, held.d,
      velocity.m128_f32[0] / worldScale, velocity.m128_f32[1] / worldScale,
      velocity.m128_f32[2] / worldScale);
  SKSE::log::info("Native entry history: attachmentsSinceLoad={} pending={} "
                  "waitingForKeyRelease={} preparing={} changedView={}",
                  attachmentsSinceLoad, jumpGrab.pending(),
                  climbEntry.waitingForRelease(), preparationStarted != 0,
                  preparationChangedView);
  observeNativeContacts(p);

  if (!std::isfinite(controller->collisionBound.extents.x))
    return;
  const float radius =
      std::clamp(controller->collisionBound.extents.x, 22.f, 45.f);
  for (float height : {12.f, 70.f, 125.f}) {
    GameWorld witness(p);
    const auto from = position + fc::Vec{0, 0, height};
    const auto hit = witness.ray(from, from + intent.unit() * (radius + 30));
    SKSE::log::info(
        "Native near-stop ray: height={:.0f} hit={} distance={:.2f} "
        "reference={:08X} base={:08X} layer={} formType={} response={} "
        "model='{}' normal=({:.3f},{:.3f},{:.3f})",
        height, hit.has_value(), witness.firstDistance, witness.firstReference,
        witness.firstBaseReference, witness.firstLayer, witness.firstFormType,
        witness.firstResponseType, witness.firstStaticModel,
        witness.firstNormal.x, witness.firstNormal.y, witness.firstNormal.z);
  }
}

/// Current flight state of the player
/// for a jump grab.
fc::GrabFlight entryFlight(RE::PlayerCharacter *p, bool nativeJump) {
  const auto *controller = p ? p->GetCharController() : nullptr;
  if (!controller)
    return {};
  const auto current = controller->context.currentState,
             wanted = controller->wantState;
  const bool inAir = current == RE::hkpCharacterStateType::kInAir ||
                     wanted == RE::hkpCharacterStateType::kInAir;
  const bool jumping = current == RE::hkpCharacterStateType::kJumping ||
                       wanted == RE::hkpCharacterStateType::kJumping;
  const float speed =
      controller->outVelocity.quad.m128_f32[2] / RE::bhkWorld::GetWorldScale();
  return fc::grabFlight(inAir, jumping, graph(p, "bInJumpState"), nativeJump,
                        speed);
}

/// Take control of the player for a new
/// climb: controller, pose, animation
/// state, TDM and locomotion.
///
/// # Returns
/// `false` (nothing taken) if any part
/// fails.
bool acquire(RE::PlayerCharacter *p) {
  const auto acquireStarted = diagnostics
                                  ? std::chrono::steady_clock::now()
                                  : std::chrono::steady_clock::time_point{};
  if (graph(p, "bIsSynced") || graph(p, "SkyParkourOngoing") ||
      graph(p, "SkyParkourSliding") || p->IsAnimationDriven())
    return false;
  if (tdm) {
    if (tdm->GetTargetLockState())
      return false;
    const auto yaw = tdm->RequestYawControl(SKSE::GetPluginHandle(), 0);
    yawOwned = yaw == TDM_API::APIResult::OK ||
               yaw == TDM_API::APIResult::AlreadyGiven;
    if (!yawOwned)
      return false;
    const auto direction =
        tdm->RequestDisableDirectionalMovement(SKSE::GetPluginHandle());
    directionOwned = direction == TDM_API::APIResult::OK ||
                     direction == TDM_API::APIResult::AlreadyGiven;
    if (!directionOwned) {
      release(p, "TDM busy");
      return false;
    }
  }
  if (!p->SetGraphVariableBool("bIsSynced", true)) {
    release(p, "unsupported animation graph");
    return false;
  }
  syncOwned = true;
  const auto attachStarted = diagnostics
                                 ? std::chrono::steady_clock::now()
                                 : std::chrono::steady_clock::time_point{};
  if (!poses.attach(p)) {
    SKSE::log::error("Cannot attach pose layer: {}", poses.bindingFailure);
    release(p, "pose binding failed");
    note("FreeClimb: pose binding failed; see log");
    return false;
  }
  const auto poseAttached = diagnostics
                                ? std::chrono::steady_clock::now()
                                : std::chrono::steady_clock::time_point{};
  takeController(p->GetCharController(), false);
  climbingCell = p->GetParentCell();
  climbingWorldspace = p->GetWorldspace();
  if (auto controls = RE::PlayerControls::GetSingleton()) {
    savedRunning = idleRunningKnown ? idleRunning : controls->data.running;
    runningSaved = true;
  }
  attachedTime = 0;
  poseHealth.reset();
  geometryCaptureGate.reset();
  preparationStarted = 0;
  stallReportTime = 0;
  poseRefreshTime = 0;
  yawReportTime = 0;
  lowStaminaNoted = false;
  motionUses.fill(0);
  outputReportTime = peakTraversalMs = peakPublishMs = 0;
  peakFrameCasts = 0;
  peakFrameRayCandidates = peakFrameRayClassifications = 0;
  lastContextStatus = lastThreepeatStatus = 0;
  lastCornerReported = lastEdgeReported = false;
  contextReportTime = 0;
  contextHops = contextCorners = contextEaves = 0;
  automaticActionBase = lastAutomaticAction = traversal.automaticActionCount();
  automaticAttemptBase = lastAutomaticAttempt =
      traversal.automaticAttemptCount();
  opportunityBase = traversal.automaticOpportunityCount();
  surfaceActionBase = traversal.surfaceActionCount();
  idleActionBase = traversal.contextIdleCount();
  renderedUses = {};
  selectedSeconds = {};
  observedSpanSeconds = {};
  inputSeconds = {};
  observedSpanContinuous = false;
  lastRenderedMotion = fc::Motion::none;
  lastRenderedSample = 0;
  obstacleJumpBase = lastObstacleJump = traversal.obstacleJumpCount();
  const auto controlsAcquired = diagnostics
                                    ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::time_point{};
  SKSE::log::info(
      "Attached at ({:.1f},{:.1f},{:.1f}); normal=({:.2f},{:.2f},{:.2f}); "
      "actorScale={:.2f}; controllerBounds=({:.2f},{:.2f},{:.2f}); "
      "actorHeight={:.2f}",
      traversal.position.x, traversal.position.y, traversal.position.z,
      traversal.normal.x, traversal.normal.y, traversal.normal.z, p->GetScale(),
      ownedController->collisionBound.extents.x,
      ownedController->collisionBound.extents.y,
      ownedController->collisionBound.extents.z, ownedController->actorHeight);
  const auto &b = activeSettings.bindings;
  const auto &g = activeSettings.gamepad.bindings;
  const auto help =
      gamepadOwned
          ? "FreeClimb: " + fc::serializeGamepadChord(g.runModifier) +
                " wall run | " + fc::serializeGamepadChord(g.hop) +
                " climb hop | " + fc::serializeGamepadChord(g.drop) + " let go"
          : "FreeClimb: " + fc::serializeKeyChord(b.runModifier) +
                " wall run | " + fc::serializeKeyChord(b.hop) +
                " climb hop | " + fc::serializeKeyChord(b.backward) + "+" +
                fc::serializeKeyChord(b.hop) + " leave wall";
  note(help.c_str());
  SKSE::log::info("Traversal input: {}",
                  gamepadOwned ? "controller" : "keyboard");
  ++attachmentsSinceLoad;
  if (diagnostics)
    SKSE::log::info(
        "Attach timing: ownershipMs={:.3f} poseMs={:.3f} controlsMs={:.3f} "
        "loggingMs={:.3f}",
        std::chrono::duration<float, std::milli>(attachStarted - acquireStarted)
            .count(),
        std::chrono::duration<float, std::milli>(poseAttached - attachStarted)
            .count(),
        std::chrono::duration<float, std::milli>(controlsAcquired -
                                                 poseAttached)
            .count(),
        std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - controlsAcquired)
            .count());
  return true;
}

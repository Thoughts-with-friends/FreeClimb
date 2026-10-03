//! Per-frame player update hook.
//!
//! Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.



/// Hooked `PlayerCharacter` update.
///
/// # Steps
/// 1. Run the game's own update. 2.
/// Apply pending settings; check input,
/// actor and controller state. 3. When
/// idle: watch for the entry chord or a
/// jump grab, prepare the pose and
/// `attach`. 4. When climbing: update
/// the traversal, publish the pose,
/// play audio and face the wall. 5.
/// Spend stamina; release on failure.
void update(RE::PlayerCharacter *p, float dt) {
  if (gamepadOwned) {
    auto *manager = RE::BSInputDeviceManager::GetSingleton();
    if (gamepadLost || !gamepadAvailable || !manager ||
        !manager->IsGamepadEnabled()) {
      release(p, "controller disconnected", false, true);
      gamepadState.reset();
      gamepadOwnership.reset();
      gamepadAvailable = false;
      gamepadPreferred = false;
    }
  }
  gamepadLost = false;
  entryLookDiagnostics.beforeNative(p);
  if (traversal.active())
    stopLocomotion();
  originalUpdate(p, dt);
  if (poses.consumeRigInvalidated()) {
    if (traversal.active())
      release(p, "character rig changed");
    else {
      cancelEntryPreparation();
      cancelGrabRequest();
    }
  }
  if (std::isfinite(dt) && dt > 1e-6f)
    poses.tick(dt);
  serviceSettings();
  if (!ready || !enabled)
    return;

  if (grabInputSuspended()) {
    groundMotionProbe.suspend();
    inputState.reset();
    gamepadState.blockUntilButtonsReleased();
    wallRunEntryGate.reset();
    traversalAudio.stop();

    if (traversal.active())
      release(p, "input suspended");
    cancelGrabRequest();
    return;
  }

  if (!std::isfinite(dt) || dt <= 1e-6f) {
    entryPreparationGrace.cancel();
    return;
  }
  traversalAudio.observe(dt);
  observeNativeShapeBaseline(p, dt);
  const bool fromGamepad = gamepadSelected();
  const auto heldKeys = keys();
  entryLookDiagnostics.sample(p, heldKeys, dt);
  if (crestAuditTime >= 0) {
    if (traversal.active())
      crestAuditTime = -1;
    else if (const auto *controller = p->GetCharController()) {
      crestAuditTime += dt;
      if (crestAuditTime > .12f)
        crestAuditSupported |= controller->surfaceInfo.supportedState ==
                               RE::hkpSurfaceInfo::SupportedState::kSupported;
      if (crestAuditTime >= .25f + .5f * crestAuditPhase) {
        const auto delta = vec(p->GetPosition()) - crestAuditStart;
        SKSE::log::info("Roof crest native follow-up: t={:.2f} "
                        "supportedSeen={} supportedState={} current={} "
                        "wanted={} displacement=({:.2f},{:.2f},{:.2f}) "
                        "velocityZ={:.2f} W={} A={} S={} D={}",
                        crestAuditTime, crestAuditSupported,
                        int(controller->surfaceInfo.supportedState.get()),
                        int(controller->context.currentState),
                        int(controller->wantState), delta.x, delta.y, delta.z,
                        controller->outVelocity.quad.m128_f32[2] /
                            RE::bhkWorld::GetWorldScale(),
                        heldKeys.w, heldKeys.a, heldKeys.s, heldKeys.d);
        if (++crestAuditPhase == 3)
          crestAuditTime = -1;
      }
    } else
      crestAuditTime = -1;
  }
  jumpGrab.tick(dt);
  const bool nativeJump = nativeJumpIntent.sample(nativeJumpHeld(), dt);
  const auto initialFlight = entryFlight(p, nativeJump);
  const auto grabIntent = climbEntry.sample(
      heldKeys, traversal.active(), false, dt, initialFlight.confirmedAirborne);
  const bool preparedRetry =
      entryPreparationGrace.sample(dt, fromGamepad, heldKeys);
  const bool grab = grabIntent.requested || preparedRetry,
             grabPressed = grabIntent.fresh;
  const bool hop = heldKeys.space, hopPressed = hop && !lastHop;
  lastHop = hop;
  if (!traversal.active() && !grab) {
    jumpGrab.cancel();
    cancelEntryPreparation();
    grabProbeCooldown = 0;
  }
  if (!traversal.active())
    if (auto controls = RE::PlayerControls::GetSingleton()) {
      idleRunning = controls->data.running;
      idleRunningKnown = true;
    }
  diagnosticCooldown =
      std::max(0.0f, diagnosticCooldown - std::clamp(dt, 0.0f, 0.05f));
  observeNativeMovement(p, heldKeys, dt);
  if (!allowed(p)) {
    cancelGrabRequest();
    if (traversal.active())
      release(p, "actor state");
    return;
  }
  if (!traversal.active()) {
    const auto &v = p->GetCharController()->outVelocity.quad;
    const float yaw = p->GetAngleZ();
    approachSpeed =
        (v.m128_f32[0] * std::sin(yaw) + v.m128_f32[1] * std::cos(yaw)) /
        RE::bhkWorld::GetWorldScale();
  }
  traversal.tickCooldown(dt);
  if (traversal.active()) {
    const auto actual = vec(p->GetPosition());
    const float error = (actual - traversal.position).length();
    if (p->GetParentCell() != climbingCell && error <= 40 &&
        climbingWorldspace && p->GetWorldspace() == climbingWorldspace &&
        p->GetParentCell()->GetbhkWorld()) {
      climbingCell = p->GetParentCell();
      SKSE::log::info("Continued climbing across an exterior cell boundary");
    }
    if (p->GetParentCell() != climbingCell || error > 150) {
      SKSE::log::info(
          "Climb interrupted after {:.2f}s: controllerChanged={}, "
          "cellChanged={}, positionError={:.1f}; "
          "actual=({:.1f},{:.1f},{:.1f}); expected=({:.1f},{:.1f},{:.1f})",
          attachedTime, p->GetCharController() != ownedController.get(),
          p->GetParentCell() != climbingCell, error, actual.x, actual.y,
          actual.z, traversal.position.x, traversal.position.y,
          traversal.position.z);
      release(p, "controller, cell or teleport change");
      return;
    }
    if (p->GetCharController() != ownedController.get()) {

      if (error > 40) {
        release(p, "replacement controller outside climb position");
        return;
      }
      if (diagnostics) {
        const auto *replacement = p->GetCharController();
        SKSE::log::info(
            "Replacement after {:.3f}s: positionError={:.2f}, nativeJump={}, "
            "currentState={}, wantState={}, velocityZ={:.2f}",
            attachedTime, error, graph(p, "bInJumpState"),
            int(replacement->context.currentState), int(replacement->wantState),
            replacement->outVelocity.quad.m128_f32[2] /
                RE::bhkWorld::GetWorldScale());
      }
      takeController(p->GetCharController(), true);
    }
    attachedTime += std::clamp(dt, 0.0f, 0.05f);
  }
  GameWorld world(p);
  bool attachedThisFrame = false;
  const float stamina =
      p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
  if (!traversal.active()) {
    const auto controls = RE::ControlMap::GetSingleton();
    if (!jumpToAttach || !controls || !controls->IsMovementControlsEnabled() ||
        p->IsSneaking() || graph(p, "bIsSynced") ||
        graph(p, "SkyParkourOngoing") || graph(p, "SkyParkourSliding") ||
        p->IsAnimationDriven() || (tdm && tdm->GetTargetLockState())) {
      cancelGrabRequest();
      return;
    }

    if (!grab) {
      jumpGrab.cancel();
      cancelEntryPreparation();
      return;
    }
    const float actorYaw = p->GetAngleZ();
    jumpGrab.hold({std::sin(actorYaw), std::cos(actorYaw), 0}, nativeJump,
                  grabIntent.airborneAtBegin, grabPressed);
    if (grabPressed && diagnostics)
      SKSE::log::info(
          "Grab request: device={} entry chord={}; airborne={} descending={} "
          "velocityZ={:.2f}; releaseCooldown={:.3f}; nativeJumpPending={} "
          "airborneAtBegin={}",
          fromGamepad ? "gamepad" : "keyboard",
          fromGamepad
              ? fc::serializeGamepadChord(activeSettings.gamepad.bindings.entry)
              : fc::serializeKeyChord(activeSettings.bindings.entry),
          initialFlight.airborne, initialFlight.descending,
          initialFlight.verticalSpeed, traversal.cooldown, nativeJump,
          grabIntent.airborneAtBegin);
    const auto flight = entryFlight(p, jumpGrab.startedNativeJump());
    const bool explicitAirCatch = jumpGrab.explicitAirCatch(flight);
    if (!jumpGrab.permitted(traversal.cooldown, flight))
      return;
    grabProbeCooldown = std::max(0.f, grabProbeCooldown - std::min(dt, .05f));
    if (grabProbeCooldown > 0 && !grabPressed)
      return;
    grabProbeCooldown = .08f;
    EntryLookDiagnostics::Probe probeTimer(entryLookDiagnostics);
    fitBody(p);

    const auto facing = jumpGrab.facing();
    const float yaw = std::atan2(facing.x, facing.y);
    auto candidate = traversal;
    if (!candidate.attach(world, vec(p->GetPosition()), facing, stamina,
                          grabMaxSnap, explicitAirCatch, !flight.airborne)) {
      if (diagnostics && diagnosticCooldown <= 0 && world.hits > 0) {
        SKSE::log::info(
            "Climb entry rejected: {}; airborne={} descending={} "
            "snap={:.2f}/{}; pos=({:.1f},{:.1f},{:.1f}); yaw={:.2f}; casts={} "
            "hits={} firstRayDistance={:.1f}; normal=({:.2f},{:.2f},{:.2f})",
            fc::name(candidate.lastFailure), flight.airborne, flight.descending,
            candidate.lastAttachDistance, grabMaxSnap, p->GetPositionX(),
            p->GetPositionY(), p->GetPositionZ(), yaw, world.casts, world.hits,
            world.firstDistance, world.firstNormal.x, world.firstNormal.y,
            world.firstNormal.z);
        reportIgnoredTrigger(world);
        diagnosticCooldown = 2;
        SKSE::log::info(
            "Grab collision: reference={:08X} base={:08X} formType={} "
            "broadphase={} motion={} response={} filter={:08X}; "
            "point=({:.2f},{:.2f},{:.2f}); fixedStaticModel='{}'",
            world.firstReference, world.firstBaseReference, world.firstFormType,
            world.firstBroadphase, world.firstMotionType,
            world.firstResponseType, world.firstFilterInfo,
            world.firstHitPoint.x, world.firstHitPoint.y, world.firstHitPoint.z,
            world.firstStaticModel);
      }
      if (preparationStarted || preparationChangedView)
        cancelEntryPreparation();
      return;
    }
    float nativeSlope = .707107f;
    if (auto *proxyController =
            skyrim_cast<RE::bhkCharProxyController *>(p->GetCharController()))
      if (const auto *proxy = proxyController->GetCharacterProxy();
          proxy && std::isfinite(proxy->maxSlopeCosine) &&
          proxy->maxSlopeCosine >= 0 && proxy->maxSlopeCosine <= 1)
        nativeSlope = std::max(nativeSlope, proxy->maxSlopeCosine);
    const auto *entryController = p->GetCharController();
    const bool grounded = fc::groundEntryGeometryAllowed(
        entryController->context.currentState ==
            RE::hkpCharacterStateType::kOnGround,
        entryController->wantState == RE::hkpCharacterStateType::kJumping,
        jumpGrab.startedNativeJump(), flight.airborne,
        flight.confirmedAirborne);
    const auto lowTop = fc::groundedLowTopFallback(
        traversal.cfg, grounded, candidate.entryFallbackTopRise());
    const auto nativePath =
        lowTop.excludes()
            ? fc::NativeWalkableResult{}
            : fc::nativeWalkableApproach(world, vec(p->GetPosition()), facing,
                                         traversal.cfg, grounded, 16.f,
                                         nativeSlope);
    auto lowEntry =
        lowTop.excludes() ? lowTop
        : nativePath.walkable
            ? fc::GroundEntryExclusionResult{}
            : fc::groundedLowFace(world, vec(p->GetPosition()), traversal.cfg,
                                  grounded, candidate.entryTarget(),
                                  candidate.surfaceNormal, nativeSlope);
    auto mergeEntryProbe = [&](const fc::GroundEntryExclusionResult &probe) {
      lowEntry.casts += probe.casts;
      if (probe.groundFound) {
        lowEntry.groundFound = true;
        lowEntry.baseHeight = probe.baseHeight;
        lowEntry.supportSamples = probe.supportSamples;
      }
      if (probe.excludes()) {
        lowEntry.reason = probe.reason;
        lowEntry.selectedRise = probe.selectedRise;
      }
    };
    if (!nativePath.walkable && !lowEntry.excludes())
      mergeEntryProbe(fc::groundedStepFace(
          world, vec(p->GetPosition()), facing, traversal.cfg, grounded,
          candidate.entryTarget(), candidate.surfaceNormal, nativeSlope));
    if (!nativePath.walkable && !lowEntry.excludes())
      mergeEntryProbe(fc::groundedEntryExclusion(
          world, vec(p->GetPosition()), facing, traversal.cfg, grounded,
          candidate.entryTarget(), candidate.surfaceNormal, nativeSlope));
    if (nativePath.walkable || lowEntry.excludes()) {
      if (preparationStarted || preparationChangedView)
        cancelEntryPreparation();
      if (diagnostics && diagnosticCooldown <= 0) {
        const auto delta = candidate.entryTarget() - vec(p->GetPosition());
        SKSE::log::info(
            "Climb entry kept native: reason={}; nativeWalkable={} "
            "nativeProbes={} lowProbes={}; selectedTopFallback={} "
            "selectedTopRise={:.2f}; targetDelta=({:.2f},{:.2f},{:.2f}); "
            "pos=({:.2f},{:.2f},{:.2f})",
            nativePath.walkable ? "continuous walkable floor and body corridor"
                                : fc::name(lowEntry.reason),
            nativePath.walkable, nativePath.casts, lowEntry.casts,
            candidate.entryFallbackTopRise().has_value(),
            candidate.entryFallbackTopRise().value_or(0.f), delta.x, delta.y,
            delta.z, p->GetPositionX(), p->GetPositionY(), p->GetPositionZ());
        diagnosticCooldown = 2;
      }

      return;
    }
    auto *camera = RE::PlayerCamera::GetSingleton();
    if (camera->IsInFirstPerson()) {
      preparationChangedView = true;
      camera->ForceThirdPerson();
    }
    const auto prepareStarted = diagnostics
                                    ? std::chrono::steady_clock::now()
                                    : std::chrono::steady_clock::time_point{};
    const auto preparation = poses.prepare(p);
    if (diagnostics && (preparation != fc::PoseRuntime::Preparation::waiting ||
                        !preparationStarted))
      SKSE::log::info(
          "Pose preflight timing: state={} bindingMs={:.3f} changedView={}",
          int(preparation),
          std::chrono::duration<float, std::milli>(
              std::chrono::steady_clock::now() - prepareStarted)
              .count(),
          preparationChangedView);
    if (preparation != fc::PoseRuntime::Preparation::ready) {
      if (preparation == fc::PoseRuntime::Preparation::waiting &&
          grabIntent.requested)
        entryPreparationGrace.arm(fromGamepad);
      const auto now = GetTickCount64();
      if (!preparationStarted)
        preparationStarted = now;
      if (preparation == fc::PoseRuntime::Preparation::rejected ||
          now - preparationStarted >= 1000) {
        SKSE::log::error("Pose preflight failed without taking player control: "
                         "reason={}, callbacks={}",
                         preparation == fc::PoseRuntime::Preparation::rejected
                             ? poses.bindingFailure
                             : "no scene callback",
                         poses.callbacks.load());
        cancelGrabRequest();
        note("FreeClimb: animation unavailable; normal movement retained");
      }
      return;
    }
    entryPreparationGrace.cancel();
    preparationStarted = 0;
    candidate.cfg.threepeatAnimations = activeSettings.threepeatAnimations;
    poses.library.configureThreepeat(candidate.cfg);
    traversal = std::move(candidate);
    gamepadOwned = fromGamepad;
    const bool nativeSpace = jumpGrab.startedNativeJump();
    reportIgnoredTrigger(world);
    const auto entry = fc::grabEntryMotion(flight);
    if (!acquire(p)) {
      traversal.stop();
      gamepadOwned = false;
      cancelEntryPreparation();
      if (diagnosticCooldown <= 0) {
        SKSE::log::info(
            "Attach blocked by animation/movement ownership; synced={}, "
            "parkour={}, sliding={}, animationDriven={}, targetLock={}",
            graph(p, "bIsSynced"), graph(p, "SkyParkourOngoing"),
            graph(p, "SkyParkourSliding"), p->IsAnimationDriven(),
            tdm && tdm->GetTargetLockState());
        diagnosticCooldown = 2;
      }
      if (grabPressed)
        note("FreeClimb: another animation or movement mod is busy");
      return;
    }
    preparationChangedView = false;
    if (diagnostics && preparedRetry && !grabIntent.requested)
      SKSE::log::info("Completed validated entry preparation after chord "
                      "release within 150ms");
    wallRunEntryGate.begin(heldKeys);
    traversal.entry(entry, !flight.airborne);
    jumpGrab.cancel();
    climbEntry.blockUntilRelease();
    attachedThisFrame = true;
    if (diagnostics)
      SKSE::log::info(
          "Entry={} preEntryForwardSpeed={:.1f} airborne={} descending={} "
          "nativeSpace={} velocityZ={:.2f} entryLift={} explicitAirCatch={} "
          "snap={:.2f} raisedTarget={:.1f} roundedCapsule={} duration={:.3f}; "
          "groundSupported={} supportedState={} nativeProbes={} lowProbes={}; "
          "target=({:.2f},{:.2f},{:.2f}) targetDelta=({:.2f},{:.2f},{:.2f})",
          int(entry), approachSpeed, flight.airborne, flight.descending,
          nativeSpace, flight.verticalSpeed, !flight.airborne, explicitAirCatch,
          traversal.lastAttachDistance, traversal.entryLiftHeight(),
          traversal.roundedEntryPath(), traversal.entryDuration(), grounded,
          int(p->GetCharController()->surfaceInfo.supportedState.get()),
          nativePath.casts, lowEntry.casts, traversal.entryTarget().x,
          traversal.entryTarget().y, traversal.entryTarget().z,
          traversal.entryTarget().x - p->GetPositionX(),
          traversal.entryTarget().y - p->GetPositionY(),
          traversal.entryTarget().z - p->GetPositionZ());
    if (diagnostics)
      SKSE::log::info(
          "Entry ground evidence: found={} samples={} rootAboveFloor={:.2f} "
          "selectedRise={:.2f}",
          lowEntry.groundFound, lowEntry.supportSamples,
          lowEntry.groundFound ? p->GetPositionZ() - lowEntry.baseHeight : 0.f,
          lowEntry.selectedRise);
    if (diagnostics)
      SKSE::log::info(
          "Entry collision witness (first query hit): reference={:08X} "
          "base={:08X} layer={} point=({:.2f},{:.2f},{:.2f}) "
          "normal=({:.3f},{:.3f},{:.3f}) model='{}'",
          world.firstReference, world.firstBaseReference, world.firstLayer,
          world.firstHitPoint.x, world.firstHitPoint.y, world.firstHitPoint.z,
          world.firstNormal.x, world.firstNormal.y, world.firstNormal.z,
          world.firstStaticModel);
  }
  fc::Input input =
      fc::wallInput(wallRunEntryGate.filter(heldKeys), hopPressed, autoMantle,
                    attachedThisFrame, traversal.wallRunning());
  const float contactSpeed = poses.surface.movementScale();
  input.x *= contactSpeed;
  input.y *= contactSpeed;

  poseHealth.sample(poses.applied.load(), dt);
  if (poseHealth.failed()) {
    SKSE::log::error("Pose output timeout: callbacks={}, matched={}, "
                     "applied={}, rejected={}; restoring player controls",
                     poses.callbacks.load(), poses.matched.load(),
                     poses.applied.load(), poses.rejected.load());
    release(p, "animation output timeout");
    note("FreeClimb: animation stopped; normal movement restored");
    return;
  }
  const bool animationReady = poseHealth.ready();
  if (input.release && !animationReady && attachedTime > .12f) {
    release(p, "manual drop while animation output unavailable", false, true);
    return;
  }
  const auto stateBefore = traversal.state;
  const bool runningBefore = traversal.wallRunning();
  const float effectiveDt = animationReady ? dt : 0.f;
  const bool captureFrame =
      diagnostics && animationReady &&
      geometryCaptureGate.arm(traversal, input, attachedTime);
  const auto traversalStarted = std::chrono::steady_clock::now();
  fc::Result result;
  if (captureFrame) {
    geometryCapture.begin(traversal, input, effectiveDt, stamina);
    fc::TraversalCapture::RecordingWorld recorded(world, geometryCapture);
    result = traversal.update(recorded, input, effectiveDt, stamina);
    geometryCapture.finish(traversal, result);
  } else
    result = traversal.update(world, input, effectiveDt, stamina);
  const float traversalMs =
      std::chrono::duration<float, std::milli>(
          std::chrono::steady_clock::now() - traversalStarted)
          .count();
  if (diagnostics) {
    const float seconds = std::clamp(effectiveDt, 0.f, .05f);
    if (int(result.motion) > 0 && int(result.motion) <= fc::motionCount)
      selectedSeconds[int(result.motion)] += seconds;
    if (stateBefore == fc::State::wall || stateBefore == fc::State::ledge) {
      const bool sideways = std::abs(input.x) > .1f,
                 moving = sideways || std::abs(input.y) > .1f;
      const unsigned mode = !moving                     ? 5u
                            : input.run && input.y >= 0 ? 4u
                            : input.y < -.1f            ? 3u
                            : sideways ? (input.y > .1f ? 2u : 1u)
                                       : 0u;
      inputSeconds[mode] += seconds;
    }
  }
  if (captureFrame) {

    try {
      SKSE::log::info("{}", geometryCapture.serialize());
    } catch (const std::exception &e) {
      SKSE::log::warn("Geometry capture unavailable: {}", e.what());
    } catch (...) {
      SKSE::log::warn("Geometry capture unavailable");
    }
  }
  if (diagnostics) {
    if (traversal.state == fc::State::mantle &&
        stateBefore != fc::State::mantle) {
      const auto from = traversal.topStart(), target = traversal.topTarget(),
                 lip = traversal.topLip();
      SKSE::log::info(
          "Mantle selected: motion={} preciseContacts={} sourceBegin={:.3f} "
          "reason={}; height={:.2f} forward={:.2f} surfaceZ={:.3f} "
          "wasRunning={} preparation={:.3f}",
          int(result.motion), traversal.preciseTopContacts(),
          traversal.topSampleBegin(), traversal.topSelectionReason(),
          lip.z - from.z, (target - from).dot(traversal.normal * -1),
          traversal.surfaceNormal.z, runningBefore, traversal.topPreparation());
    }
    if (traversal.automaticAttemptCount() != lastAutomaticAttempt) {
      lastAutomaticAttempt = traversal.automaticAttemptCount();
      SKSE::log::info(
          "Automatic climbing attempt: count={} motion={} preparing={} edge={} "
          "threepeatReason={} contextReason={} time={:.2f} "
          "input=({:.2f},{:.2f}) surfaceZ={:.3f} running={} state={}",
          lastAutomaticAttempt - automaticAttemptBase, int(result.motion),
          traversal.preparingEdge(), traversal.usesEdgeTargets(result.motion),
          traversal.threepeatReason(), traversal.contextReason(), attachedTime,
          input.x, input.y, traversal.surfaceNormal.z, traversal.wallRunning(),
          int(traversal.state));
    }
    if (traversal.obstacleJumpCount() != lastObstacleJump) {
      lastObstacleJump = traversal.obstacleJumpCount();
      SKSE::log::info("Wall-run obstacle jump committed: count={} motion={} "
                      "duration={:.3f} time={:.2f}",
                      lastObstacleJump - obstacleJumpBase, int(result.motion),
                      traversal.actionDuration(), attachedTime);
    }
    if (traversal.automaticActionCount() != lastAutomaticAction) {
      lastAutomaticAction = traversal.automaticActionCount();
      SKSE::log::info("Automatic climbing action committed: count={} motion={} "
                      "edge={} time={:.2f} contactMode={}",
                      lastAutomaticAction - automaticActionBase,
                      int(result.motion),
                      traversal.usesEdgeTargets(result.motion), attachedTime,
                      traversal.usesWallTargets(result.motion)   ? "wall-patch"
                      : traversal.usesEdgeTargets(result.motion) ? "ledge"
                                                                 : "ordinary");
    }
    const unsigned contextStatus = traversal.contextStatus(),
                   threepeatStatus = traversal.threepeatStatus();
    const bool corner = traversal.turningCorner(),
               edge = traversal.usesEdgeTargets(result.motion);
    const bool committed = edge && traversal.state == fc::State::action &&
                           stateBefore != fc::State::action;
    const bool eave =
        std::string_view(traversal.blockedReason) == "checked eave bypass";
    if (corner && !lastCornerReported)
      ++contextCorners;
    if (eave)
      ++contextEaves;
    if (committed) {
      ++contextHops;
      const auto a = traversal.edgeStart(), b = traversal.edgeTarget();
      const auto source =
          (traversal.edgeHand(0, false) + traversal.edgeHand(1, false)) * .5f;
      const auto destination =
          (traversal.edgeHand(0, true) + traversal.edgeHand(1, true)) * .5f;
      const auto delta = destination - source;
      SKSE::log::info(
          "Measured contact committed: motion={} mode={} "
          "contactDelta=({:.2f},{:.2f},{:.2f}) bodyFrom=({:.2f},{:.2f},{:.2f}) "
          "bodyTo=({:.2f},{:.2f},{:.2f})",
          int(result.motion),
          traversal.usesWallTargets(result.motion) ? "wall-patch" : "ledge",
          delta.x, delta.y, delta.z, a.x, a.y, a.z, b.x, b.y, b.z);
    }

    if ((contextStatus != lastContextStatus &&
         (contextStatus >= 7 || attachedTime >= contextReportTime)) ||
        (threepeatStatus != lastThreepeatStatus &&
         (threepeatStatus >= 7 || attachedTime >= contextReportTime)) ||
        corner != lastCornerReported || edge != lastEdgeReported || committed ||
        eave) {
      SKSE::log::info(
          "Context traversal: time={:.2f} status={} reason={} "
          "threepeatStatus={} threepeatReason={} preparing={} edge={} "
          "corner={} motion={} state={} pos=({:.2f},{:.2f},{:.2f}) route={}",
          attachedTime, contextStatus, traversal.contextReason(),
          threepeatStatus, traversal.threepeatReason(),
          traversal.preparingEdge(), edge, corner, int(result.motion),
          int(traversal.state), traversal.position.x, traversal.position.y,
          traversal.position.z, traversal.blockedReason);
      lastContextStatus = contextStatus;
      lastThreepeatStatus = threepeatStatus;
      lastCornerReported = corner;
      lastEdgeReported = edge;
      contextReportTime = attachedTime + .5f;
    }
  }
  if (diagnostics && traversal.stalledSeconds() > .35f &&
      attachedTime >= stallReportTime) {
    stallReportTime = attachedTime + 1;
    SKSE::log::info("Blocked movement: time={:.2f} dt={:.5f} "
                    "input=({:.1f},{:.1f}) pos=({:.2f},{:.2f},{:.2f}) "
                    "surface=({:.3f},{:.3f},{:.3f}) state={} casts={} hits={} "
                    "rayCandidates={} rayClassifications={} reason={} top={}",
                    traversal.stalledSeconds(), dt, input.x, input.y,
                    traversal.position.x, traversal.position.y,
                    traversal.position.z, traversal.surfaceNormal.x,
                    traversal.surfaceNormal.y, traversal.surfaceNormal.z,
                    int(traversal.state), world.casts, world.hits,
                    world.rayCandidates, world.rayClassifications,
                    traversal.blockedReason, traversal.ledgeReason);
    if (traversal.blockedHit) {
      const auto &hit = *traversal.blockedHit;
      const auto a = traversal.blockedFrom, b = traversal.blockedTo;
      SKSE::log::info("Clearance obstruction: from=({:.2f},{:.2f},{:.2f}) "
                      "to=({:.2f},{:.2f},{:.2f}) hit=({:.2f},{:.2f},{:.2f}) "
                      "normal=({:.3f},{:.3f},{:.3f}) climbable={}",
                      a.x, a.y, a.z, b.x, b.y, b.z, hit.point.x, hit.point.y,
                      hit.point.z, hit.normal.x, hit.normal.y, hit.normal.z,
                      hit.climbable);
    }
    SKSE::log::info("Collision identity: firstReference={:08X} firstLayer={} "
                    "self={} exactPlayerPhantom={}",
                    world.firstReference, world.firstLayer, world.selfHits,
                    world.controllerSelfHits);
    reportIgnoredTrigger(world);
  }

  if (ownedController) {
    stopLocomotion();
    ownedController->fallTime = 0;
    ownedController->fallStartHeight = traversal.position.z;
    if ((vec(p->GetPosition()) - traversal.position).length() > .025f)
      p->SetPosition(ni(traversal.position), true);
    if (!faceWall(p, dt)) {
      release(p, "wall-facing ownership or normal invalid", true);
      return;
    }
  }
  if (result.staminaCost > 0)
    p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                          RE::ActorValue::kStamina,
                                          -result.staminaCost);
  if (traversal.cfg.staminaEnabled && lowStaminaNotifications &&
      stamina <= 20 && !lowStaminaNoted) {
    note("FreeClimb: low stamina - stop to rest or climb down");
    lowStaminaNoted = true;
  }
  if (stamina > 30)
    lowStaminaNoted = false;
  if (poses.requestTopRecovery(traversal.state, traversal.progress(),
                               traversal.topSeconds())) {
    const bool accepted = p->NotifyAnimationGraph("IdleForceDefaultState");
    poses.resolveTopRecovery(accepted);
    if (diagnostics)
      SKSE::log::info("Top-out native standing endpoint requested at "
                      "progress={:.3f}; windowSeconds={:.3f}; accepted={}",
                      traversal.progress(),
                      (1.f - fc::topRecoveryBegin(traversal.topSeconds())) *
                          traversal.topSeconds(),
                      accepted);
  }
  const auto publishStarted = std::chrono::steady_clock::now();
  poses.update(world, traversal, result.motion, dt, p->GetScale());
  traversalAudio.update(poses.library, traversal, result,
                        poses.surface.sampledPhase(), effectiveDt,
                        animationReady);
  const float publishMs = std::chrono::duration<float, std::milli>(
                              std::chrono::steady_clock::now() - publishStarted)
                              .count();
  reportOutput(p, result.motion, traversalMs, publishMs, world.casts,
               world.rayCandidates, world.rayClassifications, result.released);
  if (poseHealth.stale() > .25f && attachedTime >= poseRefreshTime) {
    poseRefreshTime = attachedTime + .5f;
    if (poses.refresh(p)) {
      SKSE::log::info(
          "Refreshed replaced animation graph without restarting traversal");

      poseHealth.invalidate();
    }
  }
  if (result.released) {
    if (diagnostics && result.completed &&
        std::string_view(result.reason) == "roof crest reached") {
      crestAuditTime = 0;
      crestAuditPhase = 0;
      crestAuditSupported = false;
      crestAuditStart = traversal.position;
    }
    SKSE::log::info("Traversal ended after {:.2f}s; stamina={:.1f}; motion={}; "
                    "state={}->{}; reason={}; casts={}; hits={}; layer={}; "
                    "firstHitNormal=({:.2f},{:.2f},{:.2f}); "
                    "facingNormal=({:.2f},{:.2f},{:.2f}); "
                    "surfaceNormal=({:.2f},{:.2f},{:.2f})",
                    attachedTime, stamina, static_cast<int>(result.motion),
                    int(stateBefore), int(traversal.state), result.reason,
                    world.casts, world.hits, world.firstLayer,
                    world.firstNormal.x, world.firstNormal.y,
                    world.firstNormal.z, traversal.normal.x, traversal.normal.y,
                    traversal.normal.z, traversal.surfaceNormal.x,
                    traversal.surfaceNormal.y, traversal.surfaceNormal.z);
    if (diagnostics)
      SKSE::log::info("Pose frames={}, rejected={}, contactCount={}, "
                      "reachError={:.2f}, completed={}",
                      poses.applied.load(), poses.rejected.load(),
                      poses.surface.contactCount, poses.surface.maxReachError,
                      result.completed);
    const bool physicalExit =
        result.motion == fc::Motion::drop ||
        result.motion == fc::Motion::dropBack ||
        result.motion == fc::Motion::backFlipOut ||
        (!result.completed && result.releaseVelocity.length() > .01f) ||
        (stateBefore == fc::State::action &&
         (lastMotion == fc::Motion::dropBack ||
          lastMotion == fc::Motion::backFlipOut));
    release(p,
            result.completed ? (std::string_view(result.reason) != "none"
                                    ? result.reason
                                    : "top-out complete")
            : result.motion == fc::Motion::drop ? "manual drop"
                                                : result.reason,
            true, physicalExit, result.completed);

    if (result.releaseVelocity.finite() &&
        result.releaseVelocity.length() > .01f) {
      if (auto *controller = p->GetCharController()) {
        const auto v = result.releaseVelocity * RE::bhkWorld::GetWorldScale();
        const RE::hkVector4 impulse(v.x, v.y, v.z, 0), zero(0, 0, 0, 0);
        controller->outVelocity = controller->initialVelocity = impulse;
        controller->velocityMod = zero;
        controller->wantState = RE::hkpCharacterStateType::kInAir;
        controller->SetLinearVelocityImpl(impulse);
        if (diagnostics)
          SKSE::log::info("Physical back jump velocity=({:.1f},{:.1f},{:.1f}); "
                          "gravity={:.3f}",
                          result.releaseVelocity.x, result.releaseVelocity.y,
                          result.releaseVelocity.z, controller->gravity);
      }
    }
    return;
  }
  setMotion(p, result.motion);
}

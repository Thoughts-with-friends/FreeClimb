//! Motion bookkeeping, releasing the
//! player, facing the wall and output
//! reports.
//!
//! Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.



/// Record the motion shown this frame
/// and mark the graph as owned.
void setMotion(RE::PlayerCharacter *, fc::Motion m) {

  animationState.set();
  if (m != lastMotion) {
    ++motionUses[std::clamp(int(m), 0, fc::motionCount)];
    ++totalMotionUses[std::clamp(int(m), 0, fc::motionCount)];
    if (diagnostics)
      SKSE::log::info("Motion {} -> {}", int(lastMotion), int(m));
  }
  lastMotion = m;
}

/// End the climb and give everything
/// back to the game.
///
/// # Params
/// - `reason`: Logged.
/// - `fade`: Blend the pose out.
/// - `physicalFall`: Keep falling
///   motion.
/// - `completedTop`: After a finished
///   mantle.
void release(RE::PlayerCharacter *p, const char *reason, bool fade = false,
             bool physicalFall = false, bool completedTop = false) {
  const auto releaseStarted = diagnostics
                                  ? std::chrono::steady_clock::now()
                                  : std::chrono::steady_clock::time_point{};
  wallRunEntryGate.reset();
  if (preparationChangedView)
    cancelEntryPreparation();
  if (fade)
    traversalAudio.resetTiming();
  else
    traversalAudio.stop();
  entryPreparationGrace.cancel();
  preparationStarted = 0;
  poseHealth.reset();
  viewHeading.reset();
  const bool topRecoveryReady = poses.topRecoveryReady();
  poses.release(fade, physicalFall, completedTop);
  const auto poseReleased = diagnostics
                                ? std::chrono::steady_clock::now()
                                : std::chrono::steady_clock::time_point{};
  const bool wasOwned = ownedController.get() != nullptr;
  jumpGrab.cancel();
  climbEntry.blockUntilRelease();
  grabProbeCooldown = 0;
  const auto gravityRelease =
      controllerGravity.release(p ? p->GetCharController() : nullptr);
  if (gravityRelease.replacement)
    SKSE::log::info("Restored inherited gravity on an unobserved replacement "
                    "controller during release: {}",
                    reason);
  ownedController.reset();
  if (p && syncOwned)
    p->SetGraphVariableBool("bIsSynced", false);
  if (tdm && yawOwned)
    tdm->ReleaseYawControl(SKSE::GetPluginHandle());
  if (tdm && directionOwned)
    tdm->ReleaseDisableDirectionalMovement(SKSE::GetPluginHandle());
  syncOwned = yawOwned = directionOwned = false;
  if (runningSaved) {
    if (auto controls = RE::PlayerControls::GetSingleton())
      controls->data.running = savedRunning;
    runningSaved = false;
  }
  traversal.stop();
  gamepadOwned = false;
  climbingCell = nullptr;
  climbingWorldspace = nullptr;
  animationState.clear();
  if (p && wasOwned) {

    if (!topRecoveryReady && !physicalFall)
      p->NotifyAnimationGraph("IdleForceDefaultState");
    const auto controlsReleased = diagnostics
                                      ? std::chrono::steady_clock::now()
                                      : std::chrono::steady_clock::time_point{};
    SKSE::log::info("Released: {}", reason);
    if (diagnostics)
      for (int i = 1; i <= fc::motionCount; ++i)
        if (motionUses[i])
          SKSE::log::info("Session action {} entries={} selectedSeconds={:.3f}",
                          i, motionUses[i], selectedSeconds[i]);
    if (diagnostics)
      SKSE::log::info("Session measured routes: edgeHop={} corner={} eave={} "
                      "automaticClimbActions={} automaticAttempts={} "
                      "wallRunObstacleJumps={}",
                      contextHops, contextCorners, contextEaves,
                      lastAutomaticAction - automaticActionBase,
                      lastAutomaticAttempt - automaticAttemptBase,
                      lastObstacleJump - obstacleJumpBase);
    if (diagnostics) {
      SKSE::log::info("Session captured variety: opportunityScans={} "
                      "surfaceActions={} idleSettles={}",
                      traversal.automaticOpportunityCount() - opportunityBase,
                      traversal.surfaceActionCount() - surfaceActionBase,
                      traversal.contextIdleCount() - idleActionBase);
      SKSE::log::info("Session wall input seconds: up={:.3f} side={:.3f} "
                      "diagonalUp={:.3f} down={:.3f} run={:.3f} idle={:.3f}; "
                      "request exposure, not accepted travel",
                      inputSeconds[0], inputSeconds[1], inputSeconds[2],
                      inputSeconds[3], inputSeconds[4], inputSeconds[5]);
      for (unsigned motion = 39; motion <= fc::motionCount; ++motion)
        SKSE::log::info("Session rendered new action: motion={} starts={} "
                        "observedSpanSeconds={:.3f}",
                        motion, renderedUses[motion],
                        observedSpanSeconds[motion]);
      SKSE::log::info(
          "Release timing: poseMs={:.3f} controlsMs={:.3f} loggingMs={:.3f}; "
          "fade={} physicalFall={} completedTop={}",
          std::chrono::duration<float, std::milli>(poseReleased -
                                                   releaseStarted)
              .count(),
          std::chrono::duration<float, std::milli>(controlsReleased -
                                                   poseReleased)
              .count(),
          std::chrono::duration<float, std::milli>(
              std::chrono::steady_clock::now() - controlsReleased)
              .count(),
          fade, physicalFall, completedTop);
    }
  }
  lastMotion = fc::Motion::none;
}

/// Log text of a TDM API result.
const char *yawResultName(TDM_API::APIResult result) {
  switch (result) {
  case TDM_API::APIResult::OK:
    return "OK";
  case TDM_API::APIResult::NotOwner:
    return "NotOwner";
  case TDM_API::APIResult::MustKeep:
    return "MustKeep";
  case TDM_API::APIResult::AlreadyGiven:
    return "AlreadyGiven";
  case TDM_API::APIResult::AlreadyTaken:
    return "AlreadyTaken";
  case TDM_API::APIResult::BadThread:
    return "BadThread";
  }
  return "unknown";
}

/// Turn the player to face the wall
/// (through TDM when installed).
///
/// # Returns
/// `false` if facing cannot be owned or
/// the normal is invalid.
bool faceWall(RE::PlayerCharacter *p, float dt) {
  const auto desired = fc::facingWallYaw(traversal.normal);
  if (!desired) {
    SKSE::log::error("Invalid wall-facing normal; restoring native control");
    return false;
  }
  const float before = p->GetAngleZ();
  if (!std::isfinite(before)) {
    SKSE::log::error("Invalid actor yaw; restoring native control");
    return false;
  }
  if (!viewHeading.ready())
    viewHeading.begin(before);
  const float referenceYaw = viewHeading.advance(*desired, dt);
  auto result = TDM_API::APIResult::OK;
  if (tdm && yawOwned) {
    result = tdm->SetPlayerYaw(SKSE::GetPluginHandle(), referenceYaw);
    if (result != TDM_API::APIResult::OK) {
      SKSE::log::error("TDM yaw update rejected: {}; target={:.3f} "
                       "actor={:.3f}; thread={} TDMThread={}",
                       yawResultName(result), *desired, before,
                       GetCurrentThreadId(), tdm->GetTDMThreadId());

      return false;
    }
  }

  bool cameraPreserved = false;
  if (std::abs(fc::yawDifference(referenceYaw, before)) > .001f) {
    auto *camera = RE::PlayerCamera::GetSingleton();
    const auto cameraState = camera ? camera->currentState
                                    : RE::BSTSmartPointer<RE::TESCameraState>{};
    auto *third =
        cameraState && cameraState->id == RE::CameraState::kThirdPerson
            ? static_cast<RE::ThirdPersonState *>(cameraState.get())
            : nullptr;
    const auto view =
        third && third->freeRotationEnabled && third->applyOffsets &&
                camera->cameraTarget.get().get() == p
            ? fc::CameraHeading::capture(before, third->freeRotation.x)
            : std::nullopt;
    p->SetHeading(referenceYaw);
    if (view && camera->currentState.get() == cameraState.get() &&
        camera->cameraTarget.get().get() == p && third->freeRotationEnabled &&
        third->applyOffsets) {
      if (const auto relative = view->relativeTo(p->GetAngleZ())) {
        third->freeRotation.x = *relative;
        cameraPreserved = true;
      }
    }
  }
  if (diagnostics && attachedTime >= yawReportTime) {
    yawReportTime = attachedTime + 1;
    const auto pose = poses.lastOrientation();
    std::optional<float> actorRootYaw;
    if (const auto *root = p->Get3D(false)) {
      const auto &m = root->world.rotate.entry;
      actorRootYaw = fc::headingYaw({m[0][1], m[1][1], m[2][1]});
    }
    SKSE::log::info("Wall facing: target={:.3f} viewReference={:.3f} "
                    "viewRate={:.3f} actorBefore={:.3f} actorAfter={:.3f} "
                    "actor3D={:.3f}/{}; renderedTarget={:.3f} parent={:.3f}/{} "
                    "frame={:.3f}/{} ageMs={}; TDM={} cameraPreserved={}",
                    *desired, referenceYaw, viewHeading.speed(), before,
                    p->GetAngleZ(), actorRootYaw.value_or(0),
                    actorRootYaw.has_value(), pose.targetYaw, pose.parentYaw,
                    pose.parentValid, pose.renderedYaw, pose.renderedValid,
                    pose.sampledAt ? GetTickCount64() - pose.sampledAt : 0,
                    tdm ? yawResultName(result) : "absent", cameraPreserved);
  }
  return true;
}

/// Log pose and traversal costs
/// (diagnostics).
void reportOutput(RE::PlayerCharacter *p, fc::Motion motion, float coreMs,
                  float publishMs, int casts, std::uint64_t rayCandidates,
                  std::uint64_t rayClassifications, bool finishing) {
  if (!diagnostics)
    return;
  peakTraversalMs = std::max(peakTraversalMs, coreMs);
  peakPublishMs = std::max(peakPublishMs, publishMs);
  peakFrameCasts = std::max(peakFrameCasts, casts);
  peakFrameRayCandidates = std::max(peakFrameRayCandidates, rayCandidates);
  peakFrameRayClassifications =
      std::max(peakFrameRayClassifications, rayClassifications);
  const auto output = poses.lastOutputAudit();
  const bool observedOutputValid =
      output.sampledAt && output.worldMismatches == 0 &&
      output.layerWeight > .95f && GetTickCount64() - output.sampledAt < 250;
  if (!observedOutputValid)
    observedSpanContinuous = false;
  if (output.sampledAt > lastRenderedSample && observedOutputValid) {

    if (observedSpanContinuous && lastRenderedSample &&
        output.motion == lastRenderedMotion &&
        output.sampledAt - lastRenderedSample <= 100 &&
        int(output.motion) > 0 && int(output.motion) <= fc::motionCount)
      observedSpanSeconds[int(output.motion)] +=
          float(output.sampledAt - lastRenderedSample) * .001f;
    lastRenderedSample = output.sampledAt;
    observedSpanContinuous = true;
    if (output.motion != lastRenderedMotion) {
      lastRenderedMotion = output.motion;
      if (int(output.motion) > 0 && int(output.motion) <= fc::motionCount)
        ++totalObservedUses[int(output.motion)];
      if (int(output.motion) >= 39 && int(output.motion) <= fc::motionCount) {
        ++renderedUses[int(output.motion)];
        SKSE::log::info(
            "New action output observed: motion={} starts={} sampleAgeMs={}",
            int(output.motion), renderedUses[int(output.motion)],
            GetTickCount64() - output.sampledAt);
      }
    }
  }
  if (!finishing && attachedTime < outputReportTime)
    return;
  outputReportTime = attachedTime + 1;
  SKSE::log::info(
      "Pose output audit: publishedMotion={} renderedMotion={} "
      "layerWeight={:.4f} nativeRecovery={:.4f} synced={} ageMs={}; applied={} "
      "retired={} overwriteFrames={} lastOverwriteBone={} anglePeak={:.4f} "
      "distancePeak={:.3f}; worldMismatchFrames={} worldMismatches={} "
      "worldAngle={:.4f} worldDistance={:.3f} pass={} transformPasses={} "
      "selectedFlagRepairs={}; upperRollDeg={:.2f}/{:.2f}; "
      "coreMs={:.3f}/{:.3f} publishMs={:.3f}/{:.3f} callbackMs={:.3f} "
      "propagationMs={:.3f} casts={}/{} rayCandidates={}/{} "
      "rayClassifications={}/{}",
      int(motion), int(output.motion), output.layerWeight,
      output.nativeRecovery, graph(p, "bIsSynced"),
      output.sampledAt ? GetTickCount64() - output.sampledAt : 0,
      poses.applied.load(), poses.retiredOutputs.load(), output.overwriteFrames,
      output.lastOverwritten, output.peakOverwrittenAngle,
      output.peakOverwrittenDistance, output.worldMismatchFrames,
      output.worldMismatches, output.worldAngle, output.worldDistance,
      unsigned(output.pass), output.transformPasses, output.selectedFlagRepairs,
      output.upperRollDegrees[0], output.upperRollDegrees[1], coreMs,
      peakTraversalMs, publishMs, peakPublishMs, output.callbackMs,
      output.propagationMs, casts, peakFrameCasts, rayCandidates,
      peakFrameRayCandidates, rayClassifications, peakFrameRayClassifications);
  SKSE::log::info(
      "Surface placement audit: motion={} support={} samples={} "
      "gap={:.2f}/{:.2f}/{:.2f} palmPlaneGap={:.2f}/{:.2f} contacts={} "
      "reachError={:.2f} pos=({:.2f},{:.2f},{:.2f}) "
      "normal=({:.3f},{:.3f},{:.3f})",
      int(motion), poses.surface.surfaceGapValid, poses.surface.surfaceSamples,
      traversal.cfg.gap, poses.surface.measuredSurfaceGap,
      poses.surface.appliedSurfaceGap, poses.surface.surfacePalmGaps[0],
      poses.surface.surfacePalmGaps[1], poses.surface.contactCount,
      poses.surface.maxReachError, traversal.position.x, traversal.position.y,
      traversal.position.z, traversal.surfaceNormal.x,
      traversal.surfaceNormal.y, traversal.surfaceNormal.z);
  peakTraversalMs = peakPublishMs = 0;
  peakFrameCasts = 0;
  peakFrameRayCandidates = peakFrameRayClassifications = 0;
  SKSE::log::info(
      "Final display audit (sampled): lateWorldUpdates={} lateRootUpdates={} "
      "skinCalls={} ownedSkinSamples={} bodyInputs={} ownedInputs={} "
      "cachedSamples={} bodyMismatchSamples={}",
      poses.lateWorldUpdates.load(), poses.lateRootUpdates.load(),
      poses.skinCalls.load(), poses.ownedSkinCalls.load(),
      poses.skinBodyInputs.load(), poses.skinOwnedInputs.load(),
      poses.skinCacheHits.load(), poses.skinMismatchCalls.load());
  SKSE::log::info(
      "Skin audit budget: sampled={} skipped={} lockSkips={} inputs={} "
      "ancestors={} incomplete={} bodyDiffs={} bridgeDiffs={} extraDiffs={} "
      "totalUs={} peakSampleUs={}; max 4 samples/frame, 64 inputs+64 "
      "ancestors/sample; excludes original draw time",
      poses.skinAuditSamples.load(), poses.skinAuditSkipped.load(),
      poses.skinAuditLockSkips.load(), poses.skinAuditInputs.load(),
      poses.skinAuditAncestors.load(), poses.skinAuditBudgetStops.load(),
      poses.skinBodyMismatches.load(), poses.skinBridgeMismatches.load(),
      poses.skinExtraMismatches.load(), poses.skinAuditMicros.load(),
      poses.skinAuditPeakMicros.load());
}

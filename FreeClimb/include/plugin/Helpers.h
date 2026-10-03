//! Small input, HUD and controller
//! helpers shared by the update loop.
//!
//! Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.



/// Whether the gamepad is the active
/// input device.
bool gamepadSelected() {
  if (traversal.active())
    return gamepadOwned;
  if (!gamepadHookReady || !gamepadAvailable || !activeSettings.gamepad.enabled)
    return false;
  if (fc::entryChord(fc::mapKeys(inputState, activeSettings.bindings)))
    return false;
  return gamepadState.keys(activeSettings.gamepad.bindings).entry ||
         gamepadPreferred;
}
/// Climbing keys from the active
/// device.
fc::Keys keys() {
  return gamepadSelected() ? gamepadState.keys(activeSettings.gamepad.bindings)
                           : fc::mapKeys(inputState, activeSettings.bindings);
}
/// Button index of an XInput mask bit,
/// or 16 if none.
unsigned gamepadButtonIndex(std::uint32_t mask) {
  if (!RE::ControlMap::GetSingleton())
    return 16;
  const auto code = SKSE::InputMap::GamepadMaskToKeycode(mask);
  return code >= SKSE::InputMap::kMacro_GamepadOffset
             ? code - SKSE::InputMap::kMacro_GamepadOffset
             : 16;
}
/// Whether the game's jump control is
/// held.
bool nativeJumpHeld() {
  const auto *controls = RE::ControlMap::GetSingleton();
  if (!controls)
    return false;
  const auto keyboard =
      controls->GetMappedKey("Jump", RE::INPUT_DEVICE::kKeyboard);
  const auto gamepad = gamepadButtonIndex(
      controls->GetMappedKey("Jump", RE::INPUT_DEVICE::kGamepad));
  return (keyboard < 256 &&
          inputState.held(static_cast<fc::KeyCode>(keyboard)) &&
          inputOwnership.nativeDown(keyboard)) ||
         (gamepadAvailable && gamepadState.held(gamepad) &&
          gamepadOwnership.startedNativeJump(gamepad));
}

RE::NiPoint3 ni(fc::Vec v) { return {v.x, v.y, v.z}; }
fc::Vec vec(RE::NiPoint3 v) { return {v.x, v.y, v.z}; }
/// Read a bool animation graph
/// variable.
bool graph(RE::Actor *p, const char *name) {
  bool result = false;
  p->GetGraphVariableBool(name, result);
  return result;
}
/// Show a HUD message when
/// notifications are on.
void note(const char *text) {
  if (notifications)
    RE::SendHUDMessage::ShowHUDMessage(text);
}
/// Whether the game window has focus.
bool foreground() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}

/// Logs how the camera and actor turn
/// around a climb entry (diagnostics
/// only).
struct EntryLookDiagnostics {
  bool tracking{};
  float elapsed{}, next{}, peakProbeMs{};
  unsigned probes{};
  float beforeActorYaw{}, beforeCameraYaw{}, beforeLookX{}, beforeLookY{};
  void beforeNative(RE::PlayerCharacter *player) {
    if (!diagnostics || (!tracking && !fc::entryChord(keys())))
      return;
    beforeActorYaw = player->GetAngleZ();
    const auto *camera = RE::PlayerCamera::GetSingleton();
    const auto *state = camera ? camera->currentState.get() : nullptr;
    const auto *third = state && state->id == RE::CameraState::kThirdPerson
                            ? static_cast<const RE::ThirdPersonState *>(state)
                            : nullptr;
    beforeCameraYaw = third ? third->currentYaw : 0.f;
    if (const auto *controls = RE::PlayerControls::GetSingleton()) {
      beforeLookX = controls->data.lookInputVec.x;
      beforeLookY = controls->data.lookInputVec.y;
    }
  }
  std::array<std::atomic<std::uint64_t>, 2> mouseEvents{};
  std::array<std::atomic<std::int64_t>, 2> mouseX{}, mouseY{};
  void queue(RE::InputEvent *first, unsigned stage) {
    if (!diagnostics)
      return;
    unsigned visited = 0;
    for (auto *event = first; event && visited++ < 2048; event = event->next)
      if (const auto *mouse = event->AsMouseMoveEvent()) {
        mouseEvents[stage].fetch_add(1, std::memory_order_relaxed);
        mouseX[stage].fetch_add(mouse->mouseInputX, std::memory_order_relaxed);
        mouseY[stage].fetch_add(mouse->mouseInputY, std::memory_order_relaxed);
      }
  }
  void sample(RE::PlayerCharacter *player, fc::Keys keys, float dt) {
    if (!diagnostics) {
      tracking = false;
      return;
    }
    const bool held = fc::entryChord(keys), begin = held && !tracking,
               end = !held && tracking;
    if (begin) {
      tracking = true;
      elapsed = next = peakProbeMs = 0;
      probes = 0;
    }
    if (!tracking)
      return;
    elapsed += std::clamp(dt, 0.f, .05f);
    if (!begin && !end && elapsed < next)
      return;
    const auto *camera = RE::PlayerCamera::GetSingleton();
    const auto *controls = RE::PlayerControls::GetSingleton();
    const auto *map = RE::ControlMap::GetSingleton();
    const auto *state = camera ? camera->currentState.get() : nullptr;
    const auto *third = state && state->id == RE::CameraState::kThirdPerson
                            ? static_cast<const RE::ThirdPersonState *>(state)
                            : nullptr;
    SKSE::log::info(
        "Entry look: event={} t={:.2f} held={} state={} camera={} "
        "actorYaw={:.4f} cameraYaw={:.4f} free=({:.4f},{:.4f}) freeEnabled={} "
        "lookEnabled={} input=({:.5f},{:.5f}); beforeNativeActorYaw={:.4f} "
        "beforeNativeCameraYaw={:.4f} beforeNativeInput=({:.5f},{:.5f}); "
        "ownsController={} ownsYaw={} ownsDirection={} ownsSync={} synced={} "
        "animationDriven={} preparing={} tdmMode={} tdmOwner={}; probes={} "
        "peakMs={:.3f}; queueMouseBefore={} ({},{}) after={} ({},{})",
        begin ? "begin"
        : end ? "end"
              : "held",
        elapsed, held, int(traversal.state), state ? int(state->id) : -1,
        player->GetAngleZ(), third ? third->currentYaw : 0.f,
        third ? third->freeRotation.x : 0.f,
        third ? third->freeRotation.y : 0.f,
        third && third->freeRotationEnabled,
        map && map->IsLookingControlsEnabled(),
        controls ? controls->data.lookInputVec.x : 0.f,
        controls ? controls->data.lookInputVec.y : 0.f, beforeActorYaw,
        beforeCameraYaw, beforeLookX, beforeLookY, bool(ownedController),
        yawOwned, directionOwned, syncOwned, graph(player, "bIsSynced"),
        player->IsAnimationDriven(), preparationStarted != 0,
        tdm ? int(tdm->GetDirectionalMovementMode()) : -1,
        tdm ? tdm->GetDisableDirectionalMovementOwner() : 0, probes,
        peakProbeMs, mouseEvents[0].exchange(0), mouseX[0].exchange(0),
        mouseY[0].exchange(0), mouseEvents[1].exchange(0),
        mouseX[1].exchange(0), mouseY[1].exchange(0));
    next = elapsed + 1;
    peakProbeMs = 0;
    probes = 0;
    if (end)
      tracking = false;
  }
  struct Probe {
    EntryLookDiagnostics &owner;
    bool enabled;
    std::chrono::steady_clock::time_point start;
    explicit Probe(EntryLookDiagnostics &value)
        : owner(value), enabled(diagnostics),
          start(std::chrono::steady_clock::now()) {}
    ~Probe() {
      if (enabled) {
        ++owner.probes;
        owner.peakProbeMs = std::max(
            owner.peakProbeMs, std::chrono::duration<float, std::milli>(
                                   std::chrono::steady_clock::now() - start)
                                   .count());
      }
    }
  };
} entryLookDiagnostics;

/// Whether climbing input must be
/// ignored (menus, console, focus).
bool grabInputSuspended() {
  const auto ui = RE::UI::GetSingleton();
  return !foreground() || !ui || fc::settingsMenuBlocking() ||
         ui->GameIsPaused() || ui->numItemMenus > 0 ||
         ui->IsMenuOpen("Console") || ui->IsMenuOpen("Dialogue Menu") ||
         ui->IsMenuOpen("Loading Menu") || ui->IsMenuOpen("TweenMenu");
}
/// Abort a pending pose preparation.
void cancelEntryPreparation() {
  entryPreparationGrace.cancel();
  poses.cancelPreparation();
  preparationStarted = 0;
  if (preparationChangedView) {
    if (auto *camera = RE::PlayerCamera::GetSingleton();
        camera && camera->IsInThirdPerson())
      camera->ForceFirstPerson();
    preparationChangedView = false;
  }
}
/// Abort a pending jump grab and its
/// preparation.
void cancelGrabRequest() {

  if (jumpGrab.pending() || preparationStarted || preparationChangedView)
    cancelEntryPreparation();
  entryPreparationGrace.cancel();
  jumpGrab.tick(0, true);
  preparationStarted = 0;
  climbEntry.blockUntilRelease();
  grabProbeCooldown = 0;
}

/// Stop the vanilla movement and sprint
/// state.
void stopLocomotion() {
  if (auto controls = RE::PlayerControls::GetSingleton()) {
    controls->data.moveInputVec = {0, 0};
    controls->data.prevMoveVec = {0, 0};
  }
  if (ownedController) {
    const RE::hkVector4 zero(0, 0, 0, 0);
    controllerGravity.suppress();
    ownedController->outVelocity = zero;
    ownedController->initialVelocity = zero;
    ownedController->velocityMod = zero;
    ownedController->SetLinearVelocityImpl(zero);
  }
}

/// Hold the character controller
/// (gravity off) for the climb.
void takeController(RE::bhkCharacterController *controller, bool replacing) {
  controllerGravity.take(controller);
  ownedController = RE::NiPointer<RE::bhkCharacterController>(controller);
  if (replacing) {
    SKSE::log::info("Continued climbing with replacement controller; restored "
                    "gravity={:.3f}",
                    controllerGravity.savedGravity());
    stopLocomotion();
  }
}

/// Fit the controller collision to the
/// climbing body.
void fitBody(RE::PlayerCharacter *p) {
  traversal.cfg.runSpeed =
      wallRunSpeedOverride > 0 ? wallRunSpeedOverride : 379.5f;
  const float scale = p->GetScale();
  traversal.cfg.contextScale =
      std::isfinite(scale) ? std::clamp(scale, .5f, 2.f) : 1.f;
  const auto &extents = p->GetCharController()->collisionBound.extents;
  if (std::isfinite(extents.x) && std::isfinite(extents.y) &&
      std::isfinite(extents.z) && extents.z > 30 && extents.z < 120) {
    traversal.cfg.radius =
        std::clamp(std::max(extents.x, extents.y), 22.0f, 45.0f);
    traversal.cfg.gap = traversal.cfg.radius + 6;
    traversal.cfg.height = std::clamp(extents.z * 2 + 3, 125.0f, 200.0f);
  }
}

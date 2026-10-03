//! Applying, saving, reloading and
//! syncing user settings with the menu.
//!
//!  Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.


/// Copy settings into the traversal,
/// audio and input state.
void applyRuntimeSettings(const fc::UserSettings &value) {
  const auto oldGamepad = activeSettings.gamepad;
  activeSettings = fc::sanitizeUserSettings(value);
  if (activeSettings.gamepad != oldGamepad)
    gamepadState.blockUntilButtonsReleased();
  const auto &u = activeSettings;
  enabled = u.enabled;
  notifications = u.notifications;
  lowStaminaNotifications = u.lowStaminaNotifications;
  jumpToAttach = u.jumpToAttach;
  autoMantle = u.autoMantle;
  diagnostics = u.diagnostics;
  grabMaxSnap = u.grabMaxSnap;
  poses.traceOutput = diagnostics;
  lowStaminaNoted = false;
  if (!diagnostics)
    crestAuditTime = -1;
  auto &c = traversal.cfg;
  c.contextActions = u.contextActions;
  c.threepeatAnimations = u.threepeatAnimations;
  c.automaticClimbActions = u.automaticClimbActions;
  c.legacyAutomaticHops = false;
  c.surfaceActionVariants = u.surfaceActionVariants;
  c.wallRunObstacleJumps = u.wallRunObstacleJumps;
  c.contextualMantleEnabled = u.contextualMantleEnabled;
  c.automaticSideWeights = u.automaticSideWeights;
  c.climbSpeed = u.upSpeed;
  c.downSpeed = u.downSpeed;
  c.sideSpeed = u.sideSpeed;
  wallRunSpeedOverride = u.wallRunSpeed;
  c.runSpeed = u.wallRunSpeed > 0 ? u.wallRunSpeed : 379.5f;
  c.diagonalRunMultiplier = u.diagonalRunMultiplier;
  c.autoActionMinSeconds = u.autoActionMinSeconds;
  c.autoActionMaxSeconds = u.autoActionMaxSeconds;
  c.fancyJumps = u.fancyJumps;
  c.hopOut = u.hopOut;
  c.kickOut = u.kickOut;
  c.reach = u.reach;
  c.groundJumpHeight = u.groundJumpHeight;
  c.maxNormalZ = u.maxNormalZ;
  c.staminaEnabled = u.staminaEnabled;
  c.drain = u.movingPerSecond;
  c.hangDrain = u.hangingPerSecond;
  c.startStamina = u.requiredToGrab;
  traversalAudio.enabled = u.audioEnabled;
  traversalAudio.volume = u.audioVolume;
  if (!u.audioEnabled)
    traversalAudio.stop();
  cancelGrabRequest();
  wallRunEntryGate.reset();
  lastHop = keys().space;
  if (ready)
    poses.library.configureThreepeat(c);
}

/// Update the state the menu shows.
void refreshMenuSnapshot() {
  fc::SettingsMenuSnapshot snapshot;
  snapshot.ready = ready;
  snapshot.traversalActive = traversal.active();
  snapshot.diagnosticsEnabled = diagnostics;
  snapshot.audioReady = audioReady;
  snapshot.status = settingsStatus;
  snapshot.error = settingsError;
  snapshot.packName = "meshes/actors/character/animations/FreeClimb/pack.json";
  snapshot.automaticAttempts = traversal.automaticAttemptCount();
  snapshot.automaticActions = traversal.automaticActionCount();
  snapshot.wallRunObstacleJumps = traversal.obstacleJumpCount();
  for (const auto &slot : poses.packReport.slots) {
    const auto id = static_cast<unsigned>(slot.motion);
    if (id < 1 || id > fc::motionCount)
      continue;
    snapshot.slots.push_back({std::string(fc::motionSlotNames[id - 1]),
                              slot.file, slot.reason, int(slot.status),
                              totalMotionUses[id], totalObservedUses[id],
                              slot.samples, slot.seconds});
  }
  std::scoped_lock lock(settingsMutex);
  snapshot.movementPending = pendingSettings.has_value();
  snapshot.reloadPending = pendingReload;
  menuSnapshot = std::move(snapshot);
}

/// Apply pending menu requests on the
/// game thread: settings, audio, saving
/// and pack reloads (only when off the
/// wall).
void serviceSettings() {
  std::optional<fc::UserSettings> requested;
  std::optional<std::pair<bool, float>> audio;
  bool save = false, reload = false;
  std::uint64_t revision = 0, reloadRequest = 0;
  {
    std::scoped_lock lock(settingsMutex);
    requested = pendingSettings;
    audio = pendingAudio;
    pendingAudio.reset();
    save = pendingSave;
    pendingSave = false;
    reload = pendingReload;
    revision = settingsRevision;
    reloadRequest = reloadRevision;
  }
  if (audio) {
    traversalAudio.enabled = audio->first;
    traversalAudio.volume = audio->second;
    activeSettings.audioEnabled = audio->first;
    activeSettings.audioVolume = audio->second;
    if (!audio->first)
      traversalAudio.stop();
  }
  if (save && requested) {
    std::string error;
    if (!fc::saveUserSettings("Data/SKSE/Plugins/FreeClimb.ini", *requested,
                              error)) {
      settingsStatus = "save_failed";
      settingsError = std::move(error);
      SKSE::log::error("Settings save failed: {}", settingsError);
    } else {
      settingsStatus = "pending";
      settingsError.clear();
    }
  }
  if (requested) {
    notifications = requested->notifications;
    lowStaminaNotifications = requested->lowStaminaNotifications;
    traversalAudio.enabled = requested->audioEnabled;
    traversalAudio.volume = requested->audioVolume;
    if (!requested->audioEnabled)
      traversalAudio.stop();
    if (!requested->enabled && traversal.active())
      release(RE::PlayerCharacter::GetSingleton(), "disabled in settings",
              true);
    if (!traversal.active())
      cancelEntryPreparation();
    if (!traversal.active() && poses.canReloadPack()) {
      applyRuntimeSettings(*requested);
      {
        std::scoped_lock lock(settingsMutex);
        if (settingsRevision == revision)
          pendingSettings.reset();
      }
      if (settingsStatus != "save_failed") {
        settingsStatus = "applied";
        settingsError.clear();
      }
    } else if (settingsStatus != "save_failed")
      settingsStatus = "pending";
  }
  if (reload && dataLoaded && !traversal.active()) {
    cancelEntryPreparation();
    if (poses.canReloadPack()) {
      {
        std::scoped_lock lock(settingsMutex);
        if (reloadRevision == reloadRequest)
          pendingReload = false;
      }
      if (!ready)
        initializeRuntime();
      else if (poses.reloadPack()) {
        poses.library.configureThreepeat(traversal.cfg);
        cancelGrabRequest();
        settingsStatus = "reloaded";
        settingsError.clear();
      } else {
        settingsStatus = "reload_failed";
        settingsError = poses.packReport.error;
      }
    }
  }
  if (requested || audio || reload || GetTickCount64() >= nextMenuSnapshot) {
    refreshMenuSnapshot();
    nextMenuSnapshot = GetTickCount64() + 200;
  }
}

/// Register the settings menu once SKSE
/// Menu Framework is available.
void registerMenu() {
  if (menuRegistered)
    return;
  fc::SettingsMenuCallbacks callbacks;
  callbacks.getSettings = [] {
    std::scoped_lock lock(settingsMutex);
    return desiredSettings;
  };
  callbacks.snapshot = [] {
    std::scoped_lock lock(settingsMutex);
    return menuSnapshot;
  };
  callbacks.requestSave = [](fc::UserSettings value) {
    {
      std::scoped_lock lock(settingsMutex);
      desiredSettings = fc::sanitizeUserSettings(value);
      pendingSettings = desiredSettings;
      pendingSave = true;
      ++settingsRevision;
    }
    SKSE::GetTaskInterface()->AddTask([] { serviceSettings(); });
  };
  callbacks.requestAudio = [](bool enabled, float volume) {
    {
      std::scoped_lock lock(settingsMutex);
      pendingAudio = std::pair{enabled, std::clamp(volume, 0.f, 1.f)};
    }
    SKSE::GetTaskInterface()->AddTask([] { serviceSettings(); });
  };
  callbacks.requestReloadAnimations = [] {
    {
      std::scoped_lock lock(settingsMutex);
      pendingReload = true;
      ++reloadRevision;
    }
    SKSE::GetTaskInterface()->AddTask([] { serviceSettings(); });
  };
  menuRegistered = fc::registerSettingsMenu(std::move(callbacks));
  SKSE::log::info("Optional settings menu registered={}; language={}",
                  menuRegistered, desiredSettings.language);
}

/// Load `FreeClimb.ini` and apply it.
void loadSettings() {
  const auto loaded = fc::loadUserSettings("Data/SKSE/Plugins/FreeClimb.ini");
  desiredSettings = loaded.settings;
  applyRuntimeSettings(desiredSettings);
  for (const auto &warning : loaded.warnings)
    SKSE::log::warn("Settings: {}", warning);
  SKSE::log::info(
      "FreeClimb {}; standalone HKX framework; climb entry={}; wall-run "
      "modifier={}; stamina enabled={}",
      SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."),
      fc::serializeKeyChord(activeSettings.bindings.entry),
      fc::serializeKeyChord(activeSettings.bindings.runModifier),
      traversal.cfg.staminaEnabled);
  SKSE::log::info(
      "Experimental controller: enabled={} entry={} run={} hop={} drop={} "
      "deadzone={:.2f} trigger={:.2f}",
      activeSettings.gamepad.enabled,
      fc::serializeGamepadChord(activeSettings.gamepad.bindings.entry),
      fc::serializeGamepadChord(activeSettings.gamepad.bindings.runModifier),
      fc::serializeGamepadChord(activeSettings.gamepad.bindings.hop),
      fc::serializeGamepadChord(activeSettings.gamepad.bindings.drop),
      activeSettings.gamepad.deadzone, activeSettings.gamepad.triggerThreshold);
}

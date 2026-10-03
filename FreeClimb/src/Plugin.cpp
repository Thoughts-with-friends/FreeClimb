#include "PCH.h"
#include "animation/AnimationState.h"
#include "audio/TraversalAudioRuntime.h"
#include "input/GamepadInput.h"
#include "input/InputBindings.h"
#include "pose/PoseRuntime.h"
#include "ray/FilteredRayCollector.h"
#include "ray/NativeShapeWitness.h"
#include "ray/RayCandidateCache.h"
#include "ray/RayFilterPolicy.h"
#include "runtime/RuntimeLog.h"
#include "runtime/RuntimeSupport.h"
#include "settings/SettingsMenu.h"
#include "settings/UserSettings.h"
#include "traversal/ControllerGravityLease.h"
#include "traversal/Controls.h"
#include "traversal/Core.h"
#include "traversal/GroundMotionProbe.h"
#include "traversal/NativeWalkableApproach.h"
#include "traversal/TraversalCapture.h"
#include "vendor/TrueDirectionalMovementAPI.h"
#include "view/CameraHeading.h"
#include "view/ViewHeading.h"
#include <SKSE/InputMap.h>
#include <chrono>

namespace
{
#include "plugin/State.h"

#include "plugin/Helpers.h"

#include "plugin/GameWorld.h"

#include "plugin/Motion.h"

#include "plugin/Entry.h"

#include "plugin/Update.h"

#include "plugin/Hooks.h"

#include "plugin/Settings.h"

  void initializeRuntime()
  {
    if (ready)
      return;
    if (!runtimeHooksReady())
      return;
    audioReady = traversalAudio.install();
    SKSE::log::info(
        "Independent HKX framework; configurable climb-entry combination, "
        "wall-run obstacle jumps, native ground input preserved");
    if (!poses.install())
    {
      settingsStatus = "pack_unavailable";
      settingsError = poses.packReport.error;
      refreshMenuSnapshot();
      return;
    }
    poses.library.configureThreepeat(traversal.cfg);
    SKSE::log::info(
        "Threepeat animations: library={}, enabled={}, surfaceVariants={}",
        poses.library.hasThreepeat(), traversal.cfg.threepeatAnimations,
        traversal.cfg.surfaceActionVariants);
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_PlayerCharacter[0]};
    originalUpdate = vtable.write_vfunc(fc::runtime::actorUpdateSlot, update);
    AttachedSpaceGuard::install();
    GamepadGuard::install();
    InputGuard<RE::MovementHandler, 0>::install(RE::VTABLE_MovementHandler[0]);
    InputGuard<RE::JumpHandler, 1>::install(RE::VTABLE_JumpHandler[0]);
    InputGuard<RE::SneakHandler, 2>::install(RE::VTABLE_SneakHandler[0]);
    InputGuard<RE::SprintHandler, 3>::install(RE::VTABLE_SprintHandler[0]);
    InputGuard<RE::ReadyWeaponHandler, 4>::install(
        RE::VTABLE_ReadyWeaponHandler[0]);
    InputGuard<RE::AttackBlockHandler, 5>::install(
        RE::VTABLE_AttackBlockHandler[0]);
    InputGuard<RE::ActivateHandler, 6>::install(RE::VTABLE_ActivateHandler[0]);
    InputGuard<RE::TogglePOVHandler, 7>::install(RE::VTABLE_TogglePOVHandler[0]);
    InputGuard<RE::ShoutHandler, 8>::install(RE::VTABLE_ShoutHandler[0]);
    InputGuard<RE::AutoMoveHandler, 9>::install(RE::VTABLE_AutoMoveHandler[0]);
    InputGuard<RE::RunHandler, 11>::install(RE::VTABLE_RunHandler[0]);
    InputGuard<RE::ToggleRunHandler, 12>::install(RE::VTABLE_ToggleRunHandler[0]);
    RE::UI::GetSingleton()->AddEventSink(&menuListener);
    ready = true;
    settingsStatus = "ready";
    settingsError.clear();
    refreshMenuSnapshot();
    SKSE::log::info("Hooks installed; data ready");
  }

  void onMessage(SKSE::MessagingInterface::Message *m)
  {
    switch (m->type)
    {
    case SKSE::MessagingInterface::kPostLoad:
      registerMenu();
      tdm = static_cast<TDM_API::IVTDM3 *>(TDM_API::RequestPluginAPI());
      SKSE::log::info("TDM API available: {}", tdm != nullptr);
      break;
    case SKSE::MessagingInterface::kDataLoaded:
    {
      dataLoaded = true;
      registerMenu();
      initializeRuntime();
      break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
      release(RE::PlayerCharacter::GetSingleton(), "pre-load");
      break;
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame:
    {
      auto p = RE::PlayerCharacter::GetSingleton();
      release(p, "new game / loaded");
      traversal.reset();
      attachmentsSinceLoad = 0;
      nativeShapeBaselineSamples = nativeShapeBaselineAttempts = 0;
      nativeShapeBaselineAge = nativeShapeBaselineRetry = 0;
      jumpGrab.cancel();
      nativeJumpIntent = {};
      inputState.reset();
      inputOwnership.reset();
      gamepadState.reset();
      gamepadOwnership.reset();
      gamepadPreferred = false;
      gamepadLost = false;
      totalMotionUses.fill(0);
      totalObservedUses.fill(0);
      climbEntry.blockUntilRelease();
      lastHop = false;
      refreshMenuSnapshot();
      break;
    }
    case SKSE::MessagingInterface::kSaveGame:
      release(RE::PlayerCharacter::GetSingleton(), "save");
      break;
    }
  }
} // namespace

SKSEPluginLoad(const SKSE::LoadInterface *skse)
{

  fc::initializeLogging();
  if (!skse)
  {
    SKSE::log::error("Plugin load rejected: missing SKSE interface");
    return false;
  }
  const auto reportedVersion = skse->RuntimeVersion();
  const auto expectedVersion = REL::Version::unpack(
      fc::runtime::gameVersionFromSKSE(reportedVersion.pack()));
  SKSE::log::info(
      "Plugin load: FreeClimb={} game={} SKSE={} loaderRuntime={}",
      SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."),
      expectedVersion.string("."),
      REL::Version::unpack(skse->SKSEVersion()).string("."),
      reportedVersion.string("."));
  if (!fc::runtime::supportedSKSE(reportedVersion.pack()))
  {
    SKSE::log::error("Plugin load rejected: SKSE runtime {} is outside the "
                     "supported loader table",
                     reportedVersion.string("."));
    return false;
  }
  SKSE::log::info("Initializing SKSE; expected Address Library: "
                  "Data/SKSE/Plugins/{}-{}.bin",
                  fc::runtime::addressFormat(expectedVersion.pack()) == 1
                      ? "version"
                      : "versionlib",
                  expectedVersion.string("-"));
  SKSE::Init(skse, SKSE::InitInfo{.log = false});
  const auto gameVersion = REL::Module::get().version();
  if (gameVersion != expectedVersion ||
      !fc::runtime::supported(gameVersion.pack()))
  {
    SKSE::log::error(
        "Plugin load rejected: executable {} does not match SKSE runtime {}",
        gameVersion.string("."), reportedVersion.string("."));
    return false;
  }
  SKSE::log::info("Runtime {}: family={}, addressLibraryFormat={}, explicit "
                  "support table; AE builds have not been tested in-game",
                  gameVersion.string("."),
                  static_cast<int>(fc::runtime::family(gameVersion.pack())),
                  fc::runtime::addressFormat(gameVersion.pack()));
  loadSettings();
  auto serialization = SKSE::GetSerializationInterface();
  serialization->SetUniqueID(0x46434C4D);
  serialization->SetSaveCallback([](SKSE::SerializationInterface *)
                                 { release(RE::PlayerCharacter::GetSingleton(), "serialize"); });
  serialization->SetRevertCallback([](SKSE::SerializationInterface *)
                                   { release(RE::PlayerCharacter::GetSingleton(), "revert"); });
  const bool registered =
      SKSE::GetMessagingInterface()->RegisterListener(onMessage);
  if (registered)
    SKSE::log::info("Plugin load complete; waiting for game data");
  else
    SKSE::log::error(
        "Plugin load failed: SKSE message listener registration failed");
  return registered;
}

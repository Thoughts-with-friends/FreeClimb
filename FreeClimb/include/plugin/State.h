//! Plugin-wide runtime state: climbing
//! logic, input tracking, pose and
//! audio output, settings and
//! diagnostics counters.
//!
//!  Part of `Plugin.cpp`: included
//! inside its anonymous namespace, in
//! order, and relies on the fragments
//! before it. Do not include it
//! anywhere else.


/// The climbing state machine.
fc::Traversal traversal;
/// Input and entry trackers (see
/// `traversal/Controls.h` and
/// `input/`).
fc::JumpGrabGate jumpGrab;
fc::ClimbEntryIntent climbEntry;
fc::NativeJumpIntent nativeJumpIntent;
fc::EntryPreparationGrace entryPreparationGrace;
fc::InputState inputState;
fc::InputOwnership inputOwnership;
fc::GamepadState gamepadState;
fc::GamepadOwnership gamepadOwnership;
bool gamepadAvailable{}, gamepadPreferred{}, gamepadOwned{}, gamepadLost{},
    gamepadHookReady{};
/// Ignores Shift held since entry.
fc::WallRunEntryGate wallRunEntryGate;
/// Puts the solved pose on the player.
fc::PoseRuntime poses;
/// Plays traversal sounds.
fc::TraversalAudioRuntime traversalAudio;
/// Smoothed camera heading while
/// climbing.
fc::ViewHeading viewHeading;
/// Logs being stuck while walking.
fc::GroundMotionProbe groundMotionProbe;
/// Stall capture for exact replay, and
/// its per-session limit.
fc::TraversalCapture geometryCapture;
fc::TraversalCapture::SessionGate geometryCaptureGate;
/// Set while FreeClimb owns the
/// animation graph.
fc::AnimationState animationState;
/// Controller and place the climb
/// started in; a change releases.
RE::NiPointer<RE::bhkCharacterController> ownedController;
RE::TESObjectCELL *climbingCell{};
RE::TESWorldSpace *climbingWorldspace{};
/// True Directional Movement API, if
/// installed.
TDM_API::IVTDM3 *tdm{};
/// What FreeClimb currently owns: TDM
/// yaw / direction, `bIsSynced`, and
/// whether hooks are ready.
bool yawOwned{}, directionOwned{}, syncOwned{}, ready{};
/// Run toggle saved at entry and the
/// idle run state.
bool savedRunning{}, runningSaved{}, idleRunning = true, idleRunningKnown{};
/// Gravity off while climbing.
fc::ControllerGravityLease<RE::bhkCharacterController> controllerGravity;
/// Watchdog for pose output.
fc::PoseHealth poseHealth;
std::uint64_t preparationStarted{};
bool preparationChangedView{};
fc::Motion lastMotion = fc::Motion::none;
bool lastHop{};
float grabProbeCooldown{};
/// Settings mirrored for fast access in
/// the update loop.
bool notifications = true, enabled = true;
bool jumpToAttach = true, autoMantle = true, diagnostics = false;
/// Max entry snap distance, and timers
/// for diagnostics.
float grabMaxSnap = 60, diagnosticCooldown{}, attachedTime{};
float approachSpeed{}, stallReportTime{}, poseRefreshTime{},
    wallRunSpeedOverride{}, yawReportTime{};
/// Peak costs reported in the log.
float outputReportTime{}, peakTraversalMs{}, peakPublishMs{};
int peakFrameCasts{};
std::uint64_t peakFrameRayCandidates{}, peakFrameRayClassifications{};
/// Last logged plan states and
/// counters, to log only changes.
unsigned lastContextStatus{}, lastThreepeatStatus{};
bool lastCornerReported{}, lastEdgeReported{};
float contextReportTime{};
unsigned contextHops{}, contextCorners{}, contextEaves{};
unsigned lastAutomaticAction{}, automaticActionBase{}, lastObstacleJump{},
    obstacleJumpBase{};
unsigned lastAutomaticAttempt{}, automaticAttemptBase{};
unsigned opportunityBase{}, surfaceActionBase{}, idleActionBase{};
fc::Motion lastRenderedMotion = fc::Motion::none;
std::uint64_t lastRenderedSample{};
/// Per-motion usage statistics for the
/// diagnostics page.
std::array<unsigned, fc::motionCount + 1> renderedUses{};
std::array<float, fc::motionCount + 1> selectedSeconds{}, observedSpanSeconds{};
bool observedSpanContinuous{};

std::array<float, 6> inputSeconds{};
/// Climbs since the last load.
unsigned attachmentsSinceLoad{};
unsigned nativeShapeBaselineSamples{}, nativeShapeBaselineAttempts{};
float nativeShapeBaselineAge{}, nativeShapeBaselineRetry{};
/// Roof crest audit state
/// (diagnostics).
float crestAuditTime = -1;
unsigned crestAuditPhase{};
bool crestAuditSupported{};
fc::Vec crestAuditStart{};
bool lowStaminaNoted{};
bool lowStaminaNotifications = true, dataLoaded{}, menuRegistered{},
     audioReady{};
/// Guards the settings shared with the
/// menu thread below.
std::mutex settingsMutex;
/// Settings in use, and the ones the
/// menu shows.
fc::UserSettings activeSettings, desiredSettings;
/// Changes from the menu waiting to be
/// applied on the game thread.
std::optional<fc::UserSettings> pendingSettings;
std::optional<std::pair<bool, float>> pendingAudio;
bool pendingSave{}, pendingReload{};
std::uint64_t settingsRevision{}, reloadRevision{};
/// State shown by the menu.
fc::SettingsMenuSnapshot menuSnapshot;
std::string settingsStatus = "not_ready", settingsError;
std::array<std::uint64_t, fc::motionCount + 1> totalMotionUses{},
    totalObservedUses{};
std::uint64_t nextMenuSnapshot{};
/// Defined in `Settings.h`.
void serviceSettings();
/// Defined in `Plugin.cpp`.
void initializeRuntime();
void refreshMenuSnapshot();
std::array<std::uint32_t, fc::motionCount + 1> motionUses{};
/// Original player update, called first
/// by the hook.
REL::Relocation<void (*)(RE::PlayerCharacter *, float)> originalUpdate;

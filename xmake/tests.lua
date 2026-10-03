--- Regression tests (port of `cmake/Tests.cmake`).
---
--- Every test is a standalone binary that is not built by plain `xmake`.
--- Run them with:
---
--- ```sh
--- xmake test                    # all tests
--- xmake test test_pose_rig/*    # one test
--- ```
---
--- Tests that need the animation pack are skipped when `motion` does not exist.

local root = path.join(os.scriptdir(), "..")

--- Header folders of the three projects (tests may use any of them).
local headers = {
    path.join(root, "FreeClimbAnimationInput/include"),
    path.join(root, "FreeClimbSettings/include"),
    path.join(root, "FreeClimb/include"),
}

--- Read a path option. Empty or unset falls back to `default`.
--- Relative paths are taken from the project root.
local function setting(name, default)
    local value = get_config(name)
    if value == nil or value == "" then
        return default
    end
    return path.absolute(value, os.projectdir())
end

--- Paths given by `xmake f --motion=... --hkx=... --legacy=...`.
local anims = path.join(root, "runtime/meshes/actors/character/animations/FreeClimb")
local motion = setting("motion", path.join(anims, "pack.json"))
local hkx = setting("hkx", anims)
local legacy = setting("legacy", "")

local has_motion = os.isfile(motion)
local has_hkx = os.isfile(path.join(hkx, "hang.hkx"))
local has_legacy = legacy ~= "" and os.isfile(legacy)

--- `{ name, source (tests/<source>.cpp), needs motion pack }`
local cases = {
    { "pose_world_bone", "PoseWorldBoneTests", true },
    { "pose_rig", "PoseRigTests", true },
    { "animation_skeleton_binding", "AnimationSkeletonBindingTests", false },
    { "overhang_search_performance", "OverhangSearchPerformanceTests", false },
    { "ray_candidate_cache", "RayCandidateCacheTests", false },
    { "ray_collector_validation_cache", "RayCollectorValidationCacheTests", false },
    { "search_retry", "SearchRetryTests", false },
    { "obstacle_search_performance", "ObstacleSearchPerformanceTests", false },
    { "binding_capture", "BindingCaptureTests", false },
    { "gamepad_input", "GamepadInputTests", false },
    { "input_bindings", "InputBindingsTests", false },
    { "user_settings", "UserSettingsTests", false },
    { "translation_catalog", "TranslationCatalogTests", false },
    { "stamina_mode", "StaminaModeTests", false },
    { "animation_pack", "AnimationPackTests", true },
    { "runtime_policy", "RuntimePolicyTests", false },
    { "hkx_animation", "HkxAnimationTests", false },
    { "hkx_spline", "HkxSplineTests", false },
    { "finger_clearance", "FingerClearanceTests", true },
    { "no_independent_hang", "NoIndependentHangTests", true },
    { "falling_hang_entry", "FallingHangEntryTests", true },
    { "selective_automatic_actions", "SelectiveAutomaticActionsTests", true },
    { "grounded_entry_exclusion", "GroundedEntryExclusionTests", false },
    { "grounded_entry_exclusion_review", "GroundedEntryExclusionReviewTests", false },
    { "low_face_entry", "LowFaceEntryTests", false },
    { "entry_footprint", "EntryFootprintTests", false },
    { "ground_support_entry", "GroundSupportEntryTests", false },
    { "ground_entry_geometry", "GroundEntryGeometryTests", false },
    { "long_stair_entry", "LongStairEntryTests", false },
    { "attach_plane_coherence", "AttachPlaneCoherenceTests", false },
    { "supported_idle", "SupportedIdleTests", true },
    { "entry_handoff", "EntryHandoffTests", false },
    { "inclined_top_probe", "InclinedTopProbeTests", false },
    { "top_exit_continuity", "TopExitContinuityTests", true },
    { "automatic_variety_diagonal", "AutomaticVariety711", true },
    { "diagonal_variety_safety", "DiagonalVarietySafetyTests", true },
    { "fluidity_pose", "FluidityPoseTests", true },
    { "automatic_variety", "AutomaticVarietyTests", true },
    { "surface_action_pose", "SurfaceActionPoseTests", true },
    { "pose_surface_distance", "PoseSurfaceDistanceTests", true },
    { "ground_entry", "GroundEntryTests", true },
    { "random_cadence", "RandomCadenceTests", true },
    { "controller_gravity", "ControllerGravityTests", false },
    { "ground_motion_probe", "GroundMotionProbeTests", false },
    { "support_lifetime", "SupportLifetimeTests", false },
    { "native_walkable_approach", "NativeWalkableApproachTests", false },
    { "wall_run_obstacle", "WallRunObstacleTests", false },
    { "wall_run_obstacle_pose", "WallRunObstaclePoseTests", true },
    { "climb_entry", "ClimbEntryTests", true },
    { "random_actions", "RandomActionsTests", true },
    { "traversal_audio_runtime", "TraversalAudioRuntimeTests", false },
    { "diagonal_speed", "DiagonalSpeedTests", true },
    { "traversal_audio", "TraversalAudioTests", true },
    { "irregular_corner", "IrregularCornerTests", true },
    { "faceted_corner_safety", "FacetedCornerSafetyTests", true },
    { "new_motion_activation", "NewMotionActivationTests", true },
    { "threepeat_motion", "ThreepeatMotionTests", true },
    { "eave_bypass", "EaveBypassTests", true },
    { "adaptive_obstacle", "AdaptiveObstacleTests", false },
    { "obstacle_mantle", "ObstacleMantleTests", true },
    { "direct_mantle_pose", "DirectMantlePoseTests", true },
    { "mantle_selection", "MantleSelectionTests", true },
    { "corner_traversal", "CornerTraversalTests", false },
    { "slope_corner_continuation", "SlopeCornerContinuationTests", false },
    { "corner_integration", "CornerIntegrationTests", true },
    { "edge_acquisition", "EdgeAcquisitionTests", true },
    { "grip_edge", "GripEdgeTests", true },
    { "retired_motions", "RetiredMotionTests", true },
    { "context_actions", "ContextActionTests", true },
    { "roof_crest", "RoofCrestTests", false },
    { "roof_crest_pose", "RoofCrestPoseTests", true },
    { "roof_transfer", "RoofTransferTests", false },
    { "thin_wall_summit", "ThinWallSummitTests", false },
    { "margin_recovery", "MarginRecoveryTests", false },
    { "ray_filter", "RayFilterTests", false },
    { "facet_grip", "FacetGripTests", false },
    { "distance_grab", "DistanceGrabTests", false },
    { "stepped_summit", "SteppedSummitTests", false },
    { "top_candidate_search", "TopCandidateSearchTests", false },
    { "top_candidate_pose", "TopCandidatePoseTests", true },
    { "traversal_capture", "TraversalCaptureTests", false },
    { "attach_surface", "AttachSurfaceTests", false },
    { "grab_input", "GrabInputTests", false },
    { "in_place_detach", "InPlaceDetachTests", true },
    { "traversal_geometry", "TraversalGeometryTests", false },
    { "back_flip_pose", "BackFlipPoseTests", true },
    { "exit_continuity", "ExitContinuityTests", true },
    { "stop_arm", "StopArmTests", true },
    { "view_heading", "ViewHeadingTests", false },
    { "skin_audit_budget", "SkinAuditBudgetTests", false },
    { "late_world_update", "LateWorldUpdateTests", false },
    { "scene_propagation", "ScenePropagationTests", false },
    { "pose_continuation", "ContinuationTests", false },
    { "yaw_frame", "YawFrameTests", false },
    { "side_run_pose", "SideRunPoseTests", true },
    { "parkour", "ParkourTests", true },
    { "stability", "StabilityTests", true },
    { "pose_health", "PoseHealthTests", false },
    { "traversal", "CoreTests", false },
    { "pose", "PoseTests", true },
    { "actions", "ActionTests", true },
    { "scene_binding", "SceneBindingTests", true },
    { "universal_mantle_pose", "UniversalMantlePoseTests", true },
    { "recessed_wall_transfer", "RecessedWallTransferTests", false },
}

--- Tests that also run against the HKX directory when it exists.
local hkx_cases = {
    stop_arm = true,
    surface_action_pose = true,
    side_run_pose = true,
    top_exit_continuity = true,
}

--- Declare one test binary that links both FreeClimb static libraries.
---
--- # Params
--- - `name`   : Test name. The target is `test_<name>`.
--- - `source` : File name under `tests/` without `.cpp`.
--- - `runs`   : `{ { label, args } ... }` passed to `add_tests`.
local function unit(name, source, runs)
    target("test_" .. name, function ()
        set_kind("binary")
        set_default(false) -- Excluded from plain `xmake`.
        set_group("tests")
        set_rundir("$(builddir)") -- Scratch files stay out of the source tree.

        add_deps("FreeClimbAnimationInput", "FreeClimbSettings")
        add_includedirs(headers)
        add_files(path.join(root, "tests", source .. ".cpp"))

        for _, run in ipairs(runs) do
            add_tests(run[1], { runargs = run[2] })
        end
    end)
end

for _, case in ipairs(cases) do
    local name, source, needs = case[1], case[2], case[3]
    if not needs then
        unit(name, source, { { "default", {} } })
    elseif has_motion then
        local runs = { { "default", { motion } } }
        if hkx_cases[name] and has_hkx then
            table.insert(runs, { "hkx", { motion, hkx } })
        end
        if name == "animation_pack" and has_legacy then
            table.insert(runs, { "migration", { legacy, motion } })
        end
        unit(name, source, runs)
    end
end

-- Legacy-binary checks (offline migration only).
if has_legacy and has_hkx then
    unit("hkx_overrides", "AnimationOverrideTests", { { "default", { legacy, hkx } } })
end
if has_legacy then
    unit("legacy_finger_calibration", "FingerCalibrationTests", { { "default", { legacy } } })
end

--- Tests that need spdlog / CommonLibNG. Defined after `includes(CommonLibNG)`.

target("test_runtime_log", function ()
    set_kind("binary")
    set_default(false)
    set_group("tests")
    set_rundir("$(builddir)")

    add_packages("spdlog")
    add_includedirs(headers)
    add_files(path.join(root, "tests/RuntimeLogTests.cpp"))

    add_tests("default", { runargs = { path.join(root, "build", "runtime-log-tests") } })
end)

target("test_filtered_ray_collector", function ()
    set_kind("binary")
    set_default(false)
    set_group("tests")
    set_rundir("$(builddir)")

    add_deps("commonlibsse-ng")
    add_includedirs(headers)
    add_files(path.join(root, "tests/FilteredRayCollectorTests.cpp"))

    add_tests("default")
end)

--- Compiles a few CommonLibNG sources with `ENABLE_COMMONLIBSSE_TESTING`
--- instead of linking the full library.
target("test_runtime_bridge", function ()
    set_kind("binary")
    set_default(false)
    set_group("tests")
    set_rundir("$(builddir)")

    local lib = path.join(root, "deps/CommonLibSSE-NG")

    add_packages("spdlog", "directxtk", "directxmath")
    add_includedirs(headers, path.join(lib, "include"))
    add_files(path.join(root, "tests/RuntimeBridgeTests.cpp"))
    add_files(
        path.join(lib, "src/REL/Module.cpp"),
        path.join(lib, "src/REL/IDDB.cpp"),
        path.join(lib, "src/REL/Version.cpp"),
        path.join(lib, "src/REX/W32.cpp")
    )

    add_defines(
        "ENABLE_COMMONLIBSSE_TESTING=1",
        "ENABLE_SKYRIM_SE=1",
        "ENABLE_SKYRIM_AE=1",
        "HAS_SKYRIM_MULTI_TARGETING=1",
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX"
    )
    add_forceincludes("SKSE/Impl/PCH.h")
    add_cxxflags("cl::/Zc:preprocessor", "cl::/FC") -- /FC: absolute __FILE__ for fixture lookup.
    add_syslinks(
        "advapi32", "bcrypt", "d3d11", "d3dcompiler", "dbghelp",
        "dxgi", "ole32", "version", "shell32", "user32"
    )

    add_tests("default")
end)

--- FreeClimb build script.
---
--- # Usage
--- ```sh
--- git submodule update --init   # fetch deps/CommonLibSSE-NG
--- xmake                         # build FreeClimb.dll (releasedbg) and install to build/install
--- xmake test                    # build and run regression tests
--- xmake f --tools=y             # also build motion authoring tools
--- ```
---
--- `xmake install` copies the plugin to `XSE_TES5_MODS_PATH/FreeClimb` or
--- `XSE_TES5_GAME_PATH/Data` when either env var is set.
---
--- Third-party sources come from xmake packages (pinned below) plus the
--- `deps/CommonLibSSE-NG` submodule. `tools/dependencies.json` records the same pins.

local PLUGIN_NAME <const> = "FreeClimb" -- dll name
local VERSION <const> = "0.2.30"
local LICENSE <const> = "GPL-3.0-or-later"

-- Author, description and compatible runtimes: see `FreeClimb/xmake.lua`.

set_xmakever("3.0.0")
set_project(PLUGIN_NAME)
set_version(VERSION)
set_license(LICENSE)

-- SKSE plugins are MSVC/x64 only. Pin it so MSYS/Git Bash does not pick mingw.
set_allowedplats("windows")
set_defaultplat("windows")
set_arch("x64")
set_languages("c++23")
set_encodings("utf-8")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

-- Build every package from its pinned source archive, so the shipped dll matches
-- the corresponding source. Archives in `deps/packages` are used offline
-- (`xmake f --pkg_searchdirs=deps/packages`).
set_policy("package.precompiled", false)

-- Options --------------------------------------------------------------------------------------------------------------

option("tools", function()
    set_default(false)
    set_showmenu(true)
    set_description("Build motion authoring tools")
end)

option("motion", function()
    set_default("")
    set_showmenu(true)
    set_description("Animation pack (pack.json) for asset tests")
end)

option("hkx", function()
    set_default("")
    set_showmenu(true)
    set_description("HKX directory for equivalence tests")
end)

option("legacy", function()
    set_default("")
    set_showmenu(true)
    set_description("Legacy motion binary for migration tests")
end)

-- Dependencies ---------------------------------------------------------------------------------------------------------

-- FreeClimb supports SE + AE only; VR also needs openvr headers we do not use.
set_config("skyrim_vr", false)

includes("xmake/tasks.lua")
includes("xmake/plugin.lua")

includes("deps/CommonLibSSE-NG") -- need latest. So We use git submodules
-- fetch from xmake indexed repo
add_requires("directxmath latest")
add_requires("directxtk 24.2.0")
add_requires("minhook v1.3.4")
add_requires("nlohmann_json v3.11.3")


-- Projects -----------------------------------------------------------------------------------------------------------

includes("FreeClimbAnimationInput") -- climbing core + HKX / animation pack input
includes("FreeClimbSettings")       -- settings, key bindings, translations
includes("FreeClimb")               -- the SKSE plugin dll

-- Tests / tools --------------------------------------------------------------------------------------------------------

includes("xmake/tests.lua")

if has_config("tools") then
    includes("xmake/tools.lua")
end

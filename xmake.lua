--- FreeClimb build script.
---
--- # Usage
--- ```sh
--- xmake                 # build FreeClimb.dll (releasedbg) and install to build/install
--- xmake test            # build and run regression tests
--- xmake f --tools=y     # also build motion authoring tools
--- ```
---
--- `xmake install` copies the plugin to `XSE_TES5_MODS_PATH/FreeClimb` or
--- `XSE_TES5_GAME_PATH/Data` when either env var is set.

local NAME<const> = "FreeClimb" -- dll name
local AUTHOR<const> = "Epsilona" -- NOTE: Including a space seems to break the rc.
local DESCRIPTION<const> = "Independent wall climbing for Skyrim SE / AE"
local VERSION<const> = "0.2.30"
local LICENSE<const> = "GPL-3.0-or-later"

--- Runtimes the plugin declares as compatible (same list as CMakeLists.txt).
local RUNTIMES<const> = {
    "1.5.97.0",
    "1.6.317.0", "1.6.318.0", "1.6.323.0", "1.6.342.0", "1.6.353.0",
    "1.6.629.0", "1.6.640.0", "1.6.659.1",
    "1.6.1130.0", "1.6.1170.0", "1.6.1179.1",
    "1.7.99.0", "1.7.104.0",
}

set_xmakever("3.0.0")
set_project(NAME)
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

-- Options --------------------------------------------------------------------------------------------------------------

option("tools", function ()
    set_default(false)
    set_showmenu(true)
    set_description("Build motion authoring tools")
end)

option("motion", function ()
    set_default("")
    set_showmenu(true)
    set_description("Animation pack (pack.json) for asset tests")
end)

option("hkx", function ()
    set_default("")
    set_showmenu(true)
    set_description("HKX directory for equivalence tests")
end)

option("legacy", function ()
    set_default("")
    set_showmenu(true)
    set_description("Legacy motion binary for migration tests")
end)

-- Dependencies ---------------------------------------------------------------------------------------------------------

-- FreeClimb supports SE + AE only; VR also needs openvr headers we do not ship.
set_config("skyrim_vr", false)

includes("external/CommonLibNG")
includes("xmake/plugin.lua")

-- Libraries ------------------------------------------------------------------------------------------------------------

--- HKX / animation pack parsing. Shared by the plugin, tests and tools.
target("FreeClimbAnimationInput", function ()
    set_kind("static")
    set_warnings("allextra") -- /W4, same as CMake
    on_install(function () end) -- Linked into the dll; nothing to install.

    add_includedirs("src", "external/nlohmann", { public = true })
    add_files(
        "src/HkxAnimation.cpp",
        "src/HkxSpline.cpp",
        "src/AnimationOverrides.cpp",
        "src/AnimationPack.cpp"
    )
end)

--- User settings (ini) and translation catalog.
target("FreeClimbSettings", function ()
    set_kind("static")
    set_warnings("allextra") -- /W4, same as CMake
    on_install(function () end) -- Linked into the dll; nothing to install.

    add_includedirs("src", { public = true })
    add_files("src/UserSettings.cpp", "src/TranslationCatalog.cpp")
end)


-- Plugin ---------------------------------------------------------------------------------------------------------------

target(NAME, function ()
    add_deps("FreeClimbAnimationInput", "FreeClimbSettings")
    set_warnings("allextra") -- /W4, same as CMake

    add_includedirs("src")
    add_headerfiles("src/**.h")
    set_pcxxheader("src/PCH.h")
    add_files("src/Plugin.cpp", "src/SettingsMenu.cpp")

    add_ldflags("/OPT:REF", "/OPT:ICF", "/PDBALTPATH:%_PDB%", "/MAP", { tools = "link" })

    -- Builds `FreeClimb.dll` and installs it to `SKSE/Plugins` on `xmake install`.
    add_rules("freeclimb.plugin", {
        name = NAME,
        author = AUTHOR,
        description = DESCRIPTION,
        runtimes = RUNTIMES,
    })
end)

-- Tests / tools --------------------------------------------------------------------------------------------------------

includes("xmake/tests.lua")

if has_config("tools") then
    includes("xmake/tools.lua")
end

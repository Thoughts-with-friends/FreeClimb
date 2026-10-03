--- FreeClimb: the SKSE plugin (`FreeClimb.dll`).
---
--- Game integration: pose output, hooks, audio, menu and the per-frame
--- update. Project name and version are set in the root `xmake.lua`.

local AUTHOR_NAME <const> = "Epsilona" -- NOTE: Including a space seems to break the rc.
local DESCRIPTION <const> = "Independent wall climbing for Skyrim SE / AE"

--- Runtimes the plugin declares as compatible.
local RUNTIMES <const> = {
    "1.5.97.0",
    "1.6.317.0", "1.6.318.0", "1.6.323.0", "1.6.342.0", "1.6.353.0",
    "1.6.629.0", "1.6.640.0", "1.6.659.1",
    "1.6.1130.0", "1.6.1170.0", "1.6.1179.1",
    "1.7.99.0", "1.7.104.0",
}

-- Target name must match `PLUGIN_NAME` in the root `xmake.lua`.
target("FreeClimb", function()
    add_deps("FreeClimbAnimationInput", "FreeClimbSettings")
    set_warnings("allextra") -- /W4
    add_deps("commonlibsse-ng")

    add_includedirs("include", { public = true })
    add_headerfiles("include/(**.h)")
    set_pcxxheader("include/PCH.h")
    add_files("src/*.cpp")

    add_ldflags("/OPT:REF", "/OPT:ICF", "/PDBALTPATH:%_PDB%", "/MAP", { tools = "link" })

    -- Builds `FreeClimb.dll` and installs it to `SKSE/Plugins` on `xmake install`.
    add_rules("freeclimb.plugin", {
        name = PLUGIN_NAME,
        author = AUTHOR_NAME,
        description = DESCRIPTION,
        runtimes = RUNTIMES,
    })
end)

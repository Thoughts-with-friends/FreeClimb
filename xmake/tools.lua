--- Motion authoring tools. Enabled with `xmake f --tools=y`.

local root = path.join(os.scriptdir(), "..")

--- Declare one command line tool linked against the animation library.
---
--- # Params
--- - `name`   : Target name.
--- - `source` : File name under `tools/` without `.cpp`.
local function tool(name, source)
    target(name, function ()
        set_kind("binary")
        set_group("tools")

        add_deps("FreeClimbAnimationInput")
        add_includedirs(path.join(root, "src"))
        add_files(path.join(root, "tools", source .. ".cpp"))
    end)
end

tool("FreeClimbAuthoring", "AuthoringCli")
tool("FreeClimbMigrateAnimationPack", "MigrateAnimationPack")
tool("FreeClimbExportMotionHkx", "ExportMotionHkx")

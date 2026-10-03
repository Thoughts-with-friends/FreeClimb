--- FreeClimbAnimationInput: climbing core and animation input.
---
--- Header-only climbing logic (`traversal/`, `pose/`) plus HKX and
--- animation pack parsing. Shared by the plugin, settings, tests and
--- tools; it has no game dependency.

target("FreeClimbAnimationInput", function()
    set_kind("static")
    set_warnings("allextra")   -- /W4
    on_install(function() end) -- Linked into the dll; nothing to install.

    add_packages("nlohmann_json", { public = true })

    add_includedirs("include", { public = true })
    add_headerfiles("include/(**.h)")
    add_files("src/*.cpp")
end)

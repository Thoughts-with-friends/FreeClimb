--- FreeClimbSettings: user settings, key bindings and translations.
---
--- Depends on FreeClimbAnimationInput for the climbing core types
--- (`traversal/Core.h`) that key bindings map onto.

target("FreeClimbSettings", function()
    set_kind("static")
    set_warnings("allextra")   -- /W4
    on_install(function() end) -- Linked into the dll; nothing to install.

    add_deps("FreeClimbAnimationInput")

    add_includedirs("include", { public = true })
    add_headerfiles("include/(**.h)")
    add_files("src/*.cpp")
end)

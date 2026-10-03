task("update-submodules", function()
    on_run(function()
        os.exec("git submodule update --init --remote")
    end)

    set_menu({
        usage = "xmake update-submodules",
        description = "Update git submodules to their remote revisions",
    })
end)

task("lsp", function()
    on_run(function()
        os.exec("xmake project -k compile_commands --lsp=clangd --outputdir=.vscode -y")
    end)

    set_menu({
        usage = "xmake lsp",
        description = "generate compile_commands.json",
    })
end)

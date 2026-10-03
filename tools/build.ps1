[CmdletBinding()]
param(
    [switch] $Tests,
    [switch] $AuthoringTools,
    [Alias('MotionLibrary')][string] $AnimationPack = '',
    [string] $HkxDirectory = '',
    [string] $BuildDirectory = 'build',
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')] [string] $Configuration = 'Release',
    [ValidateRange(1, 128)] [int] $Parallel = 8
)

# Builds FreeClimb.dll with xmake after verifying every pinned dependency.
#
# - Git checkout: `deps/CommonLibSSE-NG` must be the pinned, clean commit.
# - Source archive (`DEPENDENCY-SOURCES.json` present): bundled files and package
#   archives are hash-checked and xmake builds offline from `deps/packages`.

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'dependencies.json') -Raw | ConvertFrom-Json
if ($lock.schema -ne 1) { throw 'Unsupported dependency lock schema' }
$sourceManifest = $null
$sourceManifestPath = Join-Path $projectRoot 'DEPENDENCY-SOURCES.json'
if (Test-Path -LiteralPath $sourceManifestPath) {
    $sourceManifest = Get-Content -LiteralPath $sourceManifestPath -Raw | ConvertFrom-Json
    if ($sourceManifest.schema -ne 2 -or $sourceManifest.dependencies.Count -ne $lock.dependencies.Count) { throw 'Invalid corresponding-source manifest' }
}
foreach ($program in @('git', 'xmake')) {
    if (!(Get-Command $program -ErrorAction SilentlyContinue)) { throw "Required program not found: $program" }
}

function Run-Checked([string] $Program, [string[]] $Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

function File-Hash([string] $Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-LocalPath([string] $Path, [string] $Parent) {
    $absolute = [IO.Path]::GetFullPath($Path)
    if (!$absolute.StartsWith($Parent.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escaped intended directory: $absolute"
    }
    $cursor = $absolute
    while ($cursor -and $cursor.Length -ge $Parent.Length) {
        if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Linked dependency/build path is unsupported: $cursor"
        }
        $cursor = Split-Path -Parent $cursor
    }
    return $absolute
}

# Vendored headers (SKSEMenuFramework API) must match the lock.
function Assert-Headers {
    foreach ($header in $lock.header_dependencies) {
        foreach ($entry in $header.files.PSObject.Properties) {
            $path = Assert-LocalPath (Join-Path $projectRoot $entry.Name) $projectRoot
            if (!(Test-Path -LiteralPath $path -PathType Leaf) -or (File-Hash $path) -ne $entry.Value) {
                throw "Vendored header dependency hash mismatch: $path"
            }
        }
    }
}

# Source archive: every bundled dependency file must match DEPENDENCY-SOURCES.json.
function Assert-Bundled($dependency, [string] $target) {
    $bundled = @($sourceManifest.dependencies | Where-Object { $_.name -eq $dependency.name })
    if ($bundled.Count -ne 1 -or $bundled[0].commit -ne $dependency.commit -or $bundled[0].directory -ne $dependency.directory -or $bundled[0].repository -ne $dependency.repository) { throw "Corresponding-source pin mismatch: $($dependency.name)" }
    if (!(Test-Path -LiteralPath $target)) { throw "Missing bundled dependency: $target" }
    $expected = @($bundled[0].files.PSObject.Properties)
    $actual = @(Get-ChildItem -LiteralPath $target -Force -Recurse -File)
    if ($actual.Count -ne $expected.Count) { throw "Bundled dependency file count changed: $target" }
    foreach ($entry in $expected) {
        $path = Assert-LocalPath (Join-Path $target $entry.Name) $target
        if (!(Test-Path -LiteralPath $path -PathType Leaf) -or (File-Hash $path) -ne $entry.Value) { throw "Bundled dependency hash mismatch: $path" }
    }
}

# Git checkout: the submodule must sit at the pinned commit with no local changes.
function Assert-Submodule($dependency, [string] $target) {
    if (!(Test-Path -LiteralPath (Join-Path $target '.git'))) {
        Run-Checked 'git' @('-C', $projectRoot, 'submodule', 'update', '--init', '--', $dependency.directory)
    }
    $actual = & git -C $target rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $dependency.commit) { throw "Unexpected dependency revision: $target" }
    Run-Checked 'git' @('-C', $target, 'diff', '--quiet', 'HEAD', '--')
    $untracked = & git -C $target ls-files --others --exclude-standard
    if ($LASTEXITCODE -ne 0 -or $untracked) { throw "Untracked files in dependency: $target" }
}

# Package archives are optional for a git checkout (xmake downloads and checks them),
# but required offline in a source archive.
function Assert-Packages([bool] $required) {
    foreach ($package in $lock.packages) {
        $path = Assert-LocalPath (Join-Path $projectRoot ('deps/packages/' + $package.file)) $projectRoot
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
            if ($required) { throw "Missing bundled package archive: $path" }
            continue
        }
        if ((File-Hash $path) -ne $package.sha256) { throw "Package archive hash mismatch: $path" }
    }
}

Push-Location $projectRoot
try {
    Assert-Headers
    $depsRoot = Join-Path $projectRoot 'deps'
    foreach ($dependency in $lock.dependencies) {
        if ($dependency.commit -notmatch '^[0-9a-f]{40}$') { throw "Invalid pin: $($dependency.name)" }
        $target = Assert-LocalPath (Join-Path $projectRoot $dependency.directory) $depsRoot
        if ($sourceManifest) { Assert-Bundled $dependency $target } else { Assert-Submodule $dependency $target }
    }
    Assert-Packages ([bool] $sourceManifest)

    $output = if ([IO.Path]::IsPathRooted($BuildDirectory)) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $projectRoot $BuildDirectory }
    $output = Assert-LocalPath $output $projectRoot
    $mode = if ($Configuration -eq 'Debug') { 'debug' } else { 'releasedbg' }
    $configure = @('f', '-y', '-m', $mode, '-o', $output, "--tools=$(if ($AuthoringTools) { 'y' } else { 'n' })")
    $configure += '--pkg_searchdirs=' + (Join-Path $projectRoot 'deps/packages')
    $configure += '--motion=' + $(if ($AnimationPack) { (Resolve-Path -LiteralPath $AnimationPack).Path } else { '' })
    $configure += '--hkx=' + $(if ($HkxDirectory) { (Resolve-Path -LiteralPath $HkxDirectory).Path } else { '' })
    Run-Checked 'xmake' $configure
    Run-Checked 'xmake' @('build', '-y', '-j', "$Parallel")
    if ($Tests) { Run-Checked 'xmake' @('test', '-y') }
    Write-Output "Built $output\windows\x64\$mode\FreeClimb.dll"
} finally {
    Pop-Location
}

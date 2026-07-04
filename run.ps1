param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$BuildType = "Release"
)

$buildDir = switch ($BuildType) {
    "Debug"          { "build-dbg" }
    "RelWithDebInfo" { "build-dbginfo" }
    default          { "build" }
}

$exe = Join-Path $PSScriptRoot "$buildDir\OrcaSlicer\OrcaSlicer.exe"

if (-not (Test-Path $exe)) {
    Write-Error "OrcaSlicer binary not found at: $exe`nBuild the project first with: .\build_release_vs2022.bat"
    exit 1
}

Write-Host "Launching $exe"
& $exe

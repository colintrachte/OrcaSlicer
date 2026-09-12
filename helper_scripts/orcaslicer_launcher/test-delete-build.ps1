$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'launcher.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
foreach ($name in @('Get-LocalBuildDirectory', 'Remove-LocalBuild', 'Get-BuildFileSizes', 'Get-OrcaBuildGroups', 'Get-OrcaLaunchTargets')) {
    $node = $ast.Find({ param($n)
        $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name
    }.GetNewClosure(), $true)
    Invoke-Expression $node.Extent.Text
}

# Exercise real deletion only within a unique disposable fixture.
$script:RepoRoot = Join-Path ([IO.Path]::GetTempPath()) ('orca-delete-' + [guid]::NewGuid().ToString('N'))
$build = Join-Path $script:RepoRoot 'build-test'
$source = Join-Path $script:RepoRoot 'resources'
$outputs = @('src\Debug', 'src\Release', 'src\RelWithDebInfo', 'OrcaSlicer')
New-Item -ItemType Directory -Path $source -Force | Out-Null
Set-Content -LiteralPath (Join-Path $source 'keep.txt') -Value 'source'
foreach ($output in $outputs) {
    $folder = Join-Path $build $output
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $folder 'orca-slicer.exe') -Value $output
    New-Item -ItemType Junction -Path (Join-Path $folder 'resources') -Target $source | Out-Null
}
Set-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Value 'shared'
function Get-Process {}
function Save-LauncherSettings {}
function Get-OrcaVersion { 'test' }
function Get-ExecutableFromDisplayIcon { $null }
$targets = @(Get-OrcaLaunchTargets | Where-Object { $_.Source -like 'build-test*' })
if ($targets.Count -ne 4) { throw 'Not all configurations were discovered' }
if (@($targets | Where-Object { $_.Kind -eq 'Development' }).Count -ne 2) { throw 'Development discovery regressed' }
$groups = @(Get-OrcaBuildGroups $targets)
if ($groups.Count -ne 1 -or $groups[0].Targets.Count -ne 4) { throw 'Outputs were not grouped into one build' }
$expectedBytes = (Get-Item -LiteralPath (Join-Path $build 'CMakeCache.txt')).Length
foreach ($output in $outputs) { $expectedBytes += (Get-Item -LiteralPath (Join-Path $build "$output\orca-slicer.exe")).Length }
if ($groups[0].Bytes -ne $expectedBytes) { throw 'Size included linked resources or omitted build files' }
if ($groups[0].SharedBytes -ne (Get-Item -LiteralPath (Join-Path $build 'CMakeCache.txt')).Length) { throw 'Shared file size is incorrect' }
$release = Join-Path $build 'src\Release\orca-slicer.exe'
for ($index = 0; $index -lt $outputs.Count; $index++) {
    $outputPath = Join-Path $build $outputs[$index]
    $target = [pscustomobject]@{ Path = Join-Path $outputPath 'orca-slicer.exe'; Configuration = 'Release' }
    $settings = [pscustomobject]@{ PinnedTargetPath = $target.Path; LastTargetPath = $release }
    if ((Get-LocalBuildDirectory $target) -ne $outputPath) { throw 'Wrong deletion scope' }
    Remove-LocalBuild $target
    if (Test-Path -LiteralPath $outputPath) { throw 'Selected output survived' }
    foreach ($sibling in $outputs | Select-Object -Skip ($index + 1)) {
        if ((Get-Content -LiteralPath (Join-Path $build "$sibling\orca-slicer.exe")) -ne $sibling) {
            throw "Sibling output changed: $sibling"
        }
    }
    if ((Get-Content -LiteralPath (Join-Path $source 'keep.txt')) -ne 'source') { throw 'Linked source changed' }
    if ((Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt')) -ne 'shared') { throw 'Shared cache changed' }
    if ($settings.PinnedTargetPath) { throw 'Deleted pin retained' }
    if ($index -eq 0 -and $settings.LastTargetPath -ne $release) { throw 'Sibling setting cleared' }
}
# Empty builds remain available for cleanup, and deleting a parent is explicit.
$emptyGroups = @(Get-OrcaBuildGroups @())
if ($emptyGroups.Count -ne 1 -or $emptyGroups[0].Targets.Count) { throw 'Empty build not discoverable' }
New-Item -ItemType Junction -Path (Join-Path $build 'resources') -Target $source | Out-Null
Remove-LocalBuild $emptyGroups[0]
if (Test-Path -LiteralPath $build) { throw 'Whole-build deletion failed' }
if ((Get-Content -LiteralPath (Join-Path $source 'keep.txt')) -ne 'source') { throw 'Whole-build deletion touched source' }
Remove-Item -LiteralPath (Join-Path $source 'keep.txt')
[IO.Directory]::Delete($source, $false)
[IO.Directory]::Delete($script:RepoRoot, $false)
'PASS: development discovery, grouping, sizes, individual and whole-build deletion; linked resources preserved.'

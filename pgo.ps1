# PGO (Profile-Guided Optimization) training script for Node.js (Clang / LLVM)
#
# Runs PGO training workloads against an instrumented Node.js binary
# (Release\node.exe) and merges the resulting .profraw files into
# node.profdata for use with -fprofile-use.
#
# Usage (from a VS Developer Command Prompt):
#   .\pgo.ps1                     # Run workloads (15s each) and merge
#   .\pgo.ps1 -Duration 30        # Run workloads (30s each) and merge
#
# Prerequisites:
#   - Release\node.exe must be an instrumented build (built with pgo-generate)
#   - llvm-profdata must be available (shipped with VS LLVM toolset)
#
# Output:
#   - node.profdata in the repo root (ready for vcbuild.bat pgo-use)

param(
    [int]$Duration = 15
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Locate llvm-profdata shipped with Visual Studio's LLVM toolset
# ---------------------------------------------------------------------------

function Find-LlvmProfdata {
    # vcbuild.bat uses %VCINSTALLDIR%\Tools\Llvm\x64\bin for clang.exe - same spot for profdata
    $vcInstallDir = $env:VCINSTALLDIR

    if ($vcInstallDir) {
        $candidate = Join-Path $vcInstallDir "Tools\Llvm\x64\bin\llvm-profdata.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    # Fallback: try VS 2022 / 2026 default install locations
    $vsPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Enterprise\VC\Tools\Llvm\x64\bin",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Community\VC\Tools\Llvm\x64\bin",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin"
    )
    foreach ($dir in $vsPaths) {
        $candidate = Join-Path $dir "llvm-profdata.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    # Last resort: PATH
    $fromPath = Get-Command llvm-profdata -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    return $null
}

# ---------------------------------------------------------------------------
# Validate prerequisites
# ---------------------------------------------------------------------------

$instrumentedNode = Join-Path $PSScriptRoot "Release\node.exe"
if (-not (Test-Path $instrumentedNode)) {
    Write-Error "Instrumented binary not found: $instrumentedNode`nBuild with: vcbuild.bat pgo-generate"
    exit 1
}

$pgoRunAll = Join-Path $PSScriptRoot "tools\pgo\pgo-run-all.js"
if (-not (Test-Path $pgoRunAll)) {
    Write-Error "PGO training script not found: $pgoRunAll"
    exit 1
}

$llvmProfdata = Find-LlvmProfdata
if (-not $llvmProfdata) {
    Write-Error "llvm-profdata not found. Install the LLVM toolset via Visual Studio Installer."
    exit 1
}

# ---------------------------------------------------------------------------
# STEP 1 – Run workloads with the instrumented binary to collect profiles
# ---------------------------------------------------------------------------

Write-Host "`n=== STEP 1: Collect PGO profiles ===" -ForegroundColor Cyan

# Directory that will receive .profraw files from the instrumented binary.
# %p (PID) and %m (module hash) keep concurrent/fork'd processes from colliding.
$profileDir = Join-Path $PSScriptRoot "pgo-profiles"

if (Test-Path $profileDir) {
    Remove-Item -Recurse -Force $profileDir
}
New-Item -ItemType Directory -Path $profileDir | Out-Null

$env:LLVM_PROFILE_FILE = Join-Path $profileDir "node-%p-%m.profraw"

Write-Host "Instrumented node : $instrumentedNode"
Write-Host "Profile output    : $($env:LLVM_PROFILE_FILE)"
Write-Host "Duration per script: ${Duration}s"
Write-Host ""

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process `
    -FilePath $instrumentedNode `
    -ArgumentList "`"$pgoRunAll`" --verbose --duration=$Duration" `
    -Wait -PassThru -NoNewWindow
$sw.Stop()
Write-Host ("PGO training completed in {0}m {1}s (exit code: {2})" -f `
    $sw.Elapsed.Minutes, $sw.Elapsed.Seconds, $proc.ExitCode)
if ($proc.ExitCode -ne 0) {
    Write-Warning "PGO training exited with code $($proc.ExitCode) - continuing with merge"
}

# Remove the env var so subsequent builds are not affected
Remove-Item Env:\LLVM_PROFILE_FILE -ErrorAction SilentlyContinue

# ---------------------------------------------------------------------------
# STEP 2 – Merge .profraw files -> node.profdata
# ---------------------------------------------------------------------------

Write-Host "`n=== STEP 2: Merge profile data ===" -ForegroundColor Cyan

Write-Host "Using llvm-profdata: $llvmProfdata"

$profrawFiles = Get-ChildItem -Path $profileDir -Filter "*.profraw" -ErrorAction SilentlyContinue
if ($profrawFiles.Count -eq 0) {
    Write-Error "No .profraw files found in '$profileDir'. The instrumented binary may not have generated profile data."
    exit 1
}

$totalSize = ($profrawFiles | Measure-Object -Property Length -Sum).Sum
$totalSizeMB = [math]::Round($totalSize / 1MB, 1)
Write-Host "Found $($profrawFiles.Count) .profraw file(s), ${totalSizeMB} MB total"

$profdata = Join-Path $PSScriptRoot "node.profdata"
$mergeArgs = @("merge", "--output=$profdata") + ($profrawFiles | Select-Object -ExpandProperty FullName)

$mergeStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $llvmProfdata @mergeArgs
$mergeExitCode = $LASTEXITCODE
$mergeStopwatch.Stop()

if ($mergeExitCode -ne 0) {
    Write-Error "llvm-profdata merge failed (exit code $mergeExitCode)"
    exit $mergeExitCode
}

$profdataSize = [math]::Round((Get-Item $profdata).Length / 1MB, 1)
Write-Host "Merge completed in $([math]::Round($mergeStopwatch.Elapsed.TotalSeconds, 1))s"

# Clean up .profraw files now that they've been merged
Remove-Item -Recurse -Force $profileDir
Write-Host "Removed $($profrawFiles.Count) .profraw file(s) (${totalSizeMB} MB reclaimed)"

Write-Host "`n=== PGO training complete ===" -ForegroundColor Green
Write-Host "  Profile data: $profdata (${profdataSize} MB)"
Write-Host "  Next step:    vcbuild.bat pgo-use"

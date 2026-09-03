<#
.SYNOPSIS
    Regenerates compile_commands.json via a Ninja-only CMake configure pass,
    then syncs it to the parent (repo root) folder for clangd to pick up.

.DESCRIPTION
    Meant to live in <repo>/build-ninja/ and be run FROM THAT FOLDER,
    inside a "Developer PowerShell for VS" (needed so cl.exe is on PATH).

    This does NOT build the project — it only configures CMake with the
    Ninja generator (the only one that supports CMAKE_EXPORT_COMPILE_COMMANDS
    on Windows) to produce a fresh compile_commands.json.

.NOTES
    Adjust the $CMakeArgs block below to match whatever -D flags your
    normal build uses (Geant4_DIR, G4Vox_DIR, CMAKE_PREFIX_PATH, etc.).
    Check your working build/CMakeCache.txt if you add new dependencies
    and this script starts failing to find them.
#>

# ----------------------------------------------------------------------
# 0. Sanity check: are we in a Developer shell with cl.exe available?
# ----------------------------------------------------------------------
$clPath = (Get-Command cl -ErrorAction SilentlyContinue)
if (-not $clPath) {
    Write-Host "ERROR: 'cl' not found on PATH." -ForegroundColor Red
    Write-Host "Run this script from a 'Developer PowerShell for VS' window," -ForegroundColor Yellow
    Write-Host "not a plain PowerShell / conda prompt." -ForegroundColor Yellow
    exit 1
}

# ----------------------------------------------------------------------
# 1. Configuration — edit these paths to match your project
# ----------------------------------------------------------------------
$SourceDir = ".."   # repo root, relative to build-ninja/

$CMakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DGeant4_DIR=C:/DEV/GEANT4/geant4-v11.4.1-install/lib/cmake/Geant4",
    "-DG4Vox_DIR=C:/DEV/GEANT4/LIB/G4Vox-install/lib/cmake/G4Vox",
    "-DCMAKE_PREFIX_PATH=C:/DEV/GEANT4/geant4-v11.4.1-install;C:/DEV/GEANT4/LIB/G4Vox-install"
)

# ----------------------------------------------------------------------
# 2. Run CMake configure
# ----------------------------------------------------------------------
Write-Host "Configuring with CMake (Ninja)..." -ForegroundColor Cyan
& cmake @CMakeArgs $SourceDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configure failed (exit code $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------------
# 3. Sync compile_commands.json to the repo root
# ----------------------------------------------------------------------
$SourceFile = Join-Path $PSScriptRoot "compile_commands.json"
$DestFile = Join-Path $PSScriptRoot "..\compile_commands.json"

if (-not (Test-Path $SourceFile)) {
    Write-Host "ERROR: compile_commands.json was not generated." -ForegroundColor Red
    exit 1
}

# Remove any stale copy/link/shortcut first
if (Test-Path $DestFile) {
    Remove-Item $DestFile -Force
}

# Try a real symlink first (needs admin rights or Developer Mode enabled).
# Falls back to a plain copy if that's not available.
try {
    New-Item -ItemType SymbolicLink -Path $DestFile -Target $SourceFile -ErrorAction Stop | Out-Null
    Write-Host "Symlinked compile_commands.json -> repo root." -ForegroundColor Green
}
catch {
    Copy-Item $SourceFile $DestFile -Force
    Write-Host "Copied compile_commands.json -> repo root (symlink not permitted; enable Developer Mode to switch to a symlink)." -ForegroundColor Yellow
}

Write-Host "Done." -ForegroundColor Green
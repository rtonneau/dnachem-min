param()

$ErrorActionPreference = "Stop"

$hookInput = [Console]::In.ReadToEnd() | ConvertFrom-Json
$filePath = $hookInput.tool_input.file_path
if (-not $filePath -or [IO.Path]::GetExtension($filePath) -notin ".cc", ".cpp", ".cxx") {
    exit 0
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = Join-Path $RepoRoot "build-ninja"
$SourceDir = $RepoRoot

if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) {
        throw "cl.exe is not available and Visual Studio's vswhere.exe was not found. Run from a Developer PowerShell for Visual Studio."
    }

    $vsInstallPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcVars = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not $vsInstallPath -or -not (Test-Path $vcVars)) {
        throw "A Visual Studio C++ developer environment could not be located. Install the Desktop development with C++ workload."
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $devEnvironment = & cmd.exe /d /s /c "call `"$vcVars`" >nul && set" 2>&1
    $devEnvironmentExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    if ($devEnvironmentExitCode -ne 0) {
        throw "Visual Studio's C++ developer environment failed to initialize (exit code $devEnvironmentExitCode)."
    }

    $devEnvironment |
        Where-Object { $_ -is [string] -and $_ -match "^[^=]+=.*$" } |
        ForEach-Object {
            $name, $value = $_ -split "=", 2
            Set-Item -Path "Env:$name" -Value $value
        }

    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        throw "Visual Studio's developer environment was initialized, but cl.exe is still unavailable."
    }
}

$CMakeArgs = @(
    "-S", $SourceDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    "-DGeant4_DIR=C:/DEV/GEANT4/geant4-v11.4.1-install/lib/cmake/Geant4",
    "-DG4Vox_DIR=C:/DEV/GEANT4/LIB/G4Vox-install/lib/cmake/G4Vox",
    "-DCMAKE_PREFIX_PATH=C:/DEV/GEANT4/geant4-v11.4.1-install;C:/DEV/GEANT4/LIB/G4Vox-install"
)

Write-Host "Configuring with CMake (Ninja)..." -ForegroundColor Cyan
& cmake @CMakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed (exit code $LASTEXITCODE)."
}

$SourceFile = Join-Path $BuildDir "compile_commands.json"
$DestFile = Join-Path $SourceDir "compile_commands.json"
if (-not (Test-Path $SourceFile)) {
    throw "compile_commands.json was not generated."
}

Copy-Item $SourceFile $DestFile -Force
Write-Host "Copied compile_commands.json to the repository root." -ForegroundColor Green

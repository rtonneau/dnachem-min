param()

$ErrorActionPreference = "Stop"

$hookInput = [Console]::In.ReadToEnd() | ConvertFrom-Json
$filePath = $hookInput.tool_input.file_path
if (-not $filePath -or [IO.Path]::GetExtension($filePath) -notin ".cc", ".cpp", ".cxx") {
    exit 0
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
& (Join-Path $repoRoot "build-ninja\update-compile-cmd.ps1")
exit $LASTEXITCODE

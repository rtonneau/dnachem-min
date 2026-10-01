<#
.SYNOPSIS
  Runs sim.exe once per dissolved-O2 level, one after another.
.EXAMPLE
  ./Scan-O2.ps1                          # 0 % and 21 %
  ./Scan-O2.ps1 -Levels 0,2.5,5,21 -Events 10
  ./Scan-O2.ps1 -RunDir C:\runs\boscolo -Force
#>
param(
  [double[]] $Levels    = @(0, 21),          # O2 levels, in %
  [string]   $Chemistry = 'BoscoloChem',
  [string]   $Energy    = '10 keV',
  [int]      $Events    = 2,
  [string]   $RunDir    = (Get-Location).Path,  # holds sim.exe and macro/
  [string]   $Results   = 'results',            # created inside RunDir
  [switch]   $Force,                            # overwrite existing level folders
  [switch]   $RateAware,                        # /chem/sbs/rateAwareReactions true
  [string]   $MaxTimeStep  = '',                # e.g. '1 ns' -> /chem/sbs/maxTimeStep
  [string]   $ReactionBins = ''                 # e.g. '1 3 10 ps' -> /chem/reaction/timeBinsList
)

$ErrorActionPreference = 'Stop'

$sim = Join-Path $RunDir 'sim.exe'
if (-not (Test-Path $sim))                     { throw "sim.exe not found in $RunDir" }
if (-not (Test-Path (Join-Path $RunDir 'macro'))) { throw "macro/ not found in $RunDir" }

# sim.exe prepends macro/ to its argument, relative to the working directory.
Push-Location $RunDir
try {
  New-Item -ItemType Directory -Force $Results | Out-Null   # --dir needs the parent

  foreach ($o2 in $Levels) {
    $tag   = 'o2_{0}pct' -f ($o2.ToString([cultureinfo]::InvariantCulture) -replace '\.', 'p')
    $out   = Join-Path $Results $tag
    $log   = Join-Path $Results "$tag.log"
    $macro = "scan_$tag.in"

    if (Test-Path $out) {
      if ($Force) { Remove-Item -Recurse -Force $out }
      else { Write-Warning "$out exists, skipping (use -Force to overwrite)"; continue }
    }

    $o2Text = $o2.ToString([cultureinfo]::InvariantCulture)
    $extra = @()
    if ($RateAware)    { $extra += '/chem/sbs/rateAwareReactions true' }
    if ($MaxTimeStep)  { $extra += "/chem/sbs/maxTimeStep $MaxTimeStep" }
    if ($ReactionBins) { $extra += "/chem/reaction/timeBinsList $ReactionBins" }
    $extraText = if ($extra) { ($extra -join "`n") + "`n" } else { '' }
    @"
/run/verbose 0
/tracking/verbose 0
/dnaLogger/verbose Info
/process/dna/e-SolvationSubType Ritchie1994
/process/chem/TimeStepModel SBS
/chem/select $Chemistry
/chem/env/scavenger O2 $o2Text %
${extraText}/run/initialize
/gun/particle e-
/gun/energy $Energy
/run/beamOn $Events
/run/dumpDataAndReset
"@ | Set-Content -Encoding ascii (Join-Path 'macro' $macro)

    Write-Host "=== O2 = $o2Text % -> $out"
    $t = Measure-Command { & $sim $macro --dir $out *> $log }
    if ($LASTEXITCODE -eq 0 -and (Select-String -Quiet -Path $log -Pattern 'dumped and reset')) {
      Write-Host ("    done in {0:n0} s" -f $t.TotalSeconds)
    } else {
      Write-Warning "    FAILED (exit $LASTEXITCODE), see $log"
    }

    Remove-Item (Join-Path 'macro' $macro)   # generated macro no longer needed
  }
}
finally {
  Pop-Location
}
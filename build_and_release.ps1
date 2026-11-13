# Parameterized PowerShell build and release script for MeshCore NM-EPD-420-BW
param(
    [string]$Version = "v1.18.0-mod-rev0.3",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot
Set-Location $workspaceRoot

function Resolve-ToolPath {
    param([string[]]$Candidates, [string]$CommandName)
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }
    if ($CommandName) {
        $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

$pio = Resolve-ToolPath -Candidates @(
    "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
) -CommandName "pio"
if (-not $pio) { throw "PlatformIO CLI ('pio') not found in `$env:USERPROFILE\.platformio or PATH." }

$python = Resolve-ToolPath -Candidates @(
    "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
) -CommandName "python"
if (-not $python) { throw "Python interpreter not found in `$env:USERPROFILE\.platformio or PATH." }

$esptool = Resolve-ToolPath -Candidates @(
    "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
)

$pioScripts = Split-Path -Parent $pio
if ($env:PATH -notlike "*$pioScripts*") {
    $env:PATH = "$pioScripts;" + $env:PATH
}
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"

Write-Host "=== Building MeshCore NM-EPD-420-BW $Version ===" -ForegroundColor Cyan
Write-Host "Workspace: $workspaceRoot"
Write-Host "Pio:       $pio"
Write-Host "Python:    $python"

$roles = @(
    @{ Env = "NM_EPD_420_BW_companion_radio_ble"; OutName = "MeshCore-NM-EPD-420-BW-companion_radio_ble-$Version.bin"; IsDefault = $true },
    @{ Env = "NM_EPD_420_BW_companion_radio_usb"; OutName = "MeshCore-NM-EPD-420-BW-companion_radio_usb-$Version.bin"; IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_repeater";            OutName = "MeshCore-NM-EPD-420-BW-repeater-$Version.bin";            IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_room_server";         OutName = "MeshCore-NM-EPD-420-BW-room_server-$Version.bin";         IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_kiss_modem";          OutName = "MeshCore-NM-EPD-420-BW-kiss_modem-$Version.bin";          IsDefault = $false }
)

$releaseDir = Join-Path $workspaceRoot "release"
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir | Out-Null
}
$bootApp0 = Join-Path $releaseDir "boot_app0.bin"

$generatedFiles = @()

foreach ($role in $roles) {
    $roleEnv = $role.Env
    $outBin = Join-Path $releaseDir $role.OutName

    if ($Clean) {
        Write-Host "`n>>> [CLEAN] $roleEnv" -ForegroundColor DarkCyan
        & $pio run -d $workspaceRoot -e $roleEnv -t clean
        if ($LASTEXITCODE -ne 0) { throw "Clean failed for $roleEnv" }
    }

    Write-Host "`n>>> [BUILDING] Role: $roleEnv -> $($role.OutName)" -ForegroundColor Yellow
    & $pio run -d $workspaceRoot -e $roleEnv -t mergebin
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $roleEnv" }

    $buildDir = Join-Path $workspaceRoot ".pio\build\$roleEnv"
    $mergedBin = Join-Path $buildDir "firmware-merged.bin"

    if (-not (Test-Path $mergedBin)) {
        Write-Host "[*] Merging manually via esptool..." -ForegroundColor DarkYellow
        $bootloader = Join-Path $buildDir "bootloader.bin"
        $partitions = Join-Path $buildDir "partitions.bin"
        $firmware = Join-Path $buildDir "firmware.bin"
        if ($esptool) {
            & $python -X utf8 $esptool --chip esp32s3 merge_bin -o $mergedBin --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 $bootloader 0x8000 $partitions 0xe000 $bootApp0 0x10000 $firmware
        } else {
            & $python -X utf8 -m esptool --chip esp32s3 merge_bin -o $mergedBin --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 $bootloader 0x8000 $partitions 0xe000 $bootApp0 0x10000 $firmware
        }
        if ($LASTEXITCODE -ne 0) { throw "Manual merge_bin failed for $roleEnv" }
    }

    Copy-Item -Path $mergedBin -Destination $outBin -Force
    $generatedFiles += $outBin
    $size = (Get-Item $outBin).Length
    Write-Host "[OK] Created $outBin ($size bytes)" -ForegroundColor Green

    if ($role.IsDefault) {
        $defaultAlias = Join-Path $releaseDir "MeshCore-NM-EPD-420-BW-$Version.bin"
        Copy-Item -Path $mergedBin -Destination $defaultAlias -Force
        $generatedFiles += $defaultAlias
        Write-Host "[OK] Created default alias $defaultAlias ($size bytes)" -ForegroundColor Green

        $firmwareBin = Join-Path $releaseDir "firmware.bin"
        Copy-Item -Path $mergedBin -Destination $firmwareBin -Force
        $generatedFiles += $firmwareBin
        Write-Host "[OK] Updated default $firmwareBin ($size bytes)" -ForegroundColor Green

        $bootloaderSrc = Join-Path $buildDir "bootloader.bin"
        $partitionsSrc = Join-Path $buildDir "partitions.bin"
        if (Test-Path $bootloaderSrc) { Copy-Item -Path $bootloaderSrc -Destination (Join-Path $releaseDir "bootloader.bin") -Force }
        if (Test-Path $partitionsSrc) { Copy-Item -Path $partitionsSrc -Destination (Join-Path $releaseDir "partitions.bin") -Force }
    }
}

Write-Host "`n=== Verifying generated binaries in release/ (size & path-leak scan) ===" -ForegroundColor Cyan
$leakPattern = '(?i)([A-Z]:[/\\]+Users[/\\]|[/\\]Users[/\\][^/\\\x00]{1,40}[/\\])'
$hasLeaks = $false
foreach ($binPath in $generatedFiles) {
    $bytes = [System.IO.File]::ReadAllBytes($binPath)
    $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    $matches = [regex]::Matches($text, $leakPattern)
    $name = Split-Path -Leaf $binPath
    Write-Host ("  {0,-68} {1,9} B | path leaks: {2}" -f $name, $bytes.Length, $matches.Count)
    if ($matches.Count -gt 0) {
        $hasLeaks = $true
        $matches | Select-Object -First 3 -ExpandProperty Value | ForEach-Object { Write-Host "    LEAK: $_" -ForegroundColor Red }
    }
}

if ($hasLeaks) {
    throw "Host path leak detected in generated release binaries! Check -fmacro-prefix-map / -ffile-prefix-map flags and perform a -Clean build."
}

Write-Host "`n[OK] All 5 roles built, merged, and verified clean for $Version." -ForegroundColor Green

# PowerShell build and release script for MeshCore NM-EPD-420-BW rev0.2
$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot
Set-Location $workspaceRoot

$pio = "C:\Users\KB\.platformio\penv\Scripts\pio.exe"
$python = "C:\Users\KB\.platformio\penv\Scripts\python.exe"
$esptool = "C:\Users\KB\.platformio\packages\tool-esptoolpy\esptool.py"
$git = "C:\cygwin64\bin\git.exe"

$env:PATH = "C:\Users\KB\.platformio\penv\Scripts;C:\cygwin64\bin;" + $env:PATH
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"

Write-Host "=== Building MeshCore NM-EPD-420-BW v1.18.0-mod-rev0.2 ===" -ForegroundColor Cyan
Write-Host "Pio: $pio"
Write-Host "Python: $python"
Write-Host "Git: $git"

$roles = @(
    @{ Env = "NM_EPD_420_BW_companion_radio_ble"; OutName = "MeshCore-NM-EPD-420-BW-companion_radio_ble-v1.18.0-mod-rev0.2.bin"; IsDefault = $true },
    @{ Env = "NM_EPD_420_BW_companion_radio_usb"; OutName = "MeshCore-NM-EPD-420-BW-companion_radio_usb-v1.18.0-mod-rev0.2.bin"; IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_repeater"; OutName = "MeshCore-NM-EPD-420-BW-repeater-v1.18.0-mod-rev0.2.bin"; IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_room_server"; OutName = "MeshCore-NM-EPD-420-BW-room_server-v1.18.0-mod-rev0.2.bin"; IsDefault = $false },
    @{ Env = "NM_EPD_420_BW_kiss_modem"; OutName = "MeshCore-NM-EPD-420-BW-kiss_modem-v1.18.0-mod-rev0.2.bin"; IsDefault = $false }
)

$releaseDir = Join-Path $workspaceRoot "release"
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir | Out-Null
}
$bootApp0 = Join-Path $releaseDir "boot_app0.bin"

foreach ($role in $roles) {
    $roleEnv = $role.Env
    $outBin = Join-Path $releaseDir $role.OutName
    Write-Host "`n>>> [BUILDING] Role: $roleEnv -> $($role.OutName)" -ForegroundColor Yellow
    
    # Run pio build with mergebin target
    & $pio run -d $workspaceRoot -e $roleEnv -t mergebin
    
    $buildDir = Join-Path $workspaceRoot ".pio\build\$roleEnv"
    $mergedBin = Join-Path $buildDir "firmware-merged.bin"
    
    if (-not (Test-Path $mergedBin)) {
        Write-Host "[*] Merging manually via esptool.py..." -ForegroundColor DarkYellow
        $bootloader = Join-Path $buildDir "bootloader.bin"
        $partitions = Join-Path $buildDir "partitions.bin"
        $firmware = Join-Path $buildDir "firmware.bin"
        
        & $python -X utf8 $esptool --chip esp32s3 merge_bin -o $mergedBin --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 $bootloader 0x8000 $partitions 0xe000 $bootApp0 0x10000 $firmware
    }
    
    if (-not (Test-Path $mergedBin)) {
        throw "Failed to create merged binary for $roleEnv!"
    }
    
    Copy-Item -Path $mergedBin -Destination $outBin -Force
    $size = (Get-Item $outBin).Length
    Write-Host "[OK] Created $outBin ($size bytes)" -ForegroundColor Green
    
    if ($role.IsDefault) {
        $defaultAlias = Join-Path $releaseDir "MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.2.bin"
        Copy-Item -Path $mergedBin -Destination $defaultAlias -Force
        Write-Host "[OK] Created default alias $defaultAlias ($size bytes)" -ForegroundColor Green

        $firmwareBin = Join-Path $releaseDir "firmware.bin"
        Copy-Item -Path $mergedBin -Destination $firmwareBin -Force
        Write-Host "[OK] Updated default $firmwareBin ($size bytes)" -ForegroundColor Green
    }
}

Write-Host "`n=== Verifying generated binaries in release/ ===" -ForegroundColor Cyan
Get-ChildItem -Path $releaseDir -Filter "*rev0.2.bin" | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize

Write-Host "`n=== Git Status & Commit ===" -ForegroundColor Cyan
& $git status --short

# Git commit and tag
& $git add -A
& $git commit -m "release(v1.18.0-mod-rev0.2): rebuild all 5 roles with power rail & lipo safety fixes"
& $git tag -fa v1.18.0-mod-rev0.2 -m "MeshCore-NM-EPD-420-BW v1.18.0-mod-rev0.2"
Write-Host "[OK] Git commit and tag completed!" -ForegroundColor Green

& $git log -1 --stat
& $git tag -l -n1 "v1.18.0-mod-rev0.2"

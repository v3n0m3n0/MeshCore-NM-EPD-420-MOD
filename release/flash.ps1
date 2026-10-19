<#
.SYNOPSIS
    Flashes MeshCore -MOD firmware suite to RockBase NM-EPD-420-BW (ESP32-S3)
.PARAMETER Port
    Optional COM port (e.g. COM15). If omitted, scans for connected serial ports.
.PARAMETER Role
    Optional firmware role: 'ble', 'usb', 'repeater', 'room_server', 'kiss_modem'.
    If omitted, an interactive menu is displayed.
#>
param(
    [string]$Port = "",
    [string]$Role = ""
)

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "   MeshCore NM-EPD-420-BW (ESP32-S3) Firmware Flasher (PowerShell)    " -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Port Selection
if (-not $Port) {
    $availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($availablePorts.Count -eq 1) {
        $Port = $availablePorts[0]
        Write-Host "[*] Automatically detected COM port: $Port" -ForegroundColor Green
    } elseif ($availablePorts.Count -gt 1) {
        Write-Host "Available COM ports:" -ForegroundColor Yellow
        for ($i = 0; $i -lt $availablePorts.Count; $i++) {
            Write-Host "  [$i] $($availablePorts[$i])"
        }
        $sel = Read-Host "Select port number [0-$($availablePorts.Count - 1)] or type COM port name"
        if ($sel -match '^\d+$' -and [int]$sel -lt $availablePorts.Count) {
            $Port = $availablePorts[[int]$sel]
        } else {
            $Port = $sel
        }
    } else {
        $Port = Read-Host "Enter COM port (e.g. COM15)"
    }
}

if (-not $Port) {
    Write-Host "[ERROR] No COM port selected. Exiting." -ForegroundColor Red
    exit 1
}

# 2. Firmware Role Selection
$firmwareMap = @{
    "0" = @{ Name = "Companion Radio BLE"; Pattern = "*companion_radio_ble*.bin"; Fallback = "MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.1.bin" }
    "1" = @{ Name = "Companion Radio USB"; Pattern = "*companion_radio_usb*.bin"; Fallback = "" }
    "2" = @{ Name = "Repeater";            Pattern = "*repeater*.bin";            Fallback = "" }
    "3" = @{ Name = "Room Server";         Pattern = "*room_server*.bin";         Fallback = "" }
    "4" = @{ Name = "KISS Modem";          Pattern = "*kiss_modem*.bin";          Fallback = "" }
}

$roleKey = ""
if ($Role) {
    switch -Regex ($Role.ToLower()) {
        "ble"     { $roleKey = "0" }
        "usb"     { $roleKey = "1" }
        "rep"     { $roleKey = "2" }
        "room"    { $roleKey = "3" }
        "kiss"    { $roleKey = "4" }
        default   { $roleKey = "" }
    }
}

if (-not $roleKey) {
    Write-Host "`nSelect Firmware Role to Flash:" -ForegroundColor Yellow
    Write-Host "  [0] Companion Radio BLE (Standard Mobile App + 4.2`" E-Paper UI) [DEFAULT]"
    Write-Host "  [1] Companion Radio USB (Desktop / Serial Host + 4.2`" E-Paper UI)"
    Write-Host "  [2] Repeater (Autonomous LoRa Packet Router + 4.2`" E-Paper UI)"
    Write-Host "  [3] Room Server (BBS Public Forum / Channel Server + 4.2`" E-Paper UI)"
    Write-Host "  [4] KISS Modem (Serial TNC / AX.25 / APRS / TCP-IP bridge)"
    $choice = Read-Host "`nEnter selection [0-4] (default: 0)"
    if ([string]::IsNullOrWhiteSpace($choice)) {
        $roleKey = "0"
    } elseif ($firmwareMap.ContainsKey($choice)) {
        $roleKey = $choice
    } else {
        Write-Host "[!] Unknown selection, defaulting to [0] Companion Radio BLE." -ForegroundColor DarkYellow
        $roleKey = "0"
    }
}

$selectedRole = $firmwareMap[$roleKey]
Write-Host "[*] Selected Role: $($selectedRole.Name)" -ForegroundColor Cyan

# Find matching file in script directory
$binCandidates = Get-ChildItem -Path $PSScriptRoot -Filter $selectedRole.Pattern | Sort-Object Name -Descending
if ($binCandidates) {
    $firmwareFile = $binCandidates[0].FullName
} elseif ($selectedRole.Fallback -and (Test-Path (Join-Path $PSScriptRoot $selectedRole.Fallback))) {
    $firmwareFile = Join-Path $PSScriptRoot $selectedRole.Fallback
} else {
    Write-Host "[ERROR] Could not find binary for $($selectedRole.Name) in $PSScriptRoot" -ForegroundColor Red
    exit 1
}

Write-Host "`n[*] Flashing $firmwareFile to $Port at 921600 baud..." -ForegroundColor Yellow

$pythonExe = "python"
if ($env:USERPROFILE -and (Test-Path "$env:USERPROFILE\.platformio\penv\Scripts\python.exe")) {
    $pythonExe = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
}

& $pythonExe -m esptool --port $Port --baud 921600 --chip esp32s3 write_flash 0x0 $firmwareFile

if ($LASTEXITCODE -ne 0) {
    Write-Host "`n[ERROR] Flashing failed! Code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n[*] Resetting device via watchdog-reset..." -ForegroundColor Yellow
& $pythonExe -m esptool --port $Port --before no-reset --after watchdog-reset flash_id *>$null

Write-Host "`n[OK] Device flashed and restarted successfully as '$($selectedRole.Name)'!" -ForegroundColor Green

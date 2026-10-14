<#
.SYNOPSIS
    Flashes MeshCore firmware to RockBase NM-EPD-420-BW (ESP32-S3)
.PARAMETER Port
    Optional COM port (e.g. COM15). If omitted, scans for connected serial ports.
#>
param(
    [string]$Port = ""
)

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "   MeshCore NM-EPD-420-BW (ESP32-S3) Firmware Flasher (PowerShell)" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host ""

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

$firmwareFile = Join-Path $PSScriptRoot "MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin"
if (-not (Test-Path $firmwareFile)) {
    Write-Host "[ERROR] Firmware binary not found at: $firmwareFile" -ForegroundColor Red
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

Write-Host "`n[OK] Device flashed and restarted successfully!" -ForegroundColor Green

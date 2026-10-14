@echo off
setlocal enabledelayedexpansion
title Flash MeshCore NM-EPD-420-BW Firmware

echo ======================================================================
echo    MeshCore NM-EPD-420-BW (ESP32-S3) Firmware Flasher
echo ======================================================================
echo.

set PORT=%1
if "%PORT%"=="" (
    set /p PORT="Enter COM port of your device (e.g. COM15): "
)

if "%PORT%"=="" (
    echo [ERROR] No COM port provided. Exiting.
    pause
    exit /b 1
)

echo.
echo [*] Target Port: %PORT%
echo [*] Writing unified image: MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin to 0x0...
echo.

python -m esptool --port %PORT% --baud 921600 --chip esp32s3 write_flash 0x0 "%~dp0MeshCore-NM-EPD-420-BW-v1.17.1-mod.bin"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Flash failed! Please check COM port connection or put device into bootloader mode (Hold BOOT, press RESET).
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [*] Flashing completed successfully! Resetting device...
python -m esptool --port %PORT% --before no-reset --after watchdog-reset flash_id >nul 2>&1

echo.
echo [OK] Done! Your RockBase NM-EPD-420-BW is now running MeshCore-mod.
echo.
pause

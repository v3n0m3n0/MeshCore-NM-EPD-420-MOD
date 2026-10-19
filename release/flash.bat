@echo off
setlocal enabledelayedexpansion
title Flash MeshCore NM-EPD-420-BW Firmware Suite

echo ======================================================================
echo    MeshCore NM-EPD-420-BW (ESP32-S3) Firmware Suite Flasher
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

set ROLE=%2
if "%ROLE%"=="" (
    echo.
    echo Select Firmware Role to Flash:
    echo   [1] Companion Radio BLE (Standard Mobile App + 4.2" E-Paper UI) [DEFAULT]
    echo   [2] Companion Radio USB (Desktop / Serial Host + 4.2" E-Paper UI)
    echo   [3] Repeater (Autonomous LoRa Packet Router + 4.2" E-Paper UI)
    echo   [4] Room Server (BBS Public Forum / Channel Server + 4.2" E-Paper UI)
    echo   [5] KISS Modem (Serial TNC / AX.25 / APRS / TCP-IP bridge)
    echo.
    set /p ROLE="Enter choice [1-5] (default: 1): "
)

if "%ROLE%"=="" set ROLE=1

set "BINFILE="
if "%ROLE%"=="1" (
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*companion_radio_ble*.bin" 2^>nul') do set "BINFILE=%%f"
    if "!BINFILE!"=="" set "BINFILE=MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.1.bin"
    set "ROLENAME=Companion Radio BLE"
) else if "%ROLE%"=="2" (
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*companion_radio_usb*.bin" 2^>nul') do set "BINFILE=%%f"
    set "ROLENAME=Companion Radio USB"
) else if "%ROLE%"=="3" (
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*repeater*.bin" 2^>nul') do set "BINFILE=%%f"
    set "ROLENAME=Repeater"
) else if "%ROLE%"=="4" (
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*room_server*.bin" 2^>nul') do set "BINFILE=%%f"
    set "ROLENAME=Room Server"
) else if "%ROLE%"=="5" (
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*kiss_modem*.bin" 2^>nul') do set "BINFILE=%%f"
    set "ROLENAME=KISS Modem"
) else (
    echo [!] Unknown role selection '%ROLE%', defaulting to Companion Radio BLE.
    for /f "delims=" %%f in ('dir /b /o:n "%~dp0*companion_radio_ble*.bin" 2^>nul') do set "BINFILE=%%f"
    if "!BINFILE!"=="" set "BINFILE=MeshCore-NM-EPD-420-BW-v1.18.0-mod-rev0.1.bin"
    set "ROLENAME=Companion Radio BLE"
)

if "!BINFILE!"=="" (
    echo [ERROR] Firmware binary for role not found in %~dp0
    pause
    exit /b 1
)

echo.
echo [*] Target Port: %PORT%
echo [*] Selected Role: !ROLENAME!
echo [*] Writing unified image: !BINFILE! to 0x0...
echo.

python -m esptool --port %PORT% --baud 921600 --chip esp32s3 write_flash 0x0 "%~dp0!BINFILE!"

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
echo [OK] Done! Your RockBase NM-EPD-420-BW is now running MeshCore-mod (!ROLENAME!).
echo.
pause

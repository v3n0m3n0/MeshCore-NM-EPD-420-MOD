#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <NMEPD420Board.h>
#include <NMEPD420EnvironmentSensorManager.h>
#include <NMEPD420SDLogger.h>
#include <RadioLib.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/RadioLibWrappers.h>

#ifdef DISPLAY_CLASS
#include <NMEPD420Display.h>
#include <helpers/ui/MomentaryButton.h>
#endif

extern NMEPD420Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern NMEPD420EnvironmentSensorManager sensors;
extern NMEPD420SDLogger sd_logger;

SPIClass& getSharedHspi();

#ifdef DISPLAY_CLASS
extern DISPLAY_CLASS display;
extern MomentaryButton boot_btn;
extern MomentaryButton user_btn;
#endif

#ifndef AUTO_SHUTDOWN_MILLIVOLTS
#define AUTO_SHUTDOWN_MILLIVOLTS 3300
#endif
#ifndef BATT_MIN_MILLIVOLTS
#define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
#define BATT_MAX_MILLIVOLTS 4200
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();

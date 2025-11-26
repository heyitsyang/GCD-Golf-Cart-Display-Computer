#ifndef NMEAGPS_CFG_H
#define NMEAGPS_CFG_H

//  Copyright (C) 2014-2017, SlashDevin
//  Modified for Golf Cart Display project - L76K GPS module
//
//  This file is part of NeoGPS
//
//  NOTE: This file must be manually copied to .pio/libdeps/gcd/NeoGPS/src/
//  after NeoGPS library updates. The library version is pinned in platformio.ini
//  to minimize update frequency.

#include "GPSfix_cfg.h"

//------------------------------------------------------
// Enable/disable the parsing of specific sentences.
// Enabled for L76K multi-constellation GPS module

#define NMEAGPS_PARSE_GGA   // Location, altitude, satellites, HDOP
#define NMEAGPS_PARSE_RMC   // Location, speed, heading, date/time
#define NMEAGPS_PARSE_VTG   // Speed and heading (redundant with RMC but useful)
//#define NMEAGPS_PARSE_GLL   // Location only - not needed
//#define NMEAGPS_PARSE_GSA   // DOP and active satellites - adds RAM usage
//#define NMEAGPS_PARSE_GSV   // Satellites in view - adds significant RAM
//#define NMEAGPS_PARSE_GST   // Position error statistics
//#define NMEAGPS_PARSE_ZDA   // Date/time only - RMC has this

//------------------------------------------------------
// Select which sentence is sent *last* by your GPS device
// in each update interval. For L76K, RMC is typically last.
// Use NMEAorder.ino to verify if needed.

#define LAST_SENTENCE_IN_INTERVAL NMEAGPS::NMEA_RMC

//------------------------------------------------------
// Choose how multiple sentences are merged into a fix:
// EXPLICIT_MERGING provides complete merged fix after last sentence

#define NMEAGPS_EXPLICIT_MERGING
//#define NMEAGPS_IMPLICIT_MERGING

#ifdef NMEAGPS_IMPLICIT_MERGING
  #define NMEAGPS_MERGING NMEAGPS::IMPLICIT_MERGING
  #define NMEAGPS_INIT_FIX(m)
  #define NMEAGPS_INVALIDATE(m) m_fix.valid.m = false
#else
  #ifdef NMEAGPS_EXPLICIT_MERGING
    #define NMEAGPS_MERGING NMEAGPS::EXPLICIT_MERGING
  #else
    #define NMEAGPS_MERGING NMEAGPS::NO_MERGING
    #define NMEAGPS_NO_MERGING
  #endif
  #define NMEAGPS_INIT_FIX(m) m.valid.init()
  #define NMEAGPS_INVALIDATE(m)
#endif

#if ( defined(NMEAGPS_NO_MERGING) + \
    defined(NMEAGPS_IMPLICIT_MERGING) + \
    defined(NMEAGPS_EXPLICIT_MERGING) )  > 1
  #error Only one MERGING technique should be enabled in NMEAGPS_cfg.h!
#endif

//------------------------------------------------------
// Define the fix buffer size.

#define NMEAGPS_FIX_MAX 1

#if defined(NMEAGPS_EXPLICIT_MERGING) && (NMEAGPS_FIX_MAX == 0)
  #error You must define FIX_MAX >= 1 to allow EXPLICIT merging in NMEAGPS_cfg.h
#endif

//------------------------------------------------------
// Define how fixes are dropped when the FIFO is full.

#define NMEAGPS_KEEP_NEWEST_FIXES true

//------------------------------------------------------
// Enable/Disable interrupt-style processing of GPS characters

//#define NMEAGPS_INTERRUPT_PROCESSING

#ifdef  NMEAGPS_INTERRUPT_PROCESSING
  #define NMEAGPS_PROCESSING_STYLE NMEAGPS::PS_INTERRUPT
#else
  #define NMEAGPS_PROCESSING_STYLE NMEAGPS::PS_POLLING
#endif

//------------------------------------------------------
// Talker ID and manufacturer ID processing
// Not needed for basic operation - saves RAM

//#define NMEAGPS_SAVE_TALKER_ID
//#define NMEAGPS_PARSE_TALKER_ID

//#define NMEAGPS_PARSE_PROPRIETARY
#ifdef NMEAGPS_PARSE_PROPRIETARY
  //#define NMEAGPS_SAVE_MFR_ID
  #define NMEAGPS_PARSE_MFR_ID
#endif

//------------------------------------------------------
// Satellite tracking - disabled to save RAM

//#define NMEAGPS_PARSE_SATELLITES
//#define NMEAGPS_PARSE_SATELLITE_INFO

#ifdef NMEAGPS_PARSE_SATELLITES
  #define NMEAGPS_MAX_SATELLITES (20)
  #ifndef GPS_FIX_SATELLITES
    #error GPS_FIX_SATELLITES must be defined in GPSfix.h!
  #endif
#endif

#if defined(NMEAGPS_PARSE_SATELLITE_INFO) & \
    !defined(NMEAGPS_PARSE_SATELLITES)
  #error NMEAGPS_PARSE_SATELLITES must be defined!
#endif

//------------------------------------------------------
// Enable/disable gathering interface statistics

#define NMEAGPS_STATS

//------------------------------------------------------
// Configuration for derived types of NMEAGPS

//#define NMEAGPS_DERIVED_TYPES

#ifdef NMEAGPS_DERIVED_TYPES
  #define NMEAGPS_VIRTUAL virtual
#else
  #define NMEAGPS_VIRTUAL
#endif

#if (defined(NMEAGPS_PARSE_TALKER_ID) | defined(NMEAGPS_PARSE_MFR_ID)) &  \
           !defined(NMEAGPS_DERIVED_TYPES)
  #error You must define NMEAGPS_DERIVED_TYPES in NMEAGPS.h in order to parse Talker and/or Mfr IDs!
#endif

//------------------------------------------------------
// Validity checks

#define NMEAGPS_VALIDATE_CHARS false
#define NMEAGPS_VALIDATE_FIELDS false

//------------------------------------------------------
// Handle devices that omit trailing commas

#define NMEAGPS_COMMA_NEEDED

//------------------------------------------------------
// Recognize all standard sentences (even if not parsing)

#define NMEAGPS_RECOGNIZE_ALL

//------------------------------------------------------
// Extra parsing space

//#define NMEAGPS_PARSING_SCRATCHPAD

//------------------------------------------------------
// Timestamp options

//#define NMEAGPS_TIMESTAMP_FROM_INTERVAL
//#define NMEAGPS_TIMESTAMP_FROM_PPS

#if defined( NMEAGPS_TIMESTAMP_FROM_INTERVAL ) &   \
    defined( NMEAGPS_TIMESTAMP_FROM_PPS )
  #error You cannot enable both TIMESTAMP_FROM_INTERVAL and PPS in NMEAGPS_cfg.h!
#endif

#endif

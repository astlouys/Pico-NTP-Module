/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* ============================================================================================================================================================= *\
   Pico-NTP-Module.c
   St-Louys, Andre - January 2024
   astlouys@gmail.com
   Revision 18-APR-2025
   Language: C
   Version 4.00

   REVISION HISTORY:
   =================
               1.00 - Initial version from Raspberry Pi (Trading) Ltd.
               cmake -DPICO_BOARD=pico_w -DPICO_STDIO_USB=1 -DWIFI_SSID=<NetworkName> -DWIFI_PASSWORD=<Password>
   18-FEB-2024 2.00 - Adapted for Pico-RGB-Matrix.
   07-OCT-2024 3.00 - Modified to make it usable as a "module" by many other projects.
   07-FEB-2025 4.00 - Add integrated support for daylight saving time.
                    - Make StructNTP a member of function arguments so that it can be declared in the parent C module.
                    - Debug automatic handling of daylight saving time.
\* ============================================================================================================================================================= */

#define RELEASE_VERSION  ///
#ifdef RELEASE_VERSION
#warning ===============> NTP module built as RELEASE_VERSION.
#else   // RELEASE_VERSION
#define DEVELOPER_VERSION
#warning ===============> NTP module built as DEVELOPER_VERSION.
#endif  // RELEASE_VERSION


#include "hardware/gpio.h"
#include "lwip/dns.h"
#include <pico/stdio_usb.h>
#include "Pico-NTP-Module.h"
#include <string.h>
#include <time.h>





/* ============================================================================================================================================================= *\
                                                                      Static function prototypes.
\* ============================================================================================================================================================= */
/* Callback with a DNS result. */
static void ntp_dns_found(const char *HostName, const ip_addr_t *ipaddr, void *ExtraArgument);

/* NTP request failed. */
static int64_t ntp_failed_handler(alarm_id_t id, void *ExtraArgument);

/* NTP data received. */
static void ntp_recv(void *ExtraArgument, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

/* Make an NTP request. */
static void ntp_request(struct struct_ntp *StructNTP);





/* ============================================================================================================================================================= *\
                                                                            Global variables.
\* ============================================================================================================================================================= */
/* Daylight saving time (DST) parameters for all countries of the world (or almost). */
struct dst_parameters
{
  UINT8  StartMonth;
  UINT8  StartDayOfWeek;
  int8_t StartDayOfMonthLow;
  int8_t StartDayOfMonthHigh;
  UINT8  StartHour;
  UINT16 StartDayOfYear;
  UINT8  EndMonth;
  UINT8  EndDayOfWeek;
  int8_t EndDayOfMonthLow;
  int8_t EndDayOfMonthHigh;
  UINT8  EndHour;
  UINT16 EndDayOfYear;
  UINT8  ShiftMinutes;
}DstParameters[25];

struct dst_parameters DstParameters[25] =
{
  { 0,  0,  0,  0, 24,  0,  0,  0,  0,  0,  0,  0, 60},  //  0 - Dummy
  {10,  0,  1,  7,  2,  0,  4,  0,  1,  7,  3,  0, 60},  //  1 - Australia
  {10,  0,  1,  7,  2,  0,  4,  0,  1,  7,  2,  0, 30},  //  2 - Australia-Howe
  { 9,  6,  1,  7, 24,  0,  4,  6,  1,  7, 24,  0, 60},  //  3 - Chile           (StartHour and EndHour changes at 24h00, will change at 00h00 the day after)
  { 3,  0,  8, 14,  0,  0, 11,  0,  1,  7,  1,  0, 60},  //  4 - Cuba
  { 3,  0, 25, 31,  1,  0, 10,  0, 25, 31,  1,  0, 60},  //  5 - European Union  (StartHour and EndHour are based on UTC time)
  { 3,  5, 23, 29,  2,  0, 10,  0, 25, 31,  2,  0, 60},  //  6 - Israel
  { 3,  0, 25, 31,  0,  0, 10,  0, 25, 31,  0,  0, 60},  //  7 - Lebanon
  { 3,  0, 25, 31,  2,  0, 10,  0, 25, 31,  3,  0, 60},  //  8 - Moldova
  { 9,  0, 24, 30,  2,  0,  4,  0,  1,  7,  2,  0, 60},  //  9 - New-Zealand     (StartHour and EndHour are based on UTC time)
  { 3,  0,  8, 14,  2,  0, 11,  0,  1,  7,  2,  0, 60},  // 10 - North America
  { 3,  6, 24, 30,  2,  0, 10,  6, 24, 30,  2,  0, 60},  // 11 - Palestine
  {10,  0,  1,  7,  0,  0,  3,  0, 22, 28,  0,  0, 60},  // 12 - Paraguay
};
// #define MAX_DST_COUNTRIES 12 must be adjusted in Pico-NTP-Module.h if we add more countries.


/* NOTE: Variables below are defined in a language file. To add a new language, see how French and English variables are defined in language files. */
/* ------------------------------------------------------------------------------------------------------------------------------------------------ */
/* Complete month names. */
UCHAR MonthName[13][13] =
{
  {" "}, {$JANUARY}, {$FEBRUARY}, {$MARCH}, {$APRIL}, {$MAY}, {$JUNE}, {$JULY}, {$AUGUST}, {$SEPTEMBER}, {$OCTOBER}, {$NOVEMBER}, {$DECEMBER}
};

/* Short month names (3 letters). */
UCHAR ShortMonth[13][4] =
{
  {" "}, {$JAN}, {$FEB}, {$MAR}, {$APR}, {$MAY}, {$JUN}, {$JUL}, {$AUG}, {$SEP}, {$OCT}, {$NOV}, {$DEC}
};

/* Complete day names. */
UCHAR DayName[7][13] =
{
  {$SUNDAY}, {$MONDAY}, {$TUESDAY}, {$WEDNESDAY}, {$THURSDAY}, {$FRIDAY}, {$SATURDAY}
};

/* Short - 3-letters - day names. */
UCHAR ShortDay[7][4] =
{
  {$SUN}, {$MON}, {$TUE}, {$WED}, {$THU}, {$FRI}, {$SAT}
};

/* Complete month names. */
/// const UCHAR MonthName[13][13] =
/// {
///   {" "}, {"January"}, {"February"}, {"March"}, {"April"}, {"May"}, {"June"}, {"July"}, {"August"}, {"September"}, {"October"}, {"November"}, {"December"}
/// };

/* Short - 3-letters - month names. */
/// const UCHAR ShortMonth[13][4] =
/// {
///   {" "}, {"JAN"}, {"FEB"}, {"MAR"}, {"APR"}, {"MAY"}, {"JUN"}, {"JUL"}, {"AUG"}, {"SEP"}, {"OCT"}, {"NOV"}, {"DEC"}
/// };

/* Complete day names. */
/// UCHAR DayName[7][13] =
/// {
///   {"Sunday"}, {"Monday"}, {"Tuesday"}, {"Wednesday"}, {"Thursday"}, {"Friday"}, {"Saturday"}
/// };

/* Short - 3-letters - day names. */
/// const UCHAR ShortDay[7][4] =
/// {
///   {"SUN"}, {"MON"}, {"TUE"}, {"WED"}, {"THU"}, {"FRI"}, {"SAT"}
/// };


/// struct struct_ntp StructNTP;





/* $PAGE */
/* $TITLE=ntp_convert_human_to_tm() */
/* ============================================================================================================================================================= *\
                                                                  Convert "HumanTime" to "tm_time".
\* ============================================================================================================================================================= */
void ntp_convert_human_to_tm(struct human_time *HumanTime, struct tm *TmTime)
{
  TmTime->tm_mday  = HumanTime->DayOfMonth;     // tm_mday: 1 to 31
  TmTime->tm_mon   = HumanTime->Month - 1;      // tm_mon:  months since January (0 to 11)
  TmTime->tm_year  = HumanTime->Year - 1900;    // tm_year: years since 1900
  TmTime->tm_wday  = HumanTime->DayOfWeek;      // tm_wday: Sunday = 0 (...)  Saturday = 6
  TmTime->tm_yday  = HumanTime->DayOfYear - 1;  // tm_yday: 0 to 365
  TmTime->tm_hour  = HumanTime->Hour;           // tm_hour: 0 to 23
  TmTime->tm_min   = HumanTime->Minute;         // tm_min:  0 to 59
  TmTime->tm_sec   = HumanTime->Second;         // tm_sec:  0 to 59
  TmTime->tm_isdst = 0;                         // tm_isdst: (if < 0 means not used)   (if > 0 means FLAG_ON)   (if = 0 means FLAG_OFF)

  return;
}





/* $PAGE */
/* $TITLE=ntp_convert_human_to_unix() */
/* ============================================================================================================================================================= *\
                                                                  Convert "HumanTime" to "Unix Time".
                                                         NOTE: Unix Time is based on UTC time, not local time.
\* ============================================================================================================================================================= */
UINT64 ntp_convert_human_to_unix(struct human_time *HumanTime)
{
  UINT64 UnixTime;

  struct tm TempTime;


  ntp_convert_human_to_tm(HumanTime, &TempTime);
  UnixTime = ntp_convert_tm_to_unix(&TempTime);

  return UnixTime;
}





/* $PAGE */
/* $TITLE=ntp_convert_tm_to_unix() */
/* ============================================================================================================================================================= *\
                                                                     Convert "TmTime" to "Unix Time".
                                                         NOTE: Unix Time is based on UTC time, not local time.
\* ============================================================================================================================================================= */
UINT64 ntp_convert_tm_to_unix(struct tm *TmTime)
{
  time_t UnixTime;


  UnixTime = mktime(TmTime);

  return UnixTime;
}





/* $PAGE */
/* $TITLE=ntp_convert_unix_time() */
/* ============================================================================================================================================================= *\
                                                             Convert Unix time to tm time and human time.
                      NOTE: DS3231 gives correct value for day-of-year but the convertion is incorrect for leap year... must be investigated.
\* ============================================================================================================================================================= */
void ntp_convert_unix_time(time_t UnixTime, struct tm *TmTime, struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION


  struct tm TempTime;


  if (FlagLocalDebug) log_info(__LINE__, __func__, "Unix time on entry:          %12llu\r", UnixTime);

  /* Find tm_time */
  TempTime = *localtime(&UnixTime);


  /*** Transfer to calling structure. *** Must be optimized. ***/
  TmTime->tm_hour  = TempTime.tm_hour;
  TmTime->tm_min   = TempTime.tm_min;
  TmTime->tm_sec   = TempTime.tm_sec;
  TmTime->tm_mday  = TempTime.tm_mday;
  TmTime->tm_mon   = TempTime.tm_mon;
  TmTime->tm_year  = TempTime.tm_year;
  TmTime->tm_wday  = TempTime.tm_wday;
  TmTime->tm_yday  = TempTime.tm_yday;
  TmTime->tm_isdst = TempTime.tm_isdst;


  /* Find equivalent in human time. */
  StructNTP->HumanTime.Hour       = TmTime->tm_hour;
  StructNTP->HumanTime.Minute     = TmTime->tm_min;
  StructNTP->HumanTime.Second     = TmTime->tm_sec;
  StructNTP->HumanTime.DayOfMonth = TmTime->tm_mday;
  StructNTP->HumanTime.Month      = TmTime->tm_mon + 1;
  StructNTP->HumanTime.Year       = TmTime->tm_year + 1900;
  StructNTP->HumanTime.DayOfWeek  = TmTime->tm_wday;
  StructNTP->HumanTime.DayOfYear  = TmTime->tm_yday;
  StructNTP->HumanTime.FlagDst    = TmTime->tm_isdst;

  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "============================================\r");
    log_info(__LINE__, __func__, "HumanTime->Hour        =   %2.2u\r", StructNTP->HumanTime.Hour);
    log_info(__LINE__, __func__, "HumanTime->Minute      =   %2.2u\r", StructNTP->HumanTime.Minute);
    log_info(__LINE__, __func__, "HumanTime->Second      =   %2.2u\r", StructNTP->HumanTime.Second);
    log_info(__LINE__, __func__, "HumanTime->DayOfMonth  =   %2.2u\r", StructNTP->HumanTime.DayOfMonth);
    log_info(__LINE__, __func__, "HumanTime->Month       =   %2.2u\r", StructNTP->HumanTime.Month);
    log_info(__LINE__, __func__, "HumanTime->Year        = %4.4u\r",   StructNTP->HumanTime.Year);
    log_info(__LINE__, __func__, "HumanTime->DayOfWeek   = %4u\r",     StructNTP->HumanTime.DayOfWeek);
    log_info(__LINE__, __func__, "HumanTime->DayOfYear   = %4u\r",     StructNTP->HumanTime.DayOfYear);
    log_info(__LINE__, __func__, "HumanTime->FlagDst     = %4u\r", StructNTP->HumanTime.FlagDst);
    log_info(__LINE__, __func__, "============================================\r");
  }

  return;
}





/* $PAGE */
/* $TITLE=ntp_display_info() */
/* ============================================================================================================================================================= *\
                                                                    Display handy NTP-related information.
\* ============================================================================================================================================================= */
void ntp_display_info(struct struct_ntp *StructNTP)
{
  UCHAR String[65];

  UINT8 FlagConnection;  // flag indicating NTP connection has already been done at least once previously.

  INT64 DeltaTime;


  FlagConnection = FLAG_OFF;  // assume no connection on entry.

  log_info(__LINE__, __func__, "======================================================================\r");
  log_info(__LINE__, __func__, "               Network Time Protocol (NTP) information\r");
  log_info(__LINE__, __func__, "======================================================================\r");


  if (strcmp(ip4addr_ntoa(&StructNTP->ServerAddress), "0.0.0.0") != 0) FlagConnection = FLAG_ON;

  if (FlagConnection)
  {
    /* At least one NTP request has already been done, display NTP health status and last server address used. */
    if (StructNTP->FlagHealth == FLAG_ON)
      strcpy(String, "Good");
    else
      strcpy(String, "Problems");
 
    log_info(__LINE__, __func__, "NTP health: %s - Last NTP server: %-15s\r",          String, ip4addr_ntoa(&StructNTP->ServerAddress));
  }
  else
  {
    /* NTP information is displayed before the first NTP request. */
    log_info(__LINE__, __func__, "No NTP cycle has been executed so far.\r");
  }


  log_info(__LINE__, __func__, "Errors: %lu     Reads: %lu     Polls: %lu\r",      StructNTP->TotalErrors, StructNTP->ReadCycles, StructNTP->PollCycles);
  log_info(__LINE__, __func__, "FlagInit:                      0x%2.2X\r",         StructNTP->FlagInit);
  log_info(__LINE__, __func__, "FlagSuccess:                   0x%2.2X\r",         StructNTP->FlagSuccess);
  log_info(__LINE__, __func__, "FlagHistory:                   0x%2.2X\r",         StructNTP->FlagHistory);
  log_info(__LINE__, __func__, "Pico internal timer:   %12llu usec   (%5llu sec)\r", get_absolute_time(), (time_us_64() / 1000000ll));
  log_info(__LINE__, __func__, "NTP update time:       %12llu usec   (%5llu sec)\r", to_us_since_boot(StructNTP->UpdateTime), to_us_since_boot(StructNTP->UpdateTime) / 1000000ll);
  log_info(__LINE__, __func__, "Time difference:       %12lld usec\r",             absolute_time_diff_us(get_absolute_time(), StructNTP->UpdateTime));
  sleep_ms(80);  // prevent communication override.

  DeltaTime = (absolute_time_diff_us(get_absolute_time(), StructNTP->UpdateTime) / 1000000ll);
  if (DeltaTime >= 0)
    log_info(__LINE__, __func__, "Time remaining:        %12llu sec        (%llu min)\r", DeltaTime, (DeltaTime / 60));
  else
    log_info(__LINE__, __func__, "Time over by:          %12llu sec        (%llu min)\r", llabs(DeltaTime), (llabs(DeltaTime) / 60));

  log_info(__LINE__, __func__, "ScanCount:                       %2u\r",       StructNTP->ScanCount);
  log_info(__LINE__, __func__, "DST country:                     %2u\r",       StructNTP->DSTCountry);
  log_info(__LINE__, __func__, "Delta time:                    %4d minutes\r", StructNTP->DeltaTime);

  if (FlagConnection)
  {
    log_info(__LINE__, __func__, "Day-of-year start:              %3u\r",    StructNTP->DoYStart);
    log_info(__LINE__, __func__, "Day-of-year end:                %3u\r",    StructNTP->DoYEnd);
    log_info(__LINE__, __func__, "UTC start:             %12llu\r",          StructNTP->DSTStart);
    log_info(__LINE__, __func__, "UTC end:               %12llu\r",          StructNTP->DSTEnd);
    log_info(__LINE__, __func__, "UTCTime:               %12llu\r",          StructNTP->UTCTime);
    log_info(__LINE__, __func__, "LocaTime:              %12llu\r",          StructNTP->LocalTime);
    log_info(__LINE__, __func__, "Flag summer time:              0x%2.2X\r", StructNTP->FlagSummerTime);
    log_info(__LINE__, __func__, "Latency (round-trip):  %12ld usec  (one-way: %ld usec)\r", StructNTP->Latency, (StructNTP->Latency / 2));
    log_info(__LINE__, __func__, "DNSRequestSent:                0x%2.2X\r", StructNTP->DNSRequestSent);
    log_info(__LINE__, __func__, "ResendAlarm:                 %6u\r",       StructNTP->ResendAlarm);
  }
  log_info(__LINE__, __func__, "======================================================================\r\r\r");
  sleep_ms(80);  // prevent communication override.

  return;
}





/* $PAGE */
/* $TITLE=ntp_dns_found() */
/* ============================================================================================================================================================= *\
                                                                   Call back with a DNS result.
\* ============================================================================================================================================================= */
static void ntp_dns_found(const char *HostName, const ip_addr_t *ipaddr, void *ExtraArgument)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  UCHAR String[20];

  struct struct_ntp *StructNTP = ExtraArgument;


  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "Entering ntp_dns_found()\r");
    log_info(__LINE__, __func__, "NTP pool host name:         <%s>\r", HostName);
    log_info(__LINE__, __func__, "NTP org host IP address:  %15s\r",   ip4addr_ntoa(&StructNTP->ServerAddress));
    log_info(__LINE__, __func__, "NTP pool IP address:      %15s\r",   ip4addr_ntoa(ipaddr));
  }

  if (ipaddr)
  {
    StructNTP->ServerAddress = *ipaddr;
    ntp_request(StructNTP);
  }
  else
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "NTP DNS request failed.\r");
    ntp_result(-1, NULL, StructNTP);
  }

  return;
}





/* $TITLE=ntp_dst_settings() */
/* $PAGE */
/* ============================================================================================================================================================= *\
                                                  Set parameters required for Daily Saving Time automatic handling.
                                            NOTE: StructNTP.UTCTime must have been initialized before calling this function.
\* ============================================================================================================================================================= */
void ntp_dst_settings(struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  UINT8 FlagNorth;   // indicate we are in a northern hemisphere country.
  UINT8 Loop1UInt8;
  UINT8 StartDoM;
  UINT8 EndDoM;
  
  UINT16 CurrentDayOfYear;

  struct human_time HumanTime;


  /* Validate DST country setting. */
  if (StructNTP->DSTCountry == 0)
  {
    if (stdio_usb_connected()) log_info(__LINE__, __func__, "Daylight saving time is currently disabled: %u\r\r\r", StructNTP->DSTCountry);
    StructNTP->FlagSummerTime = FLAG_OFF;
    return;
  }

  if (StructNTP->DSTCountry > MAX_DST_COUNTRIES)
  {
    if (stdio_usb_connected()) log_info(__LINE__, __func__, "Invalid DST country setting: %u\r\r\r", StructNTP->DSTCountry);
    StructNTP->FlagSummerTime = FLAG_OFF;
    return;
  }


  StructNTP->ShiftMinutes = DstParameters[StructNTP->DSTCountry].ShiftMinutes;



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                    Find day-of-week of Daylight Saving Time start for current year and for specific DST country.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Find the right day-of-week between the soonest and the latest possible dates. */
  for (Loop1UInt8 = DstParameters[StructNTP->DSTCountry].StartDayOfMonthLow; Loop1UInt8 <= DstParameters[StructNTP->DSTCountry].StartDayOfMonthHigh; ++Loop1UInt8)
  {
    /* Check if this date is the right day-of-week. If it is, find day-of-year for daylight saving time start. */
    if (ntp_get_day_of_week(Loop1UInt8, DstParameters[StructNTP->DSTCountry].StartMonth, StructNTP->HumanTime.Year) == DstParameters[StructNTP->DSTCountry].StartDayOfWeek)
    {
      StartDoM = Loop1UInt8;
      StructNTP->DoYStart = ntp_get_day_of_year(Loop1UInt8, DstParameters[StructNTP->DSTCountry].StartMonth, StructNTP->HumanTime.Year);
      break;
    }
  }

  /* Check if operation has been successful. */
  if (Loop1UInt8 > DstParameters[StructNTP->DSTCountry].StartDayOfMonthHigh)
  {
    log_info(__LINE__, __func__, "Date for daylight saving time start NOT FOUND\r\r\r");
    StructNTP->FlagSummerTime = FLAG_OFF;
    return;
  }
  else
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Date for daylight saving time start in %4.4u: %2.2u-%s-%4.4u\r", StructNTP->HumanTime.Year, Loop1UInt8, ShortMonth[DstParameters[StructNTP->DSTCountry].StartMonth], StructNTP->HumanTime.Year);
  }



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                    Find day-of-week of Daylight Saving Time end for current year and for specific DST country.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Find the right day-of-week between the soonest and the latest possible dates. */
  for (Loop1UInt8 = DstParameters[StructNTP->DSTCountry].EndDayOfMonthLow; Loop1UInt8 <= DstParameters[StructNTP->DSTCountry].EndDayOfMonthHigh; ++Loop1UInt8)
  {
    /* Check if this date is the right day-of-week. If it is, find day-of-year for daylight saving time end. */
    if (ntp_get_day_of_week(Loop1UInt8, DstParameters[StructNTP->DSTCountry].EndMonth, StructNTP->HumanTime.Year) == DstParameters[StructNTP->DSTCountry].EndDayOfWeek)
    {
      EndDoM = Loop1UInt8;
      StructNTP->DoYEnd = ntp_get_day_of_year(Loop1UInt8, DstParameters[StructNTP->DSTCountry].EndMonth, StructNTP->HumanTime.Year);
      break;
    }
  }

  /* Check if operation has been successful. */
  if (Loop1UInt8 > DstParameters[StructNTP->DSTCountry].EndDayOfMonthHigh)
  {
    log_info(__LINE__, __func__, "Date for daylight saving time end NOT FOUND\r\r\r");
    StructNTP->FlagSummerTime = FLAG_OFF;
    return;
  }
  else
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Date for daylight saving time end   in %4.4u: %2.2u-%s-%4.4u\r", StructNTP->HumanTime.Year, Loop1UInt8, ShortMonth[DstParameters[StructNTP->DSTCountry].EndMonth], StructNTP->HumanTime.Year);
  }



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                Check if DST country specified is for a northern or southern country.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  if (DstParameters[StructNTP->DSTCountry].StartMonth < DstParameters[StructNTP->DSTCountry].EndMonth)
  {
    FlagNorth = FLAG_ON;
    log_info(__LINE__, __func__, "Daylight saving time current parameters:\r");
    if (FlagLocalDebug)
      log_info(__LINE__, __func__, "Northern DST country: %u     Delta time with UTC: %d minutes     DST shift: %d minutes.\r", StructNTP->DSTCountry, StructNTP->DeltaTime, DstParameters[StructNTP->DSTCountry].ShiftMinutes);
  }
  else
  {
    FlagNorth = FLAG_OFF;
    if (FlagLocalDebug)
      log_info(__LINE__, __func__, "Southern DST country: %u     Delta time with UTC: %d minutes     DST shift: %d minutes.\r", StructNTP->DSTCountry, StructNTP->DeltaTime, DstParameters[StructNTP->DSTCountry].ShiftMinutes);
  }



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                  Find UTC time for DST start time and DST end time for current year.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Find and set UTC time corresponding to local DST start time. */
  HumanTime.FlagDst    = FLAG_OFF;
  HumanTime.DayOfWeek  = DstParameters[StructNTP->DSTCountry].StartDayOfWeek;
  HumanTime.DayOfMonth = StartDoM;
  HumanTime.DayOfYear  = ntp_get_day_of_year(StartDoM, DstParameters[StructNTP->DSTCountry].StartMonth, StructNTP->HumanTime.Year);
  HumanTime.Month      = DstParameters[StructNTP->DSTCountry].StartMonth;
  HumanTime.Year       = StructNTP->HumanTime.Year;
  HumanTime.Hour       = DstParameters[StructNTP->DSTCountry].StartHour;
  HumanTime.Minute     = 0;
  HumanTime.Second     = 0;

  /* Keep Day-of-year value for DST start. */
  StructNTP->DoYStart = HumanTime.DayOfYear;

  /* Convert "local human time" of DST start time to "Unix local time". */
  StructNTP->DSTStart = ntp_convert_human_to_unix(&HumanTime);

  /* "UTC time" for DST start time will be "DeltaTime" sooner or later, depending on local timezone. */
  /* NOTE: Since we are during "normal time" when Daylight Saving Time begins, there is no need to take into account the "ShiftMinutes" count. */
  StructNTP->DSTStart -= (StructNTP->DeltaTime * 60);



  /* Find and set UTC time corresponding to local DST end time. */
  HumanTime.FlagDst    = FLAG_OFF;
  HumanTime.DayOfWeek  = DstParameters[StructNTP->DSTCountry].EndDayOfWeek;
  HumanTime.DayOfMonth = EndDoM;
  HumanTime.DayOfYear  = ntp_get_day_of_year(EndDoM, DstParameters[StructNTP->DSTCountry].EndMonth, StructNTP->HumanTime.Year);
  HumanTime.Month      = DstParameters[StructNTP->DSTCountry].EndMonth;
  HumanTime.Year       = StructNTP->HumanTime.Year;
  HumanTime.Hour       = DstParameters[StructNTP->DSTCountry].EndHour;
  HumanTime.Minute     = 0;
  HumanTime.Second     = 0;

  /* Keep Day-of-year value for DST end. */
  StructNTP->DoYEnd = HumanTime.DayOfYear;

  /* Convert "local human time" of DST end time to "Unix local time". */
  StructNTP->DSTEnd = ntp_convert_human_to_unix(&HumanTime);

  /* "UTC time" for DST end time will be "DeltaTime" sooner or later, depending on local timezone. */
  StructNTP->DSTEnd -=  (StructNTP->DeltaTime * 60);

  /* Since we are during "summer time" when Daylight Saving Time ends, we must take into account the "ShiftMinutes" to get the correct UTC time. */
  StructNTP->DSTEnd -= (StructNTP->ShiftMinutes * 60);


  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                        Display parameters found for DST start and end time.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  log_info(__LINE__, __func__, "DST start date for %4.4u: %8s %2.2u-%s-%4.4u at %2.2u:00   day-of-year: %3u   UTC time: %llu\r",
            StructNTP->HumanTime.Year,
            DayName[DstParameters[StructNTP->DSTCountry].StartDayOfWeek],
            StartDoM, ShortMonth[DstParameters[StructNTP->DSTCountry].StartMonth], StructNTP->HumanTime.Year,
            DstParameters[StructNTP->DSTCountry].StartHour, StructNTP->DoYStart,
            StructNTP->DSTStart);

  log_info(__LINE__, __func__, "DST end   date for %4.4u: %8s %2.2u-%s-%4.4u at %2.2u:00   day-of-year: %3u   UTC time: %llu\r",
            StructNTP->HumanTime.Year,
            DayName[DstParameters[StructNTP->DSTCountry].EndDayOfWeek],
            EndDoM, ShortMonth[DstParameters[StructNTP->DSTCountry].EndMonth], StructNTP->HumanTime.Year,
            DstParameters[StructNTP->DSTCountry].EndHour, StructNTP->DoYEnd,
            StructNTP->DSTEnd);



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                  Update local time from UTC time in StructNTP.
              NOTE: This value can be wrong if the function is called during the one-hour period of the time change,
                    either from summer timer to winter time or vice versa... This will come back to normal after next reboot
                    or after next NTP update.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  StructNTP->LocalTime = StructNTP->UTCTime + ((INT64)(StructNTP->DeltaTime * 60));
  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "StructNTP->UTCTime:    %12llu\r",    StructNTP->UTCTime);
    log_info(__LINE__, __func__, "StructNTP->LocalTime:  %12llu\r",    StructNTP->LocalTime);
    log_info(__LINE__, __func__, "StructNTP->DeltaTime:        %6d minutes\r", StructNTP->DeltaTime);
  }



  // log_info(__LINE__, __func__, "StructNTP->UTCTime before accounting for summer time or winter time: %12llu\r", StructNTP->UTCTime);  ///

  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                Check if DST is currently On or Off.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Find if daylight saving time is active for a southern country. */
  if (FlagNorth == FLAG_ON)
  {
    /* Find for a northern country. */
    if ((StructNTP->UTCTime > StructNTP->DSTStart) && (StructNTP->UTCTime < StructNTP->DSTEnd))
    {
      StructNTP->FlagSummerTime = FLAG_ON;
      log_info(__LINE__, __func__, "DST settings is for a northern country and currently during daily saving time period of the year.\r");
    }
    else
    {
      StructNTP->FlagSummerTime = FLAG_OFF;
      log_info(__LINE__, __func__, "DST settings is for a northern country and currently not during daily saving time period of the year.\r");
    }
  }
  else
  {
    /* Find for a southern country. */
    if ((StructNTP->UTCTime > StructNTP->DSTEnd) && (StructNTP->UTCTime < StructNTP->DSTStart))
    {
      StructNTP->FlagSummerTime = FLAG_OFF;
      log_info(__LINE__, __func__, "DST settings is for a southern country and currently not during daily saving time period of the year.\r");
    }
    else
    {
      StructNTP->FlagSummerTime = FLAG_ON;
      log_info(__LINE__, __func__, "DST settings is for a southern country and currently during daily saving time period of the year.\r");
    }
  }

  if (StructNTP->FlagSummerTime) StructNTP->LocalTime += (StructNTP->ShiftMinutes * 60);

  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "StructNTP.LocalTime:   %12llu\r", StructNTP->LocalTime);
    log_info(__LINE__, __func__, "StructNTP.FlagSummerTime:      0x%2.2X\r", StructNTP->FlagSummerTime);
    log_info(__LINE__, __func__, "StructNTP.UTCTime:     %12llu\r", StructNTP->UTCTime);
  }

  return;
}





/* $PAGE */
/* $TITLE=ntp_failed_handler() */
/* ============================================================================================================================================================= *\
                                                                           NTP request failed.
\* ============================================================================================================================================================= */
static int64_t ntp_failed_handler(alarm_id_t AlarmId, void *ExtraArgument)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  struct struct_ntp *StructNTP;


  StructNTP = (struct struct_ntp *)ExtraArgument;

  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "Entering ntp_failed_handler()\r");
    log_info(__LINE__, __func__, "NTP request failed - AlarmId: %u.\r", AlarmId);
    log_info(__LINE__, __func__, "Pointer to StructNTP: 0x%p\r", StructNTP);
  }

  ntp_result(-1, NULL, StructNTP);

  return 0;
}





/* $PAGE */
/* $TITLE=ntp_get_day_of_week() */
/* ============================================================================================================================================================= *\
                                               Return the day-of-week for the specified date. Sunday =  (...) Saturday =
\* ============================================================================================================================================================= */
UINT8 ntp_get_day_of_week(UINT8 DayOfMonth, UINT8 Month, UINT16 Year)
{
  UINT8 DayOfWeek;
  UINT8 Table[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};


  Year -= Month < 3;
  DayOfWeek = ((Year + (Year / 4) - (Year / 100) + (Year / 400) + Table[Month - 1] + DayOfMonth) % 7);

  return DayOfWeek;;
}





/* $PAGE */
/* $TITLE=ntp_get_day_of_year() */
/* ============================================================================================================================================================= *\
                                                          Determine the day-of-year of date given in argument.
            NOTE: We shouldn't use log_info() in this function since the timestamp is not available when get_day_of_year() is called from ds3231_init()
\* ============================================================================================================================================================= */
UINT16 ntp_get_day_of_year(UINT8 DayOfMonth, UINT8 Month, UINT16 Year)
{
  UINT8 Loop1UInt8;
  UINT8 MonthDays;

  UINT16 TargetDayOfYear;


  /// if (DebugBitMask & DEBUG_NTP) printf("[%4u]   DayOfMonth %u   Month: %u   Year: %u\r", __LINE__, DayOfMonth, Month, Year);
  if ((Month < 1)    || (Month > 12))   return 0;
  if ((Year  < 2000) || (Year  > 2100)) Year = 2024;


  /* Initializations. */
  TargetDayOfYear = 0;

  /* Add up all complete months. */
  for (Loop1UInt8 = 1; Loop1UInt8 < Month; ++Loop1UInt8)
  {
    MonthDays = ntp_get_month_days(Loop1UInt8, Year);
    TargetDayOfYear += MonthDays;

    /// if (DebugBitMask & DEBUG_NTP)
    ///   printf("[%4u]   Adding month %2u [%3s]   Number of days: %2u   (cumulative: %3u)\r", __LINE__, Loop1UInt8, ShortMonth[Loop1UInt8], MonthDays, TargetDayOfYear);
  }

  /* Then add days of the last, partial month. */
  /// if (DebugBitMask & DEBUG_NTP)
  ///   printf("[%4u]   Final DayNumber after adding final partial month: (%u + %u) = %3u\r\r\r", __LINE__, TargetDayOfYear, DayOfMonth, TargetDayOfYear + DayOfMonth);

  TargetDayOfYear += DayOfMonth;

  return TargetDayOfYear;
}





/* $PAGE */
/* $TITLE=ntp_get_month_days() */
/* ============================================================================================================================================================= *\
                            Return the number of days of a specific month, given the specified year (to know if it is a leap year or not).
\* ============================================================================================================================================================= */
UINT8 ntp_get_month_days(UINT8 MonthNumber, UINT16 TargetYear)
{
  UINT8 NumberOfDays;


  switch (MonthNumber)
  {
    case (1):
    case (3):
    case (5):
    case (7):
    case (8):
    case (10):
    case (12):
      NumberOfDays = 31;
    break;

    case (4):
    case (6):
    case (9):
    case (11):
      NumberOfDays = 30;
    break;

    case 2:
      /* February, we must check if it is a leap year. */
      if (((TargetYear % 4 == 0) && (TargetYear % 100 != 0)) || (TargetYear % 400 == 0))
      {
        /* This is a leap year. */
        NumberOfDays = 29;
      }
      else
      {
        /* Not a leap year. */
        NumberOfDays = 28;
      }
    break;
  }

  return NumberOfDays;
}





/* $PAGE */
/* $TITLE=ntp_get_time() */
/* ============================================================================================================================================================= *\
                                                               Retrieve current UTC time from NTP server.
\* ============================================================================================================================================================= */
void ntp_get_time(struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  UCHAR Dum1Str[64];

  INT ReturnCode;

  UINT8 FlagRetry;

  INT64 DeltaTime;


  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "======================================================================\r");
    log_info(__LINE__, __func__, "                       Entering ntp_get_time()\r");
    // display_ntp_info();
  }


  if (StructNTP->FlagInit == FLAG_OFF) log_info(__LINE__, __func__, "ntp_init() has not already been done successfully. Aborting...\r");


  if ((StructNTP->FlagHealth) && (StructNTP->ScanCount < NTP_SCAN_FACTOR) && (!is_nil_time(StructNTP->UpdateTime)))
  {
    if (FlagLocalDebug)
    {
      log_info(__LINE__, __func__, "================================================================\r");
      log_info(__LINE__, __func__, "                           Poll cycle\r");
      log_info(__LINE__, __func__, "================================================================\r");
      // display_ntp_info();
    }

    StructNTP->UpdateTime = make_timeout_time_ms(NTP_REFRESH * 1000);
    StructNTP->FlagSuccess = FLAG_POLL;
    StructNTP->PollCycles++;
    StructNTP->ScanCount++;

    return;
  }


  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "======================================================================\r");
    log_info(__LINE__, __func__, "                              Read cycle\r");
    log_info(__LINE__, __func__, "======================================================================\r");
    // display_ntp_info(StructNTP);
  }

  StructNTP->UpdateTime = make_timeout_time_ms(NTP_REFRESH * 1000);
  StructNTP->ReadCycles++;
  StructNTP->ScanCount = 1;

  /* Set alarm in case udp requests are lost (10 seconds). */
  StructNTP->ResendAlarm = add_alarm_in_ms(NTP_RESEND_TIME, ntp_failed_handler, &StructNTP, true);

  /* NOTE: cyw43_arch_lwip_begin() / cyw43_arch_lwip_end() should be used around calls into LwIP to ensure correct locking.
           You can omit them if you are in a callback from LwIP. Note that when using pico_cyw_arch_poll library these calls
           are a no-op and can be omitted, but it is a good practice to use them in case you switch the cyw43_arch type later. */
  cyw43_arch_lwip_begin();
  {
    ReturnCode = dns_gethostbyname(NTP_SERVER, &StructNTP->ServerAddress, ntp_dns_found, StructNTP);
  }
  cyw43_arch_lwip_end();


  StructNTP->DNSRequestSent = true;
  if (FlagLocalDebug) log_info(__LINE__, __func__, "Request NTP server IP address from NTP pool: <%s>\r", NTP_SERVER);


  if (ReturnCode == 0)
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Cache DNS response.\r");
    ntp_request(StructNTP);  // cached result.
  }
  else
  {
    /* Other error codes. */
    switch (ReturnCode)
    {
      case (ERR_OK):
        /* ReturnCode = 0 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "No error, everything OK.\r");
      break;

      case (ERR_MEM):
        /* ReturnCode = -1 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Out of memory.\r");
      break;

      case (ERR_BUF):
        /* ReturnCode = -2 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Buffer error.\r");
      break;

      case (ERR_TIMEOUT):
        /* ReturnCode = -3 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Timeout.\r");
      break;

      case (ERR_RTE):
        /* ReturnCode = -4 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Routing problem.\r");
      break;

      case (ERR_INPROGRESS):
        /* ReturnCode = -5 */
        if (FlagLocalDebug)
        {
          log_info(__LINE__, __func__, "Request sent for an NTP server address. Return code: <ERR_INPROGRESS>.\r");
          log_info(__LINE__, __func__, "Waiting for callback.\r");
        }
        ntp_result(-1, NULL, StructNTP);
      break;

      case (ERR_VAL):
        /* ReturnCode = -6 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Illegal value.\r");
      break;

      case (ERR_WOULDBLOCK):
        /* ReturnCode = -7 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Operation would block.\r");
      break;

      case (ERR_USE):
        /* ReturnCode = -8 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Address in use.\r");
      break;

      case (ERR_ALREADY):
        /* ReturnCode = -9 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Already connecting.\r");
      break;

      case (ERR_ISCONN):
        /* ReturnCode = -10 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Connection already established.\r");
      break;

      case (ERR_CONN):
        /* ReturnCode = -11 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Not connected.\r");
      break;

      case (ERR_IF):
        /* ReturnCode = -12 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Low level netif error.\r");
      break;

      case (ERR_ABRT):
        /* ReturnCode = -13 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Connection aborted.\r");
      break;

      case (ERR_RST):
        /* ReturnCode = -14 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Connection reset.\r");
      break;

      case (ERR_CLSD):
        /* ReturnCode = -15 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Connection closed.\r");
      break;

      case (ERR_ARG):
        /* ReturnCode = -16 */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Illegal argument.\r");
      break;

      default:
        /* Unrecognized ReturnCode. */
        if (FlagLocalDebug) log_info(__LINE__, __func__, "Error: Unknown return code: %d\r", ReturnCode);
      break;
    }
  }

  return;
}





/* $PAGE */
/* $TITLE=ntp_init() */
/* ============================================================================================================================================================= *\
                                                                    Initialize NTP connection.
\* ============================================================================================================================================================= */
UINT8 ntp_init(struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  struct tm TmTime;


  if (StructNTP->FlagInit) log_info(__LINE__, __func__, "ntp_init() has already been called before. No action taken.\r");

  StructNTP->FlagSuccess    = FLAG_OFF;  // will be turned On after successful NTP answer.
  StructNTP->FlagHealth     = FLAG_OFF;  // will be set according to last NTP outcome (assume no NTP answer on entry).
  StructNTP->FlagHistory    = FLAG_OFF;  // will be set according to last NTP outcome (assume no NTP answer on entry).
  StructNTP->FlagInit       = FLAG_OFF;  // NTP has not already been initialized.
  StructNTP->FlagSummerTime = FLAG_OFF;  // assume we are not during summer time on entry and system will automatically adjust the right status.
  StructNTP->ScanCount      = 0;
  StructNTP->TotalErrors    = 0l;        // reset total number of NTP errors on entry.
  StructNTP->ReadCycles     = 0l;
  StructNTP->PollCycles     = 0l;        // reset number of NTP poll cycles on entry.
  StructNTP->UpdateTime     = nil_time;
  StructNTP->UTCTime        = (StructNTP->LocalTime - (StructNTP->DeltaTime * 60));


  StructNTP->Pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (StructNTP->Pcb == 0)
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Failed to create Pcb.\r");
    StructNTP->FlagInit = FLAG_OFF;
    return 1;
  }


  StructNTP->FlagInit = FLAG_ON;  // ntp_init() was successful.
  udp_recv(StructNTP->Pcb, ntp_recv, StructNTP);

  return 0;
}





/* $PAGE */
/* $TITLE=ntp_recv() */
/* ============================================================================================================================================================= *\
                                                                         NTP data received.
\* ============================================================================================================================================================= */
static void ntp_recv(void *ExtraArgument, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *IPAddress, u16_t port)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  UINT8 Mode;
  UINT8 SecondBuffer[4] = {0};
  UINT8 Stratum;

  UINT64 SecondsSince1900;
  UINT64 SecondsSince1970;

  time_t UnixTime;


  struct struct_ntp *StructNTP = ExtraArgument;
 
  StructNTP->Receive = time_us_32();

  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "Entering ntp_recv()\r");
    log_info(__LINE__, __func__, "ExtraArgument:                 0x%p\r",         ExtraArgument);
    log_info(__LINE__, __func__, "pcb pointer:                   0x%p\r",         pcb);
    log_info(__LINE__, __func__, "Mode:                                  %2u\r", (pbuf_get_at(p, 0) & 0x7));
    log_info(__LINE__, __func__, "Stratum:                               %2u\r", (pbuf_get_at(p, 1)));
    log_info(__LINE__, __func__, "NTP pool IP address:      %15s\r",              ip4addr_ntoa(IPAddress));
    log_info(__LINE__, __func__, "NTP server IP address:    %15s\r",              ip4addr_ntoa(&StructNTP->ServerAddress));
    log_info(__LINE__, __func__, "IP address compare:                    %2u\r", (ip_addr_cmp(IPAddress, &StructNTP->ServerAddress)));
    log_info(__LINE__, __func__, "Port:       %3u        NTP_PORT:      %3u\r",   port, NTP_PORT);
    log_info(__LINE__, __func__, "p->tot_len: %3u        NTP_MSG_LEN:   %3u\r",   p->tot_len, NTP_MSG_LEN);
  }


  /* Initializations. */
  SecondsSince1900 = 0ll;
  SecondsSince1970 = 0ll;


  Mode     = pbuf_get_at(p, 0) & 0x7;
  Stratum  = pbuf_get_at(p, 1);


  /* Check the result. */
  if (ip_addr_cmp(IPAddress, &StructNTP->ServerAddress) && (port == NTP_PORT) && (p->tot_len == NTP_MSG_LEN) && (Mode == 0x04) && (Stratum != 0))
  /// if (!ip_addr_cmp(IPAddress, &StructNTP->ServerAddress) && (port == NTP_PORT) && (p->tot_len == NTP_MSG_LEN) && (Mode == 0x04) && (Stratum != 0))
  {
    pbuf_copy_partial(p, SecondBuffer, sizeof(SecondBuffer), 40);
    /// StructNTP->Latency = absolute_time_diff_us(StructNTP->Send, StructNTP->Receive);
    StructNTP->Latency = StructNTP->Receive - StructNTP->Send;
    SecondsSince1900   = ((UINT64)SecondBuffer[0] << 24) | ((UINT64)SecondBuffer[1] << 16) | ((UINT64)SecondBuffer[2] << 8) | ((UINT64)SecondBuffer[3]);
    SecondsSince1970   = SecondsSince1900 - NTP_DELTA;
    UnixTime           = SecondsSince1970;

    if (FlagLocalDebug)
    {
      log_info(__LINE__, __func__, "Stratum:                                %u\r",                     Stratum);
      log_info(__LINE__, __func__, "Send timer:                    %10lu\r",                           StructNTP->Send);
      log_info(__LINE__, __func__, "Receive timer:                 %10lu\r",                           StructNTP->Receive);
      log_info(__LINE__, __func__, "Latency (round-trip):          %10ld msec  (one-way: %ld msec)\r", StructNTP->Latency, (StructNTP->Latency / 2));
      log_info(__LINE__, __func__, "Seconds since 1970:          %12llu\r",                            SecondsSince1970);
      // log_info(__LINE__, __func__, "Seconds between 1900 and 1970: %10lu\r",                          (UINT32)NTP_DELTA);
      // log_info(__LINE__, __func__, "Seconds since 1900:          %12llu\r",                            SecondsSince1900);
    }

    ntp_result(0, &UnixTime, StructNTP);
    /// printf("[%5u] - 10\r", __LINE__);
  }
  else
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Invalid ntp response\r");
    ntp_result(-1, NULL, StructNTP);
  }

  /// printf("[%5u] - 11\r", __LINE__);
  pbuf_free(p);

  return;
}




/* $PAGE */
/* $TITLE=ntp_request() */
/* ============================================================================================================================================================= *\
                                                                         Make an NTP request.
\* ============================================================================================================================================================= */
static void ntp_request(struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION


  if (FlagLocalDebug)
    log_info(__LINE__, __func__, "Entering ntp_request()\r");

  log_info(__LINE__, __func__, "NTP pool IP address:      %15s\r", ip4addr_ntoa(&StructNTP->ServerAddress));


  /* NOTE: cyw43_arch_lwip_begin() / cyw43_arch_lwip_end() should be used around calls into LwIP to ensure correct locking.
           You can omit them if you are in a callback from LwIP. Note that when using pico_cyw_arch_poll library these calls
           are a no-op and can be omitted, but it is a good practice to use them in case you switch the cyw43_arch type later. */
  cyw43_arch_lwip_begin();
  {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *)p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1B;
    udp_sendto(StructNTP->Pcb, p, &StructNTP->ServerAddress, NTP_PORT);
    StructNTP->Send = time_us_32();
    pbuf_free(p);
  }
  cyw43_arch_lwip_end();

  return;
}





/* $PAGE */
/* $TITLE=ntp_result() */
/* ============================================================================================================================================================= *\
                                                                  Called with results of operation.
\* ============================================================================================================================================================= */
void ntp_result(INT16 ResultStatus, time_t *UnixTime, struct struct_ntp *StructNTP)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_ON;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  struct tm TempTime;


  if (!stdio_usb_connected()) FlagLocalDebug = FLAG_OFF;

  if (FlagLocalDebug) log_info(__LINE__, __func__, "Entering ntp_result()   ResultStatus: %3d\r", ResultStatus);

  if ((ResultStatus == 0) && UnixTime)
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "ResultStatus: %d    UnixTime: %12llu\r", ResultStatus, *UnixTime);
    StructNTP->UTCTime     = *UnixTime;
    StructNTP->FlagSuccess = FLAG_ON;

    /* Proceed with first conversion of UTC time to determine current year. */
    ntp_convert_unix_time(StructNTP->UTCTime, &TempTime, StructNTP);

    /* Compute all DST parameters for current year. */
    ntp_dst_settings(StructNTP);

    /* Add DeltaTime to UTC time to get UTC equivalent of local time. */
    StructNTP->LocalTime = *UnixTime + (StructNTP->DeltaTime * 60);

    /* Determine current local time for winter time ("normal time") and then for current period of the year (summer time or winter time). */
    if (FlagLocalDebug)
    {
      log_info(__LINE__, __func__, "Unix time received from NTP server:                   %12llu\r",           StructNTP->UTCTime);
      log_info(__LINE__, __func__, "Delta time in minutes: StructNTP->DeltaTime:              %8d minutes\r",  StructNTP->DeltaTime);
      log_info(__LINE__, __func__, "Delta time in seconds: StructNTP->DeltaTime * 60:         %8d seconds\r", (StructNTP->DeltaTime * 60));
      log_info(__LINE__, __func__, "Unix time after adding delta time:                    %12llu\r",          (StructNTP->UTCTime + (StructNTP->DeltaTime * 60)));
      log_info(__LINE__, __func__, "Unix time after adjusting for DST period of the year: %12lld\r",           StructNTP->LocalTime + (StructNTP->FlagSummerTime * (StructNTP->ShiftMinutes * 60)));
    }

    StructNTP->LocalTime += (StructNTP->FlagSummerTime * (StructNTP->ShiftMinutes * 60));

    /* Convert UTC found to human time. */
    ntp_convert_unix_time(StructNTP->LocalTime, &TempTime, StructNTP);
  }
  else
  {
    StructNTP->FlagSuccess = FLAG_OFF;
    StructNTP->FlagHistory = FLAG_OFF;
    StructNTP->UpdateTime  = make_timeout_time_ms(NTP_RETRY * 1000);
  }

  if (StructNTP->ResendAlarm > 0)
  {
    if (FlagLocalDebug) log_info(__LINE__, __func__, "Cancelling alarm (0x%X)\r", StructNTP->ResendAlarm);
    cancel_alarm(StructNTP->ResendAlarm);  // removed for crash test only 
    StructNTP->ResendAlarm = 0;
  }

  if (FlagLocalDebug)
  {
    log_info(__LINE__, __func__, "Resetting DNSRequestSent\r");
    log_info(__LINE__, __func__, "======================================================================\r");
  }
  StructNTP->DNSRequestSent = false;

  return;
}

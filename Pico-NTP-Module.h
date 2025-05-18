/*
	Pico-NTP-Module.h

	Original code: Copyright (c) 2022 olav andrade; all rights reserved.

 */

/* ============================================================================================================================================================= *\
   Pico-NTP-Module.h
   Adapted as an "add-on module" for many other projects
   St-Louys, Andre - January 2024
   astlouys@gmail.com
   Revision 18-APR-2025
   Version 4.00

   REVISION HISTORY:
   =================
               1.00 - Initial version from Raspberry Pi (Trading) Ltd.
               cmake -DPICO_BOARD=pico_w -DPICO_STDIO_USB=1 -DWIFI_SSID=<NetworkName> -DWIFI_PASSWORD=<Password>
   18-FEB-2024 2.00 - Adapted for Pico-RGB-Matrix.
   17-AUG-2024 3.00 - Streamlined as a "library" for many other projects.
                    - Create a main "struct_ntp" containing all data to be shared with parent program.
   31-JAN-2025 4.00 - Add integrated support for Daylight Saving Time for most countries of the world.
\* ============================================================================================================================================================= */

#ifndef _NTP_MODULE_H
#define _NTP_MODULE_H

#include "baseline.h"
#include "pico/cyw43_arch.h"
#include "time.h"


/* --------------------------------------------------------------------------------------------------------------------------- *\
                                                      Language definitions.
\* --------------------------------------------------------------------------------------------------------------------------- */
#define LANGUAGE_LO_LIMIT 0
#define ENGLISH           0
#define CZECH             1
#define FRENCH            2
#define GERMAN            3
#define ITALIAN           4
#define SPANISH           5
#define LANGUAGE_HI_LIMIT 5
/* --------------------------------------------------------------------------------------------------------------------------- *\
                                                  End of Language definitions.
\* --------------------------------------------------------------------------------------------------------------------------- */

/* Select only one language. */
#define FIRMWARE_LANGUAGE   FRENCH
// #define FIRMWARE_LANGUAGE   ENGLISH


#if FIRMWARE_LANGUAGE == ENGLISH
#include "ntp-lang-english.h"
#endif

#if FIRMWARE_LANGUAGE == FRENCH
#include "ntp-lang-french.h"
#endif



#define FLAG_POLL               0x02

#define MAX_NTP_RETRIES            5   // number of times we try to get an answer from a NTP server.
#define MAX_NTP_CHECKS            10   // number of times we wait and check to get an answer from the callback.

#define NTP_DELTA         2208988800   // number of seconds between 01-JAN-1900 and 01-JAN-1970.
#define NTP_MSG_LEN               48
#define NTP_PORT                 123
#define NTP_REFRESH             3600
#define NTP_RESEND_TIME   (10 * 1000)
#define NTP_RETRY                600
#define NTP_SCAN_FACTOR           24
#define NTP_SERVER     "pool.ntp.org"
// #define NTP_SERVER     "north-america.pool.ntp.org"
// #define NTP_SERVER     "ca.pool.ntp.org"



/* --------------------------------------------------------------------------------------------------------------------------- *\
                                              Date and time related definitions.
\* --------------------------------------------------------------------------------------------------------------------------- */
#define H12  1  // time display mode is 12 hours.
#define H24  2  // time display mode is 24 hours.

#define SUN 0
#define MON 1
#define TUE 2
#define WED 3
#define THU 4
#define FRI 5
#define SAT 6



/* -------------------- DST_COUNTRY valid choices (see details in User Guide). -------------------- */
// #define DST_DEBUG                    // this define to be used only for intensive DST debugging purposes.
#define DST_LO_LIMIT        0           // this specific define only to make the logic easier in the code.
#define DST_NONE            0           // there is no "Daylight Saving Time" in user's country.
#define DST_AUSTRALIA       1           // daylight saving time for most of Australia.
#define DST_AUSTRALIA_HOWE  2           // daylight saving time for Australia - Lord Howe Island.
#define DST_CHILE           3           // daylight saving time for Chile.
#define DST_CUBA            4           // daylight saving time for Cuba.
#define DST_EUROPE          5           // daylight saving time for European Union.
#define DST_ISRAEL          6           // daylight saving time for Israel.
#define DST_LEBANON         7           // daylight saving time for Lebanon.
#define DST_MOLDOVA         8           // daylight saving time for Moldova.
#define DST_NEW_ZEALAND     9           // daylight saving time for New Zealand.
#define DST_NORTH_AMERICA  10           // daylight saving time for most of Canada and United States.
#define DST_PALESTINE      11           // daylight saving time for Palestine.
#define DST_PARAGUAY       12           // daylight saving time for Paraguay.IR_DISPLAY_GENERIC
#define DST_HI_LIMIT       13           // to make the logic easier in the firmware.



/* Structure to contain time stamp under "human" format instead of "tm" standard. */
struct human_time
{
  UINT8 FlagDst;
  UINT8 Hour;
  UINT8 Minute;
  UINT8 Second;
  UINT8 DayOfWeek;
  UINT8 DayOfMonth;
  UINT8 Month;
  UINT16 Year;
  UINT16 DayOfYear;
};


struct struct_ntp
{
  UINT8  FlagSuccess;            // flag indicating that NTP date and time request has succeeded.
  UINT8  FlagHealth;             // flag indicating health status of Network Time Protocol.
  UINT8  FlagInit;               // flag indicating if NTP initialization has been done with success.
  UINT8  FlagSummerTime;         // flag indicating if we are during Daylight Saving Time ("Summer time") or not.
  UINT8  FlagHistory;
	UINT8  ScanCount;
  UINT8  DSTCountry;             // host country (for DST handling purposes - see user guide).
  INT16  DeltaTime;              // local time difference with UTC time while in "normal time" period of the year.
  INT16  ShiftMinutes;           // number of minutes to shift between summer and winter time (summer is considered the reference).
  UINT16 DoYStart;               // day of year for daylight saving time start.
  UINT16 DoYEnd;                 // day of year for daylight saving time end.
  UINT64 DSTStart;               // UTC time when daylight saving time begins for target DST country.
  UINT64 DSTEnd;                 // UTC time when daylight saving time ends for target DST country.
  UINT32 TotalErrors;            // cumulative number of errors while trying to re-sync with NTP.
	UINT32 ReadCycles;
  UINT32 PollCycles;
  INT32  Latency;
  bool   DNSRequestSent;
  alarm_id_t       ResendAlarm;
  absolute_time_t  UpdateTime;
  /// absolute_time_t  Send;
  /// absolute_time_t  Receive;
  UINT32  Send;
  UINT32  Receive;
  ip_addr_t        ServerAddress;
  time_t           UTCTime;
  time_t           LocalTime;
  struct udp_pcb  *Pcb;
  struct human_time HumanTime;
};


#if 0
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
#endif  // 0
#define MAX_DST_COUNTRIES 12


/* Convert "HumanTime" to "tm_time". */
void ntp_convert_human_to_tm(struct human_time *HumanTime, struct tm *TmTime);

/* Convert "HumanTime" to "Unix Time". */
UINT64 ntp_convert_human_to_unix(struct human_time *HumanTime);

/* Convert "TmTime" to "Unix Time". */
UINT64 ntp_convert_tm_to_unix(struct tm *TmTime);

/* Convert Unix time to tm time and human time. */
void ntp_convert_unix_time(time_t UnixTime, struct tm *TmTime, struct struct_ntp *StructNTP);

/* Display NTP-related information. */
void ntp_display_info(struct struct_ntp *StructNTP);

/* Set parameters required for Daily Saving Time automatic handling. */
void ntp_dst_settings(struct struct_ntp *StructNTP);

/* Return the day-of-week for the specified date. Sunday =  (...) Saturday =  */
UINT8 ntp_get_day_of_week(UINT8 DayOfMonth, UINT8 Month, UINT16 Year);

/* Determine the day-of-year of date given in argument. */
UINT16 ntp_get_day_of_year(UINT8 DayOfMonth, UINT8 Month, UINT16 Year);

/* Return the number of days of a specific month, given the specified year (to know if it is a leap year or not). */
UINT8 ntp_get_month_days(UINT8 MonthNumber, UINT16 TargetYear);

/* Retrieve current utc time from NTP server. */
void ntp_get_time(struct struct_ntp *StructNTP);

/* Initialize variables require for NTP connection. */
UINT8 ntp_init(struct struct_ntp *StructNTP);

/* Called with results of operation. */
void ntp_result(INT16 ResultStatus, time_t *UnixTime, struct struct_ntp *StructNTP);

/* Send a string to external monitor through Pico UART (or USB CDC). */
extern void log_info(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...);

#endif  // _NTP_MODULE_H
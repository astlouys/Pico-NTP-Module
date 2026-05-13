/*
	Pico-NTP-Module.h

	Original code: Copyright (c) 2022 olav andrade; all rights reserved.

 */

/* ============================================================================================================================================================= *\
   Pico-NTP-Module.h
   Adapted as an "add-on module" for many other projects
   St-Louys, Andre - July 2025
   astlouys@gmail.com
   Revision 11-APR-2026
\* ============================================================================================================================================================= */

#ifndef PICO_NTP_MODULE_H
#define PICO_NTP_MODULE_H

#include "baseline.h"
#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"
#include "time.h"


/* ------------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                                       Language definitions.
\* ------------------------------------------------------------------------------------------------------------------------------------------------------------- */
#define LANGUAGE_LO_LIMIT 0
#define NTP_ENGLISH           0  // English is supported.
#define NTP_CZECH             1
#define NTP_FRENCH            2  // French is supported.
#define NTP_GERMAN            3
#define NTP_ITALIAN           4
#define NTP_SPANISH           5
#define LANGUAGE_HI_LIMIT 5
/* ------------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                                    End of Language definitions.
\* ------------------------------------------------------------------------------------------------------------------------------------------------------------- */

/* Select only one language (only English and English are supported for now). */
// #define NTP_LANGUAGE   NTP_FRENCH
#define NTP_LANGUAGE   NTP_ENGLISH


#if NTP_LANGUAGE == NTP_ENGLISH
#include "ntp-lang-english.h"
#endif

#if NTP_LANGUAGE == NTP_FRENCH
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



/* ------------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                                 Date and time related definitions.
\* ------------------------------------------------------------------------------------------------------------------------------------------------------------- */
#define H12  1  // time display format is 12 hours.
#define H24  2  // time display format is 24 hours.

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
  alarm_id_t        ResendAlarm;
  absolute_time_t   UpdateTime;
  UINT64            Send;
  UINT64            Receive;
  ip_addr_t         ServerAddress;
  time_t            UTCTime;
  time_t            LocalTime;
  struct udp_pcb   *Pcb;
  struct human_time HumanTime;
};

#define MAX_DST_COUNTRIES 13  // maximum number of timezones defined + 1 (to include 0 = undefined).

/* Convert "HumanTime" to "tm_time". */
void ntp_convert_human_to_tm(struct human_time *HumanTime, struct tm *TmTime);

/* Convert "HumanTime" to "Unix Time". */
UINT64 ntp_convert_human_to_unix(struct human_time *HumanTime, INT16 OffsetMinutes);

/* Convert "TmTime" to "Unix Time". */
UINT64 ntp_convert_tm_to_unix(struct tm *TmTime);

/* Convert Unix time to tm time and human time. */
void ntp_convert_unix_time(time_t UnixTime, struct tm *TmTime);

/* Display NTP-related information. */
void ntp_display_info(void);

/* Set parameters required for Daily Saving Time automatic handling. */
void ntp_dst_settings(void);

/* Return the day-of-week for the specified date. Sunday =  (...) Saturday =  */
UINT8 ntp_get_day_of_week(UINT8 DayOfMonth, UINT8 Month, UINT16 Year);

/* Determine the day-of-year of date given in argument. */
UINT16 ntp_get_day_of_year(UINT8 DayOfMonth, UINT8 Month, UINT16 Year);

/* Return the number of days of a specific month, given the specified year (to know if it is a leap year or not). */
UINT8 ntp_get_month_days(UINT8 MonthNumber, UINT16 TargetYear);

/* Retrieve current utc time from NTP server. */
void ntp_get_time(void);

/* Initialize variables require for NTP connection. */
UINT8 ntp_init(void);

/* Called with results of operation. */
void ntp_result(INT16 ResultStatus, time_t *UnixTime);

/* Send a string to external monitor through Pico UART (or USB CDC). */
extern void log_printf(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...);

#endif  // PICO_NTP_MODULE_H
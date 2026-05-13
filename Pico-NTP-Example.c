/* ============================================================================================================================================================= *\
   Pico-NTP-Example.c
   St-Louys Andre - May 2025
   astlouys@gmail.com
   https://github.com/astlouys/Pico-NTP-Module
   Revision 27-JULY-2025
   Langage: C
   Version 1.01

   Raspberry Pi Pico Firmware to test the Pico-NTP-Module.
   This firmware is very basic. Its main purpose is simply to show how to implement the Pico-NTP-Module to an existing project.


   NOTE:
   THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING USERS
   WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
   TIME. AS A RESULT, THE AUTHOR SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
   INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM
   THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY USERS OF THE CODING
   INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS. 


   REVISION HISTORY:
   =================
   17-MAY-2025 1.00 - Initial release as an "add-on module" to facilitate the addition of Network Time Protocol to an existing project.
   06-JUL-2025 1.01 - A few modifications to adapt it to the other members of the Pico-ASTL-Smarthome ecosystem.
                    - Replace all \r by \n (newline character).
                    - Retrieve often used functions from UTILITIES directory (get_pico_identifier(), input_string(), log_printf()).
                    - Add handling of real-time clock in log_printf() and disable core number in this firmware since we use only one core.
                    - Change the name of the function from uart_send() to log_info(), then to log_printf()
                    - Use common functions: get_pico_identifier(), input_string(), log_printf().
                    - Upgrade CMakeLists.txt to list more warnings, take care of environment variables, and be more verbose.
                    - Some cleanup and optimizations.
\* ============================================================================================================================================================= */



/* $PAGE */
/* $TITLE=Include files. */
/* ============================================================================================================================================================= *\
                                                                          Include files
\* ============================================================================================================================================================= */
#include "baseline.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/util/datetime.h"

#include "Pico-WiFi-Module.h"
#include "Pico-NTP-Module.h"



/* $PAGE */
/* $TITLE=Definitions and macros. */
/* ============================================================================================================================================================= *\
                                                                       Definitions and macros.
\* ============================================================================================================================================================= */
/// #define RELEASE_VERSION
#define WIFI_COUNTRY  CYW43_COUNTRY_CANADA
#define DST_COUNTRY     10  // must be setup by user: see User Guide.
#define DELTA_TIME    -300  // must be setup by user: time difference (in minutes) between UTC time and local time (always as of winter - "normal" - time).



/* $PAGE */
/* $TITLE=Global variables declaration / definition. */
/* ============================================================================================================================================================= *\
                                                             Global variables declaration / definition.
\* ============================================================================================================================================================= */
/* Complete day names. */
extern UCHAR DayName[7][13];

/* Short month names (3-letters). */
extern UCHAR ShortMonth[13][4];

struct struct_ntp  StructNTP;
struct struct_wifi StructWiFi;



/* $PAGE */
/* $TITLE=Function prototypes. */
/* ============================================================================================================================================================= *\
                                                                     Function prototypes.
\* ============================================================================================================================================================= */
/* Display "human time" whose pointer is given as a parameter. */
void display_human_time(UCHAR *Text, struct human_time *HumanTime);

/* Retrieve Pico's Unique ID from the flash IC. */
void get_pico_identifier(UCHAR *PicoUniqueId, UCHAR *PicoIdentifier, UINT8 *PicoType);

/* Read a string from stdin. */
void input_string(UCHAR *String, UINT16 StringSize, UINT32 TimeOut);

/* Send data to log file. */
void log_printf(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...);




/* $PAGE */
/* $TITLE=Main program entry point. */
/* ============================================================================================================================================================= *\
                                                                      Main program entry point.
\* ============================================================================================================================================================= */
int main()
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  UCHAR KeyStroke;
  UCHAR PicoIdentifier[40];
  UCHAR PicoUniqueId[25];

  UINT8 Delay;
  UINT8 PicoType;

  INT16 ReturnCode;

  UINT16 Loop1UInt16;

  datetime_t DateTime;            // real-time clock variable.



  /* Initialize stdin and stdout. */
  stdio_init_all();


  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                  Wait for USB CDC connection. PicoW will blink its LED while waiting. 
           Will loop until there is a valid USB CDC connection since the purpose is to synchronize Pico's real-time clock
           and then display date and time (it makes no sense if there is no terminal connection to display date and time).
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Wait for USB CDC connection (message will display only if already connected). */
  printf("[%5u] - Before delay, waiting for a USB CDC connection.\n", __LINE__);
  sleep_ms(1000);

  /* Wait until USB CDC connection is established. */
  Delay = 0;
  while (stdio_usb_connected() == 0)
  {
    ++Delay;  // one more 500 msec cycle waiting for USB CDC connection.
    wifi_blink(250, 250, 1);
  }


  /* Retrieve Pico's Unique ID from its flash memory, and its corresponding Identifier. */
  get_pico_identifier(PicoUniqueId, PicoIdentifier, &PicoType);

  log_printf(__LINE__, __func__, "cls");   // clear terminal emulator screen on entry.
  log_printf(__LINE__, __func__, "home");  // "home" terminal emulator cursor on entry.
  log_printf(__LINE__, __func__, "LOG MASK 0x11");  // turn Off date and time in log file on entry.
  log_printf(__LINE__, __func__, "==============================================================================================================\n");
  log_printf(__LINE__, __func__, "                                              Pico-NTP-Example\n");
  log_printf(__LINE__, __func__, "                                    Part of the ASTL Smart Home ecosystem.\n");
  log_printf(__LINE__, __func__, "                                    Pico unique ID: <%s>.\n", PicoUniqueId);

  log_printf(__LINE__, __func__, " ");
  for (Loop1UInt16 = 0; Loop1UInt16 < ((91 - strlen(PicoIdentifier)) / 2); ++Loop1UInt16)
    printf(" ");
  printf("Pico identifier: %s\n", PicoIdentifier);

  log_printf(__LINE__, __func__, "==============================================================================================================\n");
  log_printf(__LINE__, __func__, "Main program entry point (Delay: %u msec waiting for USB CDC connection).\n", (Delay * 50));



  /* Check if USB CDC connection has been detected.*/
  if (stdio_usb_connected()) log_printf(__LINE__, __func__, "USB Communications Device Class (CDC) connection has been detected.\n", __LINE__);


  /* Initialize CYW43 Wi-Fi module on PicoW. */
  StructWiFi.CountryCode = WIFI_COUNTRY;  // to set wi-fi frequencies allowed for target country.
  strcpy(StructWiFi.NetworkName,     WIFI_SSID);      // network name read from environment variable.
  strcpy(StructWiFi.NetworkPassword, WIFI_PASSWORD);  // network password read from environment variable.
  if (wifi_init())
  {
    log_printf(__LINE__, __func__, "Failed to initialize cyw43... aborting firmware.\n");
    return 1;
  }
  else
  {
    log_printf(__LINE__, __func__, "Cyw43 initialization successful.\n");
  }
  


  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                    Initialize Wi-Fi connection.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Initialize Wi-Fi connection. */
  log_printf(__LINE__, __func__, "Trying to establish a Wi-Fi connection with the following credentials:\n");
  log_printf(__LINE__, __func__, "Network name (SSID): <%s>\n",   StructWiFi.NetworkName);
  log_printf(__LINE__, __func__, "Network password:    <%s>\n\n", StructWiFi.NetworkPassword);
  ReturnCode = wifi_connect();
  if (ReturnCode == 0)
  {
    StructWiFi.FlagHealth = FLAG_ON;  // WiFi connection successful.
    wifi_display_info();
  }
  else
  {
    /* Failed to establish WiFi connection. */
    log_printf(__LINE__, __func__, "==============================================================\n");
    log_printf(__LINE__, __func__, "Wi-Fi connection couldn't be established (return code: %d)\n", ReturnCode);
    log_printf(__LINE__, __func__, "NTP server can't be reached... aborting firmware...\n");
    sleep_ms(1000);
    return 1;
  }


  if (StructWiFi.FlagHealth)
  {
    /* Wi-Fi connection has been successful, request time from NTP server. */
    /* --------------------------------------------------------------------------------------------------------------------------- *\
                                      Initialize variables required for NTP (Network Time Protocol)
                                              and then request UTC time from an NTP server.
    \* --------------------------------------------------------------------------------------------------------------------------- */
    /* Initialize NTP parameters. */
    StructNTP.FlagInit   = FLAG_OFF;     // will be automatically turned On when ntp_init() is called successfully.
    StructNTP.DSTCountry = DST_COUNTRY;  // origin country (see User Guide for details).
    StructNTP.DeltaTime  = DELTA_TIME;   // time difference between UTC time and local time (always as of winter - "normal" - time).


    ntp_init();
    if (StructNTP.FlagInit == FLAG_OFF)
    {
      log_printf(__LINE__, __func__, "Error while trying to initialize NTP (ntp_init() failed).\n");
      log_printf(__LINE__, __func__, "NTP server can't be reached... aborting firmware...\n");
      return 1;
    }


    while (StructNTP.FlagSuccess != FLAG_ON)
    {
      /* Retrieve UTC time from Network Time Protocol server. */
      ntp_get_time();

      /* Wait for NTP callback to return a result. */
      for (Loop1UInt16 = 1; Loop1UInt16 <= MAX_NTP_CHECKS; ++Loop1UInt16)
      {
        if (StructNTP.FlagSuccess == FLAG_POLL)
        {
          if (FlagLocalDebug)
          {
            log_printf(__LINE__, __func__, "\n\n\n\n");
            log_printf(__LINE__, __func__, "================================================================\n");
            log_printf(__LINE__, __func__, "            Variables after successful NTP poll (%u)\n", Loop1UInt16);
            ntp_display_info();
          }
          break;  // get out of "for" loop when result == FLAG_POLL.
        }


        if (StructNTP.FlagSuccess == FLAG_ON)
        {
          StructNTP.FlagHealth  = FLAG_ON;
          StructNTP.FlagHistory = StructNTP.FlagSuccess;

          if (FlagLocalDebug)
          {
            log_printf(__LINE__, __func__, "\n\n\n\n");
            log_printf(__LINE__, __func__, "======================================================================\n");
            log_printf(__LINE__, __func__, "               Variables after successful NTP read (%u)\n", Loop1UInt16);
            ntp_display_info();
            log_printf(__LINE__, __func__, "NTP read succeeded (Number of retries: %u)\n", Loop1UInt16);
          }
          break;  // get out of "for" loop when result == FLAG_ON.
        }

        wifi_blink(60, 400, Loop1UInt16);  // blink number of retries on Pico's LED.
        if (FlagLocalDebug)
        {
          log_printf(__LINE__, __func__, "Waiting for NTP... Loop count: %u   Status: %u\n", Loop1UInt16, StructNTP.FlagSuccess);
        }
        sleep_ms(400);  // slow down so that we can see current retry count through blinking LED.
       }


      if (FlagLocalDebug)
      {
        log_printf(__LINE__, __func__, "Out of NTP for loop... Loop count: %2u   Status: 0x%2.2X\n", Loop1UInt16, StructNTP.FlagSuccess);
      }


      /* If current NTP update request failed. */
      if (Loop1UInt16 >= MAX_NTP_CHECKS)
      {
        if (StructNTP.FlagHealth == FLAG_ON)
        {
          /* We increment error count only if previous health status was <good> and it is a "new" error. */
          ++StructNTP.TotalErrors;
        }
        StructNTP.FlagHistory = StructNTP.FlagSuccess;
        StructNTP.FlagHealth  = FLAG_OFF;
        StructNTP.UpdateTime  = nil_time;
        if (FlagLocalDebug)
        {
          log_printf(__LINE__, __func__, "\n\n\n\n");
          log_printf(__LINE__, __func__, "======================================================================\n");
          log_printf(__LINE__, __func__, "                  After failed NTP sync (%u retries)\n", Loop1UInt16);
          ntp_display_info();
        }

        log_printf(__LINE__, __func__, "The NTP server that has been allocated may be in problem.\n");
        log_printf(__LINE__, __func__, "Firmware will wait a few seconds and try again to retrieve time from a NTP server.\n");
        log_printf(__LINE__, __func__, "You may want to make a list of bad NTP servers allocated to your requests.\n\n");
        sleep_ms(5000);  // let some time to see error message on LCD display.
        log_printf(__LINE__, __func__, "Waiting 60 seconds before next retry.\n\n");
        sleep_ms(60000);
      }
    }
  }


  /* Set DST parameters. */
  ntp_dst_settings();



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                           Initialize Pico's real-time clock and set current time retrieve from NTP server.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Prepare Pico's real-time clock variable with values retrieved from NTP server. */
  DateTime.dotw  = StructNTP.HumanTime.DayOfWeek;
  DateTime.day   = StructNTP.HumanTime.DayOfMonth;
  DateTime.month = StructNTP.HumanTime.Month;
  DateTime.year  = StructNTP.HumanTime.Year;
  DateTime.hour  = StructNTP.HumanTime.Hour;
  DateTime.min   = StructNTP.HumanTime.Minute;
  DateTime.sec   = StructNTP.HumanTime.Second;

  log_printf(__LINE__, __func__, "Setting Pico's real-time clock with those parameters:\n");
  log_printf(__LINE__, __func__, "%s %u-%s-%4.4u   %2.2u:%2.2u:%2.2u\n", DayName[DateTime.dotw], DateTime.day, ShortMonth[DateTime.month], DateTime.year, DateTime.hour, DateTime.min, DateTime.sec);

  rtc_init();
  rtc_set_datetime(&DateTime);  // set current time on Pico's RTC.
  sleep_ms(5000);               // let some time for the real-time clock to be updated.

  /* Now that Pico's real-time clock has been updated, enable date and time display in log file. */
  log_printf(__LINE__, __func__, "LOG MASK 0x1D");  // enable log information: date, time.
  log_printf(__LINE__, __func__, "Now that Pico's real-time clock has been initialized and set, date and time are displayed in logged data.\n");
  log_printf(__LINE__, __func__, "DST start time for %4.4u: %llu\n",        StructNTP.HumanTime.Year, StructNTP.DSTStart);
  log_printf(__LINE__, __func__, "DST end   time for %4.4u: %llu\n",        StructNTP.HumanTime.Year, StructNTP.DSTEnd);


  log_printf(__LINE__, __func__, "Displaying real-time clock...\n");
  log_printf(__LINE__, __func__, "Press <Enter> to stop Firmware,\n");
  log_printf(__LINE__, __func__, "or <G> to restart Firmware,\n");
  log_printf(__LINE__, __func__, "or <ESC> to toggle Pico in upload mode.\n\n\n");


  while (1)
  {
    /* Display real-time clock on monitor screen. */
    rtc_get_datetime(&DateTime);  // retrieve current time from Pico's RTC.
    printf(" Current date and time: %s %u-%s-%4.4u   %2.2u:%2.2u:%2.2u\r", DayName[DateTime.dotw], DateTime.day, ShortMonth[DateTime.month], DateTime.year, DateTime.hour, DateTime.min, DateTime.sec);
    sleep_ms(100);

    /* If user pressed <ESC>, switch Pico in upload mode. */
    KeyStroke = getchar_timeout_us(100);
    switch (KeyStroke)
    {
      case (0x1B):
        /* User pressed <ESC>, toggle Pico in upload mode. */
        printf("\n\n");
        log_printf(__LINE__, __func__, "Switching Pico in upload mode.\n\n");
        reset_usb_boot(0l, 0l);
      break;

      case ('g'):
      case ('G'):
        /* User pressed <G>, restart Firmware. */
        printf("\n\n");
        log_printf(__LINE__, __func__, "Restarting Firmware.\n\n");
        watchdog_enable(1, 1);
      break;
      
      case (0x0D):
        /* User pressed <Enter>, exit Firmware. */
        printf("\n\n");
        log_printf(__LINE__, __func__, "Stopping Firmware.\n\n");
        return 0;
      break;
    }
  }
}

#include "get_pico_identifier.c"

#include "input_string.c"

#include "log_printf.c"

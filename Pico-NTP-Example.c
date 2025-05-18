/* ============================================================================================================================================================= *\
   Pico-NTP-Example.c
   St-Louys Andre - May 2025
   astlouys@gmail.com
   Revision 17-MAY-2025
   Langage: C with arm-none-eabi
   Version 1.00

   Raspberry Pi Pico Firmware to test the Pico-NTP-Module.
   This firmware is very basic. Its main purpose is simply to show how to implement the Pico-NTP-Module 
   in your own program / project in order to update your program clock from a NTP server over the Internet.


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
\* ============================================================================================================================================================= */


/* $PAGE */
/* $TITLE=Include files. */
/* ============================================================================================================================================================= *\
                                                                          Include files
\* ============================================================================================================================================================= */
#include "baseline.h"
#include "hardware/rtc.h"
#include "pico/bootrom.h"
#include "pico/util/datetime.h"
/// #include "hardware/clocks.h"
/// #include "hardware/vreg.h"
/// #include "hardware/watchdog.h"
/// #include "pico/cyw43_arch.h"
/// #include "pico/stdlib.h"
/// #include "ping.h"
/// #include "stdarg.h"
/// #include <stdio.h>

#include "Pico-WiFi-Module.h"
#include "Pico-NTP-Module.h"



/* $PAGE */
/* $TITLE=Definitions and macros. */
/* ============================================================================================================================================================= *\
                                                                       Definitions and macros.
\* ============================================================================================================================================================= */
/// #define RELEASE_VERSION
#define WIFI_COUNTRY  CYW43_COUNTRY_CANADA
#define DST_COUNTRY     10  // see User Guide.
#define DELTA_TIME    -300  // time difference between UTC time and local time (always as of winter - "normal" - time).
#define CURRENT_YEAR  2025  // will make a first approximation for current DST setting (summer time or winter time).



/* $PAGE */
/* $TITLE=Global variables declaration / definition. */
/* ============================================================================================================================================================= *\
                                                             Global variables declaration / definition.
\* ============================================================================================================================================================= */
/* Complete day names. */
extern UCHAR DayName[7][13];

/* Short month names (3-letters). */
extern UCHAR ShortMonth[13][4];



/* $PAGE */
/* $TITLE=Function prototypes. */
/* ============================================================================================================================================================= *\
                                                                     Function prototypes.
\* ============================================================================================================================================================= */
/* Display "human time" whose pointer is given as a parameter. */
void display_human_time(UCHAR *Text, struct human_time *HumanTime);

/* Retrieve Pico's Unique ID from the flash IC. */
void get_pico_unique_id(UCHAR *PicoUniqueId);

/* Read a string from stdin. */
void input_string(UCHAR *String);

/* Log data to log file. */
void log_info(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...);




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

  UCHAR PicoUniqueId[25];

  UINT8 Delay;
  UINT8 FlagTimeSet;

  INT16 ReturnCode;

  UINT16 Loop1UInt16;

  UINT64 UTCTime;                 // locally keep track of UTC time.

  struct human_time HumanTime;    // structure to contain time stamp under "human" format instead of "tm" standard.
  struct struct_ntp StructNTP;
  struct struct_wifi StructWiFi;

  /* Real-time clock variable. */
  datetime_t DateTime;



  /* Initialize stdin and stdout. */
  stdio_init_all();


  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                    Wait for CDC USB connection.
                                  PicoW will blink its LED while waiting for a CDC USB connection.
                               It will give up and continue after a while and continue with the code.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Wait for CDC USB connection (will display only if already connected). */
  printf("[%5u] - Before delay, waiting for a CDC USB connection.\r", __LINE__);
  sleep_ms(1000);

  /* Wait until CDC USB connection is established. */
  while (stdio_usb_connected() == 0)
  {
    ++Delay;  // one more 500 msec cycle waiting for USB CDC connection.
    wifi_blink(250, 250, 1);
  
    /* If we waited for more than 60 seconds for a USB CDC connection, get out of the loop and continue. */
    if (Delay > 120) break;
  }

  /* Retrieve Pico's Unique ID from its flash memory. */
  get_pico_unique_id(PicoUniqueId);

  log_info(__LINE__, __func__, "==============================================================================================================\r");
  log_info(__LINE__, __func__, "                                              Pico-NTP-Example\r");
  log_info(__LINE__, __func__, "                                    Part of the ASTL Smart Home ecosystem.\r");
  log_info(__LINE__, __func__, "                                    Pico unique ID: <%s>.\r", PicoUniqueId);
  log_info(__LINE__, __func__, "==============================================================================================================\r");
  log_info(__LINE__, __func__, "Main program entry point (Delay: %u msec waiting for CDC USB connection).\r", (Delay * 500));


  /* Check if USB CDC connection has been detected.*/
  if (stdio_usb_connected()) log_info(__LINE__, __func__, "USB CDC connection has been detected.\r", __LINE__);


  /* Initialize CYW43 Wi-Fi module on PicoW. */
  StructWiFi.CountryCode = WIFI_COUNTRY;  // to set wi-fi frequencies allowed for target country.
  if (wifi_init(&StructWiFi))
  {
    log_info(__LINE__, __func__, "Failed to initialize cyw43\r");
    return 1;
  }
  else
  {
    log_info(__LINE__, __func__, "Cyw43 initialization successful.\r");
  }
  


  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                    Initialize Wi-Fi connection.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Initialize Wi-Fi connection. */
  strcpy(StructWiFi.NetworkName,     WIFI_SSID);      // network name read from environment variable.
  strcpy(StructWiFi.NetworkPassword, WIFI_PASSWORD);  // network password read from environment variable.
  log_info(__LINE__, __func__, "Trying to establish a Wi-Fi connection with the following credentials:\r");
  log_info(__LINE__, __func__, "Network name (SSID): <%s>\r", StructWiFi.NetworkName);
  log_info(__LINE__, __func__, "Network password:    <%s>\r\r", StructWiFi.NetworkPassword);
  ReturnCode = wifi_connect(&StructWiFi);
  if (ReturnCode == 0)
  {
    StructWiFi.FlagHealth = FLAG_ON;  // WiFi connection successful.
    wifi_display_info(&StructWiFi);
  }
  else
  {
    /* Failed to establish WiFi connection. */
    log_info(__LINE__, __func__, "==============================================================\r");
    log_info(__LINE__, __func__, "   wifi_init(): Failed to establish a Wi-Fi connection (%d)\r\r\r", ReturnCode);
    log_info(__LINE__, __func__, "Since a Wi-Fi connection couldn't be established, a NTP server can't be reached...\r");
    log_info(__LINE__, __func__, "Aborting the Firmware...\r\r\r");
    sleep_ms(1000);
    return 1;
  }


  if (StructWiFi.FlagHealth)
  {
    /* Proceed with NTP only if Wi-Fi connection has been successful. */
    /* --------------------------------------------------------------------------------------------------------------------------- *\
                                      Initialize variables required for NTP (Network Time Protocol)
                                              and then request UTC time from an NTP server.
    \* --------------------------------------------------------------------------------------------------------------------------- */
    /* Initialize NTP parameters. */
    StructNTP.FlagInit    = FLAG_OFF;                // will be automatically turned On when ntp_init() is called successfully.
    StructNTP.DSTCountry  = DST_COUNTRY;             // origin country (see User Guide for details).
    StructNTP.DeltaTime   = DELTA_TIME;              // time difference between UTC time and local time (always as of winter - "normal" - time).
    ntp_init(&StructNTP);


    /* Set DST parameters. */
    /// StructNTP.HumanTime.Year = CURRENT_YEAR;  // to approximate current DST period of the year (winter time or summer time).
    /// ntp_dst_settings(&StructNTP);


    if (StructNTP.FlagInit == FLAG_OFF)
    {
      log_info(__LINE__, __func__, "Error while trying to initialize NTP (ntp_init() failed). By-passing NTP clock support.\r\r");
      goto ByPass1;
    }


    while (StructNTP.FlagSuccess != FLAG_ON)
    {
      /// ntp_display_info(&StructNTP);

      /* Retrieve UTC time from Network Time Protocol server. */
      ntp_get_time(&StructNTP);

      /* Wait for NTP callback to return a result. */
      for (Loop1UInt16 = 1; Loop1UInt16 <= MAX_NTP_CHECKS; ++Loop1UInt16)
      {
        if (StructNTP.FlagSuccess == FLAG_POLL)
        {
          if ((FlagLocalDebug) && (stdio_usb_connected()))
          {
            log_info(__LINE__, __func__, "\r\r\r\r");
            log_info(__LINE__, __func__, "================================================================\r");
            log_info(__LINE__, __func__, "            Variables after successful NTP poll (%u)\r", Loop1UInt16);
            ntp_display_info(&StructNTP);
          }
          break;  // get out of "for" loop when result == FLAG_POLL.
        }


        if (StructNTP.FlagSuccess == FLAG_ON)
        {
          StructNTP.FlagHealth  = FLAG_ON;
          StructNTP.FlagHistory = StructNTP.FlagSuccess;
          FlagTimeSet           = FLAG_ON;
          UTCTime               = StructNTP.UTCTime;  // keep a local copy of UTC time.

          /* Display Unix time received from NTP server. */
          if (stdio_usb_connected()) log_info(__LINE__, __func__, "Current Unix time returned from NTP server: %llu\r", StructNTP.UTCTime);

          if (FlagLocalDebug)
          {
            log_info(__LINE__, __func__, "\r\r\r\r");
            log_info(__LINE__, __func__, "======================================================================\r");
            log_info(__LINE__, __func__, "               Variables after successful NTP read (%u)\r", Loop1UInt16);
            ntp_display_info(&StructNTP);
            log_info(__LINE__, __func__, "NTP read succeeded (Number of retries: %u)\r", Loop1UInt16);
          }
          break;  // get out of "for" loop when result == FLAG_ON.
        }

        wifi_blink(60, 400, Loop1UInt16);
        if (FlagLocalDebug)
        {
          log_info(__LINE__, __func__, "Waiting for NTP... Loop count: %u   Status: %u\r", Loop1UInt16, StructNTP.FlagSuccess);
        }
        sleep_ms(400);  // slow down so that we can see current retry count through blinking LED.
      }


      if (FlagLocalDebug)
      {
        log_info(__LINE__, __func__, "Out of NTP for loop... Loop count: %2u   Status: 0x%2.2X\r", Loop1UInt16, StructNTP.FlagSuccess);
      }


      /* If current NTP update request failed. */
      if (Loop1UInt16 >= MAX_NTP_CHECKS)
      {
        FlagTimeSet = FLAG_OFF;
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
          log_info(__LINE__, __func__, "\r\r\r\r");
          log_info(__LINE__, __func__, "======================================================================\r");
          log_info(__LINE__, __func__, "                  After failed NTP sync (%u retries)\r", Loop1UInt16);
          ntp_display_info(&StructNTP);
        }

        sleep_ms(5000);  // let some time to see error message on LCD display.
        log_info(__LINE__, __func__, "The NTP server that has been allocated may be in problem.\r");
        log_info(__LINE__, __func__, "You may want to restart the Firmare to get another NTP server\r");
        log_info(__LINE__, __func__, "and / or make a list of bad servers to clarify the problems.\r\r");
        return 1;
      }
    }
ByPass1:
  }


  /* Set DST parameters. */
  ntp_dst_settings(&StructNTP);



  /* --------------------------------------------------------------------------------------------------------------------------- *\
                                                Initialize Pico's real-time clock.
  \* --------------------------------------------------------------------------------------------------------------------------- */
  /* Prepare Pico's real-time clock variable with values retrieved from NTP server. */
  DateTime.dotw  = StructNTP.HumanTime.DayOfWeek;
  DateTime.day   = StructNTP.HumanTime.DayOfMonth;
  DateTime.month = StructNTP.HumanTime.Month;
  DateTime.year  = StructNTP.HumanTime.Year;
  DateTime.hour  = StructNTP.HumanTime.Hour;
  DateTime.min   = StructNTP.HumanTime.Minute;
  DateTime.sec   = StructNTP.HumanTime.Second;

  log_info(__LINE__, __func__, "Setting Pico's real-time clock with those parameters:\r");
  log_info(__LINE__, __func__, "%s %u-%s-%4.4u   %2.2u:%2.2u:%2.2u\r", DayName[DateTime.dotw], DateTime.day, ShortMonth[DateTime.month], DateTime.year, DateTime.hour, DateTime.min, DateTime.sec);

  rtc_init();
  rtc_set_datetime(&DateTime);  // set current time on Pico's RTC.
  sleep_ms(5000);                // let some time for the real-time clock to be updated.
  log_info(__LINE__, __func__, "DST start time for %4.4u: %llu\r",        StructNTP.HumanTime.Year, StructNTP.DSTStart);
  log_info(__LINE__, __func__, "DST end   time for %4.4u: %llu\r",        StructNTP.HumanTime.Year, StructNTP.DSTEnd);


  log_info(__LINE__, __func__, "Displaying real-time clock now...\r");
  log_info(__LINE__, __func__, "You probably need to change your terminal setting to display the clock...\r");
  log_info(__LINE__, __func__, "<CR-LF> translation should be <CR> only.\r");
  log_info(__LINE__, __func__, "You may press <ESC> at any time to toggle the Pico in upload mode.\r\r\r");


  while (1)
  {
    /* Display real-time clock on monitor screen. */
    rtc_get_datetime(&DateTime);  // retrieve current time from Pico's RTC.
    printf("Current date and time: %s %u-%s-%4.4u   %2.2u:%2.2u:%2.2u\r", DayName[DateTime.dotw], DateTime.day, ShortMonth[DateTime.month], DateTime.year, DateTime.hour, DateTime.min, DateTime.sec);
    sleep_ms(900);

    /* If user pressed <ESC>, switch Pico in upload mode. */
    if (getchar_timeout_us(100) == 0x1B)
    {
      /* Switch Pico in upload mode. */
      printf("\r\r");
      log_info(__LINE__, __func__, "Switching Pico in upload mode.\r\r");
      reset_usb_boot(0l, 0l);
    }
  }
}





/* $PAGE */
/* $TITLE=display_human_time() */
/* ============================================================================================================================================================= *\
                                                  Display "human time" whose pointer is given as a parameter.
\* ============================================================================================================================================================= */
void display_human_time(UCHAR *Text, struct human_time *HumanTime)
{
  UINT8 FlagValid = FLAG_ON;


  /* Make minimal validations to prevent a crash. */
  if (HumanTime->DayOfWeek > 6) FlagValid = FLAG_OFF;
  if ((HumanTime->Month < 1) || (HumanTime->Month > 12)) FlagValid = FLAG_OFF;

  if (FlagValid == FLAG_ON)
    log_info(__LINE__, __func__, "%s %8s   %2.2u-%3s-%4u   %2.2u:%2.2u:%2.2u   (DoY: %3u   DST: 0x%2.2X)\r\r", Text, DayName[HumanTime->DayOfWeek], HumanTime->DayOfMonth, ShortMonth[HumanTime->Month], HumanTime->Year, HumanTime->Hour, HumanTime->Minute, HumanTime->Second, HumanTime->DayOfYear, HumanTime->FlagDst);
  else
    log_info(__LINE__, __func__, "%s DoW:%u   %2.2u-%2.2u-%4u   %2.2u:%2.2u:%2.2u   (DoY: %3u   DST: %2.2X)\r\r", Text, HumanTime->DayOfWeek, HumanTime->DayOfMonth, HumanTime->Month, HumanTime->Year, HumanTime->Hour, HumanTime->Minute, HumanTime->Second, HumanTime->DayOfYear, HumanTime->FlagDst);

  return;
}




/* $PAGE */
/* $TITLE=get_pico_unique_id() */
/* ============================================================================================================================================================= *\
                                                           Retrieve Pico's Unique ID from the flash IC.
\* ============================================================================================================================================================= */
void get_pico_unique_id(UCHAR *PicoUniqueId)
{
  UINT8 Loop1UInt8;

  pico_unique_board_id_t board_id;


  /* Retrieve Pico Unique ID from its flash memory IC. */
  pico_get_unique_board_id(&board_id);

  /* Build the Unique ID string in hex. */
  PicoUniqueId[0] = 0x00;  // initialize as null string on entry.
  for (Loop1UInt8 = 0; Loop1UInt8 < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++Loop1UInt8)
  {
    // log_info(__LINE__, __func__, "%2u - 0x%2.2X\r", Loop1UInt8, board_id.id[Loop1UInt8]);
    sprintf(&PicoUniqueId[strlen(PicoUniqueId)], "%2.2X", board_id.id[Loop1UInt8]);
    if ((Loop1UInt8 % 2) && (Loop1UInt8 != 7)) sprintf(&PicoUniqueId[strlen(PicoUniqueId)], "-");
  }

  return;
}





/* $PAGE */
/* $TITLE=input_string() */
/* ============================================================================================================================================================= *\
                                                                    Read a string from stdin.
\* ============================================================================================================================================================= */
void input_string(UCHAR *String)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must remain OFF all time.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // may be modified for debug purposes.
#endif  // RELEASE_VERSION

  INT8 DataInput;

  UINT8 Loop1UInt8;

  UINT32 IdleTimer;


  if (FlagLocalDebug) printf("Entering input_string().\r");

  Loop1UInt8 = 0;
  IdleTimer  = time_us_32();  // initialize time-out timer with current system timer.
  do
  {
    DataInput = getchar_timeout_us(50000);

    switch (DataInput)
    {
      case (PICO_ERROR_TIMEOUT):
      case (0):
#if 0
        /* This section if we want input_string() to return after a timeout wait time. */ 
        if ((time_us_32() - IdleTimer) > 300000000l)
        {
          printf("[%5u] - Input timeout %lu - %lu = %lu!!\r\r\r", __LINE__, time_us_32(), IdleTimer, time_us_32() - IdleTimer);
          String[0]  = 0x1B;  // time-out waiting for a keystroke.
          Loop1UInt8 = 1;     // end-of-string will be added when exiting while loop.
          DataInput  = 0x0D;
        }
#endif  // 0
        continue;
      break;

      case (8):
        /* <Backspace> */
        IdleTimer = time_us_32();  // restart time-out timer.
        if (Loop1UInt8 > 0)
        {
          --Loop1UInt8;
          String[Loop1UInt8] = 0x00;
          printf("%c %c", 0x08, 0x08);  // erase character under the cursor.
        }
      break;

      case (27):
        /* <ESC> */
        IdleTimer = time_us_32();  // restart time-out timer.
        if (Loop1UInt8 == 0)
        {
          String[Loop1UInt8++] = (UCHAR)DataInput;
          String[Loop1UInt8++] = 0x00;
        }
        printf("\r");
      break;

      case (0x0D):
        /* <Enter> */
        IdleTimer = time_us_32();  // restart time-out timer.
        if (Loop1UInt8 == 0)
        {
          String[Loop1UInt8++] = (UCHAR)DataInput;
          String[Loop1UInt8++] = 0x00;
        }
        printf("\r");
      break;

      default:
        IdleTimer = time_us_32();  // restart time-out timer.
        printf("%c", (UCHAR)DataInput);
        String[Loop1UInt8] = (UCHAR)DataInput;
        // printf("Loop1UInt8: %3u   %2.2X - %c\r", Loop1UInt8, DataInput, DataInput);  /// for debugging purposes.
        ++Loop1UInt8;
      break;
    }
    sleep_ms(10);
  } while((Loop1UInt8 < 128) && (DataInput != 0x0D));

  String[Loop1UInt8] = '\0';  // end-of-string
  /// printf("\r\r\r");

  /* Optionally display each character entered. */
  /***
  for (Loop1UInt8 = 0; Loop1UInt8 < 10; ++Loop1UInt8)
    printf("%2u:[%2.2X]   ", Loop1UInt8, String[Loop1UInt8]);
  printf("\r");
  ***/

  if (FlagLocalDebug) printf("Exiting input_string().\r");

  return;
}





/* $PAGE */
/* $TITLE=log_info()) */
/* ============================================================================================================================================================= *\
                                                                        Log data to log file.
\* ============================================================================================================================================================= */
void log_info(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...)
{
  UCHAR Dum1Str[256];
  UCHAR TimeStamp[128];

  UINT Loop1UInt;
  UINT StartChar;

  va_list argp;


  /* Transfer the text to print to variable Dum1Str */
  va_start(argp, Format);
  vsnprintf(Dum1Str, sizeof(Dum1Str), Format, argp);
  va_end(argp);

  /* Trap special control code for <HOME>. Replace "home" by appropriate control characters for "Home" on a VT101. */
  if (strcmp(Dum1Str, "home") == 0)
  {
    Dum1Str[0] = 0x1B; // ESC code
    Dum1Str[1] = '[';
    Dum1Str[2] = 'H';
    Dum1Str[3] = 0x00;
  }

  /* Trap special control code for <CLS>. Replace "cls" by appropriate control characters for "Clear screen" on a VT101. */
  if (strcmp(Dum1Str, "cls") == 0)
  {
    Dum1Str[0] = 0x1B; // ESC code
    Dum1Str[1] = '[';
    Dum1Str[2] = '2';
    Dum1Str[3] = 'J';
    Dum1Str[4] = 0x00;
  }

  /* Time stamp will not be printed if first character is a '-' (for title line when starting debug, for example),
     or if first character is a line feed '\r' when we simply want add line spacing in the debug log,
     or if first character is the beginning of a control stream (for example 'Home' or "Clear screen'). */
  if ((Dum1Str[0] != '-') && (Dum1Str[0] != '\r') && (Dum1Str[0] != 0x1B) && (Dum1Str[0] != '|'))
  {
    /* Send line number through UART. */
    printf("[%7u] - ", LineNumber);

    /* Display function name. */
    printf("[%s]", FunctionName);
    for (Loop1UInt = strlen(FunctionName); Loop1UInt < 25; ++Loop1UInt)
    {
      printf(" ");
    }
    printf("- ");


    /* Retrieve current time stamp. */
    // date_stamp(TimeStamp);

    /* Send time stamp through UART. */
    // printf(TimeStamp);
  }

  /* Send string through stdout. */
  printf(Dum1Str);

  return;
}






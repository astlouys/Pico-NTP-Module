/* $PAGE */
/* $TITLE=log_printf() */
/* Updated 25-FEB-2026 */
/* ============================================================================================================================================================= *\
                                                                       Print a string to log file.
   NOTE: If the leftmost part of the string to log corresponds to "LOG MASK", the data to the right will be decoded as an UINT16 hex number defining
         the bit mask of the extra parameters to print to log file. These parameters are:
         0x0001 = LOG_LINE     (Line number of the caller source code).
         0x0002 = LOG_CORE     (Core number of the caller).
         0x0004 = LOG_FUNCTION (Function name of the caller).
         0x0008 = LOG_DATE     (Date stamp - if it is available).
         0x0010 = LOG_TIME     (Time stamp - if it is available).
         0x0020 = Reserved.
         0x0040 = Reserved.
         0x0080 = Reserved.

   For example:
   log_printf(__LINE__, __func__, "LOG MASK %X", LOG_LINE + LOG_CORE + LOG_FUNCTION);  // Add: line number, core number and function name in log file.



   If the first characters of the string is a number enclosed inside open-angle-brackets '<xxx>', the string that follows will be centered in a line of the
   number of characters specified.

   For example:
   log_printf(__LINE__, __func__, "<120>Firmware compatible with ASTL Smart Home ecosystem.\n");
\* ============================================================================================================================================================= */
void log_printf(UINT LineNumber, const UCHAR *FunctionName, UCHAR *Format, ...)
{
#ifdef RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // must be turned OFF at all times.
#else   // RELEASE_VERSION
  UINT8 FlagLocalDebug = FLAG_OFF;  // may be turned ON for debug purposes.
#endif  // RELEASE_VERSION

  UCHAR Dum1Str[512];
  UCHAR Dum2Str[512];

  UINT FunctionSize = 25;  // specify space reserved to display function name including the two "[]". A tilde <~> will be append if function name has been truncated.
  UINT Loop1UInt;
  UINT Loop2UInt;
  UINT LineSize;

  static UINT16 LogMask = 0x13;  // bitmask of parameters to display along with text to log (Line number and Function name turned On by default):
                                 // 0x0001 = LOG_LINE     (Line number).
                                 // 0x0002 = LOG_CORE     (Core number).
                                 // 0x0004 = LOG_TIME     (Time - when real-time clock is available on target system).
                                 // 0x0008 = LOG_DATE     (Date - when real-time clock is available on target system).
                                 // 0x0010 = LOG_FUNCTION (Function name of caller function).
                                 // 0xFFFF = LOG_ALL      (All available extra log information).

  datetime_t LogTime;  // real-time clock variable.  ///// comment out this line (and other lines related to time stamping) if project does not allow time stamping.

  va_list argp;


  /* If there is no terminal connected, bypass the display. */
  if (!stdio_usb_connected()) return;

  /* Transfer the text to print to working variables Dum1Str and Dum2Str. */
  va_start(argp, Format);
  vsnprintf(Dum1Str, sizeof(Dum1Str), Format, argp);
  va_end(argp);
  strcpy(Dum2Str, Dum1Str);
  if (FlagLocalDebug) printf("[%5u] - Log string on entry: <%s> <%s>\n", __LINE__, Dum1Str, Dum2Str);


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                       Handling of <LOG MASK>: special tag to determine extra parameters to print to log file.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  if ((!strncmp(Dum2Str, "LOG MASK", 8)) || (!strncmp(Dum2Str, "log mask", 8)))
  {
    if (FlagLocalDebug) printf("[%5u] - Entering <LOG MASK> string decoding with value string: <%s>\n", __LINE__, &Dum2Str[9]);
    LogMask = strtol(&Dum2Str[9], NULL, 16);  // decode hex value sent for the <extra> parameters mask to print on each log line.
    if (FlagLocalDebug) printf("[%5u] - Decoded  <LOG MASK> hex value: 0x%2.2X\n", __LINE__, LogMask);
    return;
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                              Handling of <HOME> special control code.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /* Trap special control code for <HOME>. Replace "home" by appropriate control code for "Home" on a VT101 compatible terminal. */
  if ((!strcmp(Dum2Str, "home")) || (!strcmp(Dum2Str, "HOME")))
  {
    if (FlagLocalDebug) printf("[%5u] - Entering <HOME> tag decoding: <%s>\n", __LINE__, Dum2Str);
    /* Escape code for "home". */
    Dum2Str[0] = 0x1B;
    Dum2Str[1] = '[';
    Dum2Str[2] = 'H';
    Dum2Str[3] = 0x00;
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                                Handling of <CLS> special control code.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  /* Trap special control code for <CLS>. Replace "cls" by appropriate control code for "Clear screen" on a VT101 compatible terminal. */
  if ((!strcmp(Dum2Str, "cls")) || (!strcmp(Dum2Str, "CLS")))
  {
    if (FlagLocalDebug) printf("[%5u] - Entering <CLS> tag decoding: <%s>\n", __LINE__, Dum2Str);
    /* Since <cls> does not erase the terminal log, skip a few lines after the previous log session. */
    for (Loop1UInt = 0; Loop1UInt < 80; ++Loop1UInt) printf("\n");  // leave a blank page between previous log session and new one.

    /* Escape code for "cls". */
    Dum2Str[0] = 0x1B;
    Dum2Str[1] = '[';
    Dum2Str[2] = '2';
    Dum2Str[3] = 'J';
    Dum2Str[4] = 0x00;
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                     If first character is a "new line" character or an escape code, send the raw text only (or the escape code) to log file.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  if ((Dum2Str[0] == '\n') || (Dum2Str[0] == 0x1B))
  {
    if (FlagLocalDebug) printf("[%5u] - Sending raw data to log file <%c>.\n", __LINE__, Dum2Str[2]);
    printf(Dum2Str);
    return;
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                      Extra parameters: Optionally display source code line number and caller Pico's core number.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  if (LogMask & (LOG_LINE + LOG_CORE))  // souce code line number and / or core number.
  {
    printf("[");
    if (LogMask & LOG_LINE)
    {
      printf("%5u", LineNumber);  // note: if program is longer than 99999 lines of code, [%5u] could be replaced by [%6u] to keep everything properly aligned.
      if (LogMask & LOG_CORE) printf(" ");  // space separator.
    }
    if (LogMask & LOG_CORE) printf("%u", get_core_num());
    printf("] ");
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                     Extra parameters: Optionally display date and time stamp.
                             NOTE: Pico's real-time clock must have been initialized for date and time stamp to display valid values.
                                   You may want to take a look at my repository "Pico-WiFi-Module" and the file Pico-WiFi-Example 
                                   to see an example on using Pico's internal real-time clock.
                             NOTE: The Pico2 and Pico2W do not have an internal real-time clock. If you use one of those, you will have to
                                   implement your own real-time clock using a callback function with the appropriate clock handling functions.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
// #if 0  // Uncomment this line if your project does not allow date stamping (to prevent error messages for undefined date and time related functions / variables).
  if (LogMask & (LOG_TIME + LOG_DATE))
  {
    rtc_get_datetime(&LogTime);  // retrieve current time from Pico's RTC.
    /***
    printf("[%5u %u] LogTime retrieve in log_info() - Day: %u   Month: %u   Year: %u     Hour: %u   Minute: %u   Second: %u\n", 
           __LINE__, get_core_num(),
           LogTime.day,  LogTime.month, LogTime.year,
           LogTime.hour, LogTime.min,   LogTime.sec);
    ***/
    printf("[");

    if (LogMask & LOG_DATE)
    {
      /* Display date. */
      printf("%2.2d-%s-%2.2d", LogTime.day, ShortMonth[LogTime.month], LogTime.year);
      if (LogMask & LOG_TIME) printf("  ");  // separator.
    }
    if (LogMask & LOG_TIME)
    {
      /* Display time. */
      printf("%2.2d:%2.2d:%2.2d", LogTime.hour, LogTime.min, LogTime.sec);
    }
    printf("] ");
  }
// #endif  // 0   // Uncomment this line if your project does not allow date and time stamping.


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                                                         Extra parameters: Optionally display function name.
                        NOTE: Function names will be aligned in the log file no matter what their length is. The space reserved for function
                              names is defined at the top of the current function. If a function name is too long to fit, it will be truncated and a
                              tilde <~> will be added at the end of the printed function part. It should be relatively easy to determine the function
                              name based on the first x characters printed.
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  if (LogMask & LOG_FUNCTION)
  {
    /* Print function name and align all function names in log file (if function name is too long, it will be truncated and a tilde <~> added at the end). */
    sprintf(Dum2Str, "[%s]", FunctionName);

    /* Check if function name is too long for a clean format in the log file. */
    if (strlen(Dum2Str) > FunctionSize)
    {
      if (FlagLocalDebug) printf("Function name too long: Dum2Str > %u: <%s>\n", FunctionSize, Dum2Str);
      Dum2Str[FunctionSize - 2] = '~';   // tilde indicating function name has been truncated.
      Dum2Str[FunctionSize - 1] = ']';   // truncate function name length when it is too long.
      Dum2Str[FunctionSize]     = '\0';  // add an end-of-string.

      /***
      if (FlagLocalDebug) 
      {
        // Optionally print the ASCII characters representing the function name.
        printf("Loop %u = ");
        for (Loop1UInt = 0; Loop1UInt < 20; ++Loop1UInt)
          printf("%2u   ", Dum2Str[Loop1UInt]);
      }
      ***/
    }
    printf("%s", Dum2Str);  // display caller's function name.

    /* Pad function name with blanks when it is shorter than maximum length. */
    for (Loop1UInt = strlen(Dum2Str); Loop1UInt < FunctionSize; ++Loop1UInt)
      printf(" ");

    printf("- ");  // separator.
  }


  /* ----------------------------------------------------------------------------------------------------------------------------------------------------------- *\
                 If the first characters is a number enclosed inside angle-brackets '<xxx>', text to be displayed will be centered on a line of the 
                                                 specified number of characters (beginning after the current log header).
  \* ----------------------------------------------------------------------------------------------------------------------------------------------------------- */
  if (Dum1Str[0] == '<')
  {
    if (FlagLocalDebug) printf("[%5u] - Decoding line size to center text on the line [%s].\n", __LINE__, Dum1Str);
    LineSize = 0;  // initialize with invalid value.
    for (Loop1UInt = 0; Loop1UInt < 6; ++Loop1UInt)
    {
      if (Dum1Str[Loop1UInt] == '>')
      {
        if (FlagLocalDebug) printf("[%5u] - Closing angle-bracket found at position: %u\n", __LINE__, Loop1UInt);
        /* Closing angle-bracket has been found. */
        Dum1Str[Loop1UInt] = '\0';     // replace closing angle-bracket with an end-of-string.
        LineSize = atoi(&Dum1Str[1]);  // convert LineSize from ASCII number to binary data.
        if (FlagLocalDebug) printf("[%5u] - LineSize decoded is: %u\n", __LINE__, LineSize);
        break;
      }
    }

    if (FlagLocalDebug)
    {
      printf("[%5u] - LineSize: %u\n", __LINE__, LineSize);
      printf("[%5u] - strlen(&Dum1Str[Loop1UInt + 1]): %u\n", __LINE__, (strlen(&Dum1Str[Loop1UInt + 1]) - 1));
      printf("[%5u] - (LineSize - (strlen(&Dum1Str[Loop1UInt + 1])) - 1) / 2: %u\n", __LINE__, (LineSize - (strlen(&Dum1Str[Loop1UInt + 1]) - 1)) / 2);
      printf("[%5u] - Displaying this number of spaces: %u\n", __LINE__, (LineSize - (strlen(&Dum1Str[Loop1UInt + 1]) - 1)) / 2);
    }

    /* If a valid line size has been found, print required spaces so that text will be properly centered on the line.
       If no ending closing bracket has been found or if the number of spaces is too small or too large, display the text has is...
       It is possible that the opening angle bracket was simply part of the text to be displayed. */
    if ((Loop1UInt < 6) && (LineSize > 5) && (LineSize < 200) && (LineSize > (strlen(&Dum1Str[Loop1UInt + 1]) - 1)))
    {
      for (Loop2UInt = 0; Loop2UInt < (LineSize - (strlen(&Dum1Str[Loop1UInt + 1]) - 1)) / 2; ++Loop2UInt)
        printf(" ");
    }
    printf("%s", &Dum1Str[Loop1UInt + 1]);  // print the text to be logged while removing the heading <xxx> representing the line size.
    return;
  }

  /* If the text to log does not need to be centered, display it to log file. */
  printf(Dum1Str);

  return;
}

/* $PAGE */
/* $TITLE=input_string() */
/* ============================================================================================================================================================= *\
                                                                      Read a string from stdin.
\* ============================================================================================================================================================= */
void input_string(UCHAR *String, UINT16 StringSize, UINT32 TimeOutMSec)
{
#define MAX_HISTORY     10                  // keep track of the last 10 entries ("history").
  static UCHAR  History[MAX_HISTORY][128];
  static UINT16 NextEntry;

  INT8 DataInput;

  UINT8 FlagLocalDebug = FLAG_OFF;
  UINT8 Loop1UInt8;
  UINT8 TargetHistory;

  UINT64 IdleTimer;


  if (FlagLocalDebug) printf("\nEntering input_string().\n");

  Loop1UInt8    = 0;
  TargetHistory = MAX_HISTORY;   // initialize with invalid value on entry.
  IdleTimer     = time_us_64();  // initialize time-out timer with current system timer.

  do
  {
    DataInput = getchar_timeout_us(50000);
    /// if (DataInput != PICO_ERROR_TIMEOUT) printf("Entering handling with: [0x%2.2X]\n", DataInput);  ///

    switch (DataInput)
    {
      case (PICO_ERROR_TIMEOUT):
      case (0):
        /* Timeout: no character has been input during this 50 msec. IF A TIMEOUT HAS BEEN SPECIFIED, check if time is over. */ 
        if (TimeOutMSec && ((time_us_64() - IdleTimer) > (TimeOutMSec * 1000)))
        {
          /* Timed-out waiting for a keystroke. */
          String[0]  = 0x1B;  // cancel all characters entered so far and return with an <ESC> character as the first character.
          Loop1UInt8 = 1;     // end-of-string will be added when exiting do loop.
          DataInput  = 0x0D;  // simulate that <Enter> has been pressed to complete keystroke entry.
        }
        continue;
      break;

      case (8):
        /* <Backspace> */
        IdleTimer = time_us_64();  // restart time-out timer.
        if (Loop1UInt8 > 0)
        {
          /* If some characters have previously been entered, erase the last one entered. */
          --Loop1UInt8;
          String[Loop1UInt8] = '\0';
          printf("%c %c", 0x08, 0x08);  // erase character under the cursor.
        }
      break;

#if 0
      case (0x1B):
        /* <ESC> */
        IdleTimer = time_us_64();  // restart time-out timer.
        if (Loop1UInt8 == 0)
        {
          String[Loop1UInt8++] = 0x1B;  // keep <ESC> character as 1st character entered.
          String[Loop1UInt8++] = '\0';  // end-of-string.
          DataInput            = 0x0D;  // force an exit even if user didn't press <Enter>.
        }
        printf("\n");
      break;
#endif  // 0

      case (0x0D):
        /* <Enter> */
        IdleTimer = time_us_64();  // restart time-out timer.
        if (Loop1UInt8 == 0)
        {
          String[Loop1UInt8++] = (UCHAR)DataInput;
          String[Loop1UInt8++] = '\0';  // end-of-string.
        }
        printf("\n");
      break;

      case (0x1B):
      case (0x5B):
        /* Handling of <ESC> key and arrow keys (keystroke history). */
        // printf("Entering (0x1B) or (0x5B)\n");
        IdleTimer = time_us_64();  // restart time-out timer.
        if (DataInput == 0x1B) DataInput = getchar_timeout_us(50000);  // check if this ESC code is the beginning of an escape sequence.
        if (DataInput != 0x5B)
        {
          // printf("This <ESC> code was a "plain <ESC>" and not the beginning of an escape sequence...\n");
          if (Loop1UInt8 == 0)
          {
            String[Loop1UInt8++] = 0x1B;  // keep <ESC> character as 1st character entered.
            String[Loop1UInt8++] = '\0';  // end-of-string.
            DataInput            = 0x0D;  // force an exit even if user didn't press <Enter>.
          }
          printf("\n");
          break;
        }

        /* User pressed on an arrow extended character. */
        // printf("User pressed an arrow key.\n");
        DataInput = getchar_timeout_us(50000);  // get second tag of the arrow-key extended character.
        switch (DataInput)
        {
          case (0x41):
            /* User pressed up-arrow, retrieve "next previous keystroke history". */
            // printf("User pressed up-arrow.\n");
            /* First, erase what is currently on input line while returning cursor to beginning-of-line. */
            while (Loop1UInt8 > 0)
            {
              /* If some characters were previously displayed, erase them all. */
              --Loop1UInt8;
              String[Loop1UInt8] = '\0';
              printf("%c %c", 0x08, 0x08);  // erase character under the cursor.
            }

            if (TargetHistory == MAX_HISTORY)
            {
              /* If first press on up-arrow, initialize it with current entry position. */
              if (NextEntry > 0)
                TargetHistory = NextEntry - 1;
              else
                TargetHistory = MAX_HISTORY - 1;
            }
            else
            {
              /* If this is not the first press on up-arrow, retrieve "next previous keystroke history". */
              if (TargetHistory > 0)
                --TargetHistory;
              else
                TargetHistory = MAX_HISTORY - 1;
            }

            /* Consider this entry only if it is not null. */
            if (History[TargetHistory][0])
            {
              sprintf(String, History[TargetHistory]);
              printf("%s", String);
              Loop1UInt8 = strlen(String);
            }
          break;

          case (0x42):
            /* User pressed down-arrow, retrieve "next following keystroke history". */
            // printf("User pressed down-arrow.\n");
            /* First, erase what is currently on input line while returning cursor to beginning-of-line. */
            while (Loop1UInt8 > 0)
            {
              /* If some characters were previously displayed, erase them all. */
              --Loop1UInt8;
              String[Loop1UInt8] = '\0';
              printf("%c %c", 0x08, 0x08);  // erase character under the cursor.
            }

            if (TargetHistory == MAX_HISTORY)
            {
              /* If first press on down-arrow, initialize it with current entry position. */
              if (NextEntry < MAX_HISTORY - 1)
                TargetHistory = NextEntry;
              else
                TargetHistory = 0;
            }
            else
            {
              /* If this is not the first press on down-arrow, retrieve "next following keystroke history". */
              if (TargetHistory < MAX_HISTORY - 1)
                ++TargetHistory;
              else
                TargetHistory = 0;
            }

            /* Consider this entry only if it is not null. */
            if (History[TargetHistory][0])
            {
              sprintf(String, History[TargetHistory]);
              printf("%s", String);
              Loop1UInt8 = strlen(String);
            }
          break;
          
          case (0x43):
            /* User pressed right-arrow. */
            /* The right-arrow key could eventually be used to navigate inside current entry and edit it (insert / delete / replace characters). */
            printf("User pressed right-arrow.\n");
          break;
          
          case (0x44):
            /* User pressed left-arrow. */
            /* The left-arrow key could eventually be used to navigate inside current entry and edit it (insert / delete / replace characters). */
            printf("User pressed left-arrow.\n");
          break;
        }
      break;

      default:
        IdleTimer = time_us_64();  // restart time-out timer.
        printf("%c", (UCHAR)DataInput);
        String[Loop1UInt8] = (UCHAR)DataInput;
        // printf("Loop1UInt8: %3u   %2.2X - %c\n", Loop1UInt8, DataInput, DataInput);  // display current keystroke for debugging purposes.
        ++Loop1UInt8;  // one more character entered.
        if (Loop1UInt8 >= StringSize)
        {
          /* We reached the maximum size allowed for the string. */
          printf("\n");
          return;
        }
      break;
    }
    sleep_ms(10);
  } while((Loop1UInt8 < StringSize) && (DataInput != 0x0D));

  // printf("Exiting with Loop1UInt8: %u and DataInput: 0x%2.2X\n", Loop1UInt8, DataInput);
  String[Loop1UInt8] = '\0';  // end-of-string

  /* Keep current entry in keystroke history only if string is longer than 3 characters. */
  if (Loop1UInt8 > 3)
  {
    /* Keep current keystroke history only if it is different than previous one (since the previous one is already available with one up-arrow press).
       This will happen, for example if we recall previous entry for repeat. */
    if (NextEntry > 0)
      TargetHistory = NextEntry - 1;
    else
      TargetHistory = MAX_HISTORY;

    /* Compare current keystroke entry with the previous entry in keystroke history. */
    if (strcmp(History[TargetHistory], String))
    {
      /* If current keystroke entry is different than previous keystroke history, keep it as the last entry in history. */
      sprintf(History[NextEntry], String);
      ++NextEntry;  // point to next entry while preparing for next input.
      if (NextEntry >= MAX_HISTORY) NextEntry = 0;  // if out-of-bound, revert to 0 (circular buffer).
    }
  }

#if 0
  /* Optionally display each character entered for debugging purposes. */
  printf("\n\n\n");
  for (Loop1UInt8 = 0; Loop1UInt8 < 10; ++Loop1UInt8)
    printf("%2u:[%2.2X]   ", Loop1UInt8, String[Loop1UInt8]);
  printf("\n\n\n");
#endif  // 0

  
#if 0
  UINT8 Loop2UInt8;

  /* Optionally display keystroke history for debugging purposes. */
  printf("\n\n\n");
  for (Loop1UInt8 = 0; Loop1UInt8 < MAX_HISTORY; ++Loop1UInt8)
  {
    printf("%2u: ",   Loop1UInt8);
    
    for (Loop2UInt8 = 0; History[Loop1UInt8][Loop2UInt8]; ++Loop2UInt8)
    {
      if ((History[Loop1UInt8][Loop2UInt8] > 0x20) && (History[Loop1UInt8][Loop2UInt8] < 128))
        printf("%c", History[Loop1UInt8][Loop2UInt8]);  // printable character.
      else
        printf("?");  // non printable character.
    }
    printf("\n");
  }
  printf("\n\n\n");
#endif  // 0

  if (FlagLocalDebug) printf("Exiting input_string().\n");

  return;
}

#ifndef _BASELINE_H_
#define _BASELINE_H_

#include "stdint.h"

/* --------------------------------------------------------------------------------------------------------------------------- *\
                                                      General definitions.
\* --------------------------------------------------------------------------------------------------------------------------- */
/* Variable types. */
typedef char          CHAR;
typedef unsigned char UCHAR;
typedef int           INT;    // processor-optimized.
typedef int8_t        INT8;
typedef int16_t       INT16;
typedef int32_t       INT32;
typedef int64_t       INT64;
typedef unsigned int  UINT;    // processor-optimized.
typedef uint8_t       UINT8;
typedef uint16_t      UINT16;
typedef uint32_t      UINT32;
typedef uint64_t      UINT64;
typedef float         FLOAT;
typedef double        DOUBLE;

/* On / Off flags. */
#define FLAG_OFF     0x00
#define FLAG_ON      0x01

/* False / true. */
#define FALSE           0
#define TRUE            1

/* Microcontroler type. */
#define TYPE_PICO       1
#define TYPE_PICOW      2

/* Original Pico internal LED GPIO. */
#define GPIO_PICO_LED  25

/* Logged data extra information. */
#define LOG_NONE        0
#define LOG_LINE        1
#define LOG_CORE        2
#define LOG_TIME        4
#define LOG_DATE        8
#define LOG_FUNCTION   16
#define LOG_ALL      0xFF


/* Structure to contain time variables under "human" readable format instead of "tm" standard. */
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


#if 0
/* Day names. A few more characters are allowed in provision for foreign languages. */
UCHAR DayName[7][13] =
{
  {"Sunday"}, {"Monday"}, {"Tuesday"}, {"Wednesday"}, {"Thursday"}, {"Friday"}, {"Saturday"}
};

/* Short month names (3 letters). */
UCHAR ShortMonth[13][4] =
{
  {" "}, {"JAN"}, {"FEB"}, {"MAR"}, {"APR"}, {"MAY"}, {"JUN"}, {"JUL"}, {"AUG"}, {"SEP"}, {"OCT"}, {"NOV"}, {"DEC"}
};
#endif  // 0
#endif  // _BASELINE_H_

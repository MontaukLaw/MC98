/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file RTC_ext.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief 外置RTC驱动函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _RTC_EXT_H
#define _RTC_EXT_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
#define WEEKDAY_AUTO_SET    1

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    uint16_t year;
    uint8_t day;
    uint8_t month;
    uint8_t date;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} timeRTC_t;


/* Exported constants --------------------------------------------------------*/
#define RTC_YEAR_BASE   1970

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void ExternRTC_Init(void);
BOOL RTC_ReadTime(timeRTC_t *time);
BOOL RTC_ReadTimeStr(char *buf);
BOOL RTC_SetTime(timeRTC_t *time);

void RTC_TickToStr(uint32_t ts, char *buf);
void RTC_TimeToStr(timeRTC_t *time, char *buf);

void RTC_TickToTime(uint32_t tick, timeRTC_t *time);
uint32_t RTC_TimeToTick(timeRTC_t *tim);

uint32_t RTC_ReadTick(void);

#endif

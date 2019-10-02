/**
 * **********************************************************************
 *             Copyright (c) 2017 AFU All Rights Reserved.
 * @file control.h
 * @author 宋阳
 * @version V1.0
 * @date 2017.7.12
 * @brief 设备控制函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _CONTROL_H
#define _CONTROL_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
#define DOOR_OPEN_IN    1
#define DOOR_OPEN_OUT   2


/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/*一次扫描的最多数量*/
#define RFID_SCAN_MAX			128

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern BOOL ScanStationStart, ScanChannelStart, ReaportIR, RFID_Reset;

/* Exported functions --------------------------------------------------------*/
void Control_Init(void);

void Control_DoorOpen(uint8_t inout);
void Control_Polling(void);

#endif

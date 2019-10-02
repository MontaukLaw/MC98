/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Process.h
 * @author ����
 * @version V1.0
 * @date 2016.8.31
 * @brief ҵ���߼�������ͷ�ļ�.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _PROCESS_H
#define _PROCESS_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"
#include "cjson.h"

/* Exported define -----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum
{
	ret_ok,
	ret_fault,
	ret_timeout,
} proret_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void Process_Init(void);

void Updata_Gameoff(uint8_t result);
BOOL CMD_Updata(char* cmd, cJSON* desired);

void Status_Updata(void);
void Report_DoorStatus(char *str);
void Report_Rope_RFID(char* rfid, uint8_t status, uint16_t voltage);
void Request_Price(char* rfid);

#endif

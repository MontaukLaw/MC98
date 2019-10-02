/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file gprs.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief GPRS驱动函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _GPRS_H
#define _GPRS_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
#define GPRS_TASK_STK_SIZE          256
#define GPRS_TASK_PRIO              osPriorityNormal
#define GPRS_SEND_Q_SIZE            8

#define GPRS_UART_BDR               115200
#define GPRS_UART_REFRESH_TICK      200

#define GPRS_SEND_MAX_SIZE          1500
#define GPRS_RECEIVE_MAX_SIZE       2048

/*GPRS远程调试使能*/
#define GPRS_CMD_EN                 1

/*GPRS省电使能，使能后当GPRS长时间未连网时关机*/
#define GPRS_POWER_SAVE_EN          0

/*GPRS省电关机的时间，单位秒*/
#define GPRS_POWER_SAVE_TIME        60

/*定义GPRS模块控制选项*/
#define GPRS_OPT_RESET              0x01
#define GPRS_OPT_SET_SOCKET         0x04
#define GPRS_OPT_SET_APN            0x08

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    gprs_status_poweroff = 0,
    gprs_status_poweron,
    gprs_status_nocard,
    gprs_status_nonet,
    gprs_status_online,
    gprs_status_fault
} GPRS_Status_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void GPRS_Init(void);

void GPRS_ReStart(void);
void GPRS_SetOnOff(BOOL onoff);
GPRS_Status_t GPRS_ReadStatus(void);

int16_t GPRS_SocketSendData(uint8_t *data, uint16_t len);
int8_t GPRS_IsSocketConnect(void);
void GPRS_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback);

uint8_t GPRS_ReadRSSI(void);
BOOL    GPRS_ReadPhoneNum(char *num);

uint32_t GPRS_HTTP_Get(char *url, char *getbuf, uint32_t buflen);

uint32_t GPRS_FTP_StartGetFile(char *server, char *user, char *pwd, char *path);
BOOL GPRS_FTP_GetData(uint8_t *buf, uint16_t *getlen);

BOOL GPRS_TTS(char *text);

#endif



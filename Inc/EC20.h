/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file M4G.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief M4G驱动函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _EC20_H
#define _EC20_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
#define M4G_TASK_STK_SIZE          256
#define M4G_TASK_PRIO              osPriorityNormal
#define M4G_SEND_Q_SIZE            8

#define M4G_UART_BDR               115200
#define M4G_UART_REFRESH_TICK      1000

#define M4G_SEND_MAX_SIZE          1500
#define M4G_RECEIVE_MAX_SIZE       2048

/*M4G省电使能，使能后当M4G长时间未连网时关机*/
#define M4G_POWER_SAVE_EN          0

/*M4G省电关机的时间，单位秒*/
#define M4G_POWER_SAVE_TIME        60

/*定义M4G模块控制选项*/
#define M4G_OPT_RESET              0x01
#define M4G_OPT_SET_SOCKET         0x04
#define M4G_OPT_SET_APN            0x08

/* Exported types ------------------------------------------------------------*/
typedef enum {
    M4G_status_poweroff = 0,
    M4G_status_poweron,
    M4G_status_nocard,
    M4G_status_nonet,
    M4G_status_online,
    M4G_status_fault
} M4G_Status_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void M4G_Init(void);

void M4G_ReStart(void);
void M4G_SetOnOff(BOOL onoff);
M4G_Status_t M4G_ReadStatus(void);

int16_t M4G_SocketSendData(uint8_t *data, uint16_t len);
int8_t M4G_IsSocketConnect(void);
void M4G_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback);

uint8_t M4G_ReadRSSI(void);
BOOL    M4G_ReadPhoneNum(char *num);

BOOL M4G_TTS(char *text);

#endif



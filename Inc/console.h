/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file console.h
 * @author 宋阳
 * @version V1.1
 * @date 2016.4.1
 * @brief 控制台函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _CONSOLE_H
#define _CONSOLE_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
/*定义控制台管道的最大数量*/
#define CMD_PIPE_MAX            8

/*使能串口管道,使能后
  UART_PORT_MAX以下为实体串口，UART_PORT_MAX以上为虚拟端口*/
#define CMD_UART_EN             1

/*登账账户与密码*/
#define CMD_LOGIN_USER          "ROOT"
#define CMD_LOGIN_PWD           "12345678"

/*控制台缓存数据刷新时间*/
#define CMD_UART_REFRESH_TICKS  1000

/*控制台回显默认设置*/
#define CMD_ECHO_DEF            1

//#define CMD_PRINTF_BUFF_MAX     128
#define CMD_PRINTF_BUFF_MAX     1024
#define CMD_FIFO_BUF_SIZE       256

#define CMD_TASK_STK_SIZE       512
#define CMD_TASK_PRIO           osPriorityNormal

/* Exported types ------------------------------------------------------------*/
typedef BOOL (*CMD_SendFun)(uint8_t *dat, uint16_t len);

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
/*驱动调用*/
void CMD_Init(void);
void CMD_UART_Read_Poll(void);
void CMD_NewData(uint8_t pipe, uint8_t *dat, uint16_t len);
uint8_t CMD_Pipe_Register(CMD_SendFun fun);

/*用户调用*/
void CMD_SendData(uint8_t *dat, uint16_t len);
uint16_t CMD_ReadData(uint8_t *dat, uint16_t len);
uint16_t CMD_DataSize(void);

void CMD_SetEchoEnable(BOOL en);
void CMD_SetDebugLevel(uint8_t level);

void CMD_Printf(char *format, ...);
void CMD_HEX_Print(uint8_t *dat, uint16_t len);

void CMD_Printf_Level(uint8_t level, char *format, ...);
void CMD_HEX_Print_Level(uint8_t level, uint8_t *dat, uint16_t len);

void CMD_Virtual(char *cmd);

#endif



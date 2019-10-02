/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file uart.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief UART 函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _UART_H
#define _UART_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"
#include "stm32f1xx.h"

/* Exported define -----------------------------------------------------------*/
#define UART_PORT_MAX               5

#define UART_PRINTF_BUFF_MAX        512

#define UART1_485_EN                0
#define UART2_485_EN                0
#define UART3_485_EN                0
#define UART4_485_EN                0
#define UART5_485_EN                1

#define UART1_DMA_SEND              1
#define UART2_DMA_SEND              1
#define UART3_DMA_SEND              1
#define UART4_DMA_SEND              1

/*串口数据清空刷新时间，缓存满时为100mS，缓存未满时为UART_REFRESH_TICKS*/
#define UART_REFRESH_TICKS          5000

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void UART_PortConfig(uint8_t num, UART_HandleTypeDef *h_uart, uint16_t bufSize);

uint16_t UART_SendData(uint8_t num, uint8_t *dat, uint16_t len);
uint16_t UART_SendData_Direct(uint8_t num, uint8_t *dat, uint16_t len);
uint16_t UART_Printf(uint8_t num, char *format, ...);
uint16_t UART_Printf_Direct(uint8_t num, char *format, ...);

FIFO_t  *UART_FIFO(uint8_t num);
uint16_t UART_DataSize(uint8_t num);
uint8_t  UART_QueryByte(uint8_t num, uint16_t offset);
uint16_t UART_ReadData(uint8_t num, uint8_t *dat, uint16_t len);
uint32_t UART_GetDataIdleTicks(uint8_t num);

void UART_SetBaudrate(uint8_t num, uint32_t brd);
void UART_SetRemapping(uint8_t num1, uint8_t num2);
void UART_SetMonitor(uint8_t outNum, uint8_t sourceNum);
void UART_Refresh_Poll(void);

BOOL UART_ReceiveByte_IRQ(uint8_t num);

#endif



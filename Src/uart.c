/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file uart.c
 * @version V1.0
 * @date 2016.4.1
 * @brief UART 驱动函数文件.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define DE_RXD      GPIO_PIN_RESET
#define DE_TXD      GPIO_PIN_SET

/* Private macros ------------------------------------------------------------*/
#define RS458_SET_DE(num, state)   IO_WRITE(UART##num##_DE, state)

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef* UART_Handle_Array[UART_PORT_MAX];
static osSemaphoreDef_t     UART_SendSem_Array[UART_PORT_MAX];
static osSemaphoreId        UART_SendSemId_Array[UART_PORT_MAX];
static osMutexDef_t         UART_SendMutex_Array[UART_PORT_MAX];
static osMutexId            UART_SendMutexId_Array[UART_PORT_MAX];
static FIFO_t               UART_Receive_FIFO_Arrary[UART_PORT_MAX];
static uint32_t             UART_Receive_Ts[UART_PORT_MAX];
static uint8_t              UART_Remapping1, UART_Remapping2, UART_MonitSource, UART_MonitOut;
static FIFO_t               UART_Rp1_FIFO, UART_Rp2_FIFO, UART_Monit_FIFO;

/* Private function prototypes -----------------------------------------------*/
static BOOL RS485_SetDE(uint8_t num, uint8_t de);
static void UART_Send(uint8_t num, uint8_t* dat, uint16_t len);
static void UART_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * 串口端口初始化
 * @param num     串口号
 * @param h_uart  UART HAL的句柄
 * @param bufSize 接收缓存的大小
 */
void UART_PortConfig(uint8_t num, UART_HandleTypeDef* h_uart, uint16_t bufSize) {
    uint8_t* pBuf = NULL;
    static  BOOL f = FALSE;
    if (f == FALSE) {
        CMD_ENT_DEF(UART, UART_Console);
        Cmd_AddEntrance(CMD_ENT(UART));
        f = TRUE;
    }
    if (num > 0 && num <= UART_PORT_MAX && IS_POWER_OF_TWO(bufSize)) {
        RS485_SetDE(num, DE_RXD);
        pBuf =  MMEMORY_ALLOC(bufSize);
        if (pBuf != NULL) {
            num -= 1;
            UART_Handle_Array[num] = h_uart;
            UART_SendMutexId_Array[num] = osMutexCreate(&UART_SendMutex_Array[num]);
            UART_SendSemId_Array[num] = osSemaphoreCreate(&UART_SendSem_Array[num], 1);
            osSemaphoreWait(UART_SendSemId_Array[num], osWaitForever);
            FIFO_Init(&UART_Receive_FIFO_Arrary[num], pBuf, bufSize);
            __HAL_UART_ENABLE_IT(h_uart, UART_IT_RXNE);
        }
    }
}

/**
 * 串口发送数据
 * @param num  串口号
 * @param dat  数据指针
 * @param len  数据长度
 * @return 发送成功返回数据长度，失败返回0
 */
uint16_t UART_SendData(uint8_t num, uint8_t* dat, uint16_t len) {
    int r = 0;
    if (num > 0 && num <= UART_PORT_MAX && dat != NULL && len > 0) {
        osMutexWait(UART_SendMutexId_Array[num - 1], osWaitForever);
        RS485_SetDE(num, DE_TXD);
        UART_Send(num, dat, len);
        osSemaphoreWait(UART_SendSemId_Array[num - 1], osWaitForever);
        RS485_SetDE(num, DE_RXD);
        osMutexRelease(UART_SendMutexId_Array[num - 1]);
        r = len;
    }
    return r;
}

/**
 * 调用HAL驱动直接发出UART数据,用于特殊场合
 * @param num  串口号
 * @param dat  数据指针
 * @param len  数据长度
 */
uint16_t UART_SendData_Direct(uint8_t num, uint8_t* dat, uint16_t len) {
    if (num > 0 && num <= UART_PORT_MAX && dat != NULL && len > 0) {
        RS485_SetDE(num, DE_TXD);
        HAL_UART_Transmit(UART_Handle_Array[num - 1], dat, len, 2000);
        RS485_SetDE(num, DE_RXD);
        return len;
    }
    return 0;
}

/**
  * 串口打印可变参数字符串.
  * @param  num:串口号.
  * @param  fomat: 参数列表.
  * @param  ...:可变参数
  * @retval 发送成功的数据长度,反送失败返回0.
  */
uint16_t UART_Printf(uint8_t num, char* format, ...) {
    uint16_t sent = 0;
    char* pBuf = NULL;
    va_list args;
    if (num > 0 && num <= UART_PORT_MAX && format != NULL) {
        pBuf = MMEMORY_ALLOC(UART_PRINTF_BUFF_MAX);
        if (pBuf != NULL) {
            va_start(args, format);
            vsnprintf(pBuf, UART_PRINTF_BUFF_MAX, format, args);
            va_end(args);
            sent = strlen(pBuf);
            sent = UART_SendData(num, (uint8_t*)pBuf, sent);
            MMEMORY_FREE(pBuf);
        }
    }
    return sent;
}

/**
  * 串口打印可变参数字符串，可在中断中使用.
  * @param  num:串口号.
  * @param  fomat: 参数列表.
  * @param  ...:可变参数
  * @retval 发送成功的数据长度,反送失败返回0.
  */
uint16_t UART_Printf_Direct(uint8_t num, char* format, ...) {
    uint16_t sent = 0;
    char buf[UART_PRINTF_BUFF_MAX];
    char* pBuf = buf;
    va_list args;
    if (num > 0 && num <= UART_PORT_MAX && format != NULL) {
        va_start(args, format);
        vsnprintf(pBuf, UART_PRINTF_BUFF_MAX, format, args);
        va_end(args);
        sent = strlen(pBuf);
        sent = UART_SendData_Direct(num, (uint8_t*)pBuf, sent);
    }
    return sent;
}

/**
 * 获取串口的FIFO指针.
 * @param num  串口号
 */
FIFO_t* UART_FIFO(uint8_t num) {
    FIFO_t* pfifo = NULL;
    if (num > 0 && num <= UART_PORT_MAX) {
        pfifo = &UART_Receive_FIFO_Arrary[num - 1];
    }
    return pfifo;
}

/**
 * 获取串口数据长度.
 * @param num  串口号
 */
uint16_t UART_DataSize(uint8_t num) {
    uint16_t i = 0;
    if (num > 0 && num <= UART_PORT_MAX) {
        i = FIFO_Length(&UART_Receive_FIFO_Arrary[num - 1]);
    }
    return i;
}

/**
 * 串口读数据但不清除读过的缓存
 * @param num  串口号
 * @param offset  偏移量
 */
uint8_t UART_QueryByte(uint8_t num, uint16_t offset) {
    uint16_t i = 0;
    if (num > 0 && num <= UART_PORT_MAX) {
        i = FIFO_Query(&UART_Receive_FIFO_Arrary[num - 1], offset);
    }
    return i;
}

/**
 * 串口读数据
 * @param num  串口号
 * @param dat  数据读出的指针
 * @param len  待读出的长
 * @return 返回数据读出的长度，无数据时返回0
 */
uint16_t UART_ReadData(uint8_t num, uint8_t* dat, uint16_t len) {
    uint16_t i = 0;
    if (num > 0 && num <= UART_PORT_MAX && dat != NULL && len > 0) {
        UART_Refresh_Poll();
        i = FIFO_Read(&UART_Receive_FIFO_Arrary[num - 1], dat, len);
    }
    return i;
}

/**
 * 获取串口空闲的时间
 * @param num  串口号
 * @return 返回串口接收到的数据无变化的时间差，单位ms
 */
uint32_t UART_GetDataIdleTicks(uint8_t num) {
    uint32_t i = 0;
    if (num > 0 && num <= UART_PORT_MAX) {
        i = HAL_GetTick() - UART_Receive_Ts[num - 1];
        if (i > UART_REFRESH_TICKS) {
            i = 0;
        }
    }
    return i;
}

/**
 * 设置UART的波特率
 * @param num  串口号
 * @param brd  新的波特率
 */
void UART_SetBaudrate(uint8_t num, uint32_t brd) {
    if (num > 0 && num <= UART_PORT_MAX && brd != UART_Handle_Array[num - 1]->Init.BaudRate) {
        __HAL_UART_DISABLE_IT(UART_Handle_Array[num - 1], UART_IT_RXNE);
        UART_Handle_Array[num - 1]->Init.BaudRate = brd;
        HAL_UART_Init(UART_Handle_Array[num - 1]);
        __HAL_UART_ENABLE_IT(UART_Handle_Array[num - 1], UART_IT_RXNE);
    }
}

/**
 * UART设置数据映射
 * 设置后num1 读出的数据将转发至num2, num2读出的数据将转发至num1
 * 参数为0时取消映射
 * @param num1 串口1
 * @param num2 串口2
 */
void UART_SetRemapping(uint8_t num1, uint8_t num2) {
    if (num1 <= UART_PORT_MAX && num2 <= UART_PORT_MAX) {
        portENTER_CRITICAL();
        UART_Remapping1 = num1;
        UART_Remapping2 = num2;
        if (num1 > 0) {
            UART_Rp1_FIFO = UART_Receive_FIFO_Arrary[num1 - 1];
            FIFO_Flush(&UART_Rp1_FIFO);
        }
        if (num2 > 0) {
            UART_Rp2_FIFO = UART_Receive_FIFO_Arrary[num2 - 1];
            FIFO_Flush(&UART_Rp2_FIFO);
        }
        portEXIT_CRITICAL();
    }
}

/**
 * UART 设置端口监视，设置成功后souce端口收到的数据将转发至out
 * @param sourceNum 源端口
 * @param outNum    目标端口
 */
void UART_SetMonitor(uint8_t outNum, uint8_t sourceNum) {
    if (sourceNum <= UART_PORT_MAX && outNum <= UART_PORT_MAX) {
        portENTER_CRITICAL();
        UART_MonitSource = sourceNum;
        UART_MonitOut = outNum;
        if (sourceNum > 0) {
            UART_Monit_FIFO = UART_Receive_FIFO_Arrary[sourceNum - 1];
            FIFO_Flush(&UART_Monit_FIFO);
        }
        portEXIT_CRITICAL();
    }
}

/**
 * UART 溢出刷新，端口监视与转发的轮询函数.
 */
void UART_Refresh_Poll(void) {
    FIFO_t* pFIFO = NULL;
    uint16_t len = 0;
    uint8_t* buf = NULL, num = 0, i = 0;
    /*超时清空串口数据*/
    for (i = 0; i < UART_PORT_MAX; i++) {
        pFIFO = &UART_Receive_FIFO_Arrary[i];
        if (pFIFO != NULL) {
            len = FIFO_Length(pFIFO);
            if ((len == (pFIFO->sizeMask + 1) && UART_GetDataIdleTicks(i + 1) >= 100)
                    || (FIFO_Length(pFIFO) > 0 && (UART_GetDataIdleTicks(i + 1) >= UART_REFRESH_TICKS))) {
                FIFO_Flush(pFIFO);
                DBG_WAR("UART%u receive fifo overflow.", i + 1);
            }
        }
    }
    /*串口监听与转发轮询*/
    pFIFO = NULL;
    for (i = 0; i < 3; i++) {
        if (i == 0 && UART_MonitSource > 0 && UART_MonitOut > 0) {
            pFIFO = &UART_Monit_FIFO;
            pFIFO->wpos = UART_Receive_FIFO_Arrary[UART_MonitSource - 1].wpos;
            num = UART_MonitOut;
        } else if (i == 1 && UART_Remapping1 > 0) {
            pFIFO = &UART_Rp1_FIFO;
            pFIFO->wpos = UART_Receive_FIFO_Arrary[UART_Remapping1 - 1].wpos;
            num = UART_Remapping2;
        } else if (i == 2 && UART_Remapping2 > 0) {
            pFIFO = &UART_Rp2_FIFO;
            pFIFO->wpos = UART_Receive_FIFO_Arrary[UART_Remapping2 - 1].wpos;
            num = UART_Remapping1;
        } else {
            pFIFO = NULL;
        }
        if (pFIFO != NULL) {
            len = FIFO_Length(pFIFO);
            if (len > 0) {
                buf = MMEMORY_ALLOC(len);
                if (buf != NULL) {
                    len = FIFO_Read(pFIFO, buf, len);
                    UART_SendData(num, buf, len);
                    MMEMORY_FREE(buf);
                }
            }
        }
    }
}

/**
 * 串口接收的字节写入到FIFO
 * @param num  串口号
 * @param byte 写入的字节
 */
BOOL UART_ReceiveByte_IRQ(uint8_t num) {
    UART_HandleTypeDef* pHandle;
    uint8_t rd = 0;
    pHandle = UART_Handle_Array[num - 1];
    if (pHandle != NULL && ((__HAL_UART_GET_FLAG(pHandle, UART_FLAG_RXNE) != RESET && __HAL_UART_GET_IT_SOURCE(pHandle, UART_IT_RXNE) != RESET)
                            || __HAL_UART_GET_FLAG(pHandle, UART_FLAG_ORE) != RESET)) {
        rd = (uint8_t)(pHandle->Instance->DR);
        UART_Receive_Ts[num - 1] = HAL_GetTick();
        FIFO_Put(&UART_Receive_FIFO_Arrary[num - 1], rd);
        __HAL_UART_CLEAR_OREFLAG(pHandle);
        return TRUE;
    }
    return FALSE;
}

/**
 * 重新定位的串口HAL发送结束回调函数
 * @param huart UART HAL的句柄
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    int i = 0;
    for (i = 0; i < UART_PORT_MAX; i++) {
        if (huart == UART_Handle_Array[i]) {
            break;
        }
    }
    if (i < UART_PORT_MAX && UART_SendSemId_Array[i] != NULL) {
        osSemaphoreRelease(UART_SendSemId_Array[i]);
    }
}

/* Private function prototypes -----------------------------------------------*/

/**
 * 485设置DE引脚状态
 * @param num  串口号
 * @param de   DE管脚状态
 * @return 返回是否有485 DE引脚的控制动作
 */
static BOOL RS485_SetDE(uint8_t num, uint8_t de) {
#if UART1_485_EN == 1
    if (num == 1) {
        RS458_SET_DE(1, de);
        return TRUE;
    }
#endif
#if UART2_485_EN == 1
    if (num == 2) {
        RS458_SET_DE(2, de);
        return TRUE;
    }
#endif
#if UART3_485_EN == 1
    if (num == 3) {
        RS458_SET_DE(3, de);
        return TRUE;
    }
#endif
#if UART4_485_EN == 1
    if (num == 4) {
        RS458_SET_DE(4, de);
        return TRUE;
    }
#endif
#if UART5_485_EN == 1
    if (num == 5) {
        RS458_SET_DE(5, de);
        return TRUE;
    }
#endif
    return FALSE;
}

/**
 * 串口送出数据
 * @param num  串口号
 * @param dat  数据指针
 * @param len  数据长度
 */
static void UART_Send(uint8_t num, uint8_t* dat, uint16_t len) {
    BOOL dma = FALSE;
    switch (num) {
        case 1:
#if UART1_DMA_SEND == 1
            dma = TRUE;
#endif
            break;
        case 2:
#if UART2_DMA_SEND == 1
            dma = TRUE;
#endif
            break;
        case 3:
#if UART3_DMA_SEND == 1
            dma = TRUE;
#endif
            break;
        case 4:
#if UART4_DMA_SEND == 1
            dma = TRUE;
#endif
            break;
        default:
            break;
    }
    if (dma) {
        HAL_UART_Transmit_DMA(UART_Handle_Array[num - 1], dat, len);
    } else {
        HAL_UART_Transmit_IT(UART_Handle_Array[num - 1], dat, len);
    }
}

/**
 * UART调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void UART_Console(int argc, char* argv[]) {
    uint32_t d = 0, d1 = 0, bdr = 0;
    char* buf = NULL;
    while (argc > 0 && *argv != NULL) {
        if (strcmp(*argv, "bdr") == 0) {
            d = uatoi(argv[1]);
            bdr = uatoi(argv[2]);
            DBG_LOG("UART%d baudrate chang to %d.", d, bdr);
            UART_SetBaudrate(d, bdr);
        } else if (strcmp(*argv, "send") == 0) {
            d = uatoi(argv[1]);
            d1 = strlen_t(argv[2]);
            UART_SendData(d, (uint8_t*)argv[2], d1);
            DBG_LOG("UART%d send data length:%d", d, d1);
        } else if (strcmp(*argv, "read") == 0) {
            d = uatoi(argv[1]);
            d1 = UART_DataSize(d);
            if (d1 > 0) {
                buf = MMEMORY_ALLOC(d1);
                if (buf != NULL) {
                    UART_ReadData(d, (uint8_t*)buf, d1);
                    DBG_LOG("UART%d read size:%d", d, d1);
                    DBG_SEND((uint8_t*)buf, d1);
                    DBG_LOG("  <end.");
                    MMEMORY_FREE(buf);
                }
            } else {
                DBG_LOG("UART%d no data.", d);
            }
        } else if (strcmp(*argv, "remap") == 0) {
            d = uatoi(argv[1]);
            d1 = uatoi(argv[2]);
            UART_SetRemapping(d, d1);
            DBG_LOG("UART%d remapping to UART%d.", d, d1);
        } else if (strcmp(*argv, "monitor") == 0) {
            d = uatoi(argv[1]);
            d1 = uatoi(argv[2]);
            UART_SetMonitor(d, d1);
            DBG_LOG("UART%d monitor UART%d receive data.", d, d1);
        }
        argv++;
        argc--;
    }
}


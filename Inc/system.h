/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file system.h
 * @author ����
 * @version V1.0
 * @date 2016.12.18
 * @brief ϵͳ��������ͷ�ļ�.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _SYSTEM_H
#define _SYSTEM_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
/*��������ģ��ʹ��*/
#define NET_LAN_EN              0
#define NET_GPRS_EN             0
#define NET_4G_EN               1

#define NET_GPRS                11
#define NET_WIFI                12
#define NET_LAN                 13
#define NET_4G                 	14

/*����socket���ӳ�ʱʱ��,��λ��*/
#define SOCKET_CONNECT_TIMEOUT  45

/*����SOCKET FIFO����*/
#define SOCKET_FIFO_SIZE        2048

/*����SOCK���ն������*/
#define SOCK_REC_Q_SIZE         8

/*������־ʹ�ܣ�ʹ�ܺ�ϵͳ����ʱ����flash�м�¼������Ϣ*/
#define START_LOG_EN            0

#define LOG_SAVE_BLOCK_SIZE     64

/* Exported types ------------------------------------------------------------*/
/*�������������ṹ*/
typedef struct {
    uint16_t nrst;
    uint16_t iwdg;
    uint16_t soft;
    uint16_t por;
} ResetCount_t;

/*��־�洢�鶨��*/
typedef struct {
    uint16_t crc;
    ResetCount_t ResetCount;
    uint32_t tsPwrOn;
    uint32_t tsFault;
    char FaultTask[36];
} LogBlock_t;

/*����socket���ݽ��ջص���������*/
typedef void (*Sock_RecCBFun)(uint8_t *dat, uint16_t len);
/*����socket���ݷ��ͺ�������*/
typedef int16_t (*Sock_SendFun)(uint8_t *dat, uint16_t len);

/* Exported constants --------------------------------------------------------*/
/*������־�洢�ĵ�ַ*/
#define LOG_ADDR()          (SECTOR_ADDR(LOG_SAVE_SECTOR))
#define LOG_ADDR_MAX()      (SECTOR_ADDR(LOG_SAVE_SECTOR + SECTOR_LOG_SAVE_SIZE - 1))
#define LOG_TASK_ADDR()     (SECTOR_ADDR(LOG_SAVE_SECTOR + SECTOR_LOG_SAVE_SIZE ))

/* Exported macro ------------------------------------------------------------*/
/*IO����*/
#define IO_READ(x)              ((x##_GPIO_Port->IDR & x##_Pin) ? 1 : 0)
#define IO_H(x)                 (x##_GPIO_Port->BSRR = x##_Pin)
#define IO_L(x)                 (x##_GPIO_Port->BSRR = (uint32_t)x##_Pin << 16)
#define IO_TOGGLE(x)            (x##_GPIO_Port->ODR ^= x##_Pin)
#define IO_WRITE(x, state)      (HAL_GPIO_WritePin(x##_GPIO_Port, x##_Pin, (GPIO_PinState)state))
#define IO_IS_OUT(x)            (x##_GPIO_Port->ODR & x##_Pin)

#define GPIO_H(port, pin)       (port->BSRR = pin)
#define GPIO_L(port, pin)       (port->BSRR = (uint32_t)pin << 16)

#define LED_ON(x)               IO_H(LED_##x)
#define LED_OFF(x)              IO_L(LED_##x)
#define LED_TOGGLE(x)           IO_TOGGLE(LED_##x)
#define LED_WRITE(x, state)     IO_WRITE(LED_##x, state)
#define LED_IS_ON(x)            IO_IS_OUT(LED_##x)

#define LED_ALL_ON()            do{LED_ON(ERR);LED_ON(RUN);LED_ON(NET);}while(0)
#define LED_ALL_OFF()           do{LED_OFF(ERR);LED_OFF(RUN);LED_OFF(NET);}while(0)

/*TS*/
#define TS_INIT(ts)             do {ts = HAL_GetTick();}while(0)
#define TS_IS_OVER(ts, over)    (HAL_GetTick() - ts >= over)
#define TS_COUNT(ts)            (HAL_GetTick() - ts)

/*�ڴ����*/
#define MMEMORY_ALLOC            pvPortMalloc
#define MMEMORY_FREE             vPortFree

/*���ж�*/
#define MCPU_ENTER_CRITICAL()    portENTER_CRITICAL()
#define MCPU_EXIT_CRITICAL()     portEXIT_CRITICAL()
#define MCPU_IS_ISR()            (__get_IPSR() != 0)

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void System_Init(void);

void System_SockLockEn(BOOL lock);
BOOL System_SockIsLock(void);

int16_t System_SockSend(uint8_t *data, uint16_t len);
int16_t System_SockRecv(uint8_t *data, uint16_t len);
int16_t System_SockLength(void);

void System_SetSocket(char *addr, uint16_t port);
int8_t System_SockConnect(char *addr, uint16_t port);
int8_t System_SockIsConnected(char **addr, uint16_t *port);

void LED_FlashPoll(void);

void Read_ResetCount(ResetCount_t *count);
void Clear_ResetCount(void);
uint32_t Read_MCU_ID(void);

void StartLog_Recoder(void);
void StartLog_UART(uint32_t tsb, uint32_t tse);
void TaskFault_Save(uint32_t tsFault, char *faultTask);
BOOL SaveLog_GetBlock(uint32_t addr, LogBlock_t *block);
uint32_t SaveLog_NextAddr(uint32_t addr);

#endif



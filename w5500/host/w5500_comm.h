/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file w5500_comm.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.7.20
 * @brief W5500相关函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _5500_COMM_H
#define _5500_COMM_H


/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"
#include "prjlib.h"
#include "system.h"
#include "console.h"
#include "common.h"

/* Exported define -----------------------------------------------------------*/
#define LAN_TASK_STK_SIZE           256
#define LAN_TASK_PRIO               osPriorityNormal
#define LAN_SEND_Q_SIZE             8
#define LAN_RECEIVE_MAX_SIZE        2048
#define LAN_SEND_MAX_SIZE           1024

#define LAN_CMD_EN                  1

#define LAN_NOT_CONNECT_TIMEOUT     300
#define LAN_PHY_OFFLINE_TIMEOUT     600

#define SOCK_DHCP   0

#define SOCK_DNS    1

#define SOCK_USER   4

#define DEFAULT_MAC_ADDR   "24:BD:DC:70:80:22"
#define DEFAULT_IP_ADDR    "192.168.1.112"
#define DEFAULT_SN_MASK    "255.255.255.0"
#define DEFAULT_GW_ADDR    "192.168.1.1"
#define DEFAULT_DNS_ADDR   "192.168.1.10"

/*定义WIFI模块控制选项*/
#define LAN_OPT_RESET               0x01
#define LAN_OPT_SET_SOCKET          0x08
#define LAN_OPT_SET_IPCONFIG        0x10

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    lan_status_reset = 0,
    lan_status_phy_offline,
    lan_status_phy_online,
    lan_status_phy_dhcp_leased,
    lan_status_fault,
} LAN_Status_t;


/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void w5500_Init(SPI_HandleTypeDef *handle);

int16_t LAN_SocketSendData(uint8_t *data, uint16_t len);
int8_t LAN_IsSocketConnect(void);
void LAN_SetSocketParam(char *server, uint16_t port, Sock_RecCBFun callback);

LAN_Status_t LAN_ReadStatus(void);

void w5500_hw_reset(void);
uint32_t wizpf_get_systick(void);
uint32_t wizpf_tick_conv(uint8_t istick2sec, uint32_t tickorsec);
int32_t wizpf_tick_elapse(uint32_t tick);

#endif


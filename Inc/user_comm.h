/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file uaer_comm.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief 用户公用头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _USER_COMM_H
#define _USER_COMM_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include "prjlib.h"

#include "system.h"
#include "dfu.h"
#include "console.h"
#include "http.h"

#include "spi_flash.h"
#include "rtc_ext.h"
#include "uart.h"
#include "w5500_comm.h"
#include "gprs.h"
#include "ec20.h"
#include "ab13mqtt.h"

#include "cjson.h"
#include "mqtt_conn.h"
#include "datasave.h"
#include "process.h"
#include "control.h"

#include "radio_a7139.h"

/* Exported define -----------------------------------------------------------*/
/*调试串口号，为0时关闭打印输出*/
#define DEBUG               5

/*DEBUG 信息等级动态可设置使能*/
#define LOG_LEVEL_DYNAMIC   1

/*DEBUG信息默认等级,须设置为常量数字宏才能展开*/
#define LOG_LEVEL_DEF       4

/*GPRS串口号*/
#define GPRS_UART_PORT	    1

/*4G串口号*/
#define M4G_UART_PORT	    2

/*定义DFU标记BKP寄存器*/
#define DFU_BKP             (BKP->DR10)

/*定义复位标识记数BKP*/
#define NRST_BKP            (BKP->DR9)
#define IWDG_BKP            (BKP->DR8)
#define SWRST_BKP           (BKP->DR7)
#define PORRST_BKP          (BKP->DR6)

/*使能flash读写保护*/
#define FLASH_WRP_EN        0

/*使能硬件看门狗*/
#define IWDG_HW_EN          0

#define PRODUCT_CHOSE       1

#define PROJECT             "AD-07"
#define VERSION             "AD-07_FM_V1.0"
#define VERSION_HARDWARE    "AD-07_HD_V1.0"


#define MQTT_TLS_EN         0
#define MQTT_SERVER_DEF     "admin.littlesmall.shop"
#define MQTT_PORT_DEF       1883
#define MQTT_USER_DEF       "xiaomo"
#define MQTT_PWD_DEF        "xiaomo"


#define UART1_RECEVIE_BUFFER_SIZE   256
#define UART2_RECEVIE_BUFFER_SIZE   2048
#define UART3_RECEVIE_BUFFER_SIZE   256
#define UART4_RECEVIE_BUFFER_SIZE   256
#define UART5_RECEVIE_BUFFER_SIZE   1024
/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
#define DBG_LEVEL_ERR           1
#define DBG_LEVEL_WAR           2
#define DBG_LEVEL_LOG           3
#define DBG_LEVEL_INFO          4
#define DBG_LEVEL_DEBUG         5
#define DBG_LEVEL_TEMP          6

/* Exported macro ------------------------------------------------------------*/

/*启用DEBUG信息*/
#if DEBUG > 0

/*DEBUG 信息等级动态*/
#if LOG_LEVEL_DYNAMIC > 0
#define DBG_ERR(format, ...)    CMD_Printf_Level(DBG_LEVEL_ERR, "error> file:%s line:%d "format"\r\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define DBG_WAR(format, ...)    CMD_Printf_Level(DBG_LEVEL_WAR, "warring> "format"\r\n", ##__VA_ARGS__)
#define DBG_LOG(format, ...)    CMD_Printf_Level(DBG_LEVEL_LOG, "log> "format"\r\n", ##__VA_ARGS__)
#define DBG_INFO(format, ...)   CMD_Printf_Level(DBG_LEVEL_INFO, "inf> "format"\r\n", ##__VA_ARGS__)
#define DBG_DBG(format, ...)    CMD_Printf_Level(DBG_LEVEL_DEBUG, "dbg> "format"\r\n", ##__VA_ARGS__)
#define DBG_TEMP(format, ...)   CMD_Printf_Level(DBG_LEVEL_TEMP, "temp> "format"\r\n", ##__VA_ARGS__)

/*DEBUG 信息等级静态*/
#else
#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_ERR))
#define DBG_ERR(format, ...)    CMD_Printf("error> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_ERR(format, ...)
#endif

#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_WAR))
#define DBG_WAR(format, ...)    CMD_Printf("warring> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_WAR(format, ...)
#endif

#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_LOG))
#define DBG_LOG(format, ...)    CMD_Printf("log> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_LOG(format, ...)
#endif

#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_INFO))
#define DBG_INFO(format, ...)   CMD_Printf("inf> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_INFO(format, ...)
#endif

#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_DEBUG))
#define DBG_DBG(format, ...)    CMD_Printf("dbg> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_DBG(format, ...)
#endif

#if ((LOG_LEVEL_DEF > 0) && (LOG_LEVEL_DEF <= DBG_LEVEL_TEMP))
#define DBG_TEMP(format, ...)   CMD_Printf("temp> "format"\r\n", ##__VA_ARGS__)
#else
#define DBG_TEMP(format, ...)
#endif
#endif

#define DBG_HEX(dat, len)                       CMD_HEX_Print(dat, len)
#define DBG_SEND(dat, len)                      CMD_SendData(dat, len)
#define THROW(str)                              UART_SendData_Direct(DEBUG, (uint8_t *)str, strlen_t(str))
#define THROW_PRINTF(format, ...)               UART_Printf_Direct(DEBUG, "> "format"\r\n",  ##__VA_ARGS__);

#define DBG_PRINT(level, format, ...)           CMD_Printf_Level(level, format"\r\n", ##__VA_ARGS__)
#define DBG_PRINTBUF(level, format, buf, len)   do \
    {\
        CMD_Printf_Level(level, "%s\r\n", format);\
        CMD_HEX_Print_Level(level, buf, len);\
    } while (0)
/*禁用DEBUG信息*/
#else
#define DBG_ERR(format, ...)
#define DBG_WAR(format, ...)
#define DBG_LOG(format, ...)
#define DBG_INFO(format, ...)
#define DBG_DBG(format, ...)
#define DBG_TEMP(format, ...)
#define DBG_SEND(dat, len)
#define THROW(str)
#define DBG_HEX(dat, len)
#define DBG_PRINT(level, format, ...)
#define DBG_PRINTBUF(level, format, buf, len)

#endif

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/



#endif

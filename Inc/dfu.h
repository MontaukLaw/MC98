/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file dfu.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.4.1
 * @brief 固件升级函数头文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _DFU_H
#define _DFU_H


/* Includes ------------------------------------------------------------------*/
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
/*定义默认的HTTP升级地址*/
#define DFU_HTTP_DEF        "update.afuiot.com"
#define DFU_HTTP_PATH_DEF   "/firmware/"PROJECT"/version.txt"

/*HTTP自动升级使能*/
#define DFU_HTTP_AUTO_EN    0
/*HTTP自动升级时间间隔，单位秒*/
#define DFU_HTTP_AUTO_TIME  86400
/*开机启动HTTP升级使能*/
#define DFU_HTTP_POWER_ON   1
/*定义升级超时时间*/
#define DFU_HTTP_TIMEOUT    120

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
/*DFU标记定义*/
#define DFU_FLAG_NONE           0
#define DFU_FLAG_OK             0x30
#define DFU_FLAG_CRC_FAULT      0x33
#define DFU_FLAG_FAULT          0x34
#define DFU_FLAG_ENTER          0x35
#define DFU_FLAG_FILE_FAULT     0x36
#define DFU_FLAG_FILE_SAME      0x37
#define DFU_FLAG_NOFILE         0x38

#define DFU_BUF_SIZE            (SFLASH_PAGE_SIZE * 2)

/* Exported macro ------------------------------------------------------------*/
#define DFU_SIZE_MAX()                  (SECTOR_DFU_SIZE * SFLASH_SECTOR_SIZE)
#define DFU_SAVE_ADDR()                 (SECTOR_ADDR(SECTOR_DFU_START))

#define FLASH_WRP_STATUS()              BIT_READ(DFU_BKP, 8)
#define FLASH_WRP_STATUS_SET()          BIT_SET(DFU_BKP, 8)
#define FLASH_WRP_STATUS_CLR()          BIT_CLEAR(DFU_BKP, 8)

#define IS_FLASH_WRP_ENABLE()           BIT_READ(DFU_BKP, 9)
#define FLASH_WRP_ENABLE()              BIT_SET(DFU_BKP, 9)
#define FLASH_WRP_ENABLE_CLR()          BIT_CLEAR(DFU_BKP, 9)

#define IWDG_HW_STATUS()                BIT_READ(DFU_BKP, 10)
#define IWDG_HW_STATUS_SET()            BIT_SET(DFU_BKP, 10)
#define IWDG_HW_STATUS_CLR()            BIT_CLEAR(DFU_BKP, 10)

#define IS_IWDG_HW_ENABLE()             BIT_READ(DFU_BKP, 11)
#define IWDW_HW_ENABLE()                BIT_SET(DFU_BKP, 11)
#define IWDW_HW_ENABLE_CLR()            BIT_CLEAR(DFU_BKP, 11)

#define DFU_WRITE_FALG(flag)            (DFU_BKP = flag | (DFU_BKP & 0xFF00))
#define DFU_READ_FALG()                 (DFU_BKP & 0x00FF)


/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
void DFU_Init(void);

void DFU_Poll(void);

#endif

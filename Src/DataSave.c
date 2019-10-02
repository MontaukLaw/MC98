
/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file DataSave.c
 * @version V1.0
 * @date 2016.12.16
 * @brief 数据存储处理函数.
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

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
WorkParam_t  WorkParam;

/* Private function prototypes -----------------------------------------------*/
static void datasave_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * 业务处理初始化
 */
void DataSave_Init(void) {
    CMD_ENT_DEF(datasave, datasave_Console);
    Cmd_AddEntrance(CMD_ENT(datasave));
    DBG_LOG("DataSave Init.");
}

/**
 * 工作参数初始化，如存储数据有错或为空，则恢复默认参数.
 */
void WorkParam_Init(void) {
    uint16_t crc = 0;
    uint32_t addr = 0;
    addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
    SFlash_Read(addr, (uint8_t*)&WorkParam, OBJ_LEN(WorkParam_t));
    crc = CRC_16(0, (uint8_t*) & (WorkParam) + 4, OBJ_LEN(WorkParam_t) - 4);
    if (WorkParam.crc == crc && crc != 0 && crc != BIT16_MAX) {
        DBG_LOG("Work parameter sector1 load OK.");
        return;
    } else {
        addr = SECTOR_ADDR(WORK_PARAM_SECTOR + 1);
        SFlash_Read(addr, (uint8_t*)&WorkParam, OBJ_LEN(WorkParam_t));
        crc = CRC_16(0, (uint8_t*) & (WorkParam) + 4, OBJ_LEN(WorkParam_t) - 4);
    }
    if (WorkParam.crc == crc && crc != 0 && crc != BIT16_MAX) {
        DBG_LOG("Work parameter sector2 load OK.");
        return;
    }
    if (WorkParam.version == BIT16_MAX || WorkParam.version == 0) {
        addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
        SFlash_Read(addr, (uint8_t*)&WorkParam, OBJ_LEN(WorkParam_t));
        WorkParam.version = 0;
        DBG_LOG("NO Work parameter!!!");
    } else {
        DBG_LOG("Work parameter Break!!!");
    }
    DBG_LOG("Work parameter set default.");
    /*大于最高版本时回复出厂设置*/
    if (WorkParam.version > 1) {
        WorkParam.version = 0;
    }
    if (WorkParam.version < 1) {
        DBG_LOG("Work version1 default.");
        WorkParam.version = 1;
        /*初始化mqtt参数*/
        strcpy(WorkParam.mqtt.MQTT_Server, MQTT_SERVER_DEF);
        WorkParam.mqtt.MQTT_Port = MQTT_PORT_DEF;
#if PRODUCT_CHOSE == 1
        strcpy(WorkParam.mqtt.MQTT_ClientID, "PAD07-000000");
#else
        strcpy(WorkParam.mqtt.MQTT_ClientID, "PAD08-000000");
#endif
        strcpy(WorkParam.mqtt.MQTT_UserName, MQTT_USER_DEF);
        strcpy(WorkParam.mqtt.MQTT_UserPWD, MQTT_PWD_DEF);
        WorkParam.mqtt.MQTT_Timout = MQTT_TIMEOUT_DEF;
        WorkParam.mqtt.MQTT_PingInvt = MQTT_PING_INVT_DEF;
        WorkParam.RF_DeviceID = 0x55AF19AB;
        /*初始化启动日志记录*/
        WorkParam.StartLogAddr = 0;
    }
    WorkParam_Save();
}

/**
 * 工作参数存储
 * @return
 */
BOOL WorkParam_Save(void) {
    uint16_t crc = 0;
    uint32_t addr = 0, len = OBJ_LEN(WorkParam_t);
    crc = CRC_16(0, (uint8_t*) & (WorkParam) + 4, len - 4);
    WorkParam.crc = crc;
    addr = SECTOR_ADDR(WORK_PARAM_SECTOR + 1);
    SFlash_EraseSectors_NotCheck(addr, 1);
    SFlash_Write_NotCheck(addr, (uint8_t*)&WorkParam, len);
    addr = SECTOR_ADDR(WORK_PARAM_SECTOR);
    SFlash_EraseSectors_NotCheck(addr, 1);
    DBG_LOG("WorkParam_Save()");
    return SFlash_Write_NotCheck(addr, (uint8_t*)&WorkParam, len);
}

/* Private function prototypes -----------------------------------------------*/

/**
 * 数据存储调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void datasave_Console(int argc, char* argv[]) {
    argv++;
    argc--;
    if (ARGV_EQUAL("workparam")) {
        DBG_LOG("WorkParam length:%u", OBJ_LEN(WorkParam));
    }
}


/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Process.c
 * @version V1.0
 * @date 2016.8.31
 * @brief 业锟斤拷锟竭硷拷锟斤拷锟斤拷锟斤拷锟斤拷.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 锟斤拷锟斤拷
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/


/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static char subscribeTopic[36], publishTopic[36], playid[48];

/* Private function prototypes -----------------------------------------------*/
static void ArrivePath(uint8_t* dat, uint16_t len);
static proret_t ArriveDataProc(char* cmd, cJSON* desired);
static BOOL CMD_Confirm_Rsp(char* ordermsgid, proret_t ret);
static void process_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * 业锟斤拷锟斤拷锟斤拷锟斤拷始锟斤拷
 */
void Process_Init(void) {
    strcpy(subscribeTopic, "/");
    strcat(subscribeTopic, PROJECT);
    strcat(subscribeTopic, "/");
    strcat(subscribeTopic, WorkParam.mqtt.MQTT_ClientID);
    strcpy(publishTopic, "/");
    strcat(publishTopic, PROJECT);
    CMD_ENT_DEF(process, process_Console);
    Cmd_AddEntrance(CMD_ENT(process));
    Subscribe_MQTT(subscribeTopic, QOS1, ArrivePath);
    DBG_LOG("Process Start.");
}

/**
 * 上传游戏结果
 *
 * @param result 1为抓中，0为未抓中
 */
void Updata_Gameoff(uint8_t result) {
    cJSON* desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddStringToObject(desired, "messageid", playid);
        cJSON_AddNumberToObject(desired, "result", result);
        CMD_Updata("CMD-107", desired);
    }
}

/**
 * 向服务器发送数据
 *
 * @param cmd     发送的命令
 * @param desired 子结构
 * @return 返回发送结果
 */
BOOL CMD_Updata(char* cmd, cJSON* desired) {
    BOOL ret = FALSE;
    char* s = NULL;
    char msgidBuf[20];
    cJSON* root = NULL;
    root = cJSON_CreateObject();
    if (root != NULL) {
        uitoa(HAL_GetTick(), msgidBuf);
        cJSON_AddStringToObject(root, "messageid", msgidBuf);
        cJSON_AddNumberToObject(root, "timestamp", RTC_ReadTick());
        cJSON_AddStringToObject(root, "cmd", cmd);
        cJSON_AddStringToObject(root, "deviceid", WorkParam.mqtt.MQTT_ClientID);
        cJSON_AddItemToObjectCS(root, "desired", desired);
        s = cJSON_PrintUnformatted(root);
        if (s != NULL) {
            DBG_INFO("CMD_Updata ts:%u,data:%s", HAL_GetTick(), s);
            ret = Publish_MQTT(publishTopic, QOS0, (uint8_t*)s, strlen(s));
            MMEMORY_FREE(s);
        }
        cJSON_Delete(root);
    }
    return ret;
}

/**
 * 上报门锁状态
 *
 * @param open   开门为TRUE
 */
void Report_DoorStatus(char* str) {
    cJSON* desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddStringToObject(desired, "status", str);
        CMD_Updata("CMD-103", desired);
    }
}


/**
 * 上报环套状态
 *
 * @param rfid
 * @param onoff
 */
void Report_Rope_RFID(char* rfid, uint8_t status, uint16_t voltage) {
    cJSON* desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddStringToObject(desired, "rfid", rfid);
        if (status == 0x01) {
            cJSON_AddStringToObject(desired, "status",  "release");
        } else if (status == 0x02) {
            cJSON_AddStringToObject(desired, "status",  "set");
        } else if (status == 0x04) {
            cJSON_AddStringToObject(desired, "status",  "escape");
        }
        cJSON_AddNumberToObject(desired, "voltage", voltage);
        CMD_Updata("CMD-104", desired);
    }
}

/**
 * 请求商品价格
 *
 * @param rfid   商品的RFID
 */
void Request_Price(char* rfid) {
    cJSON* desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddStringToObject(desired, "rfid", rfid);
        CMD_Updata("CMD-105", desired);
    }
}




/**
 * 接收处理
 *
 * @param dat    接收到的数据指针
 * @param len    数据长度
 */
static void ArrivePath(uint8_t* dat, uint16_t len) {
    uint32_t tsdiff = 0, ts = 0;
    char temp[48] = { 0, 0 };
    proret_t ret = ret_ok;
    cJSON* root = NULL, *msgid = NULL, *timestamp = NULL, *cmd = NULL, *desired = NULL, *deviceid = NULL;
    *(dat + len) = 0;
    DBG_INFO("ArrivePath ts:%u,data:%s", HAL_GetTick(), dat);
    root = cJSON_Parse((const char*)dat);
    if (root != NULL) {
        msgid = cJSON_GetObjectItem(root, "messageid");
        deviceid = cJSON_GetObjectItem(root, "deviceid");
        if (msgid != NULL && msgid->type == cJSON_String
                && deviceid != NULL &&
                (strcmp(deviceid->valuestring, WorkParam.mqtt.MQTT_ClientID) == 0
                 || deviceid->valuestring[0] == '0')) {
            timestamp = cJSON_GetObjectItem(root, "timestamp");
            desired = cJSON_GetObjectItem(root, "desired");
            cmd = cJSON_GetObjectItem(root, "cmd");
            /*RTC校时*/
            if (timestamp != NULL && timestamp->type == cJSON_Number) {
                ts = timestamp->valueint;
                tsdiff = RTC_ReadTick();
                tsdiff = abs(ts - tsdiff);
            }
            if (STR_EQUAL(cmd->valuestring, "CMD-07")) {
                strcpy(playid, msgid->valuestring);
            }
            if (tsdiff < 30 || STR_EQUAL(cmd->valuestring, "CMD-02")) {
                strcpy(temp, msgid->valuestring);
                ret = ArriveDataProc(cmd->valuestring, desired);
            } else {
                DBG_WAR("tsdiff error:%d", tsdiff);
            }
        }
        cJSON_Delete(root);
    }
    if (temp[0] != 0) {
        CMD_Confirm_Rsp(temp, ret);
    }
}

/**
 * 通讯板参数与状态上传
 */
void Status_Updata(void) {
    cJSON* desired = NULL;
    desired = cJSON_CreateObject();
    if (desired != NULL) {
        cJSON_AddNumberToObject(desired, "timestamp", RTC_ReadTick());
        cJSON_AddStringToObject(desired, "ip", WorkParam.mqtt.MQTT_Server);
        cJSON_AddNumberToObject(desired, "port", WorkParam.mqtt.MQTT_Port);
        cJSON_AddNumberToObject(desired, "heartbeat", WorkParam.mqtt.MQTT_PingInvt);
        cJSON_AddStringToObject(desired, "project", PROJECT);
        cJSON_AddStringToObject(desired, "firmware", VERSION);
        cJSON_AddStringToObject(desired, "hardware", VERSION_HARDWARE);
        cJSON_AddStringToObject(desired, "status", "ok");
        CMD_Updata("CMD-102", desired);
    }
}

/**
 * 消息处理
 *
 * @param desired 接收到的消息参数
 * @return 返回执行结果
 */
static proret_t ArriveDataProc(char* cmd, cJSON* desired) {
    proret_t ret = ret_ok;
    cJSON* child = NULL;
    uint8_t save = 0;
    if (STR_EQUAL(cmd, "CMD-01")) {
        child = cJSON_GetObjectItem(desired, "devicereset");
        if (child != NULL && child->type == cJSON_True) {
            NVIC_SystemReset();
        }
        child = cJSON_GetObjectItem(desired, "devicefactoryreset");
        if (child != NULL && child->type == cJSON_True) {
            WorkParam.mqtt.MQTT_Port = MQTT_PORT_DEF;
            WorkParam.mqtt.MQTT_PingInvt = MQTT_PING_INVT_DEF;
            strcpy(WorkParam.mqtt.MQTT_Server, MQTT_SERVER_DEF);
            strcpy(WorkParam.mqtt.MQTT_UserName, MQTT_USER_DEF);
            strcpy(WorkParam.mqtt.MQTT_UserPWD, MQTT_PWD_DEF);
            WorkParam_Save();
        }
        child = cJSON_GetObjectItem(desired, "deviceparamget");
        if (child != NULL && child->type == cJSON_True) {
            Status_Updata();
        }
        child = cJSON_GetObjectItem(desired, "scanstation");
        if (child != NULL && child->type == cJSON_True) {
            MCPU_ENTER_CRITICAL();
            ScanStationStart = TRUE;
            MCPU_EXIT_CRITICAL();
        }
    } else if (STR_EQUAL(cmd, "CMD-02")) {
        child = cJSON_GetObjectItem(desired, "timestamp");
        if (child != NULL && child->type == cJSON_Number) {
            timeRTC_t time;
            RTC_TickToTime(child->valueint, &time);
            RTC_SetTime(&time);
        }
        child = cJSON_GetObjectItem(desired, "ip");
        if (child != NULL && child->type == cJSON_String
                && !STR_EQUAL(WorkParam.mqtt.MQTT_Server, child->valuestring)) {
            strcpy(WorkParam.mqtt.MQTT_Server, child->valuestring);
            save++;
        }
        child = cJSON_GetObjectItem(desired, "username");
        if (child != NULL && child->type == cJSON_String
                && !STR_EQUAL(WorkParam.mqtt.MQTT_UserName, child->valuestring)) {
            strcpy(WorkParam.mqtt.MQTT_UserName, child->valuestring);
            save++;
        }
        child = cJSON_GetObjectItem(desired, "userpwd");
        if (child != NULL && child->type == cJSON_String
                && !STR_EQUAL(WorkParam.mqtt.MQTT_UserPWD, child->valuestring)) {
            strcpy(WorkParam.mqtt.MQTT_UserPWD, child->valuestring);
            save++;
        }
        child = cJSON_GetObjectItem(desired, "port");
        if (child != NULL && child->type == cJSON_Number
                && WorkParam.mqtt.MQTT_Port != child->valueint) {
            WorkParam.mqtt.MQTT_Port = child->valueint;
            save++;
        }
        child = cJSON_GetObjectItem(desired, "heartbeat");
        if (child != NULL && child->type == cJSON_Number
                && WorkParam.mqtt.MQTT_PingInvt != child->valueint) {
            WorkParam.mqtt.MQTT_PingInvt = child->valueint;
            save++;
        }
        if (save > 0) {
            WorkParam_Save();
        }
    }
    /*开门*/
    else if (STR_EQUAL(cmd, "CMD-03")) {
        child = cJSON_GetObjectItem(desired, "path");
        if (child != NULL && child->type == cJSON_String) {
            if (STR_EQUAL(child->valuestring, "in")) {
                DBG_LOG("door open get in");
                Control_DoorOpen(DOOR_OPEN_IN);
            } else if (STR_EQUAL(child->valuestring, "out")) {
                DBG_LOG("door open get out");
                Control_DoorOpen(DOOR_OPEN_OUT);
            } else if(STR_EQUAL(child->valuestring, "over")) {
                DBG_LOG("no money");
                GPRS_TTS("亲，余额不足，请及时充值欧");
            }
        }
    }
    /*下发商品价格*/
    else if (STR_EQUAL(cmd, "CMD-05")) {
        float price = 0;
        char* ptag = NULL, buf[64], pricebuf[32], *p;
        child = cJSON_GetObjectItem(desired, "price");
        if (child != NULL && child->type == cJSON_Number) {
            price = child->valuedouble;
        }
        child = cJSON_GetObjectItem(desired, "tag");
        if (child != NULL && child->type == cJSON_String) {
            ptag = child->valuestring;
        }
        if (ptag != NULL && price != NULL) {
            DBG_LOG("tag:%s, price:%.2f", ptag, price);
            snprintf(pricebuf, 32, "%.2f", price);
            p = pricebuf;
            while (*p && *p != '.') {
                p++;
            }
            if (*p == '.') {
                if (*(p + 1) == '0' && *(p + 2) == '0') {
                    *p = 0;
                } else if (*(p + 2) == 0) {
                    *(p + 2) = 0;
                }
            }
            snprintf(buf, 64, "%s,价格:%s元", ptag, pricebuf);
            GPRS_TTS(buf);
        }
    }
    return ret;
}

/**
 * 上传数据
 *
 * @param ordermsgid 上行的消息ID
 * @param ret        执行的结果
 * @return 向服务器上传命令执行结果
 */
static BOOL CMD_Confirm_Rsp(char* ordermsgid, proret_t ret) {
    BOOL r = FALSE;
    cJSON* bodydesired;
    bodydesired = cJSON_CreateObject();
    cJSON_AddStringToObject(bodydesired, "messageid", ordermsgid);
    cJSON_AddStringToObject(bodydesired, "ret", "OK");
    r = CMD_Updata("CMD-99", bodydesired);
    return r;
}

/**
 * 锟斤拷锟斤拷锟斤拷锟斤拷
 * @param argc 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷
 * @param argv 锟斤拷锟斤拷锟叫憋拷
 */
static void process_Console(int argc, char* argv[]) {
    cJSON* desired = NULL;
    argv++;
    argc--;
    if (ARGV_EQUAL("deviceid")) {
        if (argv[1] != NULL) {
            strcpy(WorkParam.mqtt.MQTT_ClientID, argv[1]);
            WorkParam_Save();
            strcpy(subscribeTopic, "/");
            strcat(subscribeTopic, PROJECT);
            strcat(subscribeTopic, "/");
            strcat(subscribeTopic, WorkParam.mqtt.MQTT_ClientID);
            strcpy(publishTopic, "/");
            strcat(publishTopic, PROJECT);
            strcat(publishTopic, "/");
            strcat(publishTopic, WorkParam.mqtt.MQTT_ClientID);
        }
        DBG_LOG("Device ID:%s", WorkParam.mqtt.MQTT_ClientID);
    } else if (ARGV_EQUAL("receive")) {
        ArrivePath((uint8_t*)argv[1], strlen(argv[1]));
    } else if (ARGV_EQUAL("send")) {
        desired = cJSON_Parse((const char*)argv[2]);
        if (desired != NULL) {
            CMD_Updata(argv[1], desired);
        }
    } else if (ARGV_EQUAL("door")) {
        Report_DoorStatus(argv[1]);
    } else if (ARGV_EQUAL("rope")) {
        Report_Rope_RFID(argv[1], (BOOL)uatoi(argv[2]), uatoi(argv[3]));
    } else if (ARGV_EQUAL("price")) {
        Request_Price(argv[1]);
    }
}

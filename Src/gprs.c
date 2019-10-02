/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file gprs.c
 * @version V1.0
 * @date 2016.4.1
 * @brief GPRS驱动函数文件,适用于M26.
 *
 * *********************************************************************
 * @note
 * 2016.12.18 增加socket接收回调.
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/
static const char* ipRetArray[] = {
    "IP INITIAL",
    "IP START",
    "IP CONFIG",
    "IP GPRSACT",
    "IP STATUS",
    "TCP CONNECTING",
    "CONNECT OK",
    "TCP CLOSING",
    "TCP CLOSED",
    "PDP DEACT",
    NULL
};

/*TCPIP连接状态*/
typedef enum {
    ip_status_INITIAL = 0,
    ip_status_START,
    ip_status_CONFIG,
    ip_status_IND,
    ip_status_GPRSACT,
    ip_status_STATUS,
    ip_status_CONNECTING,
    ip_status_CLOSED,
    ip_status_OK,
    PDP_DECT
} IP_Status_t;

typedef struct {
    char* APN;
    char* APN_User;
    char* APN_Pwd;
    char* ConnectAddress;
    uint16_t    ConnectPort;
    Sock_RecCBFun callback;
    uint8_t     rssi;
    uint16_t    ErrorCount;
    uint16_t    ConnectFailCount;
    IP_Status_t IP_Status;
    GPRS_Status_t status;
    BOOL        powerOnOff;
} GPRS_Param_t;

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define GPRS_PWR_ON()                   IO_L(GSM_EN)
#define GPRS_PWR_OFF()                  IO_H(GSM_EN)

#define GPRS_KEY_ON()                   IO_L(GSM_PWRKEY)
#define GPRS_KEY_OFF()                  IO_H(GSM_PWRKEY)

#define GPRS_SEND_DATA(dat, len)        UART_SendData(GPRS_UART_PORT, dat, len)
#define GPRS_SEND_AT(cmd)               UART_Printf(GPRS_UART_PORT, "AT+%s\r\n", cmd)
#define GPRS_AT_PRINTF(format, ...)     UART_Printf(GPRS_UART_PORT, "AT+"format"\r\n", ##__VA_ARGS__)

#define GPRS_WAIT_ACK(token, timeout)   WaitATRsp(token"\r\n", timeout)
#define GPRS_WAIT_TOKEN(token, timeout) WaitATRsp(token, timeout)

#define GPRS_OPT_SET(opt)               do {MCPU_ENTER_CRITICAL(); MASK_SET(GPRS_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define GPRS_OPT_CLEAR(opt)             do {MCPU_ENTER_CRITICAL(); MASK_CLEAR(GPRS_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define GPRS_OPT(opt)                   IS_MASK_SET(GPRS_Opt, opt)

/* Private variables ---------------------------------------------------------*/
static osMessageQId GPRS_TCPIP_SendQId;
static GPRS_Param_t GPRS_Param;
static uint32_t GPRS_Opt;
#if GPRS_CMD_EN > 0
static uint8_t CMD_Pipe = 0;
#endif

static osMutexId GPRS_MutexId;

static uint8_t* pRspBuf = NULL;

/* Private function prototypes -----------------------------------------------*/
void GPRS_Task(void const* argument);
static void GPRS_ManagerPoll(void);
static void GPRS_Intercept_Proc(void);
static void GPRS_TCPIP_ReceiveProc(char* pReceive);
static void GPRS_TCPIP_SendProc(void);
static BOOL GPRS_ModuleInit(void);
static BOOL GPRS_ModulePowerOn(void);
static BOOL GPRS_ModulePowerOff(void);
static BOOL ConnectShut(void);
static BOOL ConnectClose(void);
static BOOL ConnectStart(char* addr, uint16_t port);
static uint8_t GetRSSI(void);
static IP_Status_t GetIPStatus(void);
static BOOL GetNetWorkStatus(void);
static BOOL ReadPhoneNum(char* num);
static uint32_t HttpGet(char* url, char* readbuf, uint32_t buflen);
static uint16_t TCPIP_Send(uint8_t* data, uint16_t len);
static uint32_t FTP_StartGet(char* server, char* user, char* pwd, char* filenme);
static BOOL FTP_GetData(uint8_t* buf, uint16_t* getlen);
static void TTS_Play(char* text);
static char* WaitATRsp(char* token, uint16_t time);
static void GPRS_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * GPRS驱动初始化
 */
void GPRS_Init(void) {
    osMutexDef(GPRS);
    GPRS_MutexId = osMutexCreate(osMutex(GPRS));
    osMessageQDef(TCPIP_SendQ, GPRS_SEND_Q_SIZE, void*);
    GPRS_TCPIP_SendQId = osMessageCreate(osMessageQ(TCPIP_SendQ), NULL);
    osThreadDef(GPRS, GPRS_Task, GPRS_TASK_PRIO, 0, GPRS_TASK_STK_SIZE);
    osThreadCreate(osThread(GPRS), NULL);
    CMD_ENT_DEF(GPRS, GPRS_Console);
    Cmd_AddEntrance(CMD_ENT(GPRS));
#if GPRS_CMD_EN > 0
    CMD_Pipe = CMD_Pipe_Register((CMD_SendFun)GPRS_SocketSendData);
    DBG_LOG("GPRS CMD pipe is %d.", CMD_Pipe);
#endif
    DBG_LOG("GPRS Init");
}

/**
 * GPRS任务
 * @param argument 初始化参数
 */
void GPRS_Task(void const* argument) {
    TWDT_DEF(gprsTask, 60000);
    TWDT_ADD(gprsTask);
    TWDT_CLEAR(gprsTask);
    DBG_LOG("GPRS task start.");
//	UART_SetMonitor(DEBUG, GPRS_UART_PORT);
    while (1) {
        osDelay(5);
        TWDT_CLEAR(gprsTask);
        if (osMutexWait(GPRS_MutexId, 1000) == osOK) {
            GPRS_ManagerPoll();
            GPRS_Intercept_Proc();
            GPRS_TCPIP_SendProc();
            if (pRspBuf != NULL) {
                MMEMORY_FREE(pRspBuf);
                pRspBuf = NULL;
            }
            osMutexRelease(GPRS_MutexId);
        }
    }
}

/**
 * GPRS模块重启
 */
void GPRS_ReStart(void) {
    if (!GPRS_OPT(GPRS_OPT_RESET)) {
        GPRS_OPT_SET(GPRS_OPT_RESET);
    }
}

/**
 * GPRS模块开关机
 */
void GPRS_SetOnOff(BOOL onoff) {
    if (GPRS_Param.powerOnOff != onoff) {
        MCPU_ENTER_CRITICAL();
        GPRS_Param.powerOnOff = onoff;
        MCPU_EXIT_CRITICAL();
        if (onoff == FALSE) {
            GPRS_OPT_SET(GPRS_OPT_RESET);
        }
    }
}

/**
 * 读模块的状态
 * @return 返回模块的状态
 */
GPRS_Status_t GPRS_ReadStatus(void) {
    return GPRS_Param.status;
}

/**
 * GPRS socket 发送数据
 * @param data 数据指针
 * @param len  数据的长度
 * @return 返回发送结果
 */
int16_t GPRS_SocketSendData(uint8_t* data, uint16_t len) {
    osStatus res = osOK;
    uint8_t* pBuf = NULL, *p = NULL;
    if (len > GPRS_SEND_MAX_SIZE) {
        len = GPRS_SEND_MAX_SIZE;
    }
    if (GPRS_Param.IP_Status == ip_status_OK) {
        pBuf = MMEMORY_ALLOC(len + 2);
        if (pBuf != NULL) {
            p = pBuf;
            *(uint16_t*)p = len;
            p += 2;
            memcpy(p, data, len);
            res = osMessagePut(GPRS_TCPIP_SendQId, (uint32_t)pBuf, 1000);
            if (res == osOK) {
                return len;
            } else {
                DBG_LOG("GPRS socket Send Q fault:%d", (int)res);
                MMEMORY_FREE(pBuf);
                return 0;
            }
        }
    }
    return -1;
}

/**
 * 读GPRS模块的RSSI
 * @return 返回RSSI的值
 */
uint8_t GPRS_ReadRSSI(void) {
    uint8_t csq = 0;
    if (osMutexWait(GPRS_MutexId, 100) == osOK) {
        csq = GetRSSI();
        GPRS_Param.rssi = csq;
        osMutexRelease(GPRS_MutexId);
    } else {
        csq = GPRS_Param.rssi;
    }
    return csq;
}

/**
 * 读手机SIM卡电话号码
 * @param num  号码返回的指针
 * @return 返回读出结果
 */
BOOL GPRS_ReadPhoneNum(char* num) {
    BOOL ret = FALSE;
    if (num != NULL && osMutexWait(GPRS_MutexId, 100) == osOK) {
        ret = ReadPhoneNum(num);
        osMutexRelease(GPRS_MutexId);
    }
    return ret;
}

/**
 * HTTP get
 * @param url     链接地址
 * @param getbuf 读出的缓存
 * @param buflen  缓存的长度
 * @return 返回get结果
 */
uint32_t GPRS_HTTP_Get(char* url, char* getbuf, uint32_t buflen) {
    uint32_t ret = FALSE;
    if (GPRS_Param.status == gprs_status_online && url != NULL) {
        osMutexWait(GPRS_MutexId, osWaitForever);
        ret = HttpGet(url, getbuf, buflen);
        osMutexRelease(GPRS_MutexId);
    }
    return ret;
}

/**
 * 开始FTP下载
 * @param server FTP服务器地址
 * @param user   FTP账号
 * @param pwd    FTP密码
 * @param path   文件路径
 * @return get成功返回文件的长度，否则返回0
 */
uint32_t GPRS_FTP_StartGetFile(char* server, char* user, char* pwd, char* path) {
    uint32_t ret = FALSE;
    if (GPRS_Param.status == gprs_status_online && server != NULL && user != NULL && pwd != NULL && path != NULL) {
        osMutexWait(GPRS_MutexId, osWaitForever);
        ret = FTP_StartGet(server, user, pwd, path);
        osMutexRelease(GPRS_MutexId);
    }
    return ret;
}

/**
 * FTP 读出数据
 * @param buf    数据读存的缓存
 * @param getlen 数据读出的长度指针，传入需读入的长度，传出实际读出的长度
 * @return FTP会话存在时返回TRUE
 */
BOOL GPRS_FTP_GetData(uint8_t* buf, uint16_t* getlen) {
    BOOL ret = FALSE;
    if (buf != NULL && getlen != NULL && *getlen > 0) {
        osMutexWait(GPRS_MutexId, osWaitForever);
        ret = FTP_GetData(buf, getlen);
        osMutexRelease(GPRS_MutexId);
    }
    return ret;
}

/**
 * GPRS播放TTS
 *
 * @param text   待播放的文本
 * @return 播放成功返回TRUE
 */
BOOL GPRS_TTS(char* text) {
    if (text != NULL && GPRS_Param.status != gprs_status_poweroff && GPRS_Param.status != gprs_status_fault) {
        osMutexWait(GPRS_MutexId, osWaitForever);
        TTS_Play(text);
        osMutexRelease(GPRS_MutexId);
        return  TRUE;
    }
    return FALSE;
}

/**
 * 获取GPRS的连接状态
 * @retur  模块故障返回-1，
 *         无连接返回0,已连接返回1.
 */
int8_t GPRS_IsSocketConnect(void) {
    int8_t ret = -1;
    if (GPRS_Param.status != gprs_status_fault && GPRS_Param.status != gprs_status_nocard) {
        ret = 0;
    }
    if (GPRS_Param.status == gprs_status_online
            && GPRS_Param.IP_Status == ip_status_OK
            && !GPRS_OPT(GPRS_OPT_SET_SOCKET)) {
        ret = 1;
    }
    return ret;
}

/**
 * GPRS模块设置socket参数
 * @param server 服务器的IP地址或者域名名
 * @param port   服务器的端口号
 */
void GPRS_SetSocketParam(char* server, uint16_t port, Sock_RecCBFun callback) {
    if (server != GPRS_Param.ConnectAddress || port != GPRS_Param.ConnectPort || callback != GPRS_Param.callback) {
        MCPU_ENTER_CRITICAL();
        GPRS_Param.ConnectAddress = server;
        GPRS_Param.ConnectPort = port;
        GPRS_Param.callback = callback;
        GPRS_Opt |= GPRS_OPT_SET_SOCKET;
        MCPU_EXIT_CRITICAL();
    }
    if (server != NULL && port > 0) {
        /*GPRS开机*/
        GPRS_SetOnOff(TRUE);
    }
}

/* Private function prototypes ----------------------------------------------*/
/**
 * GPRS管理轮询.
 */
static void GPRS_ManagerPoll(void) {
    static uint32_t tsNet = 0, tsConnect = 0, tsStatus = 0;
    /*错误重启管理*/
    if (GPRS_Param.ErrorCount > 10 || GPRS_Param.ConnectFailCount > 10 || GPRS_OPT(GPRS_OPT_RESET)) {
        GPRS_OPT_CLEAR(GPRS_OPT_RESET);
        GPRS_Param.ErrorCount = 0;
        GPRS_Param.ConnectFailCount = 0;
        GPRS_ModulePowerOff();
        GPRS_Param.status = gprs_status_poweroff;
    }
    /*自动开机与状态管理*/
    switch (GPRS_Param.status) {
        case gprs_status_poweroff:
            if (GPRS_Param.powerOnOff) {
                GPRS_ModulePowerOn();
                DBG_LOG("GPRS module power on init.");
            }
            break;
        case gprs_status_poweron:
            GPRS_Param.status = (GetNetWorkStatus() == TRUE) ? gprs_status_online : gprs_status_nonet;
            GPRS_Param.rssi =  GetRSSI();
            break;
        case gprs_status_nonet:
            if (TS_IS_OVER(tsNet, 30000)) {
                TS_INIT(tsNet);
                if (GetNetWorkStatus() == FALSE) {
//					GPRS_Param.ConnectFailCount++;
                } else {
                    GPRS_Param.status = gprs_status_online;
                }
            }
            break;
        default:
            break;
    }
    if (GPRS_Param.status == gprs_status_online) {
        /*设置参数*/
        if (GPRS_OPT(GPRS_OPT_SET_SOCKET)) {
            if (GPRS_Param.IP_Status != ip_status_CLOSED && GPRS_Param.IP_Status != ip_status_INITIAL) {
                GPRS_Param.IP_Status = ip_status_CLOSED;
                ConnectClose();
                GPRS_OPT_CLEAR(GPRS_OPT_SET_SOCKET);
            } else {
                GPRS_OPT_CLEAR(GPRS_OPT_SET_SOCKET);
            }
        }
        /*维持TCP链路*/
        switch (GPRS_Param.IP_Status) {
            case ip_status_CLOSED:
            case ip_status_INITIAL:
                TS_INIT(tsConnect);
                if (GPRS_Param.ConnectAddress != NULL && GPRS_Param.ConnectPort > 0) {
                    ConnectStart(GPRS_Param.ConnectAddress, GPRS_Param.ConnectPort);
                    GPRS_Param.IP_Status = GetIPStatus();
                }
                break;
            case ip_status_STATUS:
            case PDP_DECT:
                ConnectShut();
                GPRS_Param.IP_Status = ip_status_INITIAL;
                break;
            case ip_status_OK:
                break;
            default:
                /*20秒未成功连接重连*/
                if (TS_IS_OVER(tsConnect, 20000)) {
                    ConnectShut();
                    GPRS_Param.IP_Status = ip_status_CLOSED;
                }
                if (TS_IS_OVER(tsStatus, 3000)) {
                    TS_INIT(tsStatus);
                    GPRS_Param.IP_Status = GetIPStatus();
                }
                break;
        }
    }
    /*GSM网络离线时复位ip连接状态*/
    else {
        GPRS_Param.IP_Status = ip_status_INITIAL;
    }
    /*socket未使用时关机省电*/
#if GPRS_POWER_SAVE_EN > 0
    static uint32_t tsPowerSave = 0;
    if (GPRS_Param.ConnectAddress != NULL) {
        TS_INIT(tsPowerSave);
    } else if (TS_IS_OVER(tsPowerSave, GPRS_POWER_SAVE_TIME * 1000) && GPRS_Param.status != gprs_status_poweroff) {
        DBG_LOG("GPRS socket not used, power save.");
        TS_INIT(tsPowerSave);
        GPRS_SetOnOff(FALSE);
    }
#endif
}

/**
 * GPRS 串口数据监听处理
 */
static void GPRS_Intercept_Proc(void) {
    char* p = NULL, *pbuf = NULL;
    uint16_t len = 0;
    len = UART_DataSize(GPRS_UART_PORT);
    if (len == 0) {
        return;
    }
    if ((UART_QueryByte(GPRS_UART_PORT, len - 1) == '\n'
            && UART_QueryByte(GPRS_UART_PORT, len - 2) == '\r' && UART_GetDataIdleTicks(GPRS_UART_PORT) >= 20)
            || UART_GetDataIdleTicks(GPRS_UART_PORT) >= GPRS_UART_REFRESH_TICK) {
        if (len >= (GPRS_RECEIVE_MAX_SIZE - 1)) {
            len = GPRS_RECEIVE_MAX_SIZE - 1;
        }
        pbuf = MMEMORY_ALLOC(len + 1);
        if (pbuf != NULL) {
            len = UART_ReadData(GPRS_UART_PORT, (uint8_t*)pbuf, len);
            *(pbuf + len) = 0;
            p = (char*)pbuf;
            /*连接状态更新*/
            if (strstr(p, "CLOSED") || strstr(p, "CONNECT OK")) {
                GPRS_Param.IP_Status = GetIPStatus();
            }
            /*网络状态更新*/
            if (strstr(p, "+CREG:")) {
                GPRS_Param.status = ((GetNetWorkStatus() == TRUE) ? gprs_status_online : gprs_status_nonet);
                GPRS_Param.rssi =  GetRSSI();
            }
            GPRS_TCPIP_ReceiveProc(pbuf);
            MMEMORY_FREE(pbuf);
        }
    }
}

/**
 * TCPIP数据接收收处理
 * @param pReceive 接收到的数据的指针
 */
static void GPRS_TCPIP_ReceiveProc(char* pReceive) {
    uint16_t len = 0;
    char* p = NULL;
    p = strstr(pReceive, "IPD,");
    while (p != NULL) {
        while (*p && *p++ != ',');
        len = uatoi(p);
        while (*p && *p++ != ':');
#if GPRS_CMD_EN > 0
        CMD_NewData(CMD_Pipe, (uint8_t*)p, len);
#endif
        if (GPRS_Param.callback != NULL) {
            GPRS_Param.callback((uint8_t*)p, len);
        }
        p = p + len + 1;
        p =  strstr(p, "IPD,");
    }
}

/**
 * TCPIP数据发送处理
 */
static void GPRS_TCPIP_SendProc(void) {
    osEvent evt;
    uint16_t len = 0;
    uint8_t* p = NULL;
    if (GPRS_Param.IP_Status == ip_status_OK) {
        evt = osMessageGet(GPRS_TCPIP_SendQId, 2);
        if (evt.status == osEventMessage) {
            p = evt.value.p;
            len = *(uint16_t*)(p);
            p += 2;
            if (len > 0) {
                if (TCPIP_Send(p, len) == FALSE) {
                    GPRS_Param.IP_Status = GetIPStatus();
                }
                MMEMORY_FREE(evt.value.p);
            }
        }
    }
}

/**
 * GPRS调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static BOOL GPRS_ModuleInit(void) {
    char* p = NULL;
    BOOL r = FALSE;
    GPRS_SEND_DATA("ATE1\r", 5);
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_DATA("ATV1\r", 5);
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_DATA("ATL1\r", 5);
    GPRS_WAIT_ACK("OK", 500);
    GPRS_WAIT_TOKEN("+CPIN:", 5000);
    GPRS_SEND_AT("QSCLK=0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("IFC=0,0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QLEDMODE=0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CMGF=1");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CSCS=\"GSM\"");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CREG=2");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CLIP=1");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QIMUX=0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QIHEAD=1");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QIPROMPT=1");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QISDE=1");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CNMI=2,2,0,0,0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CMUT=0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QAUDCH=0");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("QSIDET=128");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CLVL=10");
    GPRS_WAIT_ACK("OK", 100);
    GPRS_SEND_AT("CPIN?");
    p = GPRS_WAIT_TOKEN("+CPIN:", 1000);
    if (p != NULL) {
        if (strstr(p, "READY")) {
            r = TRUE;
        }
    }
    return r;
}

/**
 * GPRS上电开机
 */
static BOOL GPRS_ModulePowerOn(void) {
    BOOL r = FALSE;
    /*掉电延时确保模块开机成功*/
    GPRS_KEY_OFF();
    GPRS_PWR_OFF();
    osDelay(200);
    UART_SetBaudrate(GPRS_UART_PORT, GPRS_UART_BDR);
    GPRS_PWR_ON();
    GPRS_KEY_ON();
    if (GPRS_WAIT_ACK("RDY", 10000)) {
        GPRS_KEY_OFF();
        if (GPRS_ModuleInit()) {
            GPRS_Param.status = gprs_status_poweron;
        } else {
            GPRS_Param.status = gprs_status_nocard;
        }
    } else {
        GPRS_KEY_OFF();
        GPRS_SEND_DATA("AT\r", 3);
        GPRS_WAIT_ACK("OK", 100);
        GPRS_SEND_DATA("AT\r", 3);
        if (GPRS_WAIT_ACK("OK", 1000)) {
            GPRS_AT_PRINTF("IPR=%d", GPRS_UART_BDR);
            GPRS_WAIT_ACK("OK", 100);
            GPRS_SEND_DATA("AT&W\r", 5);
            GPRS_WAIT_ACK("OK", 200);
            if (GPRS_ModuleInit()) {
                GPRS_Param.status = gprs_status_poweron;
            } else {
                GPRS_Param.status = gprs_status_nocard;
            }
        } else {
            DBG_LOG("GPRS module fault.");
            GPRS_Param.status = gprs_status_fault;
            GPRS_KEY_OFF();
            GPRS_PWR_OFF();
        }
    }
    if (GPRS_Param.status == gprs_status_nocard) {
        DBG_LOG("GPRS module no SIM card.");
        GPRS_KEY_OFF();
//		GPRS_PWR_OFF();
    }
    return r;
}

/**
 * GPRS关机关电
 */
static BOOL GPRS_ModulePowerOff(void) {
    BOOL r = FALSE;
    GPRS_KEY_ON();
    if (GPRS_WAIT_ACK("POWER DOWN", 3000)) {
        r = TRUE;
    }
    GPRS_KEY_OFF();
    GPRS_PWR_OFF();
    return r;
}

/**
 * GPRS关闭连接与PDP场景
 * @return 返回关闭结果
 */
static BOOL ConnectShut(void) {
    GPRS_SEND_AT("QIDECT");
    return GPRS_WAIT_ACK("DEACT OK", 10000) ? TRUE : FALSE;
}

/**
 * GPRS关闭连接
 * @return 返回关闭结果
 */
static BOOL ConnectClose(void) {
    GPRS_SEND_AT("QICLOSE");
    return GPRS_WAIT_ACK("CLOSE OK", 1000) ? TRUE : FALSE;
}

/**
 * GPRS建立连接
 * @param addr 连接的IP地址或者域名
 * @param port 连接的端口号
 * @return 返回连接结果
 */
static BOOL ConnectStart(char* addr, uint16_t port) {
    BOOL r = FALSE;
    if (ip_check(addr, NULL) != RET_OK) {
        GPRS_SEND_AT("QIDNSIP=1");
        GPRS_WAIT_ACK("OK", 1000);
    } else {
        GPRS_SEND_AT("QIDNSIP=0");
        GPRS_WAIT_ACK("OK", 1000);
    }
    GPRS_AT_PRINTF("QIOPEN=\"TCP\",\"%s\",\"%d\"", addr, port);
    if (GPRS_WAIT_ACK("OK", 3000)) {
        r = TRUE;
    }
    return  r;
}

/**
 * 获取RSII的值
 * @return 返回RSSI的值
 */
static uint8_t GetRSSI(void) {
    uint8_t rssi = 0;
    char* p = NULL;
    GPRS_SEND_AT("CSQ");
    p = GPRS_WAIT_TOKEN("+CSQ:", 1000);
    if (p != NULL) {
        while (*p && !(isdigit(*p))) {
            p++;
        }
        rssi = uatoi(p);
    }
    return rssi;
}

/**
 * 获取RSII的值
 * @return 返回RSSI的值
 */
static IP_Status_t GetIPStatus(void) {
    uint8_t i = 0;
    char* p = NULL, *ps = NULL;
    GPRS_SEND_AT("QISTAT");
    p = GPRS_WAIT_TOKEN("STATE:", 10000);
    if (p != NULL) {
        while (*p && *p++ != ':');
        while (*p == ' ')
            p++;
        do {
            ps = (char*)ipRetArray[i];
            if (strncmp(p, ps, strlen_t(ps)) == 0) {
                break;
            }
            i++;
        } while (ps != NULL);
    }
    return (IP_Status_t)i;
}

/**
 * 获取网络状态
 * @return 返回网络注册状态
 */
static BOOL GetNetWorkStatus(void) {
    uint8_t net = 0;
    char* p = NULL;
    GPRS_SEND_AT("CREG?");
    p = GPRS_WAIT_TOKEN("+CREG:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        net = uatoi(p);
    }
    return (net == 1 || net == 5) ? TRUE : FALSE;
}

/**
 * 读手机SIM卡电话号码
 * @param num  号码返回的指针
 * @return 返回读出结果
 */
static BOOL ReadPhoneNum(char* num) {
    char* p = NULL;
    BOOL ret = FALSE;
    if (num != NULL && osMutexWait(GPRS_MutexId, 100) == osOK) {
        GPRS_SEND_AT("CNUM");
        p = GPRS_WAIT_ACK("+CNUM:", 200);
        if (p !=  NULL) {
            while (*p && *p++ != ',');
            while (*p && *p++ != '\"');
            while (*p && *p != '\"') {
                *num++ = *p++;
            }
        }
        ret = TRUE;
        osMutexRelease(GPRS_MutexId);
    }
    return ret;
}

/**
 * HTTP get
 * @param url     链接地址
 * @param readbuf 读出的缓存
 * @param buflen  缓存的长度
 * @return 返回get结果
 */
static uint32_t HttpGet(char* url, char* readbuf, uint32_t buflen) {
    uint32_t len = 0, ret = 0;
    char* p = NULL;
    GPRS_SEND_AT("SAPBR=1,1");
    GPRS_WAIT_ACK("OK", 10000);
    GPRS_SEND_AT("HTTPINIT");
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_SEND_AT("HTTPPARA=\"CID\",1");
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("HTTPPARA=\"URL\",\"%s\"", url);
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_SEND_AT("HTTPACTION=0");
    p = GPRS_WAIT_TOKEN("+HTTPACTION:", 15000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        /*返加200 OK*/
        if (uatoi(p) == 200) {
            GPRS_SEND_AT("HTTPREAD");
            p = GPRS_WAIT_TOKEN("+HTTPREAD:", 10000);
            if (p != NULL) {
                while (*p && *p++ != ':');
                p++;
                len = uatoi(p);
                if (len > buflen) {
                    len = buflen;
                }
                while (*p && *p++ != '\n');
                memcpy(readbuf, p, len);
                ret = len;
            }
        }
    }
    GPRS_SEND_AT("HTTPTERM");
    GPRS_WAIT_ACK("OK", 1000);
    GPRS_SEND_AT("SAPBR=0,1");
    GPRS_WAIT_ACK("OK", 10000);
    return ret;
}

/**
 * TCPIP发送数据
 * @param data 数据指针
 * @param len  数据长度
 * @return 返回发送结果
 */
static uint16_t TCPIP_Send(uint8_t* data, uint16_t len) {
    uint16_t sent = 0;
    if (data != NULL && len > 0) {
        GPRS_AT_PRINTF("QISEND=%d", len);
        if (GPRS_WAIT_TOKEN(">", 1000)) {
            GPRS_SEND_DATA(data, len);
            GPRS_SEND_DATA("\x1A", 1);
            if (GPRS_WAIT_ACK("SEND OK", 3000)) {
                sent = len;
            }
        }
    }
    return sent;
}

/**
 * 开始FTP下载
 * @param server FTP服务器地址
 * @param user   FTP账号
 * @param pwd    FTP密码
 * @param path   文件路径
 * @return get成功返回文件的长度，否则返回0
 */
static uint32_t FTP_StartGet(char* server, char* user, char* pwd, char* filename) {
    char* p = NULL;
    uint32_t size = 0;
    GPRS_SEND_AT("SAPBR=1,1");
    GPRS_WAIT_ACK("OK", 10000);
    GPRS_SEND_AT("FTPCID=1");
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("FTPSERV=\"%s\"", server);
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("FTPUN=\"%s\"", user);
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("FTPPW=\"%s\"", pwd);
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("FTPGETNAME=\"%s\"", filename);
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_AT_PRINTF("FTPGETPATH=\"%s\"", "/");
    if (GPRS_WAIT_ACK("OK", 1000) == NULL) {
        return 0;
    }
    GPRS_SEND_AT("FTPSIZE");
    p = GPRS_WAIT_TOKEN("+FTPSIZE:", 15000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        if (*p == '0') {
            while (*p && *p++ != ',');
            size = uatoi(p);
        }
    }
    if (size > 0) {
        GPRS_SEND_AT("FTPGET=1");
        GPRS_WAIT_ACK("OK", 1000);
        GPRS_WAIT_TOKEN("+FTPGET:", 15000);
    }
    return size;
}

/**
 * FTP 读出数据
 * @param buf    数据读存的缓存
 * @param getlen 数据读出的长度指针，传入需读入的长度，传出实际读出的长度
 * @return FTP会话存在时返回TRUE
 */
static BOOL FTP_GetData(uint8_t* buf, uint16_t* getlen) {
    BOOL ret = FALSE;
    char* p = NULL;
    uint16_t size = 0;
    GPRS_AT_PRINTF("FTPGET=2,%d", *getlen);
    p = GPRS_WAIT_TOKEN("+FTPGET:", 3000);
    if (p != NULL) {
        while (*p && *p++ != ':');
        p++;
        if (*p == '1') {
            while (*p && *p++ != ',');
            if (*p == '1') {
                ret = TRUE;
            }
        } else if (*p == '2')  {
            while (*p && *p++ != ',');
            size = uatoi(p);
            if (size > 0 && buf != NULL) {
                while (*p && *p++ != '\n');
                memcpy(buf, p, size);
            }
            ret = TRUE;
        }
    }
    *getlen = size;
    if (size == 0 && ret == FALSE) {
        GPRS_SEND_AT("SAPBR=0,1");
        GPRS_WAIT_ACK("OK", 10000);
    }
    return ret;
}

/**
 * 播报TTS
 *
 * @param text   待播报的文本
 */
static void  TTS_Play(char* text) {
    GPRS_AT_PRINTF("QTTS=2,\"%s\"", text);
    GPRS_WAIT_ACK("OK", 3000);
}

/**
 * GPRS等待AT命令返回
 * @param token 等待的token
 * @param time  等待的最长时间
 * @return 返回等待的token,超时返回NULL
 */
static char* WaitATRsp(char* token, uint16_t time) {
    uint16_t len = 0;
    uint32_t ts = 0;
    char* psearch = NULL;
    ts = HAL_GetTick();
    while (HAL_GetTick() - ts <= time) {
        len = UART_DataSize(GPRS_UART_PORT);
        if (len > 0 && UART_GetDataIdleTicks(GPRS_UART_PORT) >= 20) {
            /*避免未读出的语句影响后面的指令*/
            if (UART_QueryByte(GPRS_UART_PORT, len - 1) == '\n'
                    || *token == '>'
                    || UART_GetDataIdleTicks(GPRS_UART_PORT) >= GPRS_UART_REFRESH_TICK) {
                if (len >= (GPRS_RECEIVE_MAX_SIZE - 1)) {
                    len = GPRS_RECEIVE_MAX_SIZE - 1;
                }
                if (pRspBuf != NULL) {
                    MMEMORY_FREE(pRspBuf);
                    pRspBuf = NULL;
                }
                pRspBuf = MMEMORY_ALLOC(len + 1);
                if (pRspBuf != NULL) {
                    len = UART_ReadData(GPRS_UART_PORT, pRspBuf, len);
                    pRspBuf[len] = '\0';
                    psearch = (char*)SearchMemData(pRspBuf, (uint8_t*)token, len, strlen(token));
                    if (psearch != NULL || strstr((char*)pRspBuf, "ERROR")) {
                        break;
                    }
                }
            }
        }
        osDelay(2);
    }
    if (psearch == NULL) {
        GPRS_Param.ErrorCount++;
    } else {
        GPRS_Param.ErrorCount = 0;
    }
    return psearch;
}

/**
 * GPRS调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void GPRS_Console(int argc, char* argv[]) {
    char* pssl = NULL;
    uint16_t len = 0;
    argv++;
    argc--;
    if (strcmp(argv[0], "power") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            GPRS_SetOnOff(TRUE);
        } else if (strcmp(argv[1], "off") == 0) {
            GPRS_SetOnOff(FALSE);
        }
        DBG_LOG("GPRS power:%s", argv[1]);
    }  else if (strcmp(*argv, "status") == 0) {
        DBG_LOG("GPRS status:%d, IP status:%s.", (int)GPRS_Param.status, ipRetArray[(int)GPRS_Param.IP_Status]);
    }  else if (strcmp(*argv, "key") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            GPRS_KEY_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            GPRS_KEY_OFF();
        }
        DBG_LOG("GPRS power key:%s", argv[1]);
    } else if (strcmp(argv[0], "phonenum") == 0) {
        char* num = NULL;
        BOOL ret = FALSE;
        num = MMEMORY_ALLOC(16);
        ret = ReadPhoneNum(num);
        if (ret != FALSE) {
            DBG_LOG("GPRS read phonenum ret:%s", num);
        } else {
            DBG_LOG("GPRS read phonenum fail.");
        }
        MMEMORY_FREE(num);
    } else if (strcmp(*argv, "test") == 0) {
        argv++;
        argc--;
        osMutexWait(GPRS_MutexId, osWaitForever);
        if (strcmp(argv[0], "poweron") == 0) {
            DBG_LOG("GPRS test power on.");
            GPRS_ModulePowerOn();
        } else if (strcmp(argv[0], "poweroff") == 0) {
            DBG_LOG("GPRS test power off.");
            GPRS_ModulePowerOff();
        } else if (strcmp(argv[0], "csq") == 0) {
            uint8_t rssi = GetRSSI();
            DBG_LOG("GPRS test CSQ:%d.", rssi);
        } else if (strcmp(argv[0], "at") == 0) {
            GPRS_SEND_AT(argv[1]);
            DBG_LOG("gprs test send AT :%s.", argv[1]);
        } else if (strcmp(argv[0], "send") == 0) {
            GPRS_SEND_DATA((uint8_t*)argv[1], strlen(argv[1]));
            DBG_LOG("gprs test send data OK.");
        } else if (strcmp(argv[0], "httpget") == 0) {
            len = uatoi(argv[2]);
            if (len > 0) {
                pssl = MMEMORY_ALLOC(len);
                if (pssl != NULL) {
                    DBG_LOG("gprs test HTTP get start, bufflen:%d.", len);
                    len = HttpGet(argv[1], pssl, len);
                    if (len > 0) {
                        *(pssl + len) = 0;
                        DBG_LOG("test httpget ret:%s", pssl);
                    }
                    MMEMORY_FREE(pssl);
                }
            }
        } else if (strcmp(argv[0], "ftpstart") == 0) {
            DBG_LOG("gprs test FTP get start ret:%d.",
                    (int)FTP_StartGet(argv[1], argv[2], argv[3], argv[4]));
        } else if (strcmp(argv[0], "ftpget") == 0) {
            len = uatoi(argv[1]);
            DBG_LOG("gprs test FTP get data len: %d, ret:%d.",
                    len,
                    (int)FTP_GetData(NULL, &len));
        } else  if (strcmp(argv[0], "power") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                GPRS_PWR_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                GPRS_PWR_OFF();
            }
            DBG_LOG("GPRS test power:%s", argv[1]);
        } else  if (strcmp(argv[0], "tts") == 0) {
            TTS_Play(argv[1]);
            DBG_LOG("TTS_Play done.");
        }
        osMutexRelease(GPRS_MutexId);
    }
}


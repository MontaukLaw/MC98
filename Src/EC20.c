/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file M4G.c
 * @version V1.0
 * @date 2016.4.1
 * @brief M4G驱动函数文件,适用于M26.
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
/*TCPIP连接状态*/
typedef enum {
    ip_status_initial = 0,
    ip_status_Opening,
    ip_status_Connected,
    ip_status_Listening,
    ip_status_Closing,
    ip_status_Closed
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
    M4G_Status_t status;
    BOOL        powerOnOff;
} M4G_Param_t;

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define M4G_PWR_ON()                   IO_L(M4G_EN)
#define M4G_PWR_OFF()                  IO_H(M4G_EN)

#define M4G_KEY_ON()                   IO_L(M4G_PWRKEY)
#define M4G_KEY_OFF()                  IO_H(M4G_PWRKEY)

#define M4G_SEND_DATA(dat, len)        UART_SendData(M4G_UART_PORT, dat, len)
#define M4G_SEND_AT(cmd)               UART_Printf(M4G_UART_PORT, "AT+%s\r\n", cmd)
#define M4G_AT_PRINTF(format, ...)     UART_Printf(M4G_UART_PORT, "AT+"format"\r\n", ##__VA_ARGS__)

#define M4G_WAIT_ACK(token, timeout)   WaitATRsp(token"\r\n", timeout)
#define M4G_WAIT_TOKEN(token, timeout) WaitATRsp(token, timeout)

#define M4G_OPT_SET(opt)               do {MCPU_ENTER_CRITICAL(); MASK_SET(M4G_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define M4G_OPT_CLEAR(opt)             do {MCPU_ENTER_CRITICAL(); MASK_CLEAR(M4G_Opt, opt); MCPU_EXIT_CRITICAL();} while(0)
#define M4G_OPT(opt)                   IS_MASK_SET(M4G_Opt, opt)

/* Private variables ---------------------------------------------------------*/
static osMessageQId M4G_TCPIP_SendQId;
static M4G_Param_t M4G_Param;
static uint32_t M4G_Opt;

static osMutexId M4G_MutexId;

static uint8_t* pRspBuf = NULL;

/* Private function prototypes -----------------------------------------------*/
void M4G_Task(void const* argument);
static void M4G_ManagerPoll(void);
static void M4G_Intercept_Proc(void);
static void M4G_TCPIP_ReceiveProc(char* pReceive);
static void M4G_TCPIP_SendProc(void);
static BOOL M4G_ModuleInit(void);
static BOOL M4G_ModulePowerOn(void);
static BOOL M4G_ModulePowerOff(void);
static BOOL ConnectShut(void);
static BOOL ConnectClose(void);
static BOOL ConnectStart(char* addr, uint16_t port);
static uint8_t GetRSSI(void);
static IP_Status_t GetIPStatus(void);
static BOOL GetNetWorkStatus(void);
static BOOL ReadPhoneNum(char* num);
static uint16_t TCPIP_Send(uint8_t* data, uint16_t len);
static void TTS_Play(char* text);
static char* WaitATRsp(char* token, uint16_t time);
static void M4G_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * M4G驱动初始化
 */
void M4G_Init(void) {
    osMutexDef(M4G);
    M4G_MutexId = osMutexCreate(osMutex(M4G));
    osMessageQDef(TCPIP_SendQ, M4G_SEND_Q_SIZE, void*);
    M4G_TCPIP_SendQId = osMessageCreate(osMessageQ(TCPIP_SendQ), NULL);
    osThreadDef(M4G, M4G_Task, M4G_TASK_PRIO, 0, M4G_TASK_STK_SIZE);
    osThreadCreate(osThread(M4G), NULL);
    CMD_ENT_DEF(ec20, M4G_Console);
    Cmd_AddEntrance(CMD_ENT(ec20));
#if M4G_CMD_EN > 0
    CMD_Pipe = CMD_Pipe_Register((CMD_SendFun)M4G_SocketSendData);
    DBG_LOG("M4G CMD pipe is %d.", CMD_Pipe);
#endif
    DBG_LOG("M4G Init");
}

/**
 * M4G任务
 * @param argument 初始化参数
 */
void M4G_Task(void const* argument) {
    TWDT_DEF(M4GTask, 60000);
    TWDT_ADD(M4GTask);
    TWDT_CLEAR(M4GTask);
    DBG_LOG("M4G task start.");
    UART_SetRemapping(DEBUG, M4G_UART_PORT);
    while (1) {
        osDelay(5);
        TWDT_CLEAR(M4GTask);
        if (osMutexWait(M4G_MutexId, 1000) == osOK) {
            M4G_ManagerPoll();
            M4G_Intercept_Proc();
            M4G_TCPIP_SendProc();
            if (pRspBuf != NULL) {
                MMEMORY_FREE(pRspBuf);
                pRspBuf = NULL;
            }
            osMutexRelease(M4G_MutexId);
        }
    }
}

/**
 * M4G模块重启
 */
void M4G_ReStart(void) {
    if (!M4G_OPT(M4G_OPT_RESET)) {
        M4G_OPT_SET(M4G_OPT_RESET);
    }
}

/**
 * M4G模块开关机
 */
void M4G_SetOnOff(BOOL onoff) {
    if (M4G_Param.powerOnOff != onoff) {
        MCPU_ENTER_CRITICAL();
        M4G_Param.powerOnOff = onoff;
        MCPU_EXIT_CRITICAL();
        if (onoff == FALSE) {
            M4G_OPT_SET(M4G_OPT_RESET);
        }
    }
}

/**
 * 读模块的状态
 * @return 返回模块的状态
 */
M4G_Status_t M4G_ReadStatus(void) {
    return M4G_Param.status;
}

/**
 * M4G socket 发送数据
 * @param data 数据指针
 * @param len  数据的长度
 * @return 返回发送结果
 */
int16_t M4G_SocketSendData(uint8_t* data, uint16_t len) {
    osStatus res = osOK;
    uint8_t* pBuf = NULL, *p = NULL;
    if (len > M4G_SEND_MAX_SIZE) {
        len = M4G_SEND_MAX_SIZE;
    }
    if (M4G_Param.IP_Status == ip_status_Connected) {
        pBuf = MMEMORY_ALLOC(len + 2);
        if (pBuf != NULL) {
            p = pBuf;
            *(uint16_t*)p = len;
            p += 2;
            memcpy(p, data, len);
            res = osMessagePut(M4G_TCPIP_SendQId, (uint32_t)pBuf, 1000);
            if (res == osOK) {
                return len;
            } else {
                DBG_LOG("M4G socket Send Q fault:%d", (int)res);
                MMEMORY_FREE(pBuf);
                return 0;
            }
        }
    }
    return -1;
}

/**
 * 读M4G模块的RSSI
 * @return 返回RSSI的值
 */
uint8_t M4G_ReadRSSI(void) {
    uint8_t csq = 0;
    if (osMutexWait(M4G_MutexId, 100) == osOK) {
        csq = GetRSSI();
        M4G_Param.rssi = csq;
        osMutexRelease(M4G_MutexId);
    } else {
        csq = M4G_Param.rssi;
    }
    return csq;
}

/**
 * 读手机SIM卡电话号码
 * @param num  号码返回的指针
 * @return 返回读出结果
 */
BOOL M4G_ReadPhoneNum(char* num) {
    BOOL ret = FALSE;
    if (num != NULL && osMutexWait(M4G_MutexId, 100) == osOK) {
        ret = ReadPhoneNum(num);
        osMutexRelease(M4G_MutexId);
    }
    return ret;
}

/**
 * M4G播放TTS
 *
 * @param text   待播放的文本
 * @return 播放成功返回TRUE
 */
BOOL M4G_TTS(char* text) {
    if (text != NULL && M4G_Param.status != M4G_status_poweroff && M4G_Param.status != M4G_status_fault) {
        osMutexWait(M4G_MutexId, osWaitForever);
        TTS_Play(text);
        osMutexRelease(M4G_MutexId);
        return  TRUE;
    }
    return FALSE;
}

/**
 * 获取M4G的连接状态
 * @retur  模块故障返回-1，
 *         无连接返回0,已连接返回1.
 */
int8_t M4G_IsSocketConnect(void) {
    int8_t ret = -1;
    if (M4G_Param.status != M4G_status_fault && M4G_Param.status != M4G_status_nocard) {
        ret = 0;
    }
    if (M4G_Param.status == M4G_status_online
            && M4G_Param.IP_Status == ip_status_Connected
            && !M4G_OPT(M4G_OPT_SET_SOCKET)) {
        ret = 1;
    }
    return ret;
}

/**
 * M4G模块设置socket参数
 * @param server 服务器的IP地址或者域名名
 * @param port   服务器的端口号
 */
void M4G_SetSocketParam(char* server, uint16_t port, Sock_RecCBFun callback) {
    if (server != M4G_Param.ConnectAddress || port != M4G_Param.ConnectPort || callback != M4G_Param.callback) {
        MCPU_ENTER_CRITICAL();
        M4G_Param.ConnectAddress = server;
        M4G_Param.ConnectPort = port;
        M4G_Param.callback = callback;
        M4G_Param.IP_Status = ip_status_Closing;
        M4G_Opt |= M4G_OPT_SET_SOCKET;
        MCPU_EXIT_CRITICAL();
    }
    if (server != NULL && port > 0) {
        /*M4G开机*/
        M4G_SetOnOff(TRUE);
    }
}

/* Private function prototypes ----------------------------------------------*/
/**
 * M4G管理轮询.
 */
static void M4G_ManagerPoll(void) {
    static uint32_t tsNet = 0, tsConnect = 0, tsStatus = 0;
    /*错误重启管理*/
    if (M4G_Param.ErrorCount > 10 || M4G_Param.ConnectFailCount > 10 || M4G_OPT(M4G_OPT_RESET)) {
        M4G_OPT_CLEAR(M4G_OPT_RESET);
        M4G_Param.ErrorCount = 0;
        M4G_Param.ConnectFailCount = 0;
        M4G_ModulePowerOff();
        M4G_Param.status = M4G_status_poweroff;
    }
    /*自动开机与状态管理*/
    switch (M4G_Param.status) {
        case M4G_status_poweroff:
            if (M4G_Param.powerOnOff) {
                DBG_LOG("M4G module power on init.");
                M4G_ModulePowerOn();
            }
            break;
        case M4G_status_poweron:
            M4G_Param.status = (GetNetWorkStatus() == TRUE) ? M4G_status_online : M4G_status_nonet;
            M4G_Param.rssi =  GetRSSI();
            break;
        case M4G_status_nonet:
            if (TS_IS_OVER(tsNet, 30000)) {
                TS_INIT(tsNet);
                if (GetNetWorkStatus() == FALSE) {
                    M4G_Param.ConnectFailCount++;
                } else {
                    M4G_Param.status = M4G_status_online;
                }
            }
            break;
        default:
            break;
    }
    if (M4G_Param.status == M4G_status_online) {
        /*设置参数*/
        if (M4G_OPT(M4G_OPT_SET_SOCKET)) {
            if (M4G_Param.IP_Status != ip_status_Closed && M4G_Param.IP_Status != ip_status_initial) {
                ConnectClose();
                TS_INIT(tsStatus);
                M4G_Param.IP_Status = GetIPStatus();
                M4G_OPT_CLEAR(M4G_OPT_SET_SOCKET);
            } else {
                M4G_OPT_CLEAR(M4G_OPT_SET_SOCKET);
            }
        }
        /*维持TCP链路*/
        switch (M4G_Param.IP_Status) {
            case ip_status_Closed:
            case ip_status_initial:
                TS_INIT(tsConnect);
                TS_INIT(tsStatus);
                if (M4G_Param.ConnectAddress != NULL && M4G_Param.ConnectPort > 0) {
                    ConnectStart(M4G_Param.ConnectAddress, M4G_Param.ConnectPort);
                    M4G_Param.IP_Status = GetIPStatus();
                }
                break;
            case ip_status_Connected:
            case ip_status_Listening:
                break;
            case ip_status_Closing:
            case ip_status_Opening:
                /*20秒未成功连接重连*/
                if (TS_IS_OVER(tsConnect, 20000)) {
                    ConnectShut();
                    M4G_Param.IP_Status = ip_status_Closed;
                }
                if (TS_IS_OVER(tsStatus, 3000)) {
                    TS_INIT(tsStatus);
                    M4G_Param.IP_Status = GetIPStatus();
                    if (M4G_Param.IP_Status == ip_status_Closing) {
                        ConnectShut();
                    }
                }
            default:
                break;
        }
    }
    /*GSM网络离线时复位ip连接状态*/
    else {
        M4G_Param.IP_Status = ip_status_initial;
    }
    /*socket未使用时关机省电*/
#if M4G_POWER_SAVE_EN > 0
    static uint32_t tsPowerSave = 0;
    if (M4G_Param.ConnectAddress != NULL) {
        TS_INIT(tsPowerSave);
    } else if (TS_IS_OVER(tsPowerSave, M4G_POWER_SAVE_TIME * 1000) && M4G_Param.status != M4G_status_poweroff) {
        DBG_LOG("M4G socket not used, power save.");
        TS_INIT(tsPowerSave);
        M4G_SetOnOff(FALSE);
    }
#endif
}

/**
 * M4G 串口数据监听处理
 */
static void M4G_Intercept_Proc(void) {
    char* p = NULL, *pbuf = NULL;
    uint16_t len = 0;
    len = UART_DataSize(M4G_UART_PORT);
    if (len == 0) {
        return;
    }
    if ((UART_QueryByte(M4G_UART_PORT, len - 1) == '\n'
            && UART_QueryByte(M4G_UART_PORT, len - 2) == '\r' && UART_GetDataIdleTicks(M4G_UART_PORT) >= 20)
            || UART_GetDataIdleTicks(M4G_UART_PORT) >= M4G_UART_REFRESH_TICK) {
        if (len >= (M4G_RECEIVE_MAX_SIZE - 1)) {
            len = M4G_RECEIVE_MAX_SIZE - 1;
        }
        pbuf = MMEMORY_ALLOC(len + 1);
        if (pbuf != NULL) {
            len = UART_ReadData(M4G_UART_PORT, (uint8_t*)pbuf, len);
            *(pbuf + len) = 0;
            p = (char*)pbuf;
            /*连接状态更新*/
            if (strstr(p, "+QIURC: \"closed\"") || strstr(p, "+QIOPEN:")) {
                M4G_Param.IP_Status = GetIPStatus();
            }
            /*网络状态更新*/
            if (strstr(p, "+CREG:")) {
                M4G_Param.status = ((GetNetWorkStatus() == TRUE) ? M4G_status_online : M4G_status_nonet);
                M4G_Param.rssi =  GetRSSI();
            }
            M4G_TCPIP_ReceiveProc(pbuf);
            MMEMORY_FREE(pbuf);
        }
    }
}

/**
 * TCPIP数据接收收处理
 * @param pReceive 接收到的数据的指针
 */
static void M4G_TCPIP_ReceiveProc(char* pReceive) {
    uint16_t len = 0;
    char* p = NULL;
    p = strstr(pReceive, "+QIURC: \"recv\"");
    while (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        len = uatoi(p);
        while (*p && *p++ != '\n');
        DBG_LOG("test len:%d", len);
#if M4G_CMD_EN > 0
        CMD_NewData(CMD_Pipe, (uint8_t*)p, len);
#endif
        if (M4G_Param.callback != NULL) {
            M4G_Param.callback((uint8_t*)p, len);
        }
        p = p + len + 1;
        p =  strstr(p, "IPD,");
    }
}

/**
 * TCPIP数据发送处理
 */
static void M4G_TCPIP_SendProc(void) {
    osEvent evt;
    uint16_t len = 0;
    uint8_t* p = NULL;
    if (M4G_Param.IP_Status == ip_status_Connected) {
        evt = osMessageGet(M4G_TCPIP_SendQId, 2);
        if (evt.status == osEventMessage) {
            p = evt.value.p;
            len = *(uint16_t*)(p);
            p += 2;
            if (len > 0) {
                if (TCPIP_Send(p, len) == FALSE) {
                    M4G_Param.IP_Status = GetIPStatus();
                }
                MMEMORY_FREE(evt.value.p);
            }
        }
    }
}

/**
 * M4G调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static BOOL M4G_ModuleInit(void) {
    char* p = NULL;
    BOOL r = FALSE;
    M4G_SEND_DATA("ATE1\r", 5);
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_DATA("ATV1\r", 5);
    M4G_WAIT_ACK("OK", 100);
    M4G_WAIT_TOKEN("+CPIN:", 5000);
    M4G_SEND_AT("QSCLK=0");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("IFC=0,0");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CMGF=1");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CSCS=\"GSM\"");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CREG=2");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CLIP=1");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CNMI=2,2,0,0,0");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CMUT=0");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("QSIDET=128");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CLVL=10");
    M4G_WAIT_ACK("OK", 100);
    M4G_SEND_AT("CPIN?");
    p = M4G_WAIT_TOKEN("+CPIN:", 1000);
    if (p != NULL) {
        if (strstr(p, "READY")) {
            r = TRUE;
        }
    }
    return r;
}

/**
 * M4G上电开机
 */
static BOOL M4G_ModulePowerOn(void) {
    BOOL r = FALSE;
    /*掉电延时确保模块开机成功*/
    M4G_KEY_OFF();
    M4G_PWR_OFF();
    osDelay(1000);
    UART_SetBaudrate(M4G_UART_PORT, M4G_UART_BDR);
    M4G_PWR_ON();
    M4G_KEY_ON();
    if (M4G_WAIT_ACK("RDY", 15000)) {
        M4G_KEY_OFF();
        if (M4G_ModuleInit()) {
            M4G_Param.status = M4G_status_poweron;
        } else {
            M4G_Param.status = M4G_status_nocard;
        }
    } else {
        M4G_KEY_OFF();
        M4G_SEND_DATA("AT\r\n", 4);
        M4G_WAIT_ACK("OK", 100);
        M4G_SEND_DATA("AT\r\n", 4);
        if (M4G_WAIT_ACK("OK", 1000)) {
            M4G_AT_PRINTF("IPR=%d", M4G_UART_BDR);
            M4G_WAIT_ACK("OK", 100);
            M4G_SEND_DATA("AT&W\r", 5);
            M4G_WAIT_ACK("OK", 200);
            if (M4G_ModuleInit()) {
                M4G_Param.status = M4G_status_poweron;
            } else {
                M4G_Param.status = M4G_status_nocard;
            }
        } else {
            DBG_LOG("M4G module fault.");
            M4G_Param.status = M4G_status_fault;
            M4G_KEY_OFF();
            M4G_PWR_OFF();
        }
    }
    if (M4G_Param.status == M4G_status_nocard) {
        DBG_LOG("M4G module no SIM card.");
        M4G_KEY_OFF();
        M4G_PWR_OFF();
    }
    return r;
}

/**
 * M4G关机关电
 */
static BOOL M4G_ModulePowerOff(void) {
    BOOL r = FALSE;
    M4G_KEY_ON();
    if (M4G_WAIT_ACK("POWER DOWN", 3000)) {
        r = TRUE;
    }
    M4G_KEY_OFF();
    M4G_PWR_OFF();
    return r;
}

/**
 * M4G关闭连接与PDP场景
 * @return 返回关闭结果
 */
static BOOL ConnectShut(void) {
    M4G_SEND_AT("QIDEACT=1");
    return M4G_WAIT_ACK("OK", 3000) ? TRUE : FALSE;
}

/**
 * M4G关闭连接
 * @return 返回关闭结果
 */
static BOOL ConnectClose(void) {
    M4G_SEND_AT("QICLOSE=0");
    return M4G_WAIT_ACK("CLOSE OK", 10000) ? TRUE : FALSE;
}

/**
 * M4G建立连接
 * @param addr 连接的IP地址或者域名
 * @param port 连接的端口号
 * @return 返回连接结果
 */
static BOOL ConnectStart(char* addr, uint16_t port) {
    char* p = NULL;
    M4G_SEND_AT("QIACT?");
    p = M4G_WAIT_TOKEN("+QIACT:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        if (*p == '1') {
            M4G_AT_PRINTF("QIOPEN=1,0,\"TCP\",\"%s\",%d,0,1", addr, port);
            if (M4G_WAIT_ACK("OK", 3000)) {
                return TRUE;
            }
        }
    }
    M4G_SEND_AT("QIACT=1");
    M4G_WAIT_ACK("OK", 3000);
    return FALSE;
}

/**
 * 获取RSII的值
 * @return 返回RSSI的值
 */
static uint8_t GetRSSI(void) {
    uint8_t rssi = 0;
    char* p = NULL;
    M4G_SEND_AT("CSQ");
    p = M4G_WAIT_TOKEN("+CSQ:", 1000);
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
    uint8_t ret = 5;
    char* p = NULL;
    M4G_SEND_AT("QISTATE=1,0");
    p = M4G_WAIT_TOKEN("+QISTATE:", 1000);
    if (p != NULL) {
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        while (*p && *p++ != ',');
        ret = uatoi(p);
    }
    return (IP_Status_t)ret;
}

/**
 * 获取网络状态
 * @return 返回网络注册状态
 */
static BOOL GetNetWorkStatus(void) {
    uint8_t net = 0;
    char* p = NULL;
    M4G_SEND_AT("CREG?");
    p = M4G_WAIT_TOKEN("+CREG:", 1000);
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
    if (num != NULL && osMutexWait(M4G_MutexId, 100) == osOK) {
        M4G_SEND_AT("CNUM");
        p = M4G_WAIT_ACK("+CNUM:", 200);
        if (p !=  NULL) {
            while (*p && *p++ != ',');
            while (*p && *p++ != '\"');
            while (*p && *p != '\"') {
                *num++ = *p++;
            }
        }
        ret = TRUE;
        osMutexRelease(M4G_MutexId);
    }
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
        M4G_AT_PRINTF("QISEND=0,%d", len);
        if (M4G_WAIT_TOKEN(">", 1000)) {
            M4G_SEND_DATA(data, len);
            M4G_SEND_DATA("\x1A", 1);
            if (M4G_WAIT_ACK("SEND OK", 3000)) {
                sent = len;
            }
        }
    }
    return sent;
}

/**
 * 播报TTS
 *
 * @param text   待播报的文本
 */
static void  TTS_Play(char* text) {
    M4G_AT_PRINTF("QTTS=2,\"%s\"", text);
    M4G_WAIT_ACK("OK", 3000);
}

/**
 * M4G等待AT命令返回
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
        len = UART_DataSize(M4G_UART_PORT);
        if (len > 0 && UART_GetDataIdleTicks(M4G_UART_PORT) >= 10) {
            /*避免未读出的语句影响后面的指令*/
            if ((UART_QueryByte(M4G_UART_PORT, len - 1) == '\n' && UART_QueryByte(M4G_UART_PORT, len - 2) == '\r')
                    || *token == '>'
                    || UART_GetDataIdleTicks(M4G_UART_PORT) >= M4G_UART_REFRESH_TICK) {
                if (len >= (M4G_RECEIVE_MAX_SIZE - 1)) {
                    len = M4G_RECEIVE_MAX_SIZE - 1;
                }
                if (pRspBuf != NULL) {
                    MMEMORY_FREE(pRspBuf);
                    pRspBuf = NULL;
                }
                pRspBuf = MMEMORY_ALLOC(len + 1);
                if (pRspBuf != NULL) {
                    len = UART_ReadData(M4G_UART_PORT, pRspBuf, len);
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
        M4G_Param.ErrorCount++;
    } else {
        M4G_Param.ErrorCount = 0;
    }
    return psearch;
}

/**
 * M4G调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void M4G_Console(int argc, char* argv[]) {
    argv++;
    argc--;
    if (strcmp(argv[0], "power") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M4G_PWR_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            M4G_PWR_OFF();
        }
        DBG_LOG("M4G power:%s", argv[1]);
    }  else if (strcmp(*argv, "status") == 0) {
        DBG_LOG("M4G status:%d, IP status:%d.", (int)M4G_Param.status, M4G_Param.IP_Status);
    }  else if (strcmp(*argv, "key") == 0) {
        if (strcmp(argv[1], "on") == 0) {
            M4G_KEY_ON();
        } else if (strcmp(argv[1], "off") == 0) {
            M4G_KEY_OFF();
        }
        DBG_LOG("M4G power key:%s", argv[1]);
    } else if (strcmp(argv[0], "phonenum") == 0) {
        char* num = NULL;
        BOOL ret = FALSE;
        num = MMEMORY_ALLOC(16);
        ret = ReadPhoneNum(num);
        if (ret != FALSE) {
            DBG_LOG("M4G read phonenum ret:%s", num);
        } else {
            DBG_LOG("M4G read phonenum fail.");
        }
        MMEMORY_FREE(num);
    } else if (strcmp(*argv, "test") == 0) {
        argv++;
        argc--;
        osMutexWait(M4G_MutexId, osWaitForever);
        if (strcmp(argv[0], "poweron") == 0) {
            DBG_LOG("M4G test power on.");
            M4G_ModulePowerOn();
        } else if (strcmp(argv[0], "poweroff") == 0) {
            DBG_LOG("M4G test power off.");
            M4G_ModulePowerOff();
        } else if (strcmp(argv[0], "csq") == 0) {
            uint8_t rssi = GetRSSI();
            DBG_LOG("M4G test CSQ:%d.", rssi);
        } else if (strcmp(argv[0], "at") == 0) {
            M4G_SEND_AT(argv[1]);
            DBG_LOG("M4G test send AT :%s.", argv[1]);
        } else if (strcmp(argv[0], "send") == 0) {
            M4G_SEND_DATA((uint8_t*)argv[1], strlen(argv[1]));
            DBG_LOG("M4G test send data OK.");
        } else if (strcmp(argv[0], "power") == 0) {
            if (strcmp(argv[1], "on") == 0) {
                M4G_PWR_ON();
            } else if (strcmp(argv[1], "off") == 0) {
                M4G_PWR_OFF();
            }
            DBG_LOG("M4G test power:%s", argv[1]);
        } else if (strcmp(argv[0], "tts") == 0) {
            TTS_Play(argv[1]);
            DBG_LOG("TTS_Play done.");
        } else if (strcmp(argv[0], "connectshut") == 0) {
            ConnectShut();
            DBG_LOG("ConnectShut done.");
        } else if (strcmp(argv[0], "errorcount") == 0) {
            M4G_Param.ErrorCount = uatoi(argv[1]);
            DBG_LOG("ErrorCount set done.");
        }
        osMutexRelease(M4G_MutexId);
    }
}


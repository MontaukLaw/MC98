
/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file system.c
 * @version V1.0
 * @date 2016.12.18
 * @brief sytem函数头文件.
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
typedef struct {
    char* addr;
    uint16_t port;
} socketParam_t;

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static osMutexId    SockRecMutexId;
static socketParam_t volatile socketPar;
static BOOL   SockLock = FALSE;
static FIFO_t SockRecFIFO;
static uint8_t SockFIFO_Buffer[SOCKET_FIFO_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void SocketRec(uint8_t* dat, uint16_t len);

static void ResetFlag_init(void);

static void system_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * System初始化
 */
void System_Init(void) {
    uint8_t i = 0;
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    /*初始化重启计数*/
    ResetFlag_init();
    /*FLASH 读写保护与硬件看门狗使能*/
#if FLASH_WRP_EN == 1
    if (FLASH_WRP_STATUS() == 1) {
        FLASH_WRP_ENABLE();
        DBG_LOG("System will reset to enable flash write read protect.");
        i++;
    }
#else
    DBG_LOG("Firmwave not enable flash write read protect.");
#endif
#if IWDG_HW_EN == 1
    if (IWDG_HW_STATUS() == 1) {
        IWDW_HW_ENABLE();
        DBG_LOG("System will reset to enable hardware IWDG.");
        i++;
    }
#else
    DBG_LOG("Firmwave not enable hardware IWDG.");
#endif
    if (i > 0) {
        NVIC_SystemReset();
    }
    FIFO_Init(&SockRecFIFO, SockFIFO_Buffer, SOCKET_FIFO_SIZE);
    osMutexDef(SOCK_REC);
    SockRecMutexId = osMutexCreate(osMutex(SOCK_REC));
    CMD_ENT_DEF(system, system_Console);
    Cmd_AddEntrance(CMD_ENT(system));
    DBG_LOG("System Init.");
}

/**
 * socket锁使能
 */
void System_SockLockEn(BOOL lock) {
    MCPU_ENTER_CRITICAL();
    SockLock = lock;
    MCPU_EXIT_CRITICAL();
}

/**
 * 查询socket是否上锁
 */
BOOL System_SockIsLock(void) {
    return SockLock;
}

/**
 * socket发送数据
 * @param data 数据指针
 * @param len  数据长度
 * @return 返回发送的长度，发送失败返回-1
 */
int16_t System_SockSend(uint8_t* data, uint16_t len) {
    int16_t sent = -1;
    sent = System_SockIsConnected(NULL, NULL);
#if NET_GPRS_EN > 0
    if (sent == NET_GPRS) {
        sent = GPRS_SocketSendData(data, len);
    }
#endif
#if NET_WIFI_EN > 0
    if (sent == NET_WIFI) {
        sent = WIFI_SocketSendData(data, len);
    }
#endif
#if NET_4G_EN > 0
    if (sent == NET_4G) {
        sent = M4G_SocketSendData(data, len);
    }
#endif
#if NET_LAN_EN > 0
    if (sent == NET_LAN) {
        sent = LAN_SocketSendData(data, len);
    }
#endif
    if (sent <= 0) {
        sent = -1;
    }
    return sent;
}

/**
 * socket读出数据
 * @param data 读出的数据指针
 * @param len  读出的长度
 * @return 返回实际读出的长度，设备故障返回-1
 */
int16_t System_SockRecv(uint8_t* data, uint16_t len) {
    int16_t rc = 0;
    osMutexWait(SockRecMutexId, osWaitForever);
    rc = FIFO_Read(&SockRecFIFO, data, len);
    osMutexRelease(SockRecMutexId);
    if (rc > 0) {
        DBG_INFO("System_SockRecv len:%u", len);
    }
    if (rc == 0 && System_SockIsConnected(NULL, NULL) == -1) {
        rc = -1;
    }
    return rc;
}

/**
 * 鏌ヨ??SOCKET FIFO缂撳瓨鐨勯暱搴?
 *
 * @return 杩斿洖闀垮害鍊?
 */
int16_t System_SockLength(void) {
    int16_t rc = 0;
    osMutexWait(SockRecMutexId, osWaitForever);
    rc = FIFO_Length(&SockRecFIFO);
    osMutexRelease(SockRecMutexId);
    if (rc == 0 && System_SockIsConnected(NULL, NULL) == -1) {
        rc = -1;
    }
    return rc;
}

/**
 * 设置socket连接参数
 * @param addr 连接的地址
 * @param port 连接的端口
 */
void System_SetSocket(char* addr, uint16_t port) {
    if (socketPar.addr != addr || socketPar.port != port) {
        MCPU_ENTER_CRITICAL();
        socketPar.addr = addr;
        socketPar.port = port;
        MCPU_EXIT_CRITICAL();
    }
#if NET_GPRS_EN > 0
    GPRS_SetSocketParam(socketPar.addr, socketPar.port, SocketRec);
#endif
#if NET_WIFI_EN > 0
    WIFI_SetSocketParam(socketPar.addr, socketPar.port, SocketRec);
#endif
#if NET_4G_EN > 0
    M4G_SetSocketParam(socketPar.addr, socketPar.port, SocketRec);
#endif
#if NET_LAN_EN > 0
    LAN_SetSocketParam(socketPar.addr, socketPar.port, SocketRec);
#endif
}

/**
 * 建立socket连接
 * @param addr 连接的地址
 * @param port 连接的端口
 * @return
 *         连接成功返回对应的网络通道，无连接返回0，设备故障返回-1
 */
int8_t System_SockConnect(char* addr, uint16_t port) {
    int8_t ret = -1;
    uint32_t ts = HAL_GetTick();
    MCPU_ENTER_CRITICAL();
    socketPar.addr = addr;
    socketPar.port = port;
    MCPU_EXIT_CRITICAL();
#if NET_GPRS_EN > 0
    GPRS_SetSocketParam(addr, port, SocketRec);
#endif
#if NET_WIFI_EN > 0
    WIFI_SetSocketParam(addr, port, SocketRec);
#endif
#if NET_4G_EN > 0
    M4G_SetSocketParam(addr, port, SocketRec);
#endif
#if NET_LAN_EN > 0
    LAN_SetSocketParam(addr, port, SocketRec);
#endif
    while (!TS_IS_OVER(ts, SOCKET_CONNECT_TIMEOUT * 1000)) {
        osDelay(100);
        ret = System_SockIsConnected(NULL, NULL);
        if (ret != 0) {
            break;
        }
    }
    return ret;
}

/**
 * 建立socket连接
 * @param addr 连接的地址
 * @param port 连接的端口
 * @return
 *         连接成功返回对应的网络通道，无连接返回0，设备故障返回-1
 */
int8_t System_SockIsConnected(char** addr, uint16_t* port) {
    static uint32_t tsclose = 0;
    int8_t ret = 0, temp = -1;
    if (addr != NULL) {
        *addr = socketPar.addr;
    }
    if (port != NULL) {
        *port = socketPar.port;
    }
    if (socketPar.addr == NULL || socketPar.port == 0) {
        return 0;
    }
    /*网络使用的优先级，LAN > WIFI > GPRS*/
#if NET_GPRS_EN > 0
    temp = GPRS_IsSocketConnect();
    if (temp > 0) {
        ret = NET_GPRS;
    }
#endif
#if NET_WIFI_EN > 0
    temp = WIFI_IsSocketConnect();
    if (temp > 0) {
        ret = NET_WIFI;
    }
#endif
#if NET_4G_EN > 0
    temp = M4G_IsSocketConnect();
    if (temp > 0) {
        ret = NET_4G;
    }
#endif
#if NET_LAN_EN > 0
    temp = LAN_IsSocketConnect();
    if (temp > 0) {
        ret = NET_LAN;
    }
#endif
    /*连接成功后关闭未使用的socket*/
    if (ret > 0) {
        if (TS_IS_OVER(tsclose, 60000)) {
            TS_INIT(tsclose);
#if NET_LAN_EN > 0
            if (ret != NET_LAN) {
                LAN_SetSocketParam(NULL, 0, NULL);
            }
#endif
#if NET_WIFI_EN > 0
            if (ret != NET_WIFI) {
                WIFI_SetSocketParam(NULL, 0, NULL);
            }
#endif
#if NET_GPRS_EN > 0
            if (ret != NET_GPRS) {
                GPRS_SetSocketParam(NULL, 0, NULL);
            }
#endif
#if NET_4G_EN > 0
            if (ret != NET_4G) {
                M4G_SetSocketParam(NULL, 0, NULL);
            }
#endif
        }
    } else {
        TS_INIT(tsclose);
    }
    return ret;
}

void LED_FlashPoll(void){
    static uint32_t tsNET = 0;
    if(TS_IS_OVER(tsNET, 500)){
        LED_TOGGLE(TEST);
        TS_INIT(tsNET);
    }

}

#if 0 
/**
 * LED闪烁状态指示轮询
 * @author yang (2016/9/23)
 */
void LED_FlashPollOld(void) {
    int8_t err = 0;
    static uint32_t tsNET = 0, tsERR = 0, indexOn = 0, index = 0;
    LAN_Status_t statusLAN;
    /*网络状态指示灯*/
    if (TS_IS_OVER(tsNET, 20)) {
        TS_INIT(tsNET);
        if (MQTT_IsDataFlow() > 0) {
            LED_TOGGLE(NET);
        } else if (MQTT_IsConnected()) {
            LED_ON(NET);
        } else {
            LED_OFF(NET);
        }
    }
    statusLAN = LAN_ReadStatus();
    /*故障指示灯*/
    err = System_SockIsConnected(NULL, NULL);
    if (err <= 0) {
        if (statusLAN == lan_status_phy_offline) {
            err = 2;
        }  else {
            err = 1;
        }
    } else {
        err = 0;
    }
    if (err > 0) {
        if (TS_IS_OVER(tsERR, 250)) {
            TS_INIT(tsERR);
            if (indexOn++ < err * 2) {
                LED_TOGGLE(ERR);
            } else {
                LED_OFF(ERR);
            }
            /*5秒一个周期*/
            if (index++ >= 20) {
                LED_OFF(ERR);
                index = 0;
                indexOn = 0;
            }
        }
    } else {
        index = 0;
        indexOn = 0;
        LED_OFF(ERR);
    }
}

#endif

/**
 * 读出复位计数值
 * @param count 读出的结构体
 */
void Read_ResetCount(ResetCount_t* count) {
    count->iwdg = IWDG_BKP;
    count->nrst = NRST_BKP;
    count->por = PORRST_BKP;
    count->soft = SWRST_BKP;
}

/**
 * 清除复位计数器
 */
void Clear_ResetCount(void) {
    NRST_BKP = 0;
    IWDG_BKP = 0;
    SWRST_BKP = 0;
    PORRST_BKP = 0;
}

/**
 * 获取芯片的唯一ID
 * @return 返回唯一ID的值
 */
uint32_t Read_MCU_ID(void) {
    uint32_t* p = NULL;
#ifdef STM32F1
    p = (uint32_t*)(0x1ffff7e8);
#endif
#ifdef STM32F4
    p = (uint32_t*)(0x1ffff7a10);
#endif
    return (*p >> 1) + (*(p + 1) >> 2) + (*(p + 2) >> 3);
}

/**
 * 开机时记录启动日志
 */
void StartLog_Recoder(void) {
#if START_LOG_EN > 0
    char timebuf[24];
    uint8_t buf[64];
    LogBlock_t  block;
    uint32_t addr, ts = 0;
    addr = WorkParam.StartLogAddr;
    if (addr == 0) {
        addr = LOG_ADDR();
    } else {
        addr = SaveLog_NextAddr(addr);
    }
    /*查询复位前是否有任务溢出*/
    SFlash_Read(LOG_TASK_ADDR(), (uint8_t*)buf, 64);
    ts = *(uint32_t*)buf;
    if (ts > 0 && ts < BIT32_MAX) {
        block.tsFault = ts;
        strcpy(block.FaultTask, (char*)&buf[4]);
        SFlash_EraseSectors(LOG_TASK_ADDR(), 1);
    } else {
        block.tsFault = 0;
        block.FaultTask[0] = 0;
    }
    block.tsPwrOn = RTC_ReadTick();
    Read_ResetCount(&(block.ResetCount));
    block.crc = CRC_16(0, (uint8_t*) & (block.ResetCount), sizeof(LogBlock_t) - 2);
    RTC_TickToStr(block.tsPwrOn, timebuf);
    DBG_LOG("System start: %s, IWDG:%d, NRST:%d, PORD:%d, SOFT:%d", timebuf,
            block.ResetCount.iwdg, block.ResetCount.nrst, block.ResetCount.por, block.ResetCount.soft);
    if (block.tsFault > 0) {
        RTC_TickToStr(block.tsFault, timebuf);
        DBG_LOG("Last Task:%s Fault, TS:%u", block.FaultTask, timebuf);
    }
    MCPU_ENTER_CRITICAL();
    WorkParam.StartLogAddr = addr;
    MCPU_EXIT_CRITICAL();
    WorkParam_Save();
    if (addr % SFLASH_SECTOR_SIZE == 0) {
        SFlash_EraseSectors(addr, 1);
    }
    SFlash_Write(addr, (uint8_t*)&block, sizeof(block));
#endif
}

/**
 * 启动日志UART汇报
 */
void StartLog_UART(uint32_t tsb, uint32_t tse) {
    char timebuf[24];
    uint32_t addr;
    LogBlock_t block;
    addr = LOG_ADDR();
    while (addr < LOG_ADDR_MAX()) {
        if (SaveLog_GetBlock(addr, &block) && block.tsPwrOn >= tsb && block.tsPwrOn <= tse) {
            RTC_TickToStr(block.tsPwrOn, timebuf);
            DBG_LOG("TS:%u, time:%s, IWDG:%d, NRST:%d, PORD:%d, SOFT:%d", block.tsPwrOn, timebuf,
                    block.ResetCount.iwdg, block.ResetCount.nrst, block.ResetCount.por, block.ResetCount.soft);
            if (block.tsFault > 0) {
                RTC_TickToStr(block.tsFault, timebuf);
                DBG_LOG("Last Task:%s Fault, TS:%u, time:%s", block.FaultTask, block.tsFault, timebuf);
            }
        }
        osDelay(2);
        addr += LOG_SAVE_BLOCK_SIZE;
    }
    DBG_LOG("Startlog UART report finished");
}

/**
 * 获取日志存储的数据块
 * @param addr  存储地址
 * @param block 读出的数据块
 * @return 有效数据返回TRUE
 */
BOOL SaveLog_GetBlock(uint32_t addr, LogBlock_t* block) {
    SFlash_Read(addr, (uint8_t*)block, sizeof(LogBlock_t));
    if (CRC_16(0, (uint8_t*) & (block->ResetCount), sizeof(LogBlock_t) - 2) == block->crc) {
        return TRUE;
    }
    return FALSE;
}

/**
 * LOG保存时获取下一个数据块的地址值
 * @param addr 当前地址
 * @return 返回下一个数据块的地址
 */
uint32_t SaveLog_NextAddr(uint32_t addr) {
    addr += LOG_SAVE_BLOCK_SIZE;
    if (addr >= LOG_ADDR_MAX()) {
        addr = LOG_ADDR();
        DBG_INFO("Log NextAddr OverFlow!!!");
    }
    return addr;
}

/**
 * 用于在任务看门狗溢出时记录任务的名称
 * @param faultTask 溢出的任务名称
 */
void TaskFault_Save(uint32_t tsFault, char* faultTask) {
#if START_LOG_EN > 0
    uint32_t addr = LOG_TASK_ADDR();
    SFlash_EraseSectors(addr, 1);
    SFlash_Write(addr, (uint8_t*)&tsFault, 4);
    SFlash_Write(addr + 4, (uint8_t*)faultTask, strlen(faultTask) + 1);
#endif
}

/* Private function prototypes -----------------------------------------------*/

/**
 * system socket数据接收的回调函数
 */
static void SocketRec(uint8_t* dat, uint16_t len) {
    if (System_SockIsConnected(NULL, NULL) > 0) {
        osMutexWait(SockRecMutexId, osWaitForever);
        FIFO_Write(&SockRecFIFO, dat, len);
        osMutexRelease(SockRecMutexId);
    }
}

/**
 * 复位标识初始化
 */
static void ResetFlag_init(void) {
    ResetCount_t count;
    Read_ResetCount(&count);
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        count.iwdg++;
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        count.soft++;
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
        count.por++;
    }
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        count.nrst++;
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
    IWDG_BKP = count.iwdg;
    NRST_BKP = count.nrst;
    PORRST_BKP = count.por;
    SWRST_BKP = count.soft;
}

/** system调试命令
 *
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void system_Console(int argc, char* argv[]) {
    char* p = NULL;
    uint16_t d = 0;
    uint32_t ts1, ts2;
    ResetCount_t count;
    argv++;
    argc--;
    if (ARGV_EQUAL("reset")) {
        d = uatoi(argv[1]);
        DBG_LOG("System Will Reset %dms latter.", d);
        osDelay(d + 5);
        NVIC_SystemReset();
    } else if (ARGV_EQUAL("version")) {
        DBG_LOG("Project:%s, Firmware version:%s, Hardware version:%s",
                PROJECT,
                VERSION,
                VERSION_HARDWARE);
    } else if (ARGV_EQUAL("alloc")) {
        d = uatoi(argv[1]);
        p = MMEMORY_ALLOC(d);
        if (p != NULL) {
            MMEMORY_FREE(p);
            DBG_LOG("Memory alloc size:%d OK, address:0x%p", d, p);
        } else {
            DBG_LOG("Memory alloc size:%d Failed.", d);
        }
    } else if (ARGV_EQUAL("resetcount")) {
        Read_ResetCount(&count);
        DBG_LOG("System reset count, IWDG:%d, NRST:%d, PORD:%d, SOFT:%d",
                count.iwdg, count.nrst, count.por, count.soft);
    } else if (ARGV_EQUAL("resetcountclear")) {
        Clear_ResetCount();
        DBG_LOG("System reset count clear.");
    } else if (ARGV_EQUAL("heapfreesize")) {
        DBG_LOG("Heap Memory free size:%u", xPortGetFreeHeapSize());
    } else if (ARGV_EQUAL("mcuid")) {
        DBG_LOG("MCU ID:%#x", Read_MCU_ID());
    } else if (ARGV_EQUAL("startlog")) {
        if (argv[1] && argv[2]) {
            ts1 = uatoi(argv[1]);
            ts2 = uatoi(argv[2]);
            DBG_LOG("ts begin:%u, ts end:%u", ts1, ts2);
            StartLog_UART(ts1, ts2);
        }
    } else if (ARGV_EQUAL("faulttask")) {
        char* pfault = NULL;
        uint8_t buf[64];
        SFlash_Read(LOG_TASK_ADDR(), (uint8_t*)buf, 64);
        ts1 = *(uint32_t*)buf;
        if (ts1 > 0 && ts1 < BIT32_MAX) {
            pfault = (char*)&buf[4];
            DBG_LOG("Last start %s fault, TS:%u.", pfault, ts1);
        } else {
            DBG_LOG("Last start no task fault.");
        }
    } else if (ARGV_EQUAL("isconnect")) {
        DBG_LOG("System socket isconnect:%d.", System_SockIsConnected(NULL, NULL));
    } else if (ARGV_EQUAL("setsocket")) {
        static char server[32];
        static uint16_t port = 0;
        if (argv[1] && argv[2]) {
            strcpy(server, argv[1]);
            port = uatoi(argv[2]);
            System_SetSocket(server, port);
            DBG_LOG("System_SetSocket OK.");
        }
    } else if (ARGV_EQUAL("socketlock")) {
        System_SockLockEn((BOOL)uatoi(argv[1]));
        DBG_LOG("System_SockLockEn:%u", uatoi(argv[1]));
    }
}


/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file w5500_comm.c
 * @version V1.0
 * @date 2016.7.20
 * @brief W5500相关函数文件.
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
typedef struct
{
  char* MAC;
  char* IP;
  char* SN;
  char* GW;
  BOOL        DHCP_Enable;
  char* ConnectAddress;
  uint16_t    ConnectPort;
  Sock_RecCBFun callback;
  BOOL        ConnectStatus;
  uint16_t    SockFailCount;
  LAN_Status_t status;
} LAN_Param_t;


/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define W5500_CS_OFF()       IO_H(W5500_CS)
#define W5500_CS_ON()        IO_L(W5500_CS)

#define W5500_RST_OFF()      IO_H(W5500_RST)
#define W5500_RST_ON()       IO_L(W5500_RST)

#define IS_W5500_INT()       (IO_READ(W5500_INT) == 0)

#define LAN_OPT_SET(opt)               do {MCPU_ENTER_CRITICAL(); MASK_SET(LAN_Opt, opt); MCPU_EXIT_CRITICAL();}while(0)
#define LAN_OPT_CLEAR(opt)             do {MCPU_ENTER_CRITICAL(); MASK_CLEAR(LAN_Opt, opt); MCPU_EXIT_CRITICAL();}while(0)
#define LAN_OPT(opt)                   IS_MASK_SET(LAN_Opt, opt)

/* Private variables ---------------------------------------------------------*/
static osMutexId            w5500_MutexId;
static SPI_HandleTypeDef* w5500_SPI_Handle;

static uint8_t DHCP_Buffer[550], lenSet[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
static osMessageQId LAN_SendQId;
static LAN_Param_t LAN_Param;
static uint32_t LAN_Opt;

#if LAN_CMD_EN > 0
static uint8_t LAN_Pipe = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
void LAN_Task(void const* argumen);
static void LAN_ManagerPoll(void);
static void LAN_Socket_Manager(void);
static void LAN_Socket_SendRecProc(void);
static BOOL LAN_DNS(char* domain, uint8_t* ipout);
static void w5500_Mutex_get(void);
static void w5500_Mutex_free(void);
static void w5500_cs_on(void);
static void w5500_cs_off(void);
static uint8_t w5500_read_byte(void);
static void w5500_write_byte(uint8_t c);

static void LAN_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * W5500初始化
 */
void w5500_Init(SPI_HandleTypeDef* handle) {
  W5500_CS_OFF();
  W5500_RST_OFF();

  w5500_SPI_Handle = handle;

  __HAL_SPI_ENABLE(w5500_SPI_Handle);

  osMutexDef(W5500);
  w5500_MutexId = osRecursiveMutexCreate(osMutex(W5500));


  reg_wizchip_cris_cbfunc(w5500_Mutex_get, w5500_Mutex_free);
  reg_wizchip_cs_cbfunc(w5500_cs_on, w5500_cs_off);
  reg_wizchip_spi_cbfunc(w5500_read_byte, w5500_write_byte);

  osMessageQDef(LAN_SendQ, LAN_SEND_Q_SIZE, void*);
  LAN_SendQId = osMessageCreate(osMessageQ(LAN_SendQ), NULL);

  osThreadDef(LAN, LAN_Task, LAN_TASK_PRIO, 0, LAN_TASK_STK_SIZE);
  osThreadCreate(osThread(LAN), NULL);

#if LAN_CMD_EN > 0
  LAN_Pipe = CMD_Pipe_Register((CMD_SendFun)LAN_SocketSendData);
  DBG_LOG("LAN CMD pipe is %d.", LAN_Pipe);
#endif


  CMD_ENT_DEF(LAN, LAN_Console);
  Cmd_AddEntrance(CMD_ENT(LAN));

  DBG_LOG("W5500 LAN Init.");
}

/**
 * LAN任务
 * @param argument 初始化参数
 */
void LAN_Task(void const* argument) {
  TWDT_DEF(LANTask, 60000);
  TWDT_ADD(LANTask);
  TWDT_CLEAR(LANTask);

  uint32_t ts = HAL_GetTick();

  DBG_LOG("LAN task start");
  while (1) {
    osDelay(2);
    TWDT_CLEAR(LANTask);

    if (osRecursiveMutexWait(w5500_MutexId, 1000) == osOK) {
      LAN_ManagerPoll();
      LAN_Socket_SendRecProc();
      osRecursiveMutexRelease(w5500_MutexId);
    }
    if (HAL_GetTick() - ts >= 1000) {
      ts = HAL_GetTick();
      DHCP_time_handler();
    }
  }
}

/**
 * LAN socket 发送数据
 * @param data 数据指针
 * @param len  数据的长度
 * @return 返回发送结果
 */
int16_t LAN_SocketSendData(uint8_t* data, uint16_t len) {
  osStatus res = osOK;
  uint8_t* pBuf = NULL, *p = NULL;

  if (len > LAN_SEND_MAX_SIZE) {
    DBG_WAR("test LAN_SEND_MAX_SIZE overflow.");
    len = LAN_SEND_MAX_SIZE;
  }
  if (LAN_Param.ConnectStatus != FALSE) {
    pBuf = pvPortMalloc(len + 2);
    if (pBuf != NULL) {
      p = pBuf;
      *(uint16_t*)p = len;
      p += 2;
      memcpy(p, data, len);

      res = osMessagePut(LAN_SendQId, (uint32_t)pBuf, 1000);
      if (res == osOK) {
        return len;
      } else {
        DBG_LOG("LAN socket Send Q fault:%d", (int)res);
        vPortFree(pBuf);
        return 0;
      }
    }
  }
  return -1;
}

/**
 * 查询LAN socket连接是否正常
 * @return 连接正常返回TRUE
 */
int8_t LAN_IsSocketConnect(void) {
  int8_t ret = -1;

  if (LAN_Param.status != lan_status_fault) {
    ret = 0;
  }
  if (LAN_Param.ConnectStatus != FALSE
      && LAN_Param.status == lan_status_phy_dhcp_leased  //需添加不使用DHCP的情况
      && !LAN_OPT(LAN_OPT_SET_SOCKET)) {
    ret = 1;
  }

  return ret;
}

/**
 * 设置Socket参数
 * @param server 服务器的地址
 * @param port   端口号
 */
void LAN_SetSocketParam(char* server, uint16_t port, Sock_RecCBFun callback) {
  if (server != LAN_Param.ConnectAddress || port != LAN_Param.ConnectPort || callback != LAN_Param.callback) {
    MCPU_ENTER_CRITICAL();
    LAN_Param.ConnectAddress = server;
    LAN_Param.ConnectPort = port;
    LAN_Param.callback = callback;
    LAN_Opt |= LAN_OPT_SET_SOCKET;
    LAN_Param.ConnectStatus = FALSE; 
    MCPU_EXIT_CRITICAL();
    DBG_LOG("LAN_SetSocketParam.");
  }
}

/**
 * 读LAN的状态
 */
LAN_Status_t LAN_ReadStatus(void) {
  return LAN_Param.status;
}

/**
 * W5500硬件复位
 */
void w5500_hw_reset(void) {
  W5500_RST_ON();
  osDelay(10);
  W5500_RST_OFF();
  osDelay(10);
}

/**
 * 获取系统tick
 */
uint32_t wizpf_get_systick(void) {
  return HAL_GetTick();
}

/**
 * 系统tick转换为秒
 */
uint32_t wizpf_tick_conv(uint8_t istick2sec, uint32_t tickorsec) {
  uint32_t rate = 0;

  rate = HAL_RCC_GetHCLKFreq() / 1000;
  if (istick2sec) return tickorsec / rate;
  else
    return tickorsec * rate;
}

/**
 * 计算系统tick的差值
 */
int32_t wizpf_tick_elapse(uint32_t tick) {
  uint32_t cur = wizpf_get_systick();

  return cur - tick;
}

/* Private function prototypes -----------------------------------------------*/
/**
 * Socket 状态管理
 */
static void LAN_ManagerPoll(void) {
  uint8_t reindex = 0;
  static uint32_t tsNotConnect = 0, tsPhy = 0;

  switch (LAN_Param.status) {
    case lan_status_reset:       //复位有重启，待确认
      LAN_Param.SockFailCount = 0;
      LAN_Param.ConnectStatus = FALSE;
      w5500_hw_reset();
      wizchip_init(lenSet, lenSet);
      network_init(0, NULL, NULL);
      DHCP_init(SOCK_DHCP, DHCP_Buffer);
      LAN_Param.status = lan_status_phy_offline;
      break;
    case lan_status_phy_offline:
      LAN_Param.ConnectStatus = FALSE;
      if (wizphy_getphylink() == PHY_LINK_ON) {
        LAN_Param.status = lan_status_phy_online;
      } else {
        /*PHY长时间断线重启*/
        if (HAL_GetTick() - tsPhy > LAN_PHY_OFFLINE_TIMEOUT * 1000) {
          tsPhy = HAL_GetTick();
          reindex++;
        }
      }
      break;
    case lan_status_phy_online:
      tsPhy = HAL_GetTick();
      if (DHCP_run() == DHCP_IP_LEASED) {
        LAN_Param.status = lan_status_phy_dhcp_leased;
      }
      break;
    case lan_status_phy_dhcp_leased:
      tsPhy = HAL_GetTick();
      if (wizphy_getphylink() != PHY_LINK_ON) { //改为定时检查或者INT中断提升速度
        DHCP_init(SOCK_DHCP, DHCP_Buffer);
        LAN_Param.status = lan_status_phy_offline;
      }
      LAN_Socket_Manager();
      /*超过定义的时间未建立socket连接重启*/
      if (LAN_Param.ConnectStatus) {
        tsNotConnect = HAL_GetTick();
      } else if (LAN_Param.ConnectAddress != NULL && LAN_Param.ConnectPort > 0) {
        if ((HAL_GetTick() - tsNotConnect) > LAN_NOT_CONNECT_TIMEOUT * 1000) { //
          tsNotConnect = HAL_GetTick();
          reindex++;
        }
      }
      break;
    default:
      break;
  }

  /*连续错误重启*/
  if (LAN_Param.SockFailCount > 60) {
    LAN_Param.SockFailCount = 0;
    reindex++;
  }
  /*重启LAN*/
  if (reindex > 0) {
    LAN_Param.status = lan_status_reset;
  }
}

/**
 * Socket 收发处理
 */
static void LAN_Socket_Manager(void) {
  int8_t status;
  uint8_t ip[4];
  char* pip = NULL;
  static char* stor_paddr = NULL;
  static uint16_t stor_port = 0;

  status = GetTCPSocketStatus(SOCK_USER);
  if (status == SOCKSTAT_ESTABLISHED) {
    if (wizphy_getphylink() == PHY_LINK_ON) {
      LAN_Param.ConnectStatus = TRUE;
    } else {
      LAN_Param.ConnectStatus = FALSE;
      close(SOCK_USER);
    }
    /*设置socket参数*/
    if (LAN_OPT(LAN_OPT_SET_SOCKET)) {
      LAN_OPT_CLEAR(LAN_OPT_SET_SOCKET);
      if (stor_paddr != LAN_Param.ConnectAddress || stor_port != LAN_Param.ConnectPort) {
        DBG_LOG("LAN set socket new.");
        close(SOCK_USER);
        LAN_Param.ConnectStatus = FALSE;
      }
    }
  } else {
    LAN_Param.ConnectStatus = FALSE;

    if (status == SOCKSTAT_CLOSE_WAIT) {
      close(SOCK_USER);
    } else if (status == SOCKSTAT_CLOSED)  {
      DBG_LOG("LAN socket is close.");
      socket(SOCK_USER, Sn_MR_TCP, HAL_GetTick(), SF_TCP_NODELAY);
    } else if (status == SOCKSTAT_INIT) {
      if (LAN_Param.ConnectAddress != NULL && LAN_Param.ConnectPort > 0) {
        pip = LAN_Param.ConnectAddress;
        ip[0] = 0;
        if (ip_check(pip, ip) != RET_OK && strlen(pip) > 6) {
          /*访问域名进行DNS解析*/
          LAN_DNS(pip, ip);
        }
        if (ip[0] != 0) {
          DBG_LOG("W5500 connect server:%s, port:%u.", inet_nntoa(ip), LAN_Param.ConnectPort);
          if (connect(SOCK_USER, ip, LAN_Param.ConnectPort) == SOCK_OK) {
            LAN_Param.ConnectStatus = TRUE;
            DBG_LOG("W5500 connect OK.");
            stor_paddr = LAN_Param.ConnectAddress;
            stor_port = LAN_Param.ConnectPort;
          } else {
            LAN_Param.ConnectStatus = FALSE;
            LAN_Param.SockFailCount++;
            DBG_LOG("W5500 connect failed.");
          }
        }
      }
    }
  }
}

/**
 * Socket 收发处理
 */
static void LAN_Socket_SendRecProc(void) {
  osEvent evt;
  uint16_t len = 0;
  int ret = 0;
  uint8_t* p = NULL;
  static uint32_t tsRec = 0, retRec = 0;

  if (LAN_Param.ConnectStatus) {
    /*socket发送*/
    evt = osMessageGet(LAN_SendQId, 2);
    if (evt.status == osEventMessage) {
      p = evt.value.p;
      len = *(uint16_t*)(p);
      p += 2;

      ret = send(SOCK_USER, p, len);
      vPortFree(evt.value.p);
      if (ret != len) {
        LAN_Param.SockFailCount++;
        close(SOCK_USER);
        DBG_LOG("LAN socket send fail, return:%d", ret);
      } else {
        DBG_INFO("LAN socket send oK, return:%d", ret);
      }
    }
    /*socket接收*/
    ret = GetSocketRxRecvBufferSize(SOCK_USER);
    if (ret != retRec) {
      retRec = ret;
      TS_INIT(tsRec);
    }
    if (ret > 0 && TS_IS_OVER(tsRec, 10)) {
      retRec = 0;
      TS_INIT(tsRec);
      if (ret > (LAN_RECEIVE_MAX_SIZE)) {
        DBG_WAR("test LAN_RECEIVE_MAX_SIZE overflow:%u.", ret);
        ret = LAN_RECEIVE_MAX_SIZE;
      }
      p = MMEMORY_ALLOC(ret);
      if (p != NULL) {
        ret = recv(SOCK_USER, p, ret);
        if (ret <= 0) {
          LAN_Param.SockFailCount++;
          DBG_LOG("LAN socket read fail, return:%d", ret);
        } else {
          DBG_INFO("LAN socket rece oK, return:%d", ret);
          LAN_Param.SockFailCount = 0;
#if LAN_CMD_EN > 0
          CMD_NewData(LAN_Pipe, p, len);
#endif
          if (LAN_Param.callback != NULL) {
            LAN_Param.callback((uint8_t*)p, ret);
          }
        }
        MMEMORY_FREE(p);
      }
    }
  }
}

/**
 * DNS域名解析
 * @param domain 域名
 * @return 返回解析结果
 */
static BOOL LAN_DNS(char* domain, uint8_t* ipout) {
  BOOL ret = FALSE;
  int8_t ret2 = 0;

  uint8_t * dns_buff, ipdns[4];

  /*访问域名进行DNS解析*/
  dns_buff = pvPortMalloc(MAX_DNS_BUF_SIZE);
  if (dns_buff != NULL) {
    getDNSfromDHCP(ipdns);
    DNS_init(SOCK_DNS, dns_buff);
    ret2 = DNS_run(ipdns, domain, ipout);
    if (ret2 == 1) {
      ret = TRUE;
    }
    vPortFree(dns_buff);
  }
  return ret;
}

/*LAN驱动接口*/
static void w5500_Mutex_get(void) {
  osRecursiveMutexWait(w5500_MutexId, osWaitForever);
}

static void w5500_Mutex_free(void) {
  osRecursiveMutexRelease(w5500_MutexId);
}

static void w5500_cs_on(void) {
  W5500_CS_ON();
}

static void w5500_cs_off(void) {
  W5500_CS_OFF();
  // w5500_read_byte();
}

static uint8_t w5500_read_byte(void) {
  SPI_TypeDef* spi = w5500_SPI_Handle->Instance;

  if (__HAL_SPI_GET_FLAG(w5500_SPI_Handle, SPI_FLAG_RXNE) != RESET) {
    __HAL_SPI_CLEAR_OVRFLAG(w5500_SPI_Handle);
  }

  while ((spi->SR & SPI_FLAG_TXE) == RESET);
  spi->DR = 0xFF;
  while ((spi->SR & SPI_FLAG_RXNE) == RESET);
  return spi->DR;
}

static void w5500_write_byte(uint8_t c) {
  uint8_t dummy = 0;
  SPI_TypeDef* spi = w5500_SPI_Handle->Instance;

  if (__HAL_SPI_GET_FLAG(w5500_SPI_Handle, SPI_FLAG_RXNE) != RESET) {
    __HAL_SPI_CLEAR_OVRFLAG(w5500_SPI_Handle);
  }

  while ((spi->SR & SPI_FLAG_TXE) == RESET);
  spi->DR = c;
  while ((spi->SR & SPI_FLAG_RXNE) == RESET);
  dummy = spi->DR;
  (void)dummy;
}

/**
 * SPI falsh调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void LAN_Console(int argc, char* argv[]) {
  int8_t ret = 0;
  uint8_t sock = 0, ip[4];
  uint16_t port = 0;

  argc--;
  argv++;
  if (osRecursiveMutexWait(w5500_MutexId, osWaitForever) == osOK) {
    if (strcmp(argv[0], "info") == 0) {
      network_disp(NULL);
    } else if (strcmp(argv[0], "int") == 0) {
      DBG_LOG("W5500 INT status:%d.", IS_W5500_INT());
    } else if (strcmp(*argv, "status") == 0) {
      DBG_LOG("LAN status:%d", (int)LAN_Param.status);
    } else if (strcmp(argv[0], "tcp") == 0) {
      argc--;
      argv++;
      if (strcmp(argv[0], "opens") == 0) {
        sock = uatoi(argv[1]);
        if (port_check((int8_t*)argv[2], &port) == RET_OK) {
          ret = TCPServerOpen(sock, port);
          DBG_LOG("socket:%d TCP server open return：%d.", sock, ret);
        }
      } else if (strcmp(argv[0], "openc") == 0) {
        sock = uatoi(argv[1]);
        if (port_check((int8_t*)argv[3], &port) == RET_OK && ip_check(argv[2], &ip[0]) == RET_OK) {
          ret = TCPClientOpen(sock, port, ip, port);
          DBG_LOG("socket:%d TCP client open  return：%d.", sock, ret);
        }
      } else if (strcmp(argv[0], "close") == 0) {
        sock = uatoi(argv[1]);
        ret = TCPClose(sock);
        DBG_LOG("socket:%d TCP close return：%d.", sock, ret);
      } else if (strcmp(argv[0], "send") == 0) {
        sock = uatoi(argv[1]);
        ret = TCPSend(sock, (int8_t*)argv[2], strlen(argv[2]));
        DBG_LOG("socket:%d TCP send return：%d.", sock, ret);
      } else if (strcmp(argv[0], "check") == 0) {
        sock = uatoi(argv[1]);
        DBG_LOG("socket:%d TCP connect check return：%d.", sock, TCPConnChk(sock));
      }
    } else if (strcmp(argv[0], "socket") == 0) {
      sock = uatoi(argv[1]);
      argc -= 2;
      argv += 2;
      if (strcmp(argv[0], "socket") == 0) {
        ret = socket(sock, Sn_MR_TCP, HAL_GetTick(), SF_TCP_NODELAY);
        DBG_LOG("socket%d TCP client init, ret:%d", sock, ret);
      } else if (strcmp(argv[0], "connect") == 0) {
        ip_check(argv[1], ip);
        ret = connect(sock, ip, uatoi(argv[2]));
        DBG_LOG("socket%d connect, ret:%d", sock, ret);
      } else if (strcmp(argv[0], "disconnect") == 0) {
        ret = disconnect(sock);
        DBG_LOG("socket%d disconnect, ret:%d", sock, ret);
      } else if (strcmp(argv[0], "close") == 0) {
        ret = close(sock);
        DBG_LOG("socket%d close, ret:%d", sock, ret);
      } else if (strcmp(argv[0], "send") == 0) {
        ret = send(sock, (uint8_t*)argv[1], strlen(argv[1]));
        DBG_LOG("socket%d send, ret:%d", sock, ret);
      }  else if (strcmp(argv[0], "status") == 0) {
        DBG_LOG("socket%d status:%d", sock, GetTCPSocketStatus(sock));
      }
    } else if (strcmp(argv[0], "user") == 0) {
      argc--;
      argv++;
      if (strcmp(argv[0], "send") == 0) {
        ret = LAN_SocketSendData((uint8_t*)argv[1], strlen(argv[1]));
        DBG_LOG("LAN send OK.");
      }
    } else if (strcmp(argv[0], "phy") == 0) {
      argc--;
      argv++;
      if (strcmp(argv[0], "status") == 0) {
        DBG_LOG("PHY link status:%d", wizphy_getphylink());
      }
    } else if (strcmp(argv[0], "dhcp") == 0) {
      argc--;
      argv++;
      if (strcmp(argv[0], "stop") == 0) {
        DHCP_stop();
        DBG_LOG("DHCP stoped.");
      } else if (strcmp(argv[0], "init") == 0) {
        DHCP_init(SOCK_DHCP, DHCP_Buffer);
        DBG_LOG("DHCP int.");
      } else if (strcmp(argv[0], "lease") == 0) {
        DBG_LOG("DHCP lease time is:%d.", getDHCPLeasetime());
      }
    } else if (strcmp(argv[0], "dns") == 0) {
      LAN_DNS(argv[1], ip);
      DBG_LOG("DNS running, domain：%s ip is:%s", argv[1], inet_nntoa(ip));
    }
    osRecursiveMutexRelease(w5500_MutexId);
  }
}


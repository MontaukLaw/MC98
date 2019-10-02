
/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file http.c
 * @version V1.0
 * @date 2016.12.18
 * @brief http函数文件.
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

/* Private function prototypes -----------------------------------------------*/
static char* http_readwait(uint32_t* len, uint32_t timeout);
static int http_analy_ret(char* msg);
static int http_analy_content_length(char* msg, uint32_t len);
static char* http_analy_content(char* msg);
static void http_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * HTTP初始化
 */
void HTTP_Init(void) {
  CMD_ENT_DEF(http, http_Console);
  Cmd_AddEntrance(CMD_ENT(http));

  DBG_LOG("HTTP Init.");
}

/**
 * http GET 数据
 * @param netpath 网络通道
 * @param host    主机域名
 * @param path    文件路径
 * @return 返回获取到的数据的指针，使用后需释放
 */
char* HTTP_GetData(char* host, char* path) {
  char* pbuf = NULL;
  char* ret = NULL, *prec = NULL;
  uint32_t totallen = 0, reclen = 0, len = 0, ts = 0, rethttp = 0;

  System_SockLockEn(TRUE);
  /*HEAD 获取文件长度*/
  if (HTTP_SendHEAD(host, path)) {
    ret = http_readwait(NULL, HTTP_TIMEOUT * 1000);
    if (ret != NULL) {
      if (http_analy_ret(ret) == 200) {
        totallen = http_analy_content_length(ret, 0);
      }
      MMEMORY_FREE(ret);
    }
  }
  DBG_LOG("HTTP_GetData totallen:%d", totallen);
  if (totallen > 0) {
    pbuf = MMEMORY_ALLOC(totallen + 1);
  }
  if (totallen == 0 || pbuf == NULL) {
    System_SockLockEn(FALSE);
    return NULL;
  }

  /*GET 断点续传获取文件*/
  TS_INIT(ts);
  while (reclen < totallen && !TS_IS_OVER(ts, 10000)) {
    if (HTTP_SendGET(host, path, reclen)) {
      len = 0;
      ret = http_readwait(&len, 3000);
      if (ret != NULL) {
        rethttp = http_analy_ret(ret);
        if (rethttp == 206 || rethttp == 200) {
          len = http_analy_content_length(ret, len);
          prec = http_analy_content(ret);
          if (len > 0 && prec != NULL) {
            memcpy(pbuf + reclen, prec, len);
            reclen += len;
          }
          DBG_LOG("HTTP_GetData reclen:%d", reclen);
        }
        MMEMORY_FREE(ret);
      }
    }
  }
  if (reclen != totallen) {
    MMEMORY_FREE(pbuf);
    pbuf = NULL;
  } else {
    *(pbuf + reclen) = 0;
  }
  System_SockLockEn(FALSE);

  return pbuf;
}

/**
 * http GET 数据并写入到flash.
 * @param netpath 网络通道
 * @param host    主机域名
 * @param path    文件路径
 * @return 返回获取到的数据的指针，使用后需释放
 */
BOOL HTTP_GetDataToFlash(char* host, char* path, uint32_t flashaddr, uint32_t timeout) {
  char* ret = NULL, *prec = NULL;
  uint32_t totallen = 0, reclen = 0, len = 0, ts = 0, addr = 0, rethttp = 0;

  System_SockLockEn(TRUE);
  /*HEAD 获取文件长度*/
  if (HTTP_SendHEAD(host, path)) {
    ret = http_readwait(NULL, HTTP_TIMEOUT * 1000);
    if (ret != NULL) {
      if (http_analy_ret(ret) == 200) {
        totallen = http_analy_content_length(ret, 0);
      }
      MMEMORY_FREE(ret);
    }
  }
  DBG_LOG("HTTP_GetDataToFlash totallen:%d", totallen);

  if (totallen == 0) {
    System_SockLockEn(FALSE);
    return FALSE;
  }
  /*GET 断点续传获取文件*/
  TS_INIT(ts);
  SFlash_EraseSectors(addr, totallen / SFLASH_SECTOR_SIZE + 1);
  while (reclen < totallen && !TS_IS_OVER(ts, timeout * 1000)) {
    DBG_LOG("HTTP_SendGET pos:%d", reclen);
    if (HTTP_SendGET(host, path, reclen)) {
      len = 0;
      ret = http_readwait(&len, 3000);
      if (ret != NULL) {
        rethttp = http_analy_ret(ret);
        if (rethttp == 206 || rethttp == 200) {
          len = http_analy_content_length(ret, len);
          prec = http_analy_content(ret);

          if (len > 0 && prec != NULL) {
            addr = flashaddr + reclen;
            SFlash_Write(addr, (uint8_t*)prec, len);
            reclen += len;
          }
          DBG_LOG("HTTP_GetDataToFlash reclen:%d", reclen);
        }
        MMEMORY_FREE(ret);
      }
    }
  }
  System_SockLockEn(FALSE);
  if (reclen != totallen) {
    return FALSE;
  }
  return TRUE;
}

/**
 * http 发送GET
 * @param host    主机域名
 * @param path    文件路径
 * @return 返回获取到的数据的指针，使用后需释放
 */
BOOL HTTP_SendGET(char* host, char* path, uint32_t pos) {
  char* getSend = NULL;

  if (host == NULL || path == NULL) {
    return FALSE;
  }

  if (System_SockConnect(host, 80) > 0)  {
    getSend = MMEMORY_ALLOC(256);
    if (getSend != NULL) {
      DBG_DBG("HTTP Send GET host:%s", host);
      /*socket发送GET*/
      snprintf(getSend, 256, "GET %s HTTP/1.1\r\n"\
                   "Host: %s\r\n"\
                   "Connection: Keep-Alive\r\n"\
                   "Range: bytes=%d-%d\r\n"\
                   "\r\n",
               path, host, pos, pos + HTTP_PACKET_SIZE - 1);

      System_SockSend((uint8_t*)getSend, strlen(getSend));
      MMEMORY_FREE(getSend);
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * http 发送HEAD
 * @param host    主机域名
 * @param path    文件路径
 * @return 返回获取到的数据的指针，使用后需释放
 */
BOOL HTTP_SendHEAD(char* host, char* path) {
  char* psend = NULL;

  if (host == NULL || path == NULL) {
    return FALSE;
  }
  if (System_SockConnect(host, 80) > 0) {
    psend = MMEMORY_ALLOC(256);
    if (psend != NULL) {
      DBG_DBG("HTTP Send HEAD host:%s", host);
      /*socket发送GET*/
      snprintf(psend, 256, "HEAD %s HTTP/1.1\r\n"\
                   "Host: %s\r\n"\
                   "Connection: Keep-Alive\r\n"\
                   "\r\n",
               path, host);
      System_SockSend((uint8_t*)psend, strlen(psend));
      MMEMORY_FREE(psend);
      return TRUE;
    }
  }
  return FALSE;
}

/* Private function prototypes -----------------------------------------------*/
/**
 * http socket数据接收等待
 */
static char* http_readwait(uint32_t* len, uint32_t timeout) {
  char* ret = NULL;
  int16_t rec = 0;
  uint32_t ts = 0;

  ret = MMEMORY_ALLOC(HTTP_PACKET_SIZE + 300);
  if (ret != NULL) {
    ts = HAL_GetTick();
    while (!TS_IS_OVER(ts, timeout)) {
      osDelay(2);
      rec = System_SockRecv((uint8_t*)ret, HTTP_PACKET_SIZE + 300);
      if (rec != 0) {
        if (rec > 0 && len != NULL) {
          *len = rec;
        }
        break;
      }
    }
  }
  if (rec <= 0) {
    MMEMORY_FREE(ret);
    ret = NULL;
  } else {
    *(ret + rec) = 0;
  }
  return ret;
}

/**
 * 从报文中解析HTTP响应结果
 * @param msg  报文
 * @return 返回响应结果
 */
static int http_analy_ret(char* msg) {
  char* p = NULL;

  p = strstr(msg, "HTTP/1.1");
  if (p != NULL) {
    while (*p && *p++ != ' ');
    while (*p == ' ')
      p++;
    return uatoi(p);
  } else {
    DBG_ERR("test analy error:%s\r\n.", msg);
  }
  return -1;
}

/**
 * 从报文中解析内容的长度
 * @param msg  消息
 * @return 返回内容长度
 */
static int http_analy_content_length(char* msg, uint32_t len) {
  int ret = -1;
  char* p = NULL;

  p = strstr(msg, "Content-Length");
  if (p != NULL) {
    while (*p && *p++ != ':');
    while (*p == ' ')
      p++;
    ret = uatoi(p);
    if (ret >= len && len != 0) {
      DBG_WAR();
      ret = 0;
    }
  }
  return ret;
}

/**
 * 从报文中解析内容
 * @param msg  报文
 * @return 返回消息内容
 */
static char* http_analy_content(char* msg) {
  char* p = NULL;

  /*取数据内容*/
  p = strstr(msg, "\r\n\r\n");
  if (p != NULL) {
    return (p + 4);
  }
  return NULL;
}

/**
 * http调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void http_Console(int argc, char* argv[]) {
  char* ret = NULL;

  argv++;
  argc--;
  if (ARGV_EQUAL("sendget")) {
    HTTP_SendGET(argv[1], argv[2], uatoi(argv[3]));
  } else if (ARGV_EQUAL("sendhead")) {
    HTTP_SendHEAD(argv[1], argv[2]);
  } else if (ARGV_EQUAL("getdata")) {
    ret = HTTP_GetData(argv[1], argv[2]);
    if (ret != NULL) {
      DBG_LOG("http get ret:%s", ret);
      MMEMORY_FREE(ret);
    }
  } else if (ARGV_EQUAL("getdataflash")) {
    HTTP_GetDataToFlash(argv[1], argv[2], uatoi(argv[3]), uatoi(argv[4]));
    DBG_LOG("http get data to flash OK.");
  }
}



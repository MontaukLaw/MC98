/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file conslole.c
 * @version V1.1
 * @date 2016.4.1
 * @brief 控制台函数文件.
 *
 * *********************************************************************
 * @note v1.1 CMD 接收缓存改为FIFO
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
static BOOL         CMD_EchoEnable;
static uint8_t      CMD_Pipe = 0, Login_Pipe = 0;
static FIFO_t       CMD_RecFIFO;
static uint8_t      CMD_RecBuffer[CMD_FIFO_BUF_SIZE];
static CMD_SendFun  SendFun[CMD_PIPE_MAX];

static uint8_t      g_ucDbgLevel = LOG_LEVEL_DEF;

/* Private function prototypes -----------------------------------------------*/
void CMD_Task(void const* argument);
static void CMD_ProcPoll(void);
static int  CMD_UART_IsLogin(uint8_t num);
static void CMD_LoginProc(uint8_t pipe, uint8_t* dat);
static void CMD_PipeSendData(uint8_t pipe, uint8_t* dat, uint16_t len);
static void CMD_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * 控制台初始化
 */
void CMD_Init(void) {
#if CMD_UART_EN == 1
    CMD_Pipe = DEBUG;
#endif
    CMD_SetEchoEnable((BOOL)CMD_ECHO_DEF);
    FIFO_Init(&CMD_RecFIFO, CMD_RecBuffer, sizeof(CMD_RecBuffer));
    osThreadDef(CMD, CMD_Task, CMD_TASK_PRIO, 0, CMD_TASK_STK_SIZE);
    osThreadCreate(osThread(CMD), NULL);
    CMD_ENT_DEF(CMD, CMD_Console);
    Cmd_AddEntrance(CMD_ENT(CMD));
    DBG_LOG("Console init.");
}

/**
 * 控制台处理任务
 */
void CMD_Task(void const* argument) {
    DBG_LOG("Console task start.");
    while (1) {
        CMD_ProcPoll();
        osDelay(10);
    }
}

/**
 * 控制台管道注册
 * @param fun  该管道对应的数据发送函数指针
 * @return 返回管道号
 */
uint8_t CMD_Pipe_Register(CMD_SendFun fun) {
    int i = 0, r = 0;
    portENTER_CRITICAL();
    for (i = 0; i < CMD_PIPE_MAX; i++) {
        if (SendFun[i] == NULL) {
            SendFun[i] = fun;
            r = i + 1;
#if CMD_UART_EN == 1
            r += UART_PORT_MAX;
#endif
            break;
        }
    }
    portEXIT_CRITICAL();
    return r;
}

/**
 * 控制台发送数据
 * @param dat  待发送的数据指针
 * @param len  数据的长度
 */
void CMD_SendData(uint8_t* dat, uint16_t len) {
    CMD_PipeSendData(CMD_Pipe, dat, len);
}

/**
 * 从控制台缓存中读出数据
 * @param dat  数据读出保存的指针
 * @param len  读出数据的最大长度
 */
uint16_t CMD_ReadData(uint8_t* dat, uint16_t len) {
    uint16_t l = 0;
    l = FIFO_Read(&CMD_RecFIFO, dat, len);
    return l;
}

/**
 * 获取CMD缓存数据的大小
 * @return 返回数据长度
 */
uint16_t CMD_DataSize(void) {
    uint16_t len = FIFO_Length(&CMD_RecFIFO);
    return len;
}

/**
	* 控制台打印可变参数字符串.
	* @param  fomat: 参数列表.
	* @param  ...:可变参数
	*/
void CMD_Printf(char* format, ...) {
    char* pBuf = NULL;
    va_list args;
    pBuf = MMEMORY_ALLOC(CMD_PRINTF_BUFF_MAX);
    if (pBuf != NULL) {
        va_start(args, format);
        vsnprintf(pBuf, CMD_PRINTF_BUFF_MAX, format, args);
        va_end(args);
        CMD_SendData((uint8_t*)pBuf, strlen(pBuf));
        MMEMORY_FREE(pBuf);
    }
}

/**
 * 控制台打印HEX数据
 * @param dat  数据指针
 * @param len  数据的长度
 */
void CMD_HEX_Print(uint8_t* dat, uint16_t len) {
    uint16_t line, rem, i, pos = 0;
    uint8_t* pBuf = NULL;
    if (dat != NULL && len > 0) {
        line = (len % 16) ? 1 : 0;
        line += len / 16;
        pBuf = MMEMORY_ALLOC(16 * 3 + 3);
        if (pBuf != NULL) {
            for (i = 0; i < line; i++) {
                rem = len - pos;
                /*一行最多16个字*/
                if (rem > 16) {
                    rem = 16;
                }
                Array2Hex(dat, pBuf, rem);
                CMD_Printf("0x%04X:%s", pos, pBuf);
                dat += rem;
                pos += rem;
            }
            MMEMORY_FREE(pBuf);
        }
    }
}

/**
	* 控制台打印可变参数字符串,根据优先等级决定是否打印
	* @param  fomat: 参数列表.
	* @param  ...:可变参数
	*/
void CMD_Printf_Level(uint8_t level, char* format, ...) {
    char* pBuf = NULL;
    va_list args;
    if (level > 0 && level <= g_ucDbgLevel) {
        pBuf = MMEMORY_ALLOC(CMD_PRINTF_BUFF_MAX);
        if (pBuf != NULL) {
            va_start(args, format);
            vsnprintf(pBuf, CMD_PRINTF_BUFF_MAX, format, args);
            va_end(args);
            CMD_SendData((uint8_t*)pBuf, strlen(pBuf));
            MMEMORY_FREE(pBuf);
        }
    }
}

/**
 * 控制台打印HEX数据,根据优先等级决定是否打印
 * @param dat  数据指针
 * @param len  数据的长度
 */
void CMD_HEX_Print_Level(uint8_t level, uint8_t* dat, uint16_t len) {
    if (level > 0 && level <= g_ucDbgLevel) {
        CMD_HEX_Print(dat, len);
    }
}

/**
 * 控制台设置回显开关
 * @param en 回显使能开关
 */
void CMD_SetEchoEnable(BOOL en) {
    if (en != CMD_EchoEnable) {
        portENTER_CRITICAL();
        CMD_EchoEnable = en;
        portEXIT_CRITICAL();
    }
}

/**
 * 控制台设置打印信息等级
 * @param level
 */
void CMD_SetDebugLevel(uint8_t level) {
    if (level != g_ucDbgLevel) {
        portENTER_CRITICAL();
        g_ucDbgLevel = level;
        portEXIT_CRITICAL();
    }
}

/**
 * 串口空闲数据读出.
 */
void CMD_UART_Read_Poll(void) {
    int num = 0;
    uint8_t* buf = NULL;
    uint16_t len = 0;
    int pos = -1;
    for (num = 1; num <= UART_PORT_MAX; num++) {
        len = UART_DataSize(num);
        if (len > 0) {
            /*串口为命令行占用的串口时将数据发往处理队列*/
            if (num == CMD_Pipe) {
                buf = MMEMORY_ALLOC(len + 1);
                if (buf != NULL) {
                    UART_ReadData(num, buf, len);
                    buf[len] = 0;
                    CMD_NewData(num, buf, len);
                    MMEMORY_FREE(buf);
                }
            }
            /*处理用户登录*/
            else {
                pos = CMD_UART_IsLogin(num);
                if (pos >= 0) {
                    buf = MMEMORY_ALLOC(len + 1);
                    if (buf != NULL) {
                        UART_ReadData(num, buf, len);
                        buf[len] = 0;
                        CMD_LoginProc(num, buf + pos);
                        MMEMORY_FREE(buf);
                    }
                }
            }
        }
    }
}

/**
 * 控制台接收到新的数据
 * @param pipe  管道号
 * @param dat   数据指针
 * @param len   数据的长度
 */
void CMD_NewData(uint8_t pipe, uint8_t* dat, uint16_t len) {
    /*数据处理*/
    if (pipe == CMD_Pipe) {
        /*输出回显*/
        if (CMD_EchoEnable != FALSE) {
            CMD_SendData(dat, len);
        }
        FIFO_Write(&CMD_RecFIFO, dat, len);
    }
    /*登录处理*/
    else {
        CMD_LoginProc(pipe, dat);
    }
}

/**
 * 执行虚拟命令
 * @param dat   数据指针
 */
void CMD_Virtual(char* cmd) {
    if (cmd != NULL) {
        CMD_NewData(CMD_Pipe, (uint8_t*)cmd, strlen(cmd));
        CMD_NewData(CMD_Pipe, "\r\n", 2);
    }
}

/* Private function prototypes -----------------------------------------------*/

/**
 * 控制台处理函数，在任务中调用轮询处理新的消息.
 */
static void CMD_ProcPoll(void) {
    uint16_t len = 0;
    char* pbuf = NULL, token = 0;
    len = FIFO_Length(&CMD_RecFIFO);
    if (len > 0) {
        token = FIFO_Query(&CMD_RecFIFO, len - 1);
        if (token == '\r' || token == '\n')  {
            pbuf = MMEMORY_ALLOC(len + 1);
            if (pbuf != NULL) {
                len = FIFO_Read(&CMD_RecFIFO, (uint8_t*)pbuf, len);
                *(pbuf + len) = 0;
                if (*(pbuf + len - 1) == '\r' || *(pbuf + len - 1) == '\n') {
                    *(pbuf + len - 1) = 0;
                }
                if (*(pbuf + len - 2) == '\r' || *(pbuf + len - 2) == '\n') {
                    *(pbuf + len - 2) = 0;
                }
                Cmd_Handle(pbuf);
                MMEMORY_FREE(pbuf);
            }
        }
        /*溢出归零*/
        if (len == CMD_FIFO_BUF_SIZE) {
            FIFO_Flush(&CMD_RecFIFO);
        }
    }
}

/**
 * 控制台接收到新的数据
 * @param num 串口号
 */
static int CMD_UART_IsLogin(uint8_t num) {
    static char buffer[8];
    uint16_t i = 0, j = 0, len = 0;
    if (num == Login_Pipe && UART_GetDataIdleTicks(num) >= CMD_UART_REFRESH_TICKS) {
        return 0;
    }
    len = UART_DataSize(num);
    if (len > 10 && UART_GetDataIdleTicks(num) >= CMD_UART_REFRESH_TICKS) {
        for (i = 0; i < len; i++) {
            if (UART_QueryByte(num, i) == 's' && (len - i > 6)) {
                for (j = 0; j < 6; j++) {
                    buffer[j] = UART_QueryByte(num, i + j);
                }
                buffer[6] = '\0';
                if (strcmp(buffer, "system") == 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

/**
 * 控制台接收到新的数据
 * @param pipe  管道号
 * @param dat   数据指针
 * @param len   数据的长度
 */
static void CMD_LoginProc(uint8_t pipe, uint8_t* dat) {
    char* p1 = (char*)dat;
    char* t1 = "Please input pass word.\r\n";
    char* t2 = "Console login OK.\r\n";
    char* t3 = "Password error.\r\n";
    if (strncmp(p1, "system", 6) == 0) {
        while (*p1 && *p1 != ' ') {
            p1++;
        }
        while (*p1 && *p1 == ' ') {
            p1++;
        }
        Str2Print(p1);
        if (strcmp(p1, CMD_LOGIN_USER) == 0) {
            Login_Pipe = pipe;
            CMD_PipeSendData(pipe, (uint8_t*)t1, strlen(t1));
        }
    } else if (pipe == Login_Pipe) {
        Str2Print(p1);
        if (strcmp(p1, CMD_LOGIN_PWD) == 0) {
            CMD_Pipe = pipe;
            CMD_PipeSendData(pipe, (uint8_t*)t2, strlen(t2));
        } else {
            Login_Pipe = 0;
            CMD_PipeSendData(pipe, (uint8_t*)t3, strlen(t2));
        }
    }
}

/**
 * 控制台发送数据
 * @param pipe 管道号
 * @param dat  待发送的数据指针
 * @param len  数据的长度
 */
static void CMD_PipeSendData(uint8_t pipe, uint8_t* dat, uint16_t len) {
    if (pipe > 0) {
#if CMD_UART_EN == 1
        if (pipe <= UART_PORT_MAX) {
            UART_SendData(pipe, dat, len);
        } else {
            SendFun[pipe - 1 - UART_PORT_MAX](dat, len);
        }
#else
        SendFun[pipe - 1](dat, len);
#endif
    }
}

/**
 * 系统调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void CMD_Console(int argc, char* argv[]) {
    uint32_t d = 0;
    argv++;
    argc--;
    if (ARGV_EQUAL("echo")) {
        if (strcmp(argv[1], "on") == 0) {
            CMD_SetEchoEnable(TRUE);
        } else if (strcmp(argv[1], "off") == 0) {
            CMD_SetEchoEnable(FALSE);
        }
        DBG_LOG("Console set echo:%s.", argv[1]);
    } else if (ARGV_EQUAL("logout")) {
        DBG_LOG("CMD pipe will reset.");
#if CMD_UART_EN == 1
        CMD_Pipe = DEBUG;
#else
        CMD_Pipe = 0;
#endif
    } else if (ARGV_EQUAL("virtual")) {
        CMD_Virtual(argv[1]);
    } else if (ARGV_EQUAL("loglevel")) {
        if (argv[1] != NULL) {
            d = uatoi(argv[1]);
            CMD_SetDebugLevel(d);
        }
        DBG_WAR("System log level:%d.", g_ucDbgLevel);
    }
}



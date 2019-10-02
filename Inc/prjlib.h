/***
 * File    : prjlib.h
 * By      : Jonas Song
 * Version : V1.0
 * Date    : 2012.4.25
 * Copy    :
 * Brief   : 项目库函数头文件。
 *******************************************************************************
 */

#ifndef __PRJLIB_H
#define __PRJLIB_H
/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdint.h"
#include "ctype.h"
#include "math.h"

/* Define --------------------------------------------------------------------*/
#define BIT8_MAX        0xFF
#define BIT16_MAX       0xFFFF
#define BIT32_MAX       0xFFFFFFFF

/* Macro ---------------------------------------------------------------------*/
#define IS_Q_FULL(q)    (q->count >= q->lenth)
#define IS_Q_EMPTY(q)   (q->count == 0)
#define GET_Q_COUNT(q)  (q->count)
#define GET_Q_FREE(q)   (q->lenth - q->count)

#define UPDCRC16(b, c)  (crc16tab[((c >> 8) ^ b) & 0xff] ^ (c << 8))
#define UPDCRC32(b, c)  (crc32tab[((int)c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFF))

#define CMD_ARGC_MAX    16

#define CMD_ENT_DEF(cmd, ent) \
    static CmdEntrance_t CmdEntrance_##cmd = {#cmd, ent, NULL}

#define CMD_ENT(cmd)  \
        &CmdEntrance_##cmd

/*字符串比较*/
#define STR_EQUAL(des, token)   (strcmp((char*)des, (char*)token) == 0)
#define STR_NEQUAL(des, token)  (strncmp((char*)des, (char*)token, strlen((char*)token)) == 0)

/*参数比较*/
#define ARGV_EQUAL(token)       (strcmp(argv[0], (char*)token) == 0)
#define ARGV_LEGAL(argv)        (argv != NULL && isalnum(argv[0]))

/*任务看门狗宏*/
#define TWDT_DEF(name, up) \
    static stTaskWatchDog twdt_##name = {#name, TRUE, up, 0, NULL}

#define TWDT_ADD(name)  \
        TaskWDG_Add(&twdt_##name)

#define TWDT_CLEAR(name)  \
        TaskWDG_Clear(&twdt_##name)

#define TWDT_ONOFF(name, en)  \
        TaskWDG_OnOff(&twdt_##name, en)

/*检查数值是否为2的幂次方值*/
#define IS_POWER_OF_TWO(A) ( ((A) != 0) && ((((A) - 1) & (A)) == 0) )
#define IS_WORD(A)         (A % 4 == 0)

/*位操作*/
#define BIT_SET(reg, bit)       (reg |= (1 << bit))
#define BIT_CLEAR(reg, bit)     (reg &= ~(1 << bit))
#define BIT_READ(reg, bit)      ((reg & (1 << bit)) >> bit)

/*掩码操作*/
#define MASK_SET(reg, mask)      (reg |= (mask))
#define MASK_CLEAR(reg, mask)    (reg &= (~(mask)))
#define IS_MASK_SET(reg, mask)   (reg & mask)

/*对象大小*/
#define OBJ_LEN(obj)             (sizeof(obj))
#define STRNCPY_OBJ(obj, str)    do{strncpy(obj, str, OBJ_LEN(obj)-1); obj[OBJ_LEN(obj) - 1] = 0;} while(0)

/* Typedef -------------------------------------------------------------------*/
#ifndef NULL
#define NULL                0u
#endif

 #define TRUE                1
 #define FALSE               0
 #define BOOL                int

// typedef enum
// {
//     FALSE = 0,
//     TRUE = !0
// } BOOL;

/*环形队列类型*/
typedef struct
{
    uint16_t front;            /*头指针偏移量*/
    uint16_t rear;             /*尾指针偏移量*/
    uint16_t count;            /*元素计数器*/
    uint16_t lenth;            /*缓存长度*/
    char *pbuf;                /*缓存指针*/
} CirQueue_TypeDef;            /*环形队列缓存类型*/

/*命令处理回调函数指针*/
typedef void (*Cmd_CallBack)(int argc, char *argv[]);

/*命令处理入口*/
typedef struct CmdEntrance
{
    char const *cmd;
    Cmd_CallBack pCB;
    struct CmdEntrance *next;
} CmdEntrance_t;

/* 多任务看门狗结构 */
typedef struct TaskWatchDog
{
    char *Name;
    BOOL Enable;
    uint32_t TaskUpper;
    uint32_t volatile TaskCount;
    struct TaskWatchDog *pNext;
} stTaskWatchDog;

typedef struct
{
    uint8_t *pBuffer;
    uint32_t sizeMask;
    uint32_t rpos;
    uint32_t wpos;
} FIFO_t;

/* Variable declarations -----------------------------------------------------*/
extern const uint16_t crc16tab[];
extern const uint32_t crc32tab[];

/* Function prototypes -------------------------------------------------------*/
uint8_t* SearchMemData(uint8_t *Dst, uint8_t *Dat, uint16_t lDst, uint16_t lDat);
uint32_t strlen_t(char *s);
char* uitoa(uint32_t val, char *str);
char* uitoax(uint32_t val, char *str);
char* uitoa_d(uint32_t val, char *str, uint8_t d);
char* uitoa_n(uint32_t val, char *str, uint8_t d);
char* sitoa_n(int32_t val, char *str, uint8_t d);
char* sitoa(int32_t val, char *str);
uint32_t uatoi(char *str);
uint32_t uatoi_n(char *str, uint8_t n);
int32_t satoi(char *str);
uint32_t uatoix(char *str);
uint64_t ulatoix(char *str);
uint32_t uatoix_n(char *str, uint8_t n);
uint16_t int16tBS(uint16_t *num);
uint32_t int32tBS(uint32_t *num);
uint64_t int64tBS(uint64_t *num);
uint8_t XORCheck(uint8_t *p_start, uint16_t len);
uint8_t AddCheck(uint8_t *p_start, uint16_t len);
uint16_t CRC_16(uint16_t init, uint8_t *aData, uint16_t aSize);
uint16_t crc16_compute(const uint8_t *p_data, uint32_t size, const uint16_t *p_crc);
uint32_t CRC_32(uint32_t init, uint8_t *aData, uint16_t aSize);
uint32_t BCDStr_To_Dec(char *bcd, uint8_t len);
uint8_t BCD_To_HEX(uint8_t bcd);
uint8_t HEX_To_BCD(uint8_t byte);
char* BCDStr_To_Str(char *bcd, uint16_t len, char *str);
char* Dec_To_BCDStr(uint32_t dec, uint8_t len, char *bcd);
char* Str2Print(char *str);
char* Str2Graph(char *str);
char* Str2Alpha(char *str);
char* StrTokenDel(char *str, char token);

BOOL Cmd_AddEntrance(CmdEntrance_t *pEnt);
void Cmd_Handle(char *cmd);
int  Cmd_ArgFind(char **argv, char *arg);

void delay(uint32_t de);

BOOL InitQueue(CirQueue_TypeDef *queue, char *buf, uint16_t len);
BOOL EnQueue(CirQueue_TypeDef *queue, char *buff, uint16_t len);
BOOL DeQueue(CirQueue_TypeDef *queue, char *buff, uint16_t length);

void TaskWDG_Add(stTaskWatchDog *pTWDG);
void TaskWDG_OnOff(stTaskWatchDog *pTWDG, BOOL en);
void TaskWDG_Clear(stTaskWatchDog *pTWDG);
char* TaskWDG_IsUpper(void);
char* TaskWDG_Tick(void);

BOOL FIFO_Init(FIFO_t *p_fifo, uint8_t *p_buf, uint32_t buf_size);
BOOL FIFO_Put(FIFO_t *p_fifo, uint8_t byte);
BOOL FIFO_Get(FIFO_t *p_fifo, uint8_t *p_byte);
void FIFO_Flush(FIFO_t *p_fifo);
uint32_t FIFO_Length(FIFO_t *p_fifo);
uint32_t FIFO_Read(FIFO_t *p_fifo, uint8_t *pBuf, uint32_t size);
uint32_t FIFO_Write(FIFO_t *p_fifo, uint8_t *pBuf, uint32_t size);

uint8_t  FIFO_Query(FIFO_t *p_fifo, uint32_t offset);
uint32_t FIFO_cpy(FIFO_t *p_des, FIFO_t *p_res, uint32_t size);
int32_t  FIFO_str(FIFO_t *p_fifo, uint32_t offset, char *str);
int32_t  FIFO_cmp(FIFO_t *p_fifo, uint32_t offset, char *str);
int32_t  FIFO_chr(FIFO_t *p_fifo, uint32_t offset, char ch);

void Array2Hex(uint8_t *in,  uint8_t *out, uint16_t len);

BOOL IsCPUendianBig(void);

#endif


/***************************************************************END OF FILE****/



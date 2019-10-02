/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Radio.h
 * @author 宋阳
 * @version V1.0
 * @date 2016.11.25
 * @brief 射频芯片驱动函数文件.
 *
 * **********************************************************************
 * @note
 *
 * **********************************************************************
 */


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _RADIO_H
#define _RADIO_H


/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "prjlib.h"

/* Exported define -----------------------------------------------------------*/
/*定义无线数据包头标志*/
#define RF_PACKET_HEAD      0x5abc

/*FIFO的默认长度*/
#define RF_FIFO_LENGTH      64

/*定义数据包的最大长度*/
#define RF_PAYLOAD_MAX      64

/*定义命令的最大长度*/
#define RF_CMD_SIZE_MAX     20

/*定义唤醒所需的时间，需大于WOR周期加上误差*/
#define RF_WAKEUP_TIME      330

/*WOR active period,最大63, (n + 1)/4096s*/
#define WOR_ACTIVE          45

/*WOR sleep period,最大1023, (n + 1)*32/4096s*/
#define WOR_SLEEP           41

/* Exported types ------------------------------------------------------------*/
typedef struct
{
    uint16_t head;
    uint16_t crc;
    uint16_t command;
    uint16_t length;
    uint8_t payload[RF_PAYLOAD_MAX];
} rfPacket_t;

/* Exported constants --------------------------------------------------------*/
#define SYSTEMCLOCK_REG     0x00
#define PLL1_REG            0x01
#define PLL2_REG            0x02
#define PLL3_REG            0x03
#define PLL4_REG            0x04
#define PLL5_REG            0x05
#define PLL6_REG            0x06
#define CRYSTAL_REG         0x07
#define PAGEA_REG           0x08
#define PAGEB_REG           0x09
#define RX1_REG             0x0A
#define RX2_REG             0x0B
#define ADC_REG             0x0C
#define PIN_REG             0x0D
#define CALIBRATION_REG     0x0E
#define MODE_REG            0x0F

#define TX1_PAGEA           0x00
#define WOR1_PAGEA          0x01
#define WOR2_PAGEA          0x02
#define RFI_PAGEA           0x03
#define PM_PAGEA            0x04
#define RTH_PAGEA           0x05
#define AGC1_PAGEA          0x06
#define AGC2_PAGEA          0x07
#define GIO_PAGEA           0x08
#define CKO_PAGEA           0x09
#define VCB_PAGEA           0x0A
#define CHG1_PAGEA          0x0B
#define CHG2_PAGEA          0x0C
#define FIFO_PAGEA          0x0D
#define CODE_PAGEA          0x0E
#define WCAL_PAGEA          0x0F

#define TX2_PAGEB           0x00
#define IF1_PAGEB           0x01
#define IF2_PAGEB           0x02
#define ACK_PAGEB           0x03
#define ART_PAGEB           0x04


#define CMD_Reg_W           0x00    //000x,xxxx control register write
#define CMD_Reg_R           0x80    //100x,xxxx control register read
#define CMD_ID_W            0x20    //001x,xxxx ID write
#define CMD_ID_R            0xA0    //101x,xxxx ID Read
#define CMD_FIFO_W          0x40    //010x,xxxx TX FIFO Write
#define CMD_FIFO_R          0xC0    //110x,xxxx RX FIFO Read
#define CMD_RF_RST          0x70    //x111,xxxx RF reset
#define CMD_TFR             0x60    //0110,xxxx TX FIFO address pointrt reset
#define CMD_RFR             0xE0    //1110,xxxx RX FIFO address pointer reset

#define CMD_SLEEP           0x10    //0001,0000 SLEEP mode
#define CMD_IDLE            0x12    //0001,0010 IDLE mode
#define CMD_STBY            0x14    //0001,0100 Standby mode
#define CMD_PLL             0x16    //0001,0110 PLL mode
#define CMD_RX              0x18    //0001,1000 RX mode
#define CMD_TX              0x1A    //0001,1010 TX mode
//#define CMD_DEEP_SLEEP    0x1C    //0001,1100 Deep Sleep mode(tri-state)
#define CMD_DEEP_SLEEP      0x1F    //0001,1111 Deep Sleep mode(pull-high)

/* Exported macro ------------------------------------------------------------*/
#define RF_FPF()                (IO_READ(RF_CKO) == 1)
#define RF_WTR()                (IO_READ(RF_GIO1) == 1)

#define RF_BROADCAST()          A7139_ID_ErrorBitSet(3)
#define RF_FIXED()              A7139_ID_ErrorBitSet(0)

/* Exported variables --------------------------------------------------------*/
extern uint8_t *pRecBuf;

/* Exported functions --------------------------------------------------------*/
void RF_Init(SPI_HandleTypeDef *handle, uint32_t cid, uint8_t channel);

void RF_Standby(void);
void RF_Send(uint8_t *data, uint16_t len);
BOOL RF_Receive(uint8_t *data, uint16_t len, uint16_t timeout);

void RF_SendPacket(uint16_t command, uint8_t *data, uint16_t len);
int16_t RF_ReceivePacket(uint8_t **data, uint16_t reclen, uint16_t *retlen, uint16_t timeout);

void RF_StartReceive(uint16_t length);
int16_t RF_ReadPacket(uint16_t length, uint8_t **data, uint16_t *reclen);
BOOL RF_Read(uint8_t *data, uint16_t length);

uint16_t RF_ReadRSSI(void);

void A7139_EnterWOR(uint8_t reclen);
void A7139_ExitWOR(void);
void A7139_WriteID(uint32_t id, uint8_t channel);
void A7139_ID_ErrorBitSet(uint8_t errorbit);
void A7139_CRC_Set(BOOL crc, BOOL mscrc);
BOOL A7139_Read_CRC_Result(void);
void A7139_AutoResend_ACK_Set(BOOL enable);
BOOL A7139_Read_AutoResend_Result(void);

#endif



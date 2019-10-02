/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file Radio.c
 * @version V1.0
 * @date 2016.11.25
 * @brief 射频芯片驱动函数文件.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"
#include "a7139config.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define RF_CS_H()               do {IO_H(RF_CS);} while (0)
#define RF_CS_L()               do {IO_L(RF_CS);} while (0)

#define Err_State()             DBG_ERR(".");

/* Private variables ---------------------------------------------------------*/
static uint16_t             A7139_Payload_Length = 64;
static SPI_HandleTypeDef* spi_Handle;
static rfPacket_t           recPacket;
uint8_t* pRecBuf = &recPacket.payload[0];

/* Private function prototypes -----------------------------------------------*/
static void StrobeCMD(uint8_t cmd);
static uint8_t ByteRW(uint8_t send);
static void A7139_WriteReg(uint8_t address, uint16_t dataWord);
static uint16_t A7139_ReadReg(uint8_t);
static void A7139_WritePageA(uint8_t, uint16_t);
static uint16_t A7139_ReadPageA(uint8_t);
static void A7139_WritePageB(uint8_t, uint16_t);
static uint16_t A7139_ReadPageB(uint8_t);
static void A7139_Config(void);
static void InitRF(uint32_t cid, uint8_t channel);
static void A7139_Cal(void);
static void A7139_SetPayloadLength(uint16_t length);
static void A7139_RCOSC_Cal(void);
static void RF_Console(int argc, char* argv[]);
/* Exported functions --------------------------------------------------------*/

/**
 * Radio 初始化
 */
void RF_Init(SPI_HandleTypeDef* handle, uint32_t cid, uint8_t channel) {
	spi_Handle = handle;
	__HAL_SPI_ENABLE(handle);

	/*只初始化一次有可能无线收发失败*/
	InitRF(cid, channel);
	A7139_WriteReg(PLL1_REG, 0x0A21);
	A7139_WriteReg(PLL2_REG, 0xDE05); 

	/*使能CRC与CRC过滤*/
	A7139_CRC_Set(TRUE, TRUE);

	CMD_ENT_DEF(RF, RF_Console);
	Cmd_AddEntrance(CMD_ENT(RF));

	DBG_LOG("Radio init.");
}

/**
 * RF进入待机模式
 */
void RF_Standby(void) {
	StrobeCMD(CMD_STBY);
}

/**
 * 无信模块发送数据
 * @param data 待发送的数据
 * @param len  数据长度
 */
void RF_Send(uint8_t* data, uint16_t len) {
	uint16_t i = 0, j = 0, sent = 0;

	StrobeCMD(CMD_STBY);

	A7139_SetPayloadLength(len);
	StrobeCMD(CMD_TFR);
	sent = len;
	if (sent > 64) {
		sent = 64;
	}
	/*发送首包*/
	RF_CS_L();
	ByteRW(CMD_FIFO_W);
	for (i = 0; i < sent; i++) {
		ByteRW(*(data + i));
	}
	RF_CS_H();
	StrobeCMD(CMD_TX);

	/*待等FPF发送剩余数据包*/
	while (i < len) {
		sent = len - i;
		if (sent > 48) {
			sent = 48;
		}
		/*等待FPF信号*/
		while (!RF_FPF());
		RF_CS_L();
		ByteRW(CMD_FIFO_W);
		for (j = 0; j < sent; j++) {
			ByteRW(*(data + i));
			i++;
		}
		RF_CS_H();
	}
	delay(10);
	/*等待发送完毕*/
	while (RF_WTR());

	DBG_DBG("RF send OK.");
}

/**
 * 无信模块接收数据
 * @param data 待读出的数据的指针
 * @param len  数据长度
 */
BOOL RF_Receive(uint8_t* data, uint16_t len, uint16_t timeout) {
	uint32_t ts = 0, i = 0, j = 0, rec = 0;

	TS_INIT(ts);
	StrobeCMD(CMD_STBY);

	A7139_SetPayloadLength(len);

	/*复位FIFO指针*/
	StrobeCMD(CMD_RFR);
	/*开启接收*/
	StrobeCMD(CMD_RX);

	if (len <= 64) {
		rec = len;
	} else {
		/*循环接收FIFO*/
		while (i < len) {
			rec = len - i;
			if (rec > 48) {
				rec = 48;
			} else {
				break;
			}
			/*等待FPF信号*/
			while (!RF_FPF() && !TS_IS_OVER(ts, timeout));
			if (TS_IS_OVER(ts, timeout)) {
				break;
			}
			RF_CS_L();
			ByteRW(CMD_FIFO_R);
			for (j = 0; j < rec; j++) {
				*(data + i) = ByteRW(0xFF);
				i++;
			}
			RF_CS_H();
		}
	}
	/*等待接收完成*/
	delay(10);
	while (RF_WTR() && !TS_IS_OVER(ts, timeout));
	if (TS_IS_OVER(ts, timeout)) {
		StrobeCMD(CMD_STBY);
		DBG_DBG("RF receive failed.");
		return FALSE;
	}

	/*接收最后一个数据包*/
	if (rec > 0) {
		RF_CS_L();
		ByteRW(CMD_FIFO_R);
		for (j = 0; j < rec; j++) {
			*(data + i) = ByteRW(0xFF);
			i++;
		}
		RF_CS_H();
	}
	DBG_DBG("RF receive OK.");

	return TRUE;
}

/**
 * 无信模块发送数据包
 * @param data 待发送的数据
 * @param len  数据长度
 */
void RF_SendPacket(uint16_t command, uint8_t* data, uint16_t len) {
	uint16_t temp[4];
	uint16_t i = 0, j = 0, sent = 0, crc = 0;
	uint8_t* p = (uint8_t*)&temp[0];

	StrobeCMD(CMD_STBY);

	temp[0] = RF_PACKET_HEAD;
	temp[2] = command;
	temp[3] = len;

	crc = CRC_16(0, (uint8_t*)&temp[2], 4);
	crc = CRC_16(crc, data, len);
	temp[1] = crc;

	len += 8;
	sent = len;
	A7139_SetPayloadLength(len);
	StrobeCMD(CMD_TFR);
	if (sent > 64) {
		sent = 64;
	}
	/*发送首包*/
	RF_CS_L();
	ByteRW(CMD_FIFO_W);
	for (j = 0; j < 8; j++) {
		ByteRW(*p++);
	}
	for (j = 0; j < sent - 8; j++) {
		ByteRW(*(data + i));
		i++;
	}
	RF_CS_H();
	StrobeCMD(CMD_TX);

	/*等待FPF发送剩余数据包*/
	if (data != NULL) {
		while (i < len - 8) {
			sent = len - 8 - i;
			if (sent > 48) {
				sent = 48;
			}
			/*等待FPF信号*/
			while (!RF_FPF());
			RF_CS_L();
			ByteRW(CMD_FIFO_W);
			for (j = 0; j < sent; j++) {
				ByteRW(*(data + i));
				i++;
			}
			RF_CS_H();
		}
	}
	delay(10);
	/*等待发送完毕*/
	while (RF_WTR());
	DBG_DBG("RF send packet cmd:%d", command);
}

/**
 * 无线模块接收数据包
 * @param data    读出接收到的数据
 * @param reclen  接收到的数据长度
 * @param timeout 超时时间
 * @return 接收到数据包且CRC校验成功返回命令字
 */
int16_t RF_ReceivePacket(uint8_t** data, uint16_t reclen, uint16_t* retlen, uint16_t timeout) {
	uint16_t crc = 0, rlen = 0;

	if (reclen == 0) {
		reclen = RF_PAYLOAD_MAX;
	}
	rlen = reclen + 8;
	if (rlen > sizeof(recPacket)) {
		rlen = sizeof(recPacket);
	}

	if (RF_Receive((uint8_t*)&recPacket, rlen, timeout)) {

		if (recPacket.length > reclen) {
			recPacket.length = reclen;
			DBG_WAR("RF receive length overflow.");
		}
		if (data != NULL) {
			*data = pRecBuf;
		}
		if (retlen != NULL) {
			*retlen = recPacket.length;
		}

		crc = CRC_16(0, (uint8_t*)&recPacket.command, recPacket.length + 4);
		if (RF_PACKET_HEAD == recPacket.head && recPacket.crc == crc) {
			DBG_INFO("RF receive packet cmd:%#x", recPacket.command);
			return recPacket.command;
		}
		DBG_INFO("RF receive packet failed, packet head:%#x, packet crc:%#x, crc:%#x cmd:%#x",
						 recPacket.head, recPacket.crc, crc, recPacket.command);
		return -1;
	}
	return 0;
}

/**
 * RF启动命令接收
 */
void RF_StartReceive(uint16_t length) {
	StrobeCMD(CMD_STBY);

	if (length == 0 || length > RF_FIFO_LENGTH) {
		length = RF_FIFO_LENGTH;
	}
	A7139_SetPayloadLength(length);

	/*复位FIFO指针*/
	StrobeCMD(CMD_RFR);
	/*开启接收*/
	StrobeCMD(CMD_RX);
}

/**
 * 无线模块从FIFO中读出数据包，最长为
 * @param data    读出接收到的数据
 * @param reclen  接收到的数据长度
 * @return 接收到数据包且CRC校验成功返回命令字
 */
int16_t RF_ReadPacket(uint16_t length, uint8_t** data, uint16_t* reclen) {
	uint16_t crc = 0, i;
	uint8_t* p = (uint8_t*)&recPacket;

	if (length == 0 || length > RF_FIFO_LENGTH) {
		length = RF_FIFO_LENGTH;
	}

	if (!RF_WTR()) {
		StrobeCMD(CMD_RFR);
		RF_CS_L();
		ByteRW(CMD_FIFO_R);
		for (i = 0; i < length; i++) {
			*(p + i) = 0;
			*(p + i) = ByteRW(0xFF);
		}
		RF_CS_H();

		if (recPacket.length > length) {
			recPacket.length = length;
		}
		crc = CRC_16(0, (uint8_t*)&recPacket.command, recPacket.length + 4);
		if (RF_PACKET_HEAD == recPacket.head && recPacket.crc == crc) {
			if (data != NULL) {
				*data = pRecBuf;
			}
			if (reclen != NULL) {
				*reclen = recPacket.length;
			}
			DBG_DBG("RF read CMD:%#x", recPacket.command);
			return recPacket.command;
		}
		DBG_INFO("RF read CMD failed, packet head:%#x, packet crc:%#x, crc:%#x cmd:%#x",
						 recPacket.head, recPacket.crc, crc, recPacket.command);
		return -1;
	}
	return 0;
}

/**
 * RF从FIFO中读出数据
 *
 * @param data   读出的数据指针
 * @param length 读出的长度
 * @return 读出成功返回TRUE
 */
BOOL RF_Read(uint8_t* data, uint16_t length) {
	uint16_t i = 0;
	uint8_t* p = (uint8_t*)data;

	if (data != NULL && length > 0 && !RF_WTR()) {
		if (length > RF_FIFO_LENGTH) {
			length = RF_FIFO_LENGTH;
		}

		StrobeCMD(CMD_RFR);
		RF_CS_L();
		ByteRW(CMD_FIFO_R);
		for (i = 0; i < length; i++) {
			*(p + i) = 0;
			*(p + i) = ByteRW(0xFF);
		}
		RF_CS_H();

		return TRUE;
	}
	return FALSE;
}

/**
 * 读出RSSI
 * @return 返回RSSI值
 */
uint16_t RF_ReadRSSI(void) {
	uint16_t ret = 0;
#if AGC_EN > 0

	ret = A7139_ReadReg(RX2_REG);
#endif

	DBG_LOG("test adc:%u",  A7139_ReadReg(ADC_REG) & 0x00FF);

	return ret;
}

/**
 * 进入WOR自动唤醒模式
 */
void A7139_EnterWOR(uint8_t reclen) {
	if (reclen == 0 || reclen > RF_FIFO_LENGTH) {
		reclen = RF_FIFO_LENGTH;
	}
	StrobeCMD(CMD_STBY);

	A7139_SetPayloadLength(reclen);

	/*GIO Preamble Detect Output*/
	A7139_WritePageA(GIO_PAGEA, (A7139Config_PageA[GIO_PAGEA] & 0xFFC0) | 0x0001 | (0x11 << 2));
	/*setup WOR Sleep time and Rx time*/
	A7139_WritePageA(WOR1_PAGEA, (WOR_ACTIVE << 10) | WOR_SLEEP);
	/*Byframe*/
	A7139_WritePageA(WOR2_PAGEA, A7139Config_PageA[WOR2_PAGEA] | 0x0030);

	/*WORE=1 to enable WOR function*/
	A7139_WriteReg(MODE_REG, A7139Config[MODE_REG] | 0x0200);
}

/**
 * 退出WOR自动唤醒模式
 */
void A7139_ExitWOR(void) {
	/*WORE=0 to dienable WOR function*/
	A7139_WriteReg(MODE_REG, A7139Config[MODE_REG]);

	A7139_WritePageA(GIO_PAGEA, A7139Config_PageA[GIO_PAGEA]);
}

/**
 * 7139 ID写入
 * @param cid    32位的ID
 */
void A7139_WriteID(uint32_t cid, uint8_t channel) {

	RF_CS_L();
	ByteRW(CMD_ID_W);

	ByteRW(0x55);
	ByteRW((uint8_t)(cid >> 24));
	ByteRW((uint8_t)(cid >> 16));
	ByteRW((uint8_t)(cid >> 8));
	ByteRW((uint8_t)cid);
	ByteRW((uint8_t)channel);

	RF_CS_H();
}

/**
 * 设置ID Code错误允许位
 */
void A7139_ID_ErrorBitSet(uint8_t errorbit) {
	A7139_WriteReg(RX1_REG, (A7139Config[RX1_REG] & 0x7E7F) | ((errorbit & 0x03) << 7) | (((errorbit >> 2) & 0x01) << 15));
}

/**
 * A7139CRC设置
 *
 * @param crc    CRC使能
 * @param mscrc  CRC过滤使能
 */
void A7139_CRC_Set(BOOL crc, BOOL mscrc) {
	/*CRCS Select.*/
	if (crc) {
		A7139_WritePageA(CODE_PAGEA, A7139Config_PageA[CODE_PAGEA] | (1 << 3));
	} else {
		A7139_WritePageA(CODE_PAGEA, A7139Config_PageA[CODE_PAGEA]);
	}

	/*MSCRC=1 to CRC Filtering Enable*/
	if (mscrc) {
		A7139_WriteReg(CALIBRATION_REG, A7139Config[CALIBRATION_REG] | 0x8000);
	} else {
		A7139_WriteReg(CALIBRATION_REG, A7139Config[CALIBRATION_REG]);
	}
}

/**
 * 读接收数据CRC校验结果
 *
 * @return 返回CRC校验结果
 */
BOOL A7139_Read_CRC_Result(void) {
	uint16_t tmp = A7139_ReadReg(MODE_REG);

	return (BOOL)!(tmp & 0x200);
}

/**
 * 自动重传与应答使能
 *
 */
void A7139_AutoResend_ACK_Set(BOOL enable) {
	if (enable) {
		A7139_WritePageB(ACK_PAGEB, 0x1820 | (ARC_DEF << 1) | 0x01);
		A7139_WritePageB(ART_PAGEB, ART_DEF);
	} else {
		A7139_WritePageB(ACK_PAGEB, A7139Config_PageB[ACK_PAGEB]);
		A7139_WritePageB(ART_PAGEB, A7139Config_PageB[ART_PAGEB]);
	}
}

BOOL A7139_Read_AutoResend_Result(void) {
	uint16_t tmp;

	tmp = A7139_ReadPageB(ACK_PAGEB);

	DBG_DBG("A7139 Auto-Resend RCR:%u, ACK Reg:%#x", (tmp >> 1) & 0x0F, tmp);
	return (BOOL)(tmp & (1 << 5));
}

/* Private function prototypes -----------------------------------------------*/

/**
 * StrobeCMD
 * @param cmd  发送的CMD
 */
static void StrobeCMD(uint8_t cmd) {
	RF_CS_L();
	ByteRW(cmd);
	RF_CS_H();
}

/**
 * 读写一个byte
 * @param send 发送的字节
 * @return 读出的byte
 */
static uint8_t ByteRW(uint8_t send) {
	uint8_t ret = 0;
	SPI_TypeDef* spi = spi_Handle->Instance;

	if (__HAL_SPI_GET_FLAG(spi_Handle, SPI_FLAG_RXNE) != RESET) {
		__HAL_SPI_CLEAR_OVRFLAG(spi_Handle);
	}

	while ((spi->SR & SPI_FLAG_TXE) == RESET);
	*((__IO uint8_t*)&(spi->DR)) = send;
	while ((spi->SR & SPI_FLAG_RXNE) == RESET);
	ret = *((__IO uint8_t*)&(spi->DR));
	return ret;
}

/**
 * A7139_WriteReg
 * @param address  寄存器地址
 * @param dataWord 写入的值
 */
static void A7139_WriteReg(uint8_t address, uint16_t dataWord) {
	RF_CS_L();
	address |= CMD_Reg_W;
	ByteRW(address);
	ByteRW(dataWord >> 8);
	ByteRW(dataWord);
	RF_CS_H();
}

/************************************************************************
**  A7139_ReadReg
************************************************************************/
static uint16_t A7139_ReadReg(uint8_t address) {
	uint16_t tmp = 0;

	RF_CS_L();
	address |= CMD_Reg_R;
	ByteRW(address);
	tmp = ByteRW(0xFF);
	tmp <<= 8;
	tmp |= ByteRW(0xFF);
	RF_CS_H();

	return tmp;
}

/************************************************************************
**  A7139_WritePageA
************************************************************************/
static void A7139_WritePageA(uint8_t address, uint16_t dataWord) {
	uint16_t tmp;

	tmp = address;
	tmp = ((tmp << 12) | A7139Config[CRYSTAL_REG]);
	A7139_WriteReg(CRYSTAL_REG, tmp);
	A7139_WriteReg(PAGEA_REG, dataWord);
}

/************************************************************************
**  A7139_ReadPageA
************************************************************************/
static uint16_t A7139_ReadPageA(uint8_t address) {
	uint16_t tmp;

	tmp = address;
	tmp = ((tmp << 12) | A7139Config[CRYSTAL_REG]);
	A7139_WriteReg(CRYSTAL_REG, tmp);
	tmp = A7139_ReadReg(PAGEA_REG);
	return tmp;
}

/************************************************************************
**  A7139_WritePageB
************************************************************************/
static void A7139_WritePageB(uint8_t address, uint16_t dataWord) {
	uint16_t tmp;

	tmp = address;
	tmp = ((tmp << 7) | A7139Config[CRYSTAL_REG]);
	A7139_WriteReg(CRYSTAL_REG, tmp);
	A7139_WriteReg(PAGEB_REG, dataWord);
}

/************************************************************************
**  A7139_ReadPageB
************************************************************************/
static uint16_t A7139_ReadPageB(uint8_t address) {
	uint16_t tmp;

	tmp = address;
	tmp = ((tmp << 7) | A7139Config[CRYSTAL_REG]);
	A7139_WriteReg(CRYSTAL_REG, tmp);
	tmp = A7139_ReadReg(PAGEB_REG);
	return tmp;
}

/*********************************************************************
** A7139_Config
*********************************************************************/
static void A7139_Config(void) {
	uint8_t i;
	uint16_t tmp;

	for (i = 0; i < 8; i++)
		A7139_WriteReg(i, A7139Config[i]);

	for (i = 10; i < 16; i++)
		A7139_WriteReg(i, A7139Config[i]);

	for (i = 0; i < 16; i++)
		A7139_WritePageA(i, A7139Config_PageA[i]);

	for (i = 0; i < 5; i++)
		A7139_WritePageB(i, A7139Config_PageB[i]);

	tmp = A7139_ReadReg(SYSTEMCLOCK_REG);
	if (tmp != A7139Config[SYSTEMCLOCK_REG]) {
		DBG_LOG("SYSTEMCLOCK_REG:%#x", tmp);
		Err_State();
	}
}

/*********************************************************************
** initRF
*********************************************************************/
static void InitRF(uint32_t cid, uint8_t channel) {
	/*reset A7139 chip*/
	StrobeCMD(CMD_RF_RST);

	/*config A7139 chip*/
	A7139_Config();
	/*delay 800us for crystal stabilized*/
	HAL_Delay(2);

	/*write ID code*/
	A7139_WriteID(cid, channel);

	/*IF and VCO calibration*/
	A7139_Cal();

	/*校准WOR RC定时器*/
	A7139_RCOSC_Cal();

	DBG_LOG("RF init CID:%#x, channel:%#x", cid, channel);
}

uint8_t fb, fcd, fbcf;
uint8_t vb, vbcf;
uint8_t vcb, vccf;

/*********************************************************************
** A7139_Cal
*********************************************************************/
static void A7139_Cal(void) {

	uint16_t tmp;

	/*IF calibration procedure @STB state*/
	A7139_WriteReg(MODE_REG, A7139Config[MODE_REG] | 0x0802);      /*IF Filter & VCO Current Calibration*/
	do {
		tmp = A7139_ReadReg(MODE_REG);
	}while (tmp & 0x0802);

	/*for check(IF Filter)*/
	tmp = A7139_ReadReg(CALIBRATION_REG);
	fb = tmp & 0x0F;
	fcd = (tmp >> 11) & 0x1F;
	fbcf = (tmp >> 4) & 0x01;
	if (fbcf) {
		Err_State();
	}

	/*for check(VCO Current)*/
	tmp = A7139_ReadPageA(VCB_PAGEA);
	vcb = tmp & 0x0F;
	vccf = (tmp >> 4) & 0x01;
	if (vccf) {
		Err_State();
	}

	/*RSSI Calibration procedure @STB state*/
	A7139_WriteReg(ADC_REG, 0x4C00);                                    /*set ADC average=64*/
	A7139_WriteReg(MODE_REG, A7139Config[MODE_REG] | 0x1000);           /*RSSI Calibration*/
	do {
		tmp = A7139_ReadReg(MODE_REG);
	}while (tmp & 0x1000);
	A7139_WriteReg(ADC_REG, A7139Config[ADC_REG]);


	/*VCO calibration procedure @STB state*/
	A7139_WriteReg(PLL1_REG, A7139Config[PLL1_REG]);
	A7139_WriteReg(PLL2_REG, A7139Config[PLL2_REG]);
	A7139_WriteReg(MODE_REG, A7139Config[MODE_REG] | 0x0004);           /*VCO Band Calibration*/
	do {
		tmp = A7139_ReadReg(MODE_REG);
	}while (tmp & 0x0004);

	/*for check(VCO Band)*/
	tmp = A7139_ReadReg(CALIBRATION_REG);
	vb = (tmp >> 5) & 0x07;
	vbcf = (tmp >> 8) & 0x01;
	if (vbcf) {
		Err_State();
	}
}

/**
 * 7139设置负载的长度
 * @param length 长度
 */
static void A7139_SetPayloadLength(uint16_t length) {
	if (length != A7139_Payload_Length) {
		A7139_Payload_Length = length;
		if (length > 64) {
			length -= 1;
			A7139_WritePageA(VCB_PAGEA, (length & 0x3F00) | 0x04);
			A7139_WritePageA(FIFO_PAGEA, (3 << 14) | (length & 0xFF));
			A7139_WritePageA(CKO_PAGEA, A7139Config_PageA[CKO_PAGEA] | 0x0013);
		} else {
			length -= 1;
			A7139_WritePageA(VCB_PAGEA,  0x04);
			A7139_WritePageA(FIFO_PAGEA, length & 0x3F);
			A7139_WritePageA(CKO_PAGEA, A7139Config_PageA[CKO_PAGEA]);
		}
	}
}

/**
 * WOR时钟校准
 */
static void A7139_RCOSC_Cal(void) {
	uint16_t tmp;

	A7139_WritePageA(WOR2_PAGEA, A7139Config_PageA[WOR2_PAGEA] | 0x0010);       /*enable RC OSC*/

	while (1) {
		A7139_WritePageA(WCAL_PAGEA, A7139Config_PageA[WCAL_PAGEA] | 0x0001);   /*set ENCAL=1 to start RC OSC CAL*/
		do {
			tmp = A7139_ReadPageA(WCAL_PAGEA);
		}while (tmp & 0x0001);

		tmp = (A7139_ReadPageA(WCAL_PAGEA) & 0x03FF);       /*read NUMLH[8:0]*/
		tmp >>= 1;

		/*NUMLH[8:0]=194+-10 (PF8M=6.4M) if((tmp > 232) && (tmp < 254))*/
		/*NUMLH[8:0]=243+-10 (PF8M=8M)*/
		if ((tmp > 183) && (tmp < 205)) {
			break;
		}
	}
}

/** RF调试命令
 * @param argc 参数项数量
 * @param argv 参数列表
 */
static void RF_Console(int argc, char* argv[]) {
	uint16_t temp = 0;

	argv++;
	argc--;
	if (ARGV_EQUAL("cmd")) {
		temp = uatoix(argv[1]);
		StrobeCMD(temp);
		DBG_LOG("StrobeCMD:%#0x", temp);
	} else if (ARGV_EQUAL("init")) {
		if (argv[1] != NULL && argv[2] != NULL) {
			InitRF(uatoix(argv[1]), uatoi(argv[2]));
		}
		DBG_LOG("A7139 init OK .");
	} else if (ARGV_EQUAL("reg")) {
		argv++;
		argc--;
		temp = uatoix(argv[1]);
		if (ARGV_EQUAL("read")) {
			DBG_LOG("A7139 read reg:%#x ret:%#x.", temp, A7139_ReadReg(temp));
		} else if (ARGV_EQUAL("exit")) {
			A7139_WriteReg(temp, uatoix(argv[2]));
			DBG_LOG("A7139 write reg:%#x OK.", temp);
		}
	} else if (ARGV_EQUAL("send")) {
		RF_Send((uint8_t*)argv[1], strlen(argv[1]));
		DBG_LOG("A7139 send data.");
	} else if (ARGV_EQUAL("receive")) {
		BOOL ret;
		uint8_t* buff = (uint8_t*)&recPacket;

		memset(buff, 0, sizeof(recPacket));
		ret = RF_Receive(buff, uatoi(argv[1]), uatoi(argv[2]));
		DBG_LOG("A7139 receive ret:%u, content:%s.", ret, buff);
	} else if (ARGV_EQUAL("sendpacket")) {
		RF_SendPacket(uatoi(argv[1]), (uint8_t*)argv[2], strlen(argv[2]));
		DBG_LOG("A7139 sendpacket.");
	} else if (ARGV_EQUAL("receivepacket")) {
		uint16_t len = 0, cmd;
		uint8_t* payload = NULL;

		cmd = RF_ReceivePacket(&payload, uatoi(argv[1]), &len, uatoi(argv[2]));
		if (cmd != 0) {
			*(payload + len) = 0;
			DBG_LOG("A7139 receivepacket cmd:%u len:%u, payload:%s.", cmd, len, payload);
		} else {
			DBG_LOG("A7139 receivepacket failed.");
		}
	} else if (ARGV_EQUAL("wor")) {
		argv++;
		argc--;
		if (ARGV_EQUAL("enter")) {
			A7139_EnterWOR(uatoi(argv[1]));
			DBG_LOG("A7139 enter WOR.");
		} else if (ARGV_EQUAL("exit")) {
			A7139_ExitWOR();
			DBG_LOG("A7139 exit WOR.");
		}
	} else if (ARGV_EQUAL("rssi")) {
		DBG_LOG("RF rssi read:%u", RF_ReadRSSI());
	} else if (ARGV_EQUAL("ETH")) {
		A7139_WriteReg(RX1_REG, A7139Config[RX1_REG] | (uatoi(argv[1]) << 7));
		DBG_LOG("RF RX1_REG write OK.");
	} else if (ARGV_EQUAL("writeid")) {
		A7139_WriteID(uatoix(argv[1]), uatoi(argv[2]));
		DBG_LOG("A7139 write ID OK.");
	} else if (ARGV_EQUAL("iderror")) {
		A7139_ID_ErrorBitSet(uatoi(argv[1]));
		DBG_LOG("A7139_ID_ErrorBitSet OK.");
	} else if (ARGV_EQUAL("crcset")) {
		A7139_CRC_Set((BOOL)uatoi(argv[1]), (BOOL)uatoi(argv[2]));
		DBG_LOG("A7139_CRC_Set OK.");
	} else if (ARGV_EQUAL("autoset")) {
		A7139_AutoResend_ACK_Set((BOOL)uatoi(argv[1]));
		DBG_LOG("A7139_AutoResend_ACK_Set OK.");
	} else if (ARGV_EQUAL("crcres")) {
		DBG_LOG("A7139_Read_CRC_Result:%u.", A7139_Read_CRC_Result());
	} else if (ARGV_EQUAL("autores")) {
		DBG_LOG("A7139_Read_AutoResend_Result:%u.", A7139_Read_AutoResend_Result());
	}
}


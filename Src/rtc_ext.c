/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file rtc_ext.c
 * @version V1.0
 * @date 2016.4.1
 * @brief 外置RTC驱动函数文件.
 *
 * *********************************************************************
 * @note
 * 2016.12.15 增加RTC_TickToStr与RTC_TimeToStr函数
 * 2016.12.26 增加tick转换查询命令
 * 2017.7.19  修复RTC_SetTime BUG
 *
 * *********************************************************************
 * @author 宋阳
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/
#define RTC_CS_H()               IO_H(RTC_CS)
#define RTC_CS_L()               IO_L(RTC_CS)

#define RTC_DATA_CFG_OUT()          do {  GPIO_InitStruct.Pin = RTC_DATA_Pin;\
                                    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;\
                                    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;\
                                    HAL_GPIO_Init(RTC_DATA_GPIO_Port, &GPIO_InitStruct);\
                                    IO_L(RTC_DATA);\
                                    }while(0)

#define RTC_DATA_CFG_IN()           do {    GPIO_InitStruct.Pin = RTC_DATA_Pin;\
                                    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;\
                                    GPIO_InitStruct.Pull = GPIO_PULLUP;\
                                    HAL_GPIO_Init(RTC_DATA_GPIO_Port, &GPIO_InitStruct);\
                                    }while(0)

#define RTC_WP_ON()              RTC_WriteByte(0x8E, 0x80)
#define RTC_WP_OFF()             RTC_WriteByte(0x8E, 0x00)

#define IS_LEAP(y)               ((y % 4 == 0) && (y % 100 != 0)) || ((y % 100 == 00) && (y % 400 == 0))


/* Private variables ---------------------------------------------------------*/
static osMutexId        ExtRTC_MultexId;
static GPIO_InitTypeDef GPIO_InitStruct;

static const uint8_t week_table[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
static const uint8_t mon_table[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/* Private function prototypes -----------------------------------------------*/
static void RTC_WriteByte(uint8_t addr, uint8_t c);
static uint8_t RTC_ReadByte(uint8_t addr);
static uint8_t RTC_GetWeek(uint16_t year, uint8_t month, uint8_t date);

static void Time_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/
/**
 * 初始化RTC 驱动.
 */
void ExternRTC_Init(void) {
	uint8_t temp = 0;
	timeRTC_t time;

	/*开始计时*/
	temp = RTC_ReadByte(0x81);
	if (temp & 0x80) {
		RTC_WP_OFF();
		RTC_WriteByte(0x80, temp - 0x80);
	}
	/*24小时模式*/
	temp = RTC_ReadByte(0x85);
	if (temp & 0x80) {
		RTC_WP_OFF();
		RTC_WriteByte(0x84, temp - 0x80);
	}
	RTC_WP_ON();

	osMutexDef(RTC);
	ExtRTC_MultexId = osMutexCreate(osMutex(RTC));

	RTC_ReadTime(&time);

	if (time.year < 2010 || time.year > 2030) {
		time.year = 2017;
		time.month = 12;
		time.date = 1;
		time.day = 1;
		time.hours = 12;
		time.minutes = 0;
		time.seconds = 0;
		RTC_SetTime(&time);
		DBG_LOG("RTC Set Default.");
	}

	CMD_ENT_DEF(time, Time_Console);
	Cmd_AddEntrance(CMD_ENT(time));

	DBG_LOG("Extern RTC Init.");
}

/**
	* @brief  读RTC.
	* @param  time: 数据读出的指针.
	* @retval 读出成功返回TRUE.
	*/
BOOL RTC_ReadTime(timeRTC_t* time) {
	uint8_t i = 0, addr = 0, rdat[8];
	BOOL r = FALSE;

	if (MCPU_IS_ISR() || osMutexWait(ExtRTC_MultexId, osWaitForever) == osOK) {
		addr = 0x81;
		for (i = 0; i < 7; i++) {
			rdat[i] = RTC_ReadByte(addr);
			addr += 2;
		}
		if (MCPU_IS_ISR() == 0) {
			osMutexRelease(ExtRTC_MultexId);
		}
		if (rdat[0] != 0xFF) {
			time->seconds = BCD_To_HEX(rdat[0] & 0x7F);
			time->minutes = BCD_To_HEX(rdat[1] & 0x7F);
			time->hours = BCD_To_HEX(rdat[2] & 0x3F);
			time->date = BCD_To_HEX(rdat[3] & 0x3F);
			time->month = BCD_To_HEX(rdat[4] & 0x1F);
			time->day = BCD_To_HEX(rdat[5] & 0x07);
			time->year = RTC_YEAR_BASE + BCD_To_HEX(rdat[6] & 0xFF);
			r = TRUE;
		}
	}
	return r;
}

/**
 * RTC读出时间的字符串
 * @param buf  读出的位置
 * @return 返回读出结果
 */
BOOL RTC_ReadTimeStr(char* buf) {
	timeRTC_t time;

	if (RTC_ReadTime(&time)) {
		sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d", time.year,
						time.month, time.date, time.hours, time.minutes, time.seconds);
		return TRUE;
	}
	return FALSE;
}

/**
	* @brief  写RTC.
	* @param  time: 数据读入的指针.
	* @retval 写入成功返回TRUE.
	*/
BOOL RTC_SetTime(timeRTC_t* time) {
	uint8_t i = 0, addr = 0, dat[8];
	;
	BOOL r = FALSE;

	if (time->seconds < 60 && time->minutes < 60 && time->hours < 24
			&& time->date <= 31 && time->date != 0 && time->month <= 12
			&& time->month != 0 && time->year >= RTC_YEAR_BASE && time->year < 2100) {
		dat[0] = HEX_To_BCD(time->seconds);
		dat[1] = HEX_To_BCD(time->minutes);
		dat[2] = HEX_To_BCD(time->hours);
		dat[3] = HEX_To_BCD(time->date);
		dat[4] = HEX_To_BCD(time->month);

#if WEEKDAY_AUTO_SET > 0
		time->day = RTC_GetWeek(time->year, time->month, time->date);
#endif
		dat[5] = HEX_To_BCD(time->day);
		dat[6] = HEX_To_BCD(time->year - RTC_YEAR_BASE);
		if (MCPU_IS_ISR() || osMutexWait(ExtRTC_MultexId, osWaitForever) == osOK) {
			RTC_WP_OFF();
			addr = 0x80;
			for (i = 0; i < 7; i++) {
				RTC_WriteByte(addr, dat[i]);
				addr += 2;
			}
			RTC_WP_ON();
			if (MCPU_IS_ISR() == 0) {
				osMutexRelease(ExtRTC_MultexId);
			}
			r = TRUE;
		}
	}
	return r;
}

/**
 * RTC时间戳转为字符串输出
 * @param ts   输入的时间戳
 * @param buf  输出的字符串
 */
void RTC_TickToStr(uint32_t ts, char* buf) {
	timeRTC_t time;

	RTC_TickToTime(ts, &time);
	sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d", time.year,
					time.month, time.date, time.hours, time.minutes, time.seconds);
}

/**
 * RTC时间转为字符串输出
 * @param tim  输入的时间
 * @param buf  输出的字符串指针
 */
void RTC_TimeToStr(timeRTC_t* tim, char* buf) {
	timeRTC_t time = *tim;

	sprintf(buf, "%04d-%02d-%02d-%02d:%02d:%02d", time.year,
					time.month, time.date, time.hours, time.minutes, time.seconds);
}

/**
 * RTC时钟戳转化为格式化的时间数据
 * @param tick 待转换的时间戳
 * @param time 转换输出的时间数据
 */
void RTC_TickToTime(uint32_t tick, timeRTC_t* time) {
	uint8_t tmpMonth = 0, tmpHour = 0, tmpMinute = 0;
	uint16_t tmpYear = 0, temp = 0;
	uint32_t tmpresSecond = 0, TotalSeconds = 0, TotalDays = 0;

	TotalSeconds = tick;
	TotalDays = TotalSeconds / 86400;
	tmpresSecond = TotalSeconds % 86400;

	/*计算年*/
	tmpYear = RTC_YEAR_BASE;
	temp = (IS_LEAP(tmpYear)) ? 366 : 365;
	while (TotalDays >= temp) {
		TotalDays -= temp;
		tmpYear++;
		temp = (IS_LEAP(tmpYear)) ? 366 : 365;
	}
	/*计算月*/
	tmpMonth = 1;
	temp = mon_table[0];
	while (TotalDays >= temp && tmpMonth < 12) {
		TotalDays -= temp;
		tmpMonth++;
		temp = mon_table[tmpMonth - 1];
		if (tmpMonth == 2) {
			temp = (IS_LEAP(tmpYear)) ? 29 : 28;
		}
	}
	/*计算时，分，秒*/
	tmpHour = tmpresSecond / 3600;
	tmpMinute = tmpresSecond % 3600 / 60;
	tmpresSecond = tmpresSecond % 60;

	time->seconds = tmpresSecond;
	time->minutes = tmpMinute;
	time->hours = tmpHour;
	time->date = TotalDays + 1;
	time->month = tmpMonth;
	time->year = tmpYear;
	time->day = RTC_GetWeek(time->year, time->month, time->date);
}

/**
	* @brief  RTC时间转换成秒.
	* @param  time: 待转换的时间.
	* @retval none.
	*/
uint32_t RTC_TimeToTick(timeRTC_t* time) {
	uint32_t temp = 0, i = 0;

	if (time->year < RTC_YEAR_BASE) {
		return 0;
	}

	temp = (time->year - RTC_YEAR_BASE) * 365 * 86400;
	for (i = RTC_YEAR_BASE; i < time->year; i++) {
		if (IS_LEAP(i)) {
			temp += 86400;
		}
	}
	for (i = 0; i < (time->month - 1); i++) {
		temp += mon_table[i] * 86400;
		if (i == 1 && (IS_LEAP(time->year))) {
			temp += 86400;
		}
	}
	temp += (time->date - 1) * 86400;
	temp += time->hours * 3600;
	temp += time->minutes * 60;
	temp += time->seconds;

	return temp;
}

/**
	* @brief  读出当前时间的秒计算.
	* @retval none.
	*/
uint32_t RTC_ReadTick(void) {
	timeRTC_t time;

	RTC_ReadTime(&time);
	return RTC_TimeToTick(&time);
}

/* Private function prototypes -----------------------------------------------*/
/**
	* @brief  RTC写一个字节.
	* @param  addr:写入的地址
	* @param  c: 写入的字节.
	* @retval none.
	*/
static void RTC_WriteByte(uint8_t addr, uint8_t c) {
	uint8_t i = 0, temp = 0;

	RTC_CS_H();
	RTC_DATA_CFG_OUT();

	temp = addr;
	for (i = 0; i < 8; i++) {
		IO_L(RTC_CLK);
		delay(2);
		(temp & 0x01) ? IO_H(RTC_DATA) : IO_L(RTC_DATA);
		IO_H(RTC_CLK);
		delay(2);
		temp >>= 1;
	}

	temp = c;
	for (i = 0; i < 8; i++) {
		IO_L(RTC_CLK);
		delay(2);
		(temp & 0x01) ? IO_H(RTC_DATA) : IO_L(RTC_DATA);
		IO_H(RTC_CLK);
		delay(2);
		temp >>= 1;
	}

	IO_L(RTC_CLK);
	RTC_CS_L();
}

/**
	* @brief  RTC读一个字节.
	* @param  addr:读出的地址
	* @retval 返回读出的字节.
	*/
static uint8_t RTC_ReadByte(uint8_t addr) {
	uint8_t i = 0, temp = 0;

	RTC_CS_H();
	RTC_DATA_CFG_OUT();

	temp = addr;
	for (i = 0; i < 8; i++) {
		IO_L(RTC_CLK);
		delay(2);
		(temp & 0x01) ? IO_H(RTC_DATA) : IO_L(RTC_DATA);
		IO_H(RTC_CLK);
		delay(2);
		temp >>= 1;
	}

	RTC_DATA_CFG_IN();
	temp = 0;
	for (i = 0; i < 8; i++) {
		temp >>= 1;
		IO_L(RTC_CLK);
		delay(2);
		if (IO_READ(RTC_DATA)) {
			temp |= 0x80;
		}
		IO_H(RTC_CLK);
		delay(2);
	}
	IO_L(RTC_CLK);
	RTC_CS_L();
	return temp;
}

/**
	* @brief  RTC计算星期值.
	*/
static uint8_t RTC_GetWeek(uint16_t year, uint8_t month, uint8_t date) {
	uint16_t  temp2;
	uint8_t yearH, yearL;

	yearH = year / 100;
	yearL = year % 100;
	/*如果为21世纪,年份数加100 */
	if (yearH > 19) {
		yearL += 100;
	}
	/* 所过闰年数只算1900年之后的*/
	temp2 = yearL + yearL / 4;
	temp2 = temp2 % 7;
	temp2 = temp2 + date + week_table[month - 1];
	if (yearL % 4 == 0 && month < 3) {
		temp2--;
	}
	temp2 = temp2 % 7;
	if (temp2 == 0) {
		return 7;
	} else {
		return temp2;
	}
}

/**
 * 时间命令控制台处理.
 * @param argc 参数项数量
 * @param argv 参数项列表
 */
static void Time_Console(int argc, char* argv[]) {
	char* p = NULL, timebuf[20];
	uint32_t i = 0, ts = 0;
	timeRTC_t time;

	argc--;
	argv++;
	if (ARGV_EQUAL("time2ts")) {
		p = argv[1];
		time.year = uatoi(p);
		while (p && *p && *p != '-')
			p++;
		while (p && *p && *p == '-')
			p++;
		time.month = uatoi(p);
		while (p && *p && *p != '-')
			p++;
		while (p && *p && *p == '-')
			p++;
		time.date = uatoi(p);
		while (p && *p && *p != '-')
			p++;
		while (p && *p && *p == '-')
			p++;
		time.hours = uatoi(p);
		while (p && *p && *p != ':')
			p++;
		while (p && *p && *p == ':')
			p++;
		time.minutes = uatoi(p);
		while (p && *p && *p != ':')
			p++;
		while (p && *p && *p == ':')
			p++;
		time.seconds = uatoi(p);
		ts = RTC_TimeToTick(&time);
		DBG_LOG("time to ts:%u", ts);
	} else if (ARGV_EQUAL("ts2time")) {
		ts = uatoi(argv[1]);
		RTC_TickToStr(ts, timebuf);
		DBG_LOG("time:%s", timebuf);
	} else if (ARGV_EQUAL("readtick")) {
		DBG_LOG("timm ts:%u", RTC_ReadTick());
	} else {
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "set") == 0) {
				time.year = uatoi(argv[i + 1]);
				time.month = uatoi(argv[i + 2]);
				time.date = uatoi(argv[i + 3]);
				time.hours = uatoi(argv[i + 4]);
				time.minutes = uatoi(argv[i + 5]);
				time.seconds = uatoi(argv[i + 6]);
				time.day = uatoi(argv[i + 7]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "year") == 0) {
				RTC_ReadTime(&time);
				time.year = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "month") == 0) {
				RTC_ReadTime(&time);
				time.month = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "date") == 0) {
				RTC_ReadTime(&time);
				time.date = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "hour") == 0) {
				RTC_ReadTime(&time);
				time.hours = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "minute") == 0) {
				RTC_ReadTime(&time);
				time.minutes = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			} else if (strcmp(argv[i], "day") == 0) {
				RTC_ReadTime(&time);
				time.day = uatoi(argv[i + 1]);
				RTC_SetTime(&time);
			}
		}
		RTC_ReadTimeStr(timebuf);
		DBG_LOG("time:%s", timebuf);
	}
}


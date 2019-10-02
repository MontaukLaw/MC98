
/**
 * *********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file dfu.c
 * @version V1.0
 * @date 2016.4.1
 * @brief ??????????????.
 *
 * *********************************************************************
 * @note
 *
 * *********************************************************************
 * @author ????
 */



/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
#if DFU_HTTP_AUTO_EN > 0
static uint32_t tsHTTP = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
static void DFU_Console(int argc, char* argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * DFU?????
 */
void DFU_Init(void) {
	uint8_t flag = 0;

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_BKP_CLK_ENABLE();
	HAL_PWR_EnableBkUpAccess();


	flag = DFU_READ_FALG();
	switch (flag) {
		case DFU_FLAG_OK:
		case DFU_FLAG_CRC_FAULT:
		case DFU_FLAG_FAULT:
			break;
		default:
			break;
	}
	DFU_WRITE_FALG(DFU_FLAG_NONE);

	CMD_ENT_DEF(DFU, DFU_Console);
	Cmd_AddEntrance(CMD_ENT(DFU));

#if DFU_HTTP_POWER_ON > 0
	DBG_LOG("HTTP DFU power on start.");
	CMD_Virtual("DFU HTTP");
#endif

	DBG_LOG("DFU Init, flag:%#x", flag);
}

/**
 * DFU???????
 */
void DFU_Poll(void) {
	/*HTTP ???????*/
#if DFU_HTTP_AUTO_EN > 0
	if (TS_IS_OVER(tsHTTP, DFU_HTTP_AUTO_TIME * 1000)) {
		TS_INIT(tsHTTP);
		DBG_LOG("HTTP DFU auto start.");
		CMD_Virtual("DFU HTTP");
	}
#endif
}

/**
 * http????????
 * @return ??????????
 */
BOOL DFU_HTTP_Download(char* host, char* path) {
	char* ret = NULL, *ppoject = NULL, *pver = NULL, *ppath = NULL, *p;

	if (host == NULL || path == NULL) {
		host = DFU_HTTP_DEF;
		path = DFU_HTTP_PATH_DEF;
	}
	DBG_LOG("DFU http start, servers:%s, path:%s", host, path);
	ret = HTTP_GetData(host, path);
	if (ret != NULL) {
		ppoject = strstr(ret, "project:");
		if (ppoject != NULL) {
			while (*ppoject++ != ':');
		}
		pver = strstr(ret, "version:");
		if (pver != NULL) {
			while (*pver++ != ':');
		}
		ppath = strstr(ret, "path:");
		if (ppath != NULL) {
			while (*ppath++ != ':');
		}
		p = ret;

		/*?????????*/
		while (*p) {
			if (!isgraph(*p)) {
				*p = 0;
			}
			p++;
		}
		if (ppoject != NULL && pver != NULL && ppath != NULL) {
			if (STR_NEQUAL(ppoject, PROJECT) && strcmp(pver, VERSION) > 0) {
				DBG_LOG("DFU http get data, servers:%s, path:%s", host, ppath);
				if (HTTP_GetDataToFlash(host, ppath, DFU_SAVE_ADDR(), DFU_HTTP_TIMEOUT)) {
					DBG_LOG("DFU http data get don, system reset to pagram...");
					NVIC_SystemReset();
				}
			} else {
				DBG_LOG("DFU http image version not allow, remote version:%s, local version:%s.", pver, VERSION);
			}
		}
		MMEMORY_FREE(ret);
	}
	return FALSE;
}


/* Private function prototypes -----------------------------------------------*/

/**
 * DFU????????
 * @param argc ??????????
 * @param argv ?????งา?
 */
static void DFU_Console(int argc, char* argv[]) {
	uint8_t* buf = NULL;
	uint32_t size = 0,  write = 0, l = 0, addr = 0;

	argv++;
	argc--;
	if (ARGV_EQUAL("download")) {
		size = uatoi(argv[1]);
		DBG_LOG("DFU memory erase start.");
		SFlash_EraseSectors(DFU_SAVE_ADDR(), size / SFLASH_SECTOR_SIZE + 1);
		DBG_LOG("Erease OK, sectors number:%u", size / SFLASH_SECTOR_SIZE + 1);
		DBG_LOG("Please transmit image file.");

		buf = MMEMORY_ALLOC(DFU_BUF_SIZE);
		if (buf != NULL) {
			write = 0;
			addr = DFU_SAVE_ADDR();
			while (write < size) {
				l = size - write;
				if (l > DFU_BUF_SIZE) {
					l = DFU_BUF_SIZE;
				}
				if (CMD_DataSize() >= l) {
					LED_TOGGLE(NET);
					l = CMD_ReadData(buf, l);
					SFlash_Write(addr, (uint8_t*)buf, l);
					write += l;
					addr += l;
				} else {
					osDelay(2);
				}
			}
			MMEMORY_FREE(buf);
		}
		DBG_LOG("Transmit Complete, System will reset to program.");
		NVIC_SystemReset();
	} else if (ARGV_EQUAL("clear")) {
		DBG_LOG("DFU memory clear start");
		SFlash_EraseSectors(DFU_SAVE_ADDR(), SECTOR_DFU_SIZE);
		DBG_LOG("DFU memory clear OK, sectors:%u", SECTOR_DFU_SIZE);
	}
	if (ARGV_EQUAL("HTTP")) {
		DBG_LOG("DFU http get file start, please wait for download.");
		DFU_HTTP_Download(argv[1], argv[2]);
	}
}

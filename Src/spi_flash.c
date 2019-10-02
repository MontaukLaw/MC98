/**
 * **********************************************************************
 *             Copyright (c) 2016 temp. All Rights Reserved.
 * @file spi_flash.c
 * @author ����
 * @version V1.0
 * @date 2016.4.1
 * @brief spi_flash ���������ļ�.
 *
 * **********************************************************************
 * @note
 * 2016.12.26 ���ӽ���mutex����.
 *
 * **********************************************************************
 */



/* Includes ------------------------------------------------------------------*/
#include "spi_flash.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

#define SFLASH_WP_OFF()             IO_H(SFLASH_WP)
#define SFLASH_WP_ON()              IO_L(SFLASH_WP)

#define SFLASH_CS_H()               IO_H(SFLASH_CS)
#define SFLASH_CS_L()               IO_L(SFLASH_CS)

#define ADDR_ALLOW(addr)            (addr >= SECTOR_ADDR(SECTOR_SAFE_NUM) && addr < SFlash_Info.Capacity)

/* Private variables ---------------------------------------------------------*/
static uint32_t             SwapSector_Num;
static osMutexId            SFlash_MutexId;
static SPI_HandleTypeDef    *SFlash_Handle;

SPI_Flash_Info_t            SFlash_Info;

/* Private function prototypes -----------------------------------------------*/
static BOOL SFlash_PageWrite(uint32_t addr, uint8_t *wdat, uint32_t len);
static void SFlash_WriteEn(void);
static BOOL SFlash_WriteProtect(void);
static BOOL SFlash_UnWriteProtect(void);
static BOOL SFlash_Get(void);
static void SFlash_Release(void);
static uint8_t SFlash_RW(uint8_t c);
static BOOL SFlash_ReadInfo(void);
static BOOL SFlash_WaitForWriteEnd(uint32_t ms);

static void SFlash_Console(int argc, char *argv[]);

/* Exported functions --------------------------------------------------------*/

/**
 * spi flash ������ʼ��
 * @param h_spi  spi HAL�ľ��
 */
void SFlash_Init(SPI_HandleTypeDef *handle)
{
    IO_H(SFLASH_CS);
    SFlash_Handle = handle;

    __HAL_SPI_ENABLE(SFlash_Handle);

    osMutexDef(SFlash);
    SFlash_MutexId = osMutexCreate(osMutex(SFlash));

    SFlash_WriteProtect();

    SFlash_ReadInfo();

    SwapSector_Num = SFlash_Info.SectorCount - SECTOR_SWAP_NUM;

    CMD_ENT_DEF(flash, SFlash_Console);
    Cmd_AddEntrance(CMD_ENT(flash));

    DBG_LOG("SPI falsh Init.");
}

/**
 * ��ȡflash�������ĵ�ַ.
 * @return ���ؽ��������ĵ�ַ
 */
BOOL SFlash_GetSwapSector(uint32_t *addr)
{
    BOOL r = FALSE;

    if (SFlash_Get()) {
        SwapSector_Num++;
        if (SwapSector_Num >= SFlash_Info.SectorCount - 1) {
            SwapSector_Num = SFlash_Info.SectorCount - SECTOR_SWAP_NUM;
        }
        SFlash_Release();
        r = TRUE;
    }
    *addr = SECTOR_ADDR(SwapSector_Num);
    return r;
}

/**
 *  ��SPI FLASH.
 * @param addr �����ĵ�ַ
 * @param rdat �������ݱ����ָ��
 * @param len  �����ĳ���
 * @return �����ɹ�����TRUE
 */
BOOL SFlash_Read(uint32_t addr, uint8_t *rdat, uint32_t len)
{
    uint32_t i = 0;
    BOOL r = FALSE;

    if (SFlash_Get()) {
        SFLASH_CS_L();

        SFlash_RW(SFLASH_CMD_READ);
        SFlash_RW((addr & 0xFF0000) >> 16);
        SFlash_RW((addr & 0xFF00) >> 8);
        SFlash_RW(addr & 0xFF);

        for (i = 0; i < len; i++) {
            *(rdat + i) = SFlash_RW(SFLASH_CMD_DUMMY);
        }
        SFLASH_CS_H();
        SFlash_Release();
        r = TRUE;
    }
    return r;
}

/**
 * �е�ַ���SPI flashд�뺯��
 * @param addr д��ĵ�ַ
 * @param wdat ��д�������
 * @param len  ��д������ݳ���
 * @return ����д��״̬
 */
BOOL SFlash_Write(uint32_t addr, uint8_t *wdat, uint32_t len)
{
    BOOL r = FALSE;

    if (ADDR_ALLOW(addr)) {
        r = SFlash_Write_NotCheck(addr, wdat, len);
    }
    return r;
}

/**
 * �޵�ַ���SPI flashд�뺯��,����������д��SPI flash��ȫ����.
 * @param addr д��ĵ�ַ
 * @param wdat ��д�������
 * @param len  ��д������ݳ���
 * @return ����д��״̬
 */
BOOL SFlash_Write_NotCheck(uint32_t addr, uint8_t *wdat, uint32_t len)
{
    uint32_t rem = 0, wlen = 0;

    if (wdat == NULL || len == 0 || (len + addr) >= SFlash_Info.Capacity) {
        return FALSE;
    }

    if (SFlash_Get()) {
        SFlash_UnWriteProtect();

        rem = addr % SFLASH_PAGE_SIZE;
        if (rem > 0) {
            rem = SFLASH_PAGE_SIZE - rem;
            if (rem > len - wlen) {
                rem = len - wlen;
            }
            SFlash_PageWrite(addr, wdat, rem);
            wlen += rem;
            addr += rem;
            wdat += rem;
        }

        while (wlen < len) {
            rem = SFLASH_PAGE_SIZE;
            if (rem > len - wlen) {
                rem = len - wlen;
            }
            if (SFlash_PageWrite(addr, wdat, rem) == FALSE) {
                break;
            }
            addr += rem;
            wdat += rem;
            wlen += rem;
        }
        SFlash_WriteProtect();
        SFlash_Release();
    }
    return (wlen == len) ? TRUE : FALSE;
}

/**
 * SPI FLASH ��Ƭ����.
 * @return ���ز������
 */
BOOL SFlash_EraseChip(void)
{
    BOOL r = FALSE;

    if (SFlash_Get()) {
        SFlash_UnWriteProtect();

        SFlash_WriteEn();
        SFLASH_CS_L();
        SFlash_RW(SFLASH_CMD_BE);
        SFLASH_CS_H();
        r = SFlash_WaitForWriteEnd(30000);

        SFlash_WriteProtect();
        SFlash_Release();
    }
    return r;
}

/**
 * ����ַ����SPI FLASH ��������.
 * @param addr ������������ʼ��ַ
 * @param nums ����������������
 * @return ���ز������
 */
BOOL SFlash_EraseSectors(uint32_t addr, uint16_t nums)
{
    BOOL r = FALSE;

    if (ADDR_ALLOW(addr)) {
        r = SFlash_EraseSectors_NotCheck(addr, nums);
    }
    return r;
}

/**
 * �޵�ַ����SPI FLASH ��������,������������ȫ����
 * @param addr ������������ʼ��ַ
 * @param nums ����������������
 * @return ���ز������
 */
BOOL SFlash_EraseSectors_NotCheck(uint32_t addr, uint16_t nums)
{
    uint16_t i = 0;

    if (nums == 0) {
        return FALSE;
    }

    if (SFlash_Get()) {
        SFlash_UnWriteProtect();
        for (i = 0; i < nums; i++) {
            SFlash_WriteEn();
            SFLASH_CS_L();
            SFlash_RW(SFLASH_CMD_SE);
            SFlash_RW((addr & 0xFF0000) >> 16);
            SFlash_RW((addr & 0xFF00) >> 8);
            SFlash_RW(addr & 0xFF);
            SFLASH_CS_H();
            if (SFlash_WaitForWriteEnd(400) == FALSE) {
                break;
            }
            addr += SFLASH_SECTOR_SIZE;
        }
        SFlash_WriteProtect();
        SFlash_Release();
    }
    return (i == nums) ? TRUE : FALSE;
}

/* Private function prototypes -----------------------------------------------*/

/**
 * �ޱ���SPI FLASHҳ���,������ʹ��ǰ���Ƚ��д����.
 * @param addr д��ĵ�ַ
 * @param wdat ��д�������
 * @param len  ��д������ݳ���
 * @return ����д��״̬
 */
static BOOL SFlash_PageWrite(uint32_t addr, uint8_t *wdat, uint32_t len)
{
    uint16_t i = 0;
    BOOL r = FALSE;

    if (wdat != NULL && len > 0 && len <= SFLASH_PAGE_SIZE) {

        SFlash_WriteEn();
        SFLASH_CS_L();
        SFlash_RW(SFLASH_CMD_WRITE);
        SFlash_RW((addr & 0xFF0000) >> 16);
        SFlash_RW((addr & 0xFF00) >> 8);
        SFlash_RW(addr & 0xFF);

        for (i = 0; i < len; i++) {
            SFlash_RW(*(wdat + i));
        }
        SFLASH_CS_H();

        /*�ȴ���̣���ʱ100����*/
        r = SFlash_WaitForWriteEnd(100);
    }
    return r;
}

/**
  * @brief  SPI FLASHдʹ������.
  */
static void SFlash_WriteEn(void)
{
    SFLASH_CS_L();
    SFlash_RW(SFLASH_CMD_WREN);
    SFLASH_CS_H();
}

/**
 * SPI FLASHд����.
 * @return ����ִ�н��.
 */
static BOOL SFlash_WriteProtect(void)
{
    SFlash_WriteEn();

    SFLASH_WP_OFF();
    SFLASH_CS_L();
    SFlash_RW(SFLASH_CMD_WRSR);
    SFlash_RW(0x9E);
    SFLASH_CS_H();
    SFLASH_WP_ON();

    return SFlash_WaitForWriteEnd(100);
}

/**
 * SPI FLASH�������.
 * @return ����ִ�н��.
 */
static BOOL SFlash_UnWriteProtect(void)
{
    SFlash_WriteEn();

    SFLASH_WP_OFF();
    SFLASH_CS_L();
    SFlash_RW(SFLASH_CMD_WRSR);
    SFlash_RW(0x82);
    SFLASH_CS_H();
    SFLASH_WP_ON();

    return SFlash_WaitForWriteEnd(100);
}

/**
 * ��ȡ SPI Flash,����ȡmutex.
 * @return ����mutex��ȡ�����
 */
static BOOL SFlash_Get(void)
{
    if (MCPU_IS_ISR()) {
        return TRUE;
    }
    return (osMutexWait(SFlash_MutexId, osWaitForever) == osOK) ? TRUE : FALSE;
}

/**
 * �ͷ� SPI Flash,���ͷ�mutex������CS����.
 */
static void SFlash_Release(void)
{
    SFLASH_CS_H();
    SFlash_RW(SFLASH_CMD_DUMMY);

    if (!MCPU_IS_ISR()) {
        osMutexRelease(SFlash_MutexId);
    }
}

/**
 * SPI FLASH��дһ���ֽ�.
 * @param c    д����ֽ�
 * @return ���ص��ֽ�
 */
static uint8_t SFlash_RW(uint8_t c)
{
    SPI_TypeDef *spi = SFlash_Handle->Instance;

    if (__HAL_SPI_GET_FLAG(SFlash_Handle, SPI_FLAG_RXNE) != RESET) {
        __HAL_SPI_CLEAR_OVRFLAG(SFlash_Handle);
    }

    while ((spi->SR & SPI_FLAG_TXE) == RESET);
    spi->DR = c;
    while ((spi->SR & SPI_FLAG_RXNE) == RESET);
    return spi->DR;
}

/**
 * SPI FLASH��оƬ��Ϣ.
 * @return �����ɹ�����TRUE.
 */
static BOOL SFlash_ReadInfo(void)
{
    BOOL r = FALSE;

    /*��flash�����Ϣ*/
    if (SFlash_Get()) {

        /*���������豸ID*/
        SFLASH_CS_L();
        SFlash_RW(SFLASH_CMD_RDID);
        SFlash_Info.ManFID = SFlash_RW(SFLASH_CMD_DUMMY);
        SFlash_Info.Type = SFlash_RW(SFLASH_CMD_DUMMY);
        SFlash_Info.DevID = SFlash_RW(SFLASH_CMD_DUMMY);
        SFLASH_CS_H();

        /*֧��25Q, 25Pϵ��  SPI FLASH*/
        if (SFlash_Info.ManFID == 0xEF) {
            /*֧��25Q80 - 25Q256*/
            if (SFlash_Info.DevID >= 0x14 && SFlash_Info.DevID <= 0x19) {
                SFlash_Info.Capacity = 0x100000 * (1 << (SFlash_Info.DevID - 0x14));
                SFlash_Info.PageSize = SFLASH_PAGE_SIZE;
                SFlash_Info.SectorSize = SFLASH_SECTOR_SIZE;
                SFlash_Info.SectorCount = SFlash_Info.Capacity / SFLASH_SECTOR_SIZE;

                DBG_LOG("SPI flash config OK, id:0x%X, Sector count:%d, Capacity: %dKB.",
                        SFlash_Info.DevID,
                        SFlash_Info.SectorCount,
                        SFlash_Info.Capacity / 1024);
                r =  TRUE;
            } else {
                DBG_LOG("Not Support This Capacity Flash.");
            }
        } else {
            DBG_LOG("Not Support This Firm Flash:FID 0x%02X.", SFlash_Info.ManFID);
        }
        SFlash_Release();
    }
    return r;
}

/**
 * SPI FLASH�ȴ�д���.
 * @param ms     �ȴ���ʱ��,��λms
 * @return �����ӳ�ʱ����д��ɷ���TRUE.
 */
static BOOL SFlash_WaitForWriteEnd(uint32_t ms)
{
    uint8_t res = 0;
    uint32_t t = 0;

    t = HAL_GetTick();
    SFLASH_CS_L();
    SFlash_RW(SFLASH_CMD_RDSR);

    res = SFlash_RW(SFLASH_CMD_DUMMY);
    while ((res & SFLASH_WIP_FLAG) && (HAL_GetTick() - t <= ms)) {
        res = SFlash_RW(SFLASH_CMD_DUMMY);
        if (!MCPU_IS_ISR()) {
            osDelay(2);
        }
    }
    SFLASH_CS_H();

    if (res & SFLASH_WIP_FLAG) {
        THROW_PRINTF("SFlash_WaitForWriteEnd fail:%#x.", res);
    }

    return ((res & SFLASH_WIP_FLAG) ? FALSE : TRUE);
}

/**
 * SPI falsh��������
 * @param argc ����������
 * @param argv �����б�
 */
static void SFlash_Console(int argc, char *argv[])
{
    BOOL r = FALSE;
    uint32_t i = 0, addr = 0, len = 0, j = 0, write = 0;
    char *p = NULL, *buf = NULL;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "CS") == 0) {
            j = uatoi(argv[i + 1]);
            if (j) {
                SFLASH_CS_H();
            } else {
                SFLASH_CS_L();
            }
            DBG_LOG("SPI flash CS set:%d", j);
        } else if (strcmp(argv[i], "RW") == 0) {
            p = argv[i + 1];
            j = SFlash_RW(uatoi(p));
            DBG_LOG("SPI flash read write res:%d", j);
        } else if (strcmp(argv[i], "erase") == 0) {
            p = argv[i + 1];
            if (strcmp(p, "all") == 0) {
                DBG_LOG("SPI flash erase chip begin.");
                SFlash_EraseChip();
                DBG_LOG("SPI flash erase chip OK.");
            } else if (isxdigit(*p)) {
                if (Cmd_ArgFind(&argv[i + 2], "-x") >= 0) {
                    addr = uatoix(p);
                } else {
                    addr = uatoi(p);
                }
                if (Cmd_ArgFind(&argv[i + 2], "-d") >= 0) {
                    r = SFlash_EraseSectors_NotCheck(addr, 1);
                } else {
                    r = SFlash_EraseSectors(addr, 1);
                }
                DBG_LOG("SPI flash erase sector address 0x%02X res:%d.", addr, (int)r);
            }
        } else if (strcmp(argv[i], "write") == 0) {
            if (Cmd_ArgFind(&argv[i + 3], "-x") >= 0) {
                addr = uatoix(argv[i + 1]);
                len = uatoix(argv[i + 2]);
            } else {
                addr = uatoi(argv[i + 1]);
                len = uatoi(argv[i + 2]);
            }
            j = 0;
            write = 0;
            buf = MMEMORY_ALLOC(512);
            if (buf != NULL) {
                r = (Cmd_ArgFind(&argv[i + 3], "-d") >= 0) ? TRUE : FALSE;
                DBG_LOG("SPI flash write address 0x%02X begin.", addr);
                while (write < len) {
                    j = len - write;
                    if (j > 512) {
                        j = 512;
                    }
                    if (CMD_DataSize() >= j) {
                        j = CMD_ReadData((uint8_t *)buf, j);
                        if (r) {
                            SFlash_Write_NotCheck(addr, (uint8_t *)buf, j);
                        } else {
                            SFlash_Write(addr, (uint8_t *)buf, j);
                        }
                        write += j;
                        addr += j;
                    } else {
                        osDelay(2);
                    }
                }
                DBG_LOG("SPI flash write OK, end address:0x%0X, length:%d.", addr, write);
                MMEMORY_FREE(buf);
            }
        } else if (strcmp(argv[i], "swap") == 0) {
            SFlash_GetSwapSector(&addr);
            DBG_LOG("SPI swap address 0x%X now.", addr);
        } else if (strcmp(argv[i], "read") == 0) {
            if (Cmd_ArgFind(&argv[i + 3], "-x") >= 0) {
                addr = uatoix(argv[i + 1]);
                len = uatoix(argv[i + 2]);
            } else {
                addr = uatoi(argv[i + 1]);
                len = uatoi(argv[i + 2]);
            }
            buf = MMEMORY_ALLOC(len);
            if (buf != NULL) {
                if (SFlash_Read(addr, (uint8_t *)buf, len)) {
                    DBG_LOG("SPI flash read address 0x%02X:", addr);
                    CMD_HEX_Print((uint8_t *)buf, len);
                }
                MMEMORY_FREE(buf);
            }
        }
    }
}



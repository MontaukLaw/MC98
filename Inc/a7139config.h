/********************************************************************
*   A7139CONFIG.h
*   RF Chip-A7139 Configure Definitions
*
*   This file provides the constants associated with the
*   AMICCOM A7139 device.
*
********************************************************************/
#ifndef _A7139CONFIG_h_
#define _A7139CONFIG_h_

#define DR_10Kbps_50KIFBW       //433MHz, 10kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
//#define DR_10Kbps_100KIFBW      //433MHz, 10kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
//#define DR_50Kbps_50KIFBW       //433MHz, 50kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
//#define DR_100Kbps_100KIFBW     //433MHz, 100kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
//#define DR_150Kbps_150KIFBW     //433MHz, 150kbps (IFBW = 150KHz, Fdev = 56.25KHz), Crystal=19.2MHz
//#define DR_250Kbps_250KIFBW     //433MHz, 250kbps (IFBW = 250KHz, Fdev = 93.75KHz), Crystal=16MHz

/*50K bps时 使能MSC 传输速度为24.3Kbps， 使能FEC为
  29.5Kbps， 数据白化不影响速度为46.4Kbps*/

/*GFSK使能*/
#define GFSK_EN             0

/*高斯滤形形状匹配，0为2.0，1为1.0，2为0.5*/
#define GFSK_SHAPE          0

/*数据白化使能*/
#define WHITENING_EN        1

/*曼彻斯特编码使能*/
#define MSC_EN              0

/*汉明纠错使能*/
#define FEC_EN              1

/*低电流接收关闭使能*/
#define LOWCURRENT_OFF      0

/*定义发射功率*/
/*0x0301 2.4db, 0x0302 5.4db, 0x0304 11.9db, 0x0307 15.4db,
  0x0347 17.4db, 0x035F 18.3db*/
#define RF_TX_POWER_INIT    0x032D

/*定义ID Code Error容忍*/
/*[000]: 0 bit. [001]: 1bit. [010]: 2 bits. [011]: 3 bits. [100]: 4 bits. [101]: 5 bits. [110]: 6 bits. [111]: 7 bits*/
#define RX1_ETH             1

/*定义Auto-Resend的重传次数，最大为15*/
#define ARC_DEF             5

/*定义Auto-Resend的间隔时间,最大为255*/
/*ARD Delay = 200 us * (ARD+1) , (200us ~ 51.2 ms)*/
#define ART_DEF             9


#ifdef DR_10Kbps_50KIFBW

const uint16_t A7139Config[] =        //433MHz, 10kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    0x0823,     //SYSTEM CLOCK register,
    0x0A21,     //PLL1 register,
    0xDA05,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x1850 | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=50KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 10kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    0xF606 | (GFSK_EN << 11),     //TX1 register,     Fdev = 18.75kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x9B70,     //PM register,      CST=1
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x0641,     //GIO register,
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0A21,     //CHG1 register,    430MHz
    0x0022,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 10kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8200,     //IF1 register,     Enable Auto-IF, IF=100KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif


#ifdef DR_10Kbps_100KIFBW

const uint16_t A7139Config[] =        //433MHz, 10kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    0x1221,     //SYSTEM CLOCK register,
    0x0A21,     //PLL1 register,
    0xDA05,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x1854 | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=100KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 10kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    0xF706 | (GFSK_EN << 11),     //TX1 register,     Fdev = 37.5kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x9B70,     //PM register,      CST=1
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x0641,     //GIO register,     GIO2=WTR, GIO1=FSYNC
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0A21,     //CHG1 register,    430MHz
    0x0022,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 10kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8400,     //IF1 register,     Enable Auto-IF, IF=200KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif


#ifdef DR_50Kbps_50KIFBW

const uint16_t A7139Config[] =        //433MHz, 50kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    0x0023,     //SYSTEM CLOCK register,
    0x0A21,     //PLL1 register,
    0xDA05,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x1850  | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=50KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 50kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    0xF606 | (GFSK_EN << 11),     //TX1 register,     Fdev = 18.75kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x1B70,     //PM register,      CST=1
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x0641,     //GIO register,
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0A21,     //CHG1 register,    430MHz
    0x0022,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 50kbps (IFBW = 50KHz, Fdev = 18.75KHz), Crystal=12.8MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8200,     //IF1 register,     Enable Auto-IF, IF=100KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif


#ifdef DR_100Kbps_100KIFBW

const uint16_t A7139Config[] =        //433MHz, 100kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    0x0021,     //SYSTEM CLOCK register,
    0x0A21,     //PLL1 register,
    0xDA05,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x1854  | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=100KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 100kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    0xF706 | (GFSK_EN << 11),     //TX1 register,     Fdev = 37.5kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x1B70,     //PM register,
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x0641,     //GIO register,
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0A21,     //CHG1 register,    430MHz
    0x0022,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 100kbps (IFBW = 100KHz, Fdev = 37.5KHz), Crystal=12.8MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8400,     //IF1 register,     Enable Auto-IF, IF=200KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif



#ifdef DR_150Kbps_150KIFBW

const uint16_t A7139Config[] =        //433MHz, 150kbps (IFBW = 150KHz, Fdev = 56.25KHz), Crystal=19.2MHz
{
    0x0021,     //SYSTEM CLOCK register,
    0x0A16,     //PLL1 register,
    0x9158,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x1858  | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=150KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 150kbps (IFBW = 150KHz, Fdev = 56.25KHz), Crystal=19.2MHz
{
    0xF706 | (GFSK_EN << 11),     //TX1 register,     Fdev = 56.25kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x1B70,     //PM register,
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x4641,     //GIO register,     GIO2=WTR, GIO1=FSYNC
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0616,     //CHG1 register,    430MHz
    0x0B16,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 150kbps (IFBW = 150KHz, Fdev = 56.25KHz), Crystal=19.2MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8400,     //IF1 register,     Enable Auto-IF, IF=300KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif


#ifdef DR_250Kbps_250KIFBW

const uint16_t A7139Config[] =        //433MHz, 250kbps (IFBW = 250KHz, Fdev = 93.75KHz), Crystal=16MHz
{
    0x0020,     //SYSTEM CLOCK register,
    0x0A1B,     //PLL1 register,
    0x14D0,     //PLL2 register,    433.301MHz
    0x0000,     //PLL3 register,
    0x0E20 | (LOWCURRENT_OFF << 8),     //PLL4 register,
    0x0024,     //PLL5 register,
    0x0000,     //PLL6 register,
    0x0011,     //CRYSTAL register,
    0x0000,     //PAGEA,
    0x0000,     //PAGEB,
    0x185C | ((RX1_ETH & 0x03) << 7) | (((RX1_ETH >> 2) & 0x01) << 15),     //RX1 register,     IFBW=250KHz
    0x7009,     //RX2 register,     by preamble
    0x4000,     //ADC register,
    0x0800,     //PIN CONTROL register,     Use Strobe CMD
    0x4C45,     //CALIBRATION register,
    0x20C0      //MODE CONTROL register,    Use FIFO mode
};

const uint16_t A7139Config_PageA[] =   //433MHz, 250kbps (IFBW = 250KHz, Fdev = 93.75KHz), Crystal=16MHz
{
    0xF530 | (GFSK_EN << 11),     //TX1 register,     Fdev = 93.75kHz
    0x0000,     //WOR1 register,
    0xF800,     //WOR2 register,
    0x1907,     //RFI register,     Enable Tx Ramp up/down
    0x1B70,     //PM register,
    0x0201,     //RTH register,
    0x400F,     //AGC1 register,
    0x2AC0,     //AGC2 register,
    0x8641,     //GIO register,     GIO2=WTR, GIO1=FSYNC
    0xD181,     //CKO register
    0x0004,     //VCB register,
    0x0E1A,     //CHG1 register,    430MHz
    0x031B,     //CHG2 register,    435MHz
    0x003F,     //FIFO register,    FEP=63+1=64bytes
    0x5503 | (WHITENING_EN << 5) | (MSC_EN << 6) | (FEC_EN << 4),     //CODE register,    Preamble=4bytes, ID=6bytes
    0x0000      //WCAL register,
};

const uint16_t A7139Config_PageB[] =   //433MHz, 250kbps (IFBW = 250KHz, Fdev = 93.75KHz), Crystal=16MHz
{
    RF_TX_POWER_INIT | (GFSK_SHAPE << 10),     //TX2 register,
    0x8800,     //IF1 register,     Enable Auto-IF, IF=500KHz
    0x0000,     //IF2 register,
    0x0000,     //ACK register,
    0x0000      //ART register,
};

#endif


#endif




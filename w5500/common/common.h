/**
 * @file        common.h
 * @brief       Library Common Header File.
 * This header file influence on all library area directly.
 *
 * @version 1.0
 * @date        2013/02/22
 * @par Revision
 *          2013/02/22 - 1.0 Release
 * @author  Mike Jeong
 * \n\n @par Copyright (C) 2013 WIZnet. All rights reserved.
 */

#ifndef _COMMON_H
#define _COMMON_H



//-------------------------------------- Common Includes ----------------------------------------
#include "w5500_comm.h"

#include "wizchip_conf.h"

#include "util.h"

#include "socket.h"
#include "netctrl.h"
#include "sockutil.h"

#include "socket_bsd.h"

#include "internet/dns/dns.h"
#include "internet/dhcp/dhcp.h"


#define RET_FAIL  1
#define RET_OK    0
#define RET_NOK      -1

#define VAL_HIGH  1
#define VAL_LOW      0

#define VAL_TOG      2
#define VAL_ON    1
#define VAL_OFF      0

#define VAL_SET      1
#define VAL_CLEAR 0

#define VAL_TRUE  1
#define VAL_FALSE 0

#define VAL_ENABLE   1
#define VAL_DISABLE  0

#define VAL_NONE  -1
#define VAL_INVALID  -2

//------------------------------------------- LOG ---------------------------------------------
#if !defined(WIZ_LOG_LEVEL) || (WIZ_LOG_LEVEL < 0) || (WIZ_LOG_LEVEL > 3)
#define WIZ_LOG_LEVEL 3
#endif

#if (WIZ_LOG_LEVEL > 0) && defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define ERR(fmt)  do { CMD_Printf("### ERROR ### [%5d.%03d] %s(%d): "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__); } while(0)
#define ERRA(fmt, ...)  do { CMD_Printf("### ERROR ### [%5d.%03d] %s(%d): "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#define ERRF(fmt) do { CMD_Printf("### ERROR ### [%5d.%03d] %s(%d): "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__); } while(0)
#define ERRFA(fmt, ...) do { CMD_Printf("### ERROR ### [%5d.%03d] %s(%d): "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#elif (WIZ_LOG_LEVEL > 0) && !defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define ERR(fmt)  do { CMD_Printf("### ERROR ### %s(%d): "fmt"\r\n", __FUNCTION__, __LINE__); } while(0)
#define ERRA(fmt, ...)  do { CMD_Printf("### ERROR ### %s(%d): "fmt"\r\n", __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#define ERRF(fmt) do { CMD_Printf("### ERROR ### %s(%d): "fmt, __FUNCTION__, __LINE__); } while(0)
#define ERRFA(fmt, ...) do { CMD_Printf("### ERROR ### %s(%d): "fmt, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#else
#define ERR(fmt)
#define ERRA(fmt, ...)
#define ERRF(fmt)
#define ERRFA(fmt, ...)
#endif

#if (WIZ_LOG_LEVEL > 1) && defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define LOG(fmt)  do { CMD_Printf("[%5d.%03d] "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000); } while(0)
#define LOGA(fmt, ...)  do { CMD_Printf("[%5d.%03d] "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __VA_ARGS__); } while(0)
#define LOGF(fmt) do { CMD_Printf("[%5d.%03d] "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000); } while(0)
#define LOGFA(fmt, ...) do { CMD_Printf("[%5d.%03d] "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __VA_ARGS__); } while(0)
#elif (WIZ_LOG_LEVEL > 1) && !defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define LOG(fmt)  do { CMD_Printf(fmt"\r\n"); } while(0)
#define LOGA(fmt, ...)  do { CMD_Printf(fmt"\r\n", __VA_ARGS__); } while(0)
#define LOGF(fmt) do { CMD_Printf(fmt); } while(0)
#define LOGFA(fmt, ...) do { CMD_Printf(fmt, __VA_ARGS__); } while(0)
#else
#define LOG(fmt)
#define LOGA(fmt, ...)
#define LOGF(fmt)
#define LOGFA(fmt, ...)
#endif

#if (WIZ_LOG_LEVEL > 2) && defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define DBG(fmt)  do { CMD_Printf("[D] [%5d.%03d] %s(%d): "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__); } while(0)
#define DBGA(fmt, ...)  do { CMD_Printf("[D] [%5d.%03d] %s(%d): "fmt"\r\n", \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#define DBGF(fmt) do { CMD_Printf("[D] [%5d.%03d] %s(%d): "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__); } while(0)
#define DBGFA(fmt, ...) do { CMD_Printf("[D] [%5d.%03d] %s(%d): "fmt, \
    wizpf_get_systick()/1000, wizpf_get_systick()%1000, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#elif (WIZ_LOG_LEVEL > 2) && !defined(PRINT_TIME_LOG) && !defined(FILE_LOG_SILENCE)
#define DBG(fmt)  do { CMD_Printf("[D] %s(%d): "fmt"\r\n", __FUNCTION__, __LINE__); } while(0)
#define DBGA(fmt, ...)  do { CMD_Printf("[D] %s(%d): "fmt"\r\n", __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#define DBGF(fmt) do { CMD_Printf("[D] %s(%d): "fmt, __FUNCTION__, __LINE__); } while(0)
#define DBGFA(fmt, ...) do { CMD_Printf("[D] %s(%d): "fmt, __FUNCTION__, __LINE__, __VA_ARGS__); } while(0)
#else
#define DBG(fmt)
#define DBGA(fmt, ...)
#define DBGF(fmt)
#define DBGFA(fmt, ...)
#endif

#if (WIZ_LOG_LEVEL > 2) && !defined(FILE_LOG_SILENCE)
#define DBGCRTC(cond_v, fmt) do { if(cond_v) {ERR(fmt); while(1); } } while(0)
#define DBGCRTCA(cond_v, fmt, ...) do { if(cond_v) {ERRA(fmt, __VA_ARGS__); while(1); } } while(0)
#define DBGDUMP(data_p, len_v) print_dump(data_p, len_v)
#define DBGFUNC(func_p) func_p
#else
#define DBGCRTC(cond_v, fmt)
#define DBGCRTCA(cond_v, fmt, ...)
#define DBGDUMP(data_p, len_v)
#define DBGFUNC(func_p)
#endif

#if (WIZ_LOG_LEVEL > 0) && !defined(FILE_LOG_SILENCE)
#define NL1 CMD_Printf("\r\n")
#define NL2 CMD_Printf("\r\n\r\n")
#define NL3 CMD_Printf("\r\n\r\n\r\n")
#else
#define NL1
#define NL2
#define NL3
#endif

//-------------------------------------------------------------------------------------------



#endif //_COMMON_H


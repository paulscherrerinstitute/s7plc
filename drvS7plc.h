/* $Author: zimoch $ */ 
/* $Date: 2005/03/11 15:16:13 $ */ 
/* $Id: drvS7plc.h,v 1.3 2005/03/11 15:16:13 zimoch Exp $ */  
/* $Name:  $ */ 
/* $Revision: 1.3 $ */ 

#ifndef drvS7plc_h
#define drvS7plc_h

#include <dbScan.h>

#ifndef __GNUC__
#define __attribute__(a)
#endif

#ifndef DEBUG
#define STATIC static
#else
#define STATIC
#endif

/*  driver initialisation define  */

typedef struct s7plcStation s7plcStation;

extern int s7plcDebug;

void s7plcDebugLog(int level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

s7plcStation *s7plcOpen(char *name);
IOSCANPVT s7plcGetInScanPvt(s7plcStation *station);
IOSCANPVT s7plcGetOutScanPvt(s7plcStation *station);

int s7plcReadArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* pdata
);

int s7plcWriteMaskedArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* pdata,
    void* pmask
);

#define s7plcWriteArray(station, offset, dlen, nelem, pdata) \
    s7plcWriteMaskedArray((station), (offset), (dlen), (nelem), (pdata), NULL)

#define s7plcWriteMasked(station, offset, dlen, pdata, mask) \
    s7plcWriteMaskedArray((station), (offset), (dlen), 1, (pdata), (mask))

#define s7plcWrite(station, offset, dlen, pdata) \
    s7plcWriteMaskedArray((station), (offset), (dlen), 1, (pdata), NULL)

#define s7plcRead(station, offset, dlen, pdata) \
    s7plcReadArray((station), (offset), (dlen), 1, (pdata))

/************************************************************************/
/* * DRV driver error codes */
#define M_drvLib (1003<<16U)
#define drvError(CODE) (M_drvLib | (CODE))

#define S_drv_OK 0 /* success */
#define S_drv_badParam drvError(1) /*driver: bad parameter*/
#define S_drv_noMemory drvError(2) /*driver: no memory*/
#define S_drv_noDevice drvError(3) /*driver: device not configured*/
#define S_drv_invSigMode drvError(4)/*driver: signal mode conflicts with device config*/
#define S_drv_cbackChg drvError(5) /*driver: specified callback differs from previous config*/
#define S_drv_alreadyQd drvError(6)/*driver: a read request is already queued for the channel*/
#define S_drv_noConn drvError(7) /*driver:   connection to plc lost*/

#endif /* drvS7plc_h */

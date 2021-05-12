/* $Author: zimoch $ */
/* $Date: 2015/06/29 09:45:47 $ */
/* $Id: drvS7plc.h,v 1.4 2015/06/29 09:45:47 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.4 $ */

#ifndef drvS7plc_h
#define drvS7plc_h

#include <dbScan.h>

#ifndef DEBUG
#define STATIC static
#else
#define STATIC
#endif

/*  driver initialisation define  */

typedef struct s7plcStation s7plcStation;

extern int s7plcDebug;

s7plcStation *s7plcOpen(char *name);
IOSCANPVT s7plcGetInScanPvt(s7plcStation *station);
IOSCANPVT s7plcGetOutScanPvt(s7plcStation *station);
int s7plcGetAddr(s7plcStation* station, char* addr);
int s7plcSetAddr(s7plcStation* station, const char* addr);

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

#define s7plcDebugLog(level, fmt, args...) if (level <= s7plcDebug) errlogPrintf(fmt, ##args);

#endif /* drvS7plc_h */

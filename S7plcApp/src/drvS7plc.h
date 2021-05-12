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

char* s7plcCurrentTime();

#if defined __GNUC__ && __GNUC__ < 3
/* old GCC style */
#define s7plcDebugLog(level, fmt, args...) do{if (level <= s7plcDebug) errlogPrintf("%s " fmt, s7plcCurrentTime() , ##args);}while(0)
#define s7plcErrorLog(fmt, args...) errlogPrintf("%s " fmt, s7plcCurrentTime() , ##args)
#else
/* posix style */
#define s7plcDebugLog(level, fmt, ...) do{if (level <= s7plcDebug) errlogPrintf("%s " fmt, s7plcCurrentTime(), ##__VA_ARGS__);}while(0)
#define s7plcErrorLog(fmt, ...) errlogPrintf("%s " fmt, s7plcCurrentTime() , ##__VA_ARGS__)
#endif



#endif /* drvS7plc_h */

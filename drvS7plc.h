/* $Author: zimoch $ */ 
/* $Date: 2005/02/07 16:06:41 $ */ 
/* $Id: drvS7plc.h,v 1.1 2005/02/07 16:06:41 zimoch Exp $ */  
/* $Name:  $ */ 
/* $Revision: 1.1 $ */ 

#ifndef drvS7plc_h
#define drvS7plc_h

#define Ok 0
#define ERR -1

#define MAX_DEVNAME_SIZE	15

#ifndef __GNUC__
#define __attribute__()
#endif

/*  driver initialisation define  */

typedef struct s7plcStation s7plcStation;

extern int s7plcDebug;

void s7plcDebugLog(int level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

s7plcStation *s7plcOpen(
    char *name,
    unsigned int stationNumber
);

int s7plcReadArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dleng,
    unsigned int nelem,
    void* pdata
);

int s7plcWriteMaskedArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dleng,
    unsigned int nelem,
    void* pdata,
    void* pmask
);

#define s7plcWriteArray(station, offset, dleng, nelem, pdata) \
    s7plcWriteMaskedArray((station), (offset), (dleng), (nelem), (pdata), NULL)

#define s7plcWriteMasked(station, offset, dleng, pdata, mask) \
    s7plcWriteMaskedArray((station), (offset), (dleng), 1, (pdata), (mask))

#define s7plcWrite(station, offset, dleng, pdata) \
    s7plcWriteMaskedArray((station), (offset), (dleng), 1, (pdata), NULL)

#define s7plcRead(station, offset, dleng, pdata) \
    s7plcReadArray((station), (offset), (dleng), 1, (pdata))

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

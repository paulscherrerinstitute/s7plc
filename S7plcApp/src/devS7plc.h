#ifndef devS7plc_h
#define devS7plc_h


#include <devSup.h>
#include <menuFtype.h>
#include <errlog.h>
#include <dbAccess.h>
#include <alarm.h>
#include <recGbl.h>
#include "drvS7plc.h"

#define isnan(x) ((x)!=(x))

/* suppress compiler warning concerning long long with __extension__ */
#if (!defined __GNUC__) || (__GNUC__ < 2) || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __extension__
#endif

#ifndef epicsUInt64
#if (LONG_MAX > 2147483647L)
#define epicsUInt64 unsigned long
#define CONV64 "%016lx"
#else
#define epicsUInt64 unsigned long long
#define CONV64 "%016llx"
#endif
#endif

#ifdef DBR_INT64
#define LIM_FORMAT "%#llx"
#else
#define LIM_FORMAT "%#x"
#endif

#define S7MEM_TIME 100

typedef struct {              /* Private structure to save IO arguments */
    s7plcStation *station;    /* Card id */
    unsigned short offs;      /* Offset (in bytes) within memory block */
    unsigned short bit;       /* Bit number (0-15) for bi/bo */
    unsigned short dtype;     /* Data type */
    unsigned short dlen;      /* Data length (in bytes) */
#ifdef DBR_INT64
    epicsInt64 hwLow;         /* Hardware Low limit */
    epicsInt64 hwHigh;        /* Hardware High limit */
#else
    epicsInt32 hwLow;         /* Hardware Low limit */
    epicsInt32 hwHigh;        /* Hardware High limit */
#endif
} S7memPrivate_t;

int s7plcIoParse(char* recordName, char *parameters, S7memPrivate_t *);
long s7plcGetInIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);
long s7plcGetOutIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);

struct devsup {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
};



#endif /* devS7plc_h */

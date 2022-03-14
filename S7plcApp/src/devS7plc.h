#ifndef devS7plc_h
#define devS7plc_h

#include <devSup.h>
#include <devLib.h>
#include <menuFtype.h>
#include <errlog.h>
#include <dbAccess.h>
#include <alarm.h>
#include <recGbl.h>
#include <epicsMath.h>
#include "drvS7plc.h"

#ifndef epicsUInt64
#define epicsUInt64 unsigned long long
#define epicsInt64  long long
#define CONV64 "%016llx"
#endif

#define S7MEM_TIME 100

typedef struct {              /* Private structure to save IO arguments */
    s7plcStation *station;    /* Card id */
    unsigned short offs;      /* Offset (in bytes) within memory block */
    unsigned short bit;       /* Bit number (0-15) for bi/bo */
    unsigned short dtype;     /* Data type */
    unsigned short dlen;      /* Data length (in bytes) */
    epicsInt64 hwLow;         /* Hardware Low limit */
    epicsInt64 hwHigh;        /* Hardware High limit */
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

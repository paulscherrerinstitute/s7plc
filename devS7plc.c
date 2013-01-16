/* $Author: zimoch $ */
/* $Date: 2013/01/16 10:17:14 $ */
/* $Id: devS7plc.c,v 1.16 2013/01/16 10:17:14 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.16 $ */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <alarm.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <devLib.h>
#include <errlog.h>

#include <epicsVersion.h>
#include <drvS7plc.h>

#include <biRecord.h>
#include <boRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <waveformRecord.h>

#if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)
/* R3.14 */
#include <postfix.h>
#include <calcoutRecord.h>
#include <cantProceed.h>
#include <epicsExport.h>
#else
/* R3.13 */
#include "compat3_13.h"
#endif
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

#define S7MEM_TIME 100

typedef struct {              /* Private structure to save IO arguments */
    s7plcStation *station;    /* Card id */
    unsigned short offs;      /* Offset (in bytes) within memory block */
    unsigned short bit;       /* Bit number (0-15) for bi/bo */
    unsigned short dtype;     /* Data type */
    unsigned short dlen;      /* Data length (in bytes) */
    epicsInt32 hwLow;         /* Hardware Low limit */
    epicsInt32 hwHigh;        /* Hardware High limit */
} S7memPrivate_t;

static char cvsid_devS7plc[] =
    "$Id: devS7plc.c,v 1.16 2013/01/16 10:17:14 zimoch Exp $";

STATIC long s7plcReport();

STATIC int s7plcIoParse(char* recordName, char *parameters, S7memPrivate_t *);
STATIC long s7plcGetInIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);
STATIC long s7plcGetOutIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);

struct devsup {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
};

/* bi for status bit ************************************************/

STATIC long s7plcInitRecordStat(biRecord *);
STATIC long s7plcReadStat(biRecord *);

struct devsup s7plcStat =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStat,
    s7plcGetInIntInfo,
    s7plcReadStat
};

epicsExportAddress(dset, s7plcStat);

/* bi ***************************************************************/

STATIC long s7plcInitRecordBi(biRecord *);
STATIC long s7plcReadBi(biRecord *);

struct devsup s7plcBi =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordBi,
    s7plcGetInIntInfo,
    s7plcReadBi
};

epicsExportAddress(dset, s7plcBi);

/* bo ***************************************************************/

STATIC long s7plcInitRecordBo(boRecord *);
STATIC long s7plcWriteBo(boRecord *);

struct devsup s7plcBo =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordBo,
    s7plcGetOutIntInfo,
    s7plcWriteBo
};

epicsExportAddress(dset, s7plcBo);

/* mbbi *************************************************************/

STATIC long s7plcInitRecordMbbi(mbbiRecord *);
STATIC long s7plcReadMbbi(mbbiRecord *);

struct devsup s7plcMbbi =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbbi,
    s7plcGetInIntInfo,
    s7plcReadMbbi
};

epicsExportAddress(dset, s7plcMbbi);

/* mbbo *************************************************************/

STATIC long s7plcInitRecordMbbo(mbboRecord *);
STATIC long s7plcWriteMbbo(mbboRecord *);

struct devsup s7plcMbbo =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbbo,
    s7plcGetOutIntInfo,
    s7plcWriteMbbo
};

epicsExportAddress(dset, s7plcMbbo);

/* mbbiDirect *******************************************************/

STATIC long s7plcInitRecordMbbiDirect(mbbiDirectRecord *);
STATIC long s7plcReadMbbiDirect(mbbiDirectRecord *);

struct devsup s7plcMbbiDirect =
{
    5,
    s7plcReport,
    NULL,
    s7plcInitRecordMbbiDirect,
    s7plcGetInIntInfo,
    s7plcReadMbbiDirect
};

epicsExportAddress(dset, s7plcMbbiDirect);

/* mbboDirect *******************************************************/

STATIC long s7plcInitRecordMbboDirect(mbboDirectRecord *);
STATIC long s7plcWriteMbboDirect(mbboDirectRecord *);

struct devsup s7plcMbboDirect =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordMbboDirect,
    s7plcGetOutIntInfo,
    s7plcWriteMbboDirect
};

epicsExportAddress(dset, s7plcMbboDirect);

/* longin ***********************************************************/

STATIC long s7plcInitRecordLongin(longinRecord *);
STATIC long s7plcReadLongin(longinRecord *);

struct devsup s7plcLongin =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordLongin,
    s7plcGetInIntInfo,
    s7plcReadLongin
};

epicsExportAddress(dset, s7plcLongin);

/* longout **********************************************************/

STATIC long s7plcInitRecordLongout(longoutRecord *);
STATIC long s7plcWriteLongout(longoutRecord *);

struct devsup s7plcLongout =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordLongout,
    s7plcGetOutIntInfo,
    s7plcWriteLongout
};

epicsExportAddress(dset, s7plcLongout);

/* ai ***************************************************************/

STATIC long s7plcInitRecordAi(aiRecord *);
STATIC long s7plcReadAi(aiRecord *);
STATIC long s7plcSpecialLinconvAi(aiRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} s7plcAi =
{
    6,
    NULL,
    NULL,
    s7plcInitRecordAi,
    s7plcGetInIntInfo,
    s7plcReadAi,
    s7plcSpecialLinconvAi
};

epicsExportAddress(dset, s7plcAi);

/* ao ***************************************************************/

STATIC long s7plcInitRecordAo(aoRecord *);
STATIC long s7plcWriteAo(aoRecord *);
STATIC long s7plcSpecialLinconvAo(aoRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} s7plcAo =
{
    6,
    NULL,
    NULL,
    s7plcInitRecordAo,
    s7plcGetOutIntInfo,
    s7plcWriteAo,
    s7plcSpecialLinconvAo
};

epicsExportAddress(dset, s7plcAo);

/* stringin *********************************************************/

STATIC long s7plcInitRecordStringin(stringinRecord *);
STATIC long s7plcReadStringin(stringinRecord *);

struct devsup s7plcStringin =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStringin,
    s7plcGetInIntInfo,
    s7plcReadStringin
};

epicsExportAddress(dset, s7plcStringin);

/* stringout ********************************************************/

STATIC long s7plcInitRecordStringout(stringoutRecord *);
STATIC long s7plcWriteStringout(stringoutRecord *);

struct devsup s7plcStringout =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordStringout,
    s7plcGetOutIntInfo,
    s7plcWriteStringout
};

epicsExportAddress(dset, s7plcStringout);

/* waveform *********************************************************/

STATIC long s7plcInitRecordWaveform(waveformRecord *);
STATIC long s7plcReadWaveform(waveformRecord *);

struct devsup s7plcWaveform =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordWaveform,
    s7plcGetInIntInfo,
    s7plcReadWaveform
};

epicsExportAddress(dset, s7plcWaveform);

/* calcout **********************************************************/
#if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)

STATIC long s7plcInitRecordCalcout(calcoutRecord *);
STATIC long s7plcWriteCalcout(calcoutRecord *);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} s7plcCalcout = {
    6,
    NULL,
    NULL,
    s7plcInitRecordCalcout,
    s7plcGetOutIntInfo,
    s7plcWriteCalcout,
    NULL
};

epicsExportAddress(dset, s7plcCalcout);
#endif

/*********  Report routine ********************************************/

STATIC long s7plcReport()
{
   printf("devS7mem version: %s\n", cvsid_devS7plc);
   return 0;
}

/*********  Support for "I/O Intr" for input records ******************/

STATIC long s7plcGetInIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    S7memPrivate_t* p = record->dpvt;
    if (p == NULL)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcGetInIntInfo: uninitialized record");
        return -1;
    }
    *ppvt = s7plcGetInScanPvt(p->station);
    return 0;
}

/*********  Support for "I/O Intr" for output records ****************/

STATIC long s7plcGetOutIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    S7memPrivate_t* p = record->dpvt;
    if (p == NULL)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcGetInIntInfo: uninitialized record");
        return -1;
    }
    *ppvt = s7plcGetOutScanPvt(p->station);
    return 0;
}

/***********************************************************************
 *   Routine to parse IO arguments
 *   IO address line format:
 *
 *    <devName>/<a>[+<o>] [T=<datatype>] [B=<bitnumber>] [L=<hwLow|strLen>] [H=<hwHigh>]
 *
 *   where: <devName>   - symbolic device name
 *          <a+o>       - address (byte number) within memory block
 *          <params>    - parameters to be passed to a particular
 *                        devSup parsering routine
 *          <datatype>  - INT8, INT16, INT32,
 *                        UINT16 (or UNSIGN16), UINT32 (or UNSIGN32),
 *                        REAL32 (or FLOAT), REAL64 (or DOUBLE),
 *                        STRING,TIME
 *          <bitnumber> - least significant bit is 0
 *          <hwLow>     - raw value that mapps to EGUL
 *          <hwHigh>    - raw value that mapps to EGUF
 **********************************************************************/

STATIC int s7plcIoParse(char* recordName, char *par, S7memPrivate_t *priv)
{
    char devName[255];
    char *p = par, separator;
    int nchar, i;
    int status = 0;

    struct {char* name; int dlen; epicsType type;} datatypes [] =
    {
        { "INT8",     1, epicsInt8T    },
        
        { "UINT8",    1, epicsUInt8T   },
        { "UNSIGN8",  1, epicsUInt8T   },
        { "BYTE",     1, epicsUInt8T   },
        { "CHAR",     1, epicsUInt8T   },
        
        { "INT16",    2, epicsInt16T   },
        { "SHORT",    2, epicsInt16T   },
        
        { "UINT16",   2, epicsUInt16T  },
        { "UNSIGN16", 2, epicsUInt16T  },
        { "WORD",     2, epicsUInt16T  },
        
        { "INT32",    4, epicsInt32T   },
        { "LONG",     4, epicsInt32T   },
        
        { "UINT32",   4, epicsUInt32T  },
        { "UNSIGN32", 4, epicsUInt32T  },
        { "DWORD",    4, epicsUInt32T  },

        { "REAL32",   4, epicsFloat32T },
        { "FLOAT32",  4, epicsFloat32T },
        { "FLOAT",    4, epicsFloat32T },

        { "REAL64",   8, epicsFloat64T },
        { "FLOAT64",  8, epicsFloat64T },
        { "DOUBLE",   8, epicsFloat64T },

        { "TIME",     1, S7MEM_TIME    },
        { "BCD",      1, S7MEM_TIME    }
    };

    /* Get rid of leading whitespace and non-alphanumeric chars */
    while (!isalnum((unsigned char)*p))
        if (*p++ == '\0') return S_drv_badParam;

    /* Get device name */
    nchar = strcspn(p, "/");
    strncpy(devName, p, nchar);
    devName[nchar] = '\0';
    p += nchar;
    separator = *p++;
    s7plcDebugLog(1, "s7plcIoParse %s: station=%s\n", recordName, devName);

    priv->station = s7plcOpen(devName);
    if (!priv->station)
    {
        errlogSevPrintf(errlogFatal, "s7plcIoParse %s: device not found\n",
            recordName);
        return S_drv_noDevice;
    }

    /* Check station offset */
    if (separator == '/')
    {
        priv->offs = strtol(p, &p, 0);
        separator = *p++;
        /* Handle any number of optional +o additions to the offs */
        while (separator == '+')
        {
            priv->offs += strtol(p, &p, 0);
            separator = *p++;
        }
    }
    else
    {
        priv->offs = 0;
    }

    s7plcDebugLog(1,
        "s7plcIoParse %s: offs=%d\n", recordName, priv->offs);

    /* set default values for parameters */
    if (!priv->dtype && !priv->dlen)
    {
        priv->dtype = epicsInt16T;
        priv->dlen = 2;
    }
    priv->bit = 0;
    priv->hwLow = 0;
    priv->hwHigh = 0;
    
    /* allow whitespaces before parameter for device support */
    while ((separator == '\t') || (separator == ' '))
        separator = *p++;

    /* handle parameter for device support if present */
    nchar = 0;
    if (separator != '\'') p--; /* quote is optional*/
    
    /* parse parameters */
    while (p && *p)
    {
        switch (*p)
        {
            case ' ':
            case '\t':
                p++;
                break;
            case 'T': /* T=<datatype> */
                p+=2; 
                
                if (strncmp(p,"STRING",6) == 0)
                {
                    priv->dtype = epicsStringT;
                    p += 6;
                }
                else
                {
                    static int maxtype =
                        sizeof(datatypes)/sizeof(*datatypes);
                    for (i = 0; i < maxtype; i++)
                    {
                        nchar = strlen(datatypes[i].name);
                        if (strncmp(p, datatypes[i].name, nchar) == 0)
                        {
                            priv->dtype = datatypes[i].type;
                            priv->dlen = datatypes[i].dlen;
                            p += nchar;
                            break;
                        }
                    }
                    if (i == maxtype)
                    {
                        errlogSevPrintf(errlogFatal,
                            "s7plcIoParse %s: invalid datatype %s\n",
                            recordName, p);
                        return S_drv_badParam;
                    }
                }
                break;
            case 'B': /* B=<bitnumber> */
                p += 2;
                priv->bit = strtol(p,&p,0);
                break;
            case 'L': /* L=<low raw value> (converts to EGUL)*/
                p += 2;
                priv->hwLow = strtol(p,&p,0);
                break;
            case 'H': /* L=<high raw value> (converts to EGUF)*/
                p += 2;
                priv->hwHigh = strtol(p,&p,0);
                break;
            case '\'':
                if (separator == '\'')
                {
                    p = 0;
                    break;
                }
            default:
                errlogSevPrintf(errlogFatal,
                    "s7plcIoParse %s: unknown parameter '%c'\n",
                    recordName, *p);
                return S_drv_badParam;
        }
    }
    
    /* for T=STRING L=... means length, not low */
    if (priv->dtype == epicsStringT && priv->hwLow)
    {
        priv->dlen = priv->hwLow;
        priv->hwLow = 0;
    }
    
    /* check if bit number is in range */
    if (priv->bit && priv->bit >= priv->dlen*8)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcIoParse %s: invalid bit number %d (>%d)\n",
            recordName, priv->bit, priv->dlen*8-1);
        return S_drv_badParam;
    }
    
    /* get default values for L and H if user did'n define them */
    switch (priv->dtype)
    {
        case epicsUInt8T:
            if (priv->hwHigh > 0xFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x00;
            if (!priv->hwHigh) priv->hwHigh = 0xFF;
            break;
        case epicsUInt16T:
            if (priv->hwHigh > 0xFFFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x0000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFF;
            break;
        case epicsUInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x00000000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFFFFFF;
            break;
        case epicsInt8T:
            if (priv->hwHigh > 0x7F) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFFFF81;
            if (!priv->hwHigh) priv->hwHigh = 0x0000007F;
            break;
        case epicsInt16T:
            if (priv->hwHigh > 0x7FFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFF8001;
            if (!priv->hwHigh) priv->hwHigh = 0x00007FFF;
            break;
        case epicsInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x80000001;
            if (!priv->hwHigh) priv->hwHigh = 0x7FFFFFFF;
            break;
        default:
            if (priv->hwHigh || priv->hwLow) {
                errlogSevPrintf(errlogMinor,
                    "s7plcIoParse %s: L or H makes"
                    " no sense with this data type\n",
                    recordName);
            } 
            break;   
    }
    s7plcDebugLog(1, "s7plcIoParse %s: dlen=%d\n",recordName, priv->dlen);
    s7plcDebugLog(1, "s7plcIoParse %s: B=%d\n",   recordName, priv->bit);
    s7plcDebugLog(1, "s7plcIoParse %s: L=%#x\n",  recordName, priv->hwLow);
    s7plcDebugLog(1, "s7plcIoParse %s: H=%#x\n",  recordName, priv->hwHigh);

    if (status)
    {
        errlogSevPrintf(errlogMinor,
            "s7plcIoParse %s: L or H out of range for this data type\n",
            recordName);
        return status;
    }
    
    return 0;
}

/* bi for status bit ************************************************/

STATIC long s7plcInitRecordStat(biRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStat: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordStat");
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStat: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    record->dpvt = priv;
    return 0;
}


STATIC long s7plcReadStat(biRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    /* psudo-read (0 bytes) just to get the connection status */
    status = s7plcReadArray(priv->station, 0, 0, 0, NULL);
    if (status == S_drv_noConn)
    {
        record->rval = 0;
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        record->rval = 0;
        return status;
    }
    record->rval = 1;
    return 0;
}

/* bi ***************************************************************/

STATIC long s7plcInitRecordBi(biRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordBi");
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordBi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcReadBi(biRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &rval8);
            s7plcDebugLog(3, "bi %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &rval16);
            s7plcDebugLog(3, "bi %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &rval32);
            s7plcDebugLog(3, "bi %s: read 32bit %04x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* bo ***************************************************************/

STATIC long s7plcInitRecordBo(boRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordBo");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordBo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordBo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long s7plcWriteBo(boRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "bo %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "bo %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "bo %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbi *************************************************************/

STATIC long s7plcInitRecordMbbi(mbbiRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordMbbi");
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    if (record->shft > 0) record->mask <<= record->shft;
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcReadMbbi(mbbiRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &rval8);
            s7plcDebugLog(3, "mbbi %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &rval16);
            s7plcDebugLog(3, "mbbi %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &rval32);
            s7plcDebugLog(3, "mbbi %s: read 32bit %04x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbo *************************************************************/

STATIC long s7plcInitRecordMbbo(mbboRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordMbbo");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long s7plcWriteMbbo(mbboRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "mbbo %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbbiDirect *******************************************************/

STATIC long s7plcInitRecordMbbiDirect(mbbiDirectRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbiDirect: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordMbbiDirect");
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbbiDirect: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbbiDirect %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcReadMbbiDirect(mbbiDirectRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &rval8);
            s7plcDebugLog(3, "mbbiDirect %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &rval16);
            s7plcDebugLog(3, "mbbiDirect %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &rval32);
            s7plcDebugLog(3, "mbbiDirect %s: read 32bit %08x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* mbboDirect *******************************************************/

STATIC long s7plcInitRecordMbboDirect(mbboDirectRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbboDirect: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordMbboDirect");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordMbboDirect: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (record->shft > 0) record->mask <<= record->shft;
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordMbboDirect %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long s7plcWriteMbboDirect(mbboDirectRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = s7plcWriteMasked(priv->station, priv->offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = s7plcWriteMasked(priv->station, priv->offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            s7plcDebugLog(2, "mbboDirect %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = s7plcWriteMasked(priv->station, priv->offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n", record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* longin ***********************************************************/

STATIC long s7plcInitRecordLongin(longinRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordLongin");
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordLongin %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcReadLongin(longinRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    signed char sval8;
    epicsUInt8 uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &sval8);
            s7plcDebugLog(3, "longin %s: read 8bit %02x\n",
                record->name, sval8);
            record->val = sval8;
            break;
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &uval8);
            s7plcDebugLog(3, "longin %s: read 8bit %02x\n",
                record->name, uval8);
            record->val = uval8;
            break;
        case epicsInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &sval16);
            s7plcDebugLog(3, "longin %s: read 16bit %04x\n",
                record->name, sval16);
            record->val = sval16;
            break;
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &uval16);
            s7plcDebugLog(3, "longin %s: read 16bit %04x\n",
                record->name, uval16);
            record->val = uval16;
            break;
        case epicsInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &sval32);
            s7plcDebugLog(3, "longin %s: read 32bit %04x\n",
                record->name, sval32);
            record->val = sval32;
            break;
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &uval32);
            s7plcDebugLog(3, "longin %s: read 32bit %04x\n",
                record->name, uval32);
            record->val = uval32;
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* longout **********************************************************/

STATIC long s7plcInitRecordLongout(longoutRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongout: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordLongout");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongout: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordLongout %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcWriteLongout(longoutRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->val;
            s7plcDebugLog(2, "longout %s: write 8bit %02x\n",
                record->name, rval8);
            status = s7plcWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->val;
            s7plcDebugLog(2, "longout %s: write 16bit %04x\n",
                record->name, rval16);
            status = s7plcWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->val;
            s7plcDebugLog(2, "longout %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* ai ***************************************************************/

STATIC long s7plcInitRecordAi(aiRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAi: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1, sizeof(S7memPrivate_t),
        "s7plcInitRecordAi");
    status = s7plcIoParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordAi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    s7plcSpecialLinconvAi(record, TRUE);
    return 0;
}

STATIC long s7plcReadAi(aiRecord *record)
{
    int status, floatval = FALSE;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    signed char sval8;
    unsigned char uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;
    union {epicsFloat32 f; epicsUInt32 i; } val32;
    __extension__ union {epicsFloat64 f; epicsUInt64 i; } val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &sval8);
            s7plcDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, sval8);
            record->rval = sval8;
            break;
        case epicsUInt8T:
            status = s7plcRead(priv->station, priv->offs,
                1, &uval8);
            s7plcDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, uval8);
            record->rval = uval8;
            break;
        case epicsInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &sval16);
            s7plcDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, sval16);
            record->rval = sval16;
            break;
        case epicsUInt16T:
            status = s7plcRead(priv->station, priv->offs,
                2, &uval16);
            s7plcDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, uval16);
            record->rval = uval16;
            break;
        case epicsInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &sval32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, sval32);
            record->rval = sval32;
            break;
        case epicsUInt32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &uval32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, uval32);
            record->rval = uval32;
            break;
        case epicsFloat32T:
            status = s7plcRead(priv->station, priv->offs,
                4, &val32);
            s7plcDebugLog(3, "ai %s: read 32bit %04x = %g\n",
                record->name, val32.i, val32.f);
            val64.f = val32.f;
            floatval = TRUE;
            break;
        case epicsFloat64T:
            status = s7plcRead(priv->station, priv->offs,
                8, &val64);
            __extension__ s7plcDebugLog(3, "ai %s: read 64bit " CONV64 " = %g\n",
                record->name, val64.i, val64.f);
            floatval = TRUE;
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        return status;
    }
    if (floatval)
    {
        /* emulate scaling */
        if (record->aslo != 0.0) val64.f *= record->aslo;
        val64.f += record->aoff;
        if (record->udf)
            record->val = val64.f;
        else
            /* emulate smoothing */
            record->val = record->val * record->smoo +
                val64.f * (1.0 - record->smoo);
        record->udf = isnan(record->val);
        return 2;
    }
    return 0;
}

STATIC long s7plcSpecialLinconvAi(aiRecord *record, int after)
{
    epicsUInt32 hwSpan;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff =
            (priv->hwHigh*record->egul - priv->hwLow*record->eguf)
            / hwSpan;
    }
    return 0;
}

/* ao ***************************************************************/

STATIC long s7plcInitRecordAo(aoRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAo: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordAo");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordAo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordAo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    s7plcSpecialLinconvAo(record, TRUE);
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long s7plcWriteAo(aoRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;
    union {epicsFloat32 f; epicsUInt32 i; } val32;
    __extension__ union {epicsFloat64 f; epicsUInt64 i; } val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    rval32 = record->rval;
    switch (priv->dtype)
    {
        case epicsInt8T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            s7plcDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            status = s7plcWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case epicsUInt8T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            s7plcDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            status = s7plcWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case epicsInt16T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            s7plcDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case epicsUInt16T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            s7plcDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case epicsInt32T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            s7plcDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        case epicsUInt32T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            s7plcDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        case epicsFloat32T:
            /* emulate scaling */
            val32.f = record->oval - record->aoff;
            if (record->aslo != 0) val32.f /= record->aslo;
            s7plcDebugLog(2, "ao %s: write 32bit %08x = %g\n",
                record->name, val32.i, val32.f);
            status = s7plcWrite(priv->station, priv->offs,
                4, &val32);
            break;
        case epicsFloat64T:
            /* emulate scaling */
            val64.f = record->oval - record->aoff;
            if (record->aslo != 0) val64.f /= record->aslo;
            __extension__ s7plcDebugLog(2, "ao %s: write 64bit " CONV64 " = %g\n",
                record->name, val64.i, val64.f);
            status = s7plcWrite(priv->station, priv->offs,
                8, &val64);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

STATIC long s7plcSpecialLinconvAo(aoRecord *record, int after)
{
    epicsUInt32 hwSpan;
    S7memPrivate_t *priv = (S7memPrivate_t *) record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff = 
            (priv->hwHigh*record->egul -priv->hwLow*record->eguf)
            / hwSpan;
    }
    return 0;
}

/* stringin *********************************************************/

STATIC long s7plcInitRecordStringin(stringinRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringin: illegal INP field type");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordStringin");
    priv->dtype = epicsStringT;
    priv->dlen = sizeof(record->val);
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordLongin: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    if (priv->dtype != epicsStringT)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordStringin %s: illegal data type\n",
            record->name);
        return S_db_badField;
    }
    if (priv->dlen > sizeof(record->val))
    {
        errlogSevPrintf(errlogMinor,
            "%s: string size reduced from %d to %d\n",
            record->name, priv->dlen, (int)sizeof(record->val));
        priv->dlen = sizeof(record->val);
    }
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcReadStringin(stringinRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    memset(record->val, 0, priv->dlen);
    status = s7plcReadArray(priv->station, priv->offs,
                1, priv->dlen, record->val);
    s7plcDebugLog(3, "stringin %s: read array of %d 8bit values\n",
        record->name, priv->dlen);
    if (record->val[priv->dlen] && !memchr(record->val, 0, priv->dlen))
    {
        /* truncate oversize string */
        record->val[priv->dlen] = 0;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* stringout ********************************************************/

STATIC long s7plcInitRecordStringout(stringoutRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringout: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordStringout");
    priv->dtype = epicsStringT;
    priv->dlen = sizeof(record->val);
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordStringout: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    if (priv->dtype != epicsStringT)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordStringout %s: illegal data type\n",
            record->name);
        return S_db_badField;
    }
    if (priv->dlen > sizeof(record->val))
    {
        errlogSevPrintf(errlogMinor,
            "%s: string size reduced from %d to %d\n",
            record->name, priv->dlen, (int)sizeof(record->val));
        priv->dlen = sizeof(record->val);
    }
    record->dpvt = priv;
    return 0;
}

STATIC long s7plcWriteStringout(stringoutRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    s7plcDebugLog(2, "stringout %s: write %d 8bit values: \"%.*s\"\n",
        record->name, priv->dlen, priv->dlen, record->val);
    status = s7plcWriteArray(priv->station, priv->offs,
        1, priv->dlen, record->val);
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

/* waveform *********************************************************/

STATIC long s7plcInitRecordWaveform(waveformRecord *record)
{
    S7memPrivate_t *priv;
    int status;
    
    if (record->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordWaveform: illegal INP field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordWaveform");
    switch (record->ftvl)
    {
        case DBF_CHAR:
            priv->dtype = epicsInt8T;
            priv->dlen = 1;
            break;
        case DBF_UCHAR:
            priv->dtype = epicsUInt8T;
            priv->dlen = 1;
            break;
        case DBF_SHORT:
            priv->dtype = epicsInt16T;
            priv->dlen = 2;
            break;
        case DBF_USHORT:
            priv->dtype = epicsUInt16T;
            priv->dlen = 2;
            break;
        case DBF_LONG:
            priv->dtype = epicsInt32T;
            priv->dlen = 4;
            break;
        case DBF_ULONG:
            priv->dtype = epicsUInt32T;
            priv->dlen = 4;
            break;
        case DBF_FLOAT:
            priv->dtype = epicsFloat32T;
            priv->dlen = 4;
            break;
        case DBF_DOUBLE:
            priv->dtype = epicsFloat64T;
            priv->dlen = 8;
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordWaveform %s: illegal FTVL value\n",
                record->name);
            return S_db_badField;
    }
    status = s7plcIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordWaveform: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    record->nord = record->nelm;
    switch (priv->dtype)
    {
        case S7MEM_TIME:
            if ((record->ftvl != DBF_CHAR) && (record->ftvl != DBF_UCHAR))
            {
                status = S_db_badField;
            }
            break;
        case epicsFloat64T:
            if (record->ftvl != DBF_DOUBLE)
            {
                status = S_db_badField;
            }
            break;
        case epicsFloat32T:
            if (record->ftvl != DBF_FLOAT)
            {
                status = S_db_badField;
            }
            break;
        case epicsStringT:    
            if ((record->ftvl == DBF_CHAR) || (record->ftvl == DBF_UCHAR))
            {
                if (!priv->dlen) priv->dlen = record->nelm;
                if (priv->dlen > record->nelm) priv->dlen = record->nelm;
                break;
            }
            break;
        case epicsInt8T:
        case epicsUInt8T:
            if ((record->ftvl != DBF_CHAR) && (record->ftvl == DBF_UCHAR))
            {
                status = S_db_badField;
            }
            break;
        case epicsInt16T:
        case epicsUInt16T:
            if ((record->ftvl != DBF_SHORT) && (record->ftvl == DBF_USHORT))
            {
                status = S_db_badField;
            }
            break;
        case epicsInt32T:
        case epicsUInt32T:
            if ((record->ftvl != DBF_LONG) && (record->ftvl == DBF_ULONG))
            {
                status = S_db_badField;
            }
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordWaveform %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcInitRecordWaveform %s: "
            "wrong FTVL field for this data type",
            record->name);
        return status;
    }
    record->dpvt = priv;
    return 0;
}

/*
 * bcd2d routine to convert byte from BCD to decimal format.
 */
static unsigned char bcd2d(unsigned char bcd)
{
    unsigned char tmp;

    tmp = bcd & 0xF;
    tmp += ((bcd >> 4) & 0xF)*10;

    return tmp;
}

STATIC long s7plcReadWaveform(waveformRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    char Time[8];
    int i;
    char *p;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsStringT:
            status = s7plcReadArray(priv->station, priv->offs,
                1, record->nelm, record->bptr);
            s7plcDebugLog(3,
                "waveform %s: read %ld values of 8bit to %p\n",
                record->name, (long) record->nelm, record->bptr);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = s7plcReadArray(priv->station, priv->offs,
                2, record->nelm, record->bptr);
            s7plcDebugLog(3,
                "waveform %s: read %ld values of 16bit to %p\n",
                record->name, (long) record->nelm, record->bptr);
            break;
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
            status = s7plcReadArray(priv->station, priv->offs,
                4, record->nelm, record->bptr);
            s7plcDebugLog(3,
                "waveform %s: read %ld values of 32bit to %p\n",
                record->name, (long) record->nelm, record->bptr);
            break;
        case epicsFloat64T:
            status = s7plcReadArray(priv->station, priv->offs,
                8, record->nelm, record->bptr);
            s7plcDebugLog(3,
                "waveform %s: read %ld values of 64bit to %p\n",
                record->name, (long) record->nelm, record->bptr);
            break;
        case S7MEM_TIME:
            status = s7plcReadArray(priv->station, priv->offs,
                1, 8, Time);
            s7plcDebugLog(3,
                "waveform %s: read 8 values of 8bit to %p\n",
                record->name, record->bptr);
            if (status) break;
            for (i = 0, p = record->bptr; i < record->nelm; i++)
                *p++ = (i == 7)? Time[i] : bcd2d(Time[i]);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->nord = record->nelm;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

#if (EPICS_REVISION>=14)
/* calcout **********************************************************/

STATIC long s7plcInitRecordCalcout(calcoutRecord *record)
{
    S7memPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordCalcout: illegal OUT field");
        return S_db_badField;
    }
    priv = (S7memPrivate_t *)callocMustSucceed(1,
        sizeof(S7memPrivate_t), "s7plcInitRecordCalcout");
    status = s7plcIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "s7plcInitRecordCalcout: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "s7plcInitRecordCalcout %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long s7plcWriteCalcout(calcoutRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 uval8;
    epicsUInt16 uval16;
    epicsUInt32 uval32;
    epicsInt8 sval8;
    epicsInt16 sval16;
    epicsInt32 sval32;
    union {epicsFloat32 f; epicsUInt32 i; } val32;
    __extension__ union {epicsFloat64 f; epicsUInt64 i; } val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    val64.f = record->oval;
    switch (priv->dtype)
    {
        case epicsInt8T:
            sval8 = val64.f;
            if (val64.f > priv->hwHigh) sval8 = priv->hwHigh;
            if (val64.f < priv->hwLow) sval8 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 8bit %02x\n",
                record->name, sval8 & 0xff);
            status = s7plcWrite(priv->station, priv->offs,
                1, &sval8);
            break;
        case epicsUInt8T:
            uval8 = val64.f;
            if (val64.f > priv->hwHigh) uval8 = priv->hwHigh;
            if (val64.f < priv->hwLow) uval8 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 8bit %02x\n",
                record->name, uval8 & 0xff);
            status = s7plcWrite(priv->station, priv->offs,
                1, &uval8);
            break;
        case epicsInt16T:
            sval16 = val64.f;
            if (val64.f > priv->hwHigh) sval16 = priv->hwHigh;
            if (val64.f < priv->hwLow) sval16 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 16bit %04x\n",
                record->name, sval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->offs,
                2, &sval16);
            break;
        case epicsUInt16T:
            uval16 = val64.f;
            if (val64.f > priv->hwHigh) uval16 = priv->hwHigh;
            if (val64.f < priv->hwLow) uval16 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 16bit %04x\n",
                record->name, uval16 & 0xffff);
            status = s7plcWrite(priv->station, priv->offs,
                2, &uval16);
            break;
        case epicsInt32T:
            sval32 = val64.f;
            if (val64.f > priv->hwHigh) sval32 = priv->hwHigh;
            if (val64.f < priv->hwLow) sval32 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 32bit %08x\n",
                record->name, sval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &sval32);
            break;
        case epicsUInt32T:
            uval32 = val64.f;
            if (val64.f > priv->hwHigh) uval32 = priv->hwHigh;
            if (val64.f < priv->hwLow) uval32 = priv->hwLow;
            s7plcDebugLog(2, "calcout %s: write 32bit %08x\n",
                record->name, uval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &uval32);
            break;
        case epicsFloat32T:
            val32.f = val64.f;
            s7plcDebugLog(2, "calcout %s: write 32bit %08x = %g\n",
                record->name, val32.i, val32.f);
            status = s7plcWrite(priv->station, priv->offs,
                4, &val32);
            break;
        case epicsFloat64T:
            __extension__ s7plcDebugLog(2, "calcout %s: write 64bit " CONV64 " = %g\n",
                record->name, val64.i, val64.f);
            status = s7plcWrite(priv->station, priv->offs,
                8, &val64);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

#endif

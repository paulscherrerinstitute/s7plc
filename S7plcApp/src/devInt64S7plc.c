#include <int64inRecord.h>
#include <int64outRecord.h>
#include <epicsExport.h>
#include <cantProceed.h>
#include "devS7plc.h"

STATIC long s7plcInitRecordInt64in(int64inRecord *record)
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
        case menuFtypeCHAR:
        case menuFtypeUCHAR:
        case menuFtypeSHORT:
        case menuFtypeUSHORT:
        case menuFtypeLONG:
        case menuFtypeULONG:
        case menuFtypeINT64:
        case menuFtypeUINT64:
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

STATIC long s7plcReadInt64in(int64inRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    signed char sval8;
    epicsUInt8 uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;
    epicsInt64 sval64;
    epicsUInt64 uval64;

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
        case menuFtypeCHAR:
            status = s7plcRead(priv->station, priv->offs,
                1, &sval8);
            s7plcDebugLog(3, "int64in %s: read 8bit %02x\n",
                record->name, sval8);
            record->val = sval8;
            break;
        case menuFtypeUCHAR:
            status = s7plcRead(priv->station, priv->offs,
                1, &uval8);
            s7plcDebugLog(3, "int64in %s: read 8bit %02x\n",
                record->name, uval8);
            record->val = uval8;
            break;
        case menuFtypeSHORT:
            status = s7plcRead(priv->station, priv->offs,
                2, &sval16);
            s7plcDebugLog(3, "int64in %s: read 16bit %04x\n",
                record->name, sval16);
            record->val = sval16;
            break;
        case menuFtypeUSHORT:
            status = s7plcRead(priv->station, priv->offs,
                2, &uval16);
            s7plcDebugLog(3, "int64in %s: read 16bit %04x\n",
                record->name, uval16);
            record->val = uval16;
            break;
        case menuFtypeLONG:
            status = s7plcRead(priv->station, priv->offs,
                4, &sval32);
            s7plcDebugLog(3, "int64in %s: read 32bit %08x\n",
                record->name, sval32);
            record->val = sval32;
            break;
        case menuFtypeULONG:
            status = s7plcRead(priv->station, priv->offs,
                4, &uval32);
            s7plcDebugLog(3, "int64in %s: read 32bit %08x\n",
                record->name, uval32);
            record->val = uval32;
            break;
        case menuFtypeINT64:
            status = s7plcRead(priv->station, priv->offs,
                8, &sval64);
            s7plcDebugLog(3, "int64in %s: read 64bit %016llx\n",
                record->name, sval64);
            record->val = sval64;
            break;
        case menuFtypeUINT64:
            status = s7plcRead(priv->station, priv->offs,
                8, &uval64);
            s7plcDebugLog(3, "int64in %s: read 64bit "CONV64"\n",
                record->name, uval64);
            record->val = uval64;
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

struct devsup s7plcInt64in =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordInt64in,
    s7plcGetInIntInfo,
    s7plcReadInt64in
};

epicsExportAddress(dset, s7plcInt64in);


STATIC long s7plcInitRecordInt64out(int64outRecord *record)
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
        case menuFtypeCHAR:
        case menuFtypeUCHAR:
        case menuFtypeSHORT:
        case menuFtypeUSHORT:
        case menuFtypeLONG:
        case menuFtypeULONG:
        case menuFtypeINT64:
        case menuFtypeUINT64:
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

STATIC long s7plcWriteInt64out(int64outRecord *record)
{
    int status;
    S7memPrivate_t *priv = (S7memPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;
    epicsUInt64 rval64;

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
        case menuFtypeCHAR:
        case menuFtypeUCHAR:
            rval8 = record->val;
            s7plcDebugLog(2, "int64out %s: write 8bit %02x\n",
                record->name, rval8);
            status = s7plcWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case menuFtypeSHORT:
        case menuFtypeUSHORT:
            rval16 = record->val;
            s7plcDebugLog(2, "int64out %s: write 16bit %04x\n",
                record->name, rval16);
            status = s7plcWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case menuFtypeLONG:
        case menuFtypeULONG:
            rval32 = record->val;
            s7plcDebugLog(2, "int64out %s: write 32bit %08x\n",
                record->name, rval32);
            status = s7plcWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        case menuFtypeINT64:
        case menuFtypeUINT64:
            rval64 = record->val;
            s7plcDebugLog(2, "int64out %s: write 64bit "CONV64"\n",
                record->name, rval64);
            status = s7plcWrite(priv->station, priv->offs,
                8, &rval64);
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

struct devsup s7plcInt64out =
{
    5,
    NULL,
    NULL,
    s7plcInitRecordInt64out,
    s7plcGetOutIntInfo,
    s7plcWriteInt64out
};

epicsExportAddress(dset, s7plcInt64out);
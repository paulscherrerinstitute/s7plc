/* $Id: drvS7plc.c,v 1.7 2005/03/01 16:43:18 zimoch Exp $ */  
 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <dbScan.h>
#include <drvSup.h>
#include <devLib.h>
#include <epicsVersion.h>
#include "ticp.h"
#include "drvS7plc.h"

#if (EPICS_REVISION<14)
/* R3.13 */
#include "compat3_13.h"
#else
/* R3.14 */
#include <iocsh.h>
#include <epicsString.h>
#include <cantProceed.h>
#include <epicsExport.h>
#endif

#ifdef __vxworks
#define __BYTE_ORDER _BYTE_ORDER
#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN _BIG_ENDIAN
#else
#include <endian.h>
#endif


static char cvsid[] __attribute__((unused)) =
"$Id: drvS7plc.c,v 1.7 2005/03/01 16:43:18 zimoch Exp $";

static long s7plcIoReport(int level); 
static long s7plcInit();

struct {
    long number;
    long (*report)();
    long (*init)();
} s7plc = {
    2,
    s7plcIoReport,
    s7plcInit
};

epicsExportAddress(drvet, s7plc);

struct s7plcStation {
    char *downBuffer;
    unsigned int downBufferSize;
    char *upBuffer;
    unsigned int upBufferSize;
    int swapBytes;
    epicsMutexId upMutex;
    epicsMutexId downMutex;
    epicsUInt16 *connStatus;
};

typedef struct s7plcDevice s7plcDevice;

struct s7plcDevice {
    s7plcDevice *next;
    char *name;
    unsigned int numStations;
    s7plcStation *station;
    sm_layout *mem;
};

static s7plcDevice *devices = NULL;

int s7plcDebug = 0;

epicsExportAddress(int, s7plcDebug);

void s7plcDebugLog(int level, const char *fmt, ...)
{
    va_list args;
    
    if (level > s7plcDebug) return;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static long s7plcIoReport(int level)
{
    s7plcDevice *device;
    unsigned int i;

    if (level == 2) return ticpReport();
    printf("%s", cvsid);
    printf("S7mem devices:\n");
    for (device = devices; device; device = device->next)
    {
        printf("  Device name: \"%s\"\n", device->name);
        if (level == 1) for (i = 0; i < device->numStations; i++)
        {
            printf("  Station %2d: %sconnected (%x)\n",
                i, *device->station[i].connStatus ? "" : "dis", *device->station[i].connStatus);
            printf("              downBuffer @ %p (%d bytes)\n",
                device->station[i].downBuffer,  device->station[i].downBufferSize);
            printf("              upBuffer   @ %p (%d bytes)\n",
                device->station[i].upBuffer,  device->station[i].upBufferSize);
            printf("              connStatus @ %p\n",
                device->station[i].connStatus);
            printf("              swapBytes  %s\n",
                device->station[i].swapBytes ? 
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
                    "ioc:intel <-> s7:motorola" : "no, both intel"
#else
                    "ioc:motorola <-> s7:intel" : "no, both motorola"
#endif
                    );
        }
    }
    return 0;
}

static long s7plcInit()
{
    s7plcDevice *device;
    
    for (device = devices; device; device = device->next)
    {
        s7plcDebugLog(1, "starting ICP \"%s\"\n", device->name);
        ICP_start(device->mem);
    }
    return 0;
}

int s7plcConfigure(char *devName, int bigEndian)
{
    s7plcDevice **pdevice;
    unsigned int i;
    unsigned int numStations = MAX_COC;
    s7plcStation *station;
    sm_layout* mem;
    
    if (!devName)
    {
        errlogSevPrintf(errlogFatal, "devName missing\n");
        return -1;
    }
    s7plcDebugLog(1, "s7plcConfigure(devName=%s, bigEndian=%i)\n",
        devName, bigEndian);
    for (pdevice = &devices; *pdevice; pdevice = &(*pdevice)->next)
    {
        if (strcmp(devName,(*pdevice)->name) == 0)
        {
            errlogSevPrintf(errlogMinor, "%s device already in use\n", devName);
            return S_drv_badParam;
        }
    }
    *pdevice = callocMustSucceed(1, sizeof(struct s7plcDevice), "s7plcConfigure");
    station = callocMustSucceed(numStations, sizeof(struct s7plcStation), "s7plcConfigure");
    mem = callocMustSucceed(1, sizeof(sm_layout), "s7plcConfigure");
    s7plcDebugLog(1, "device %s at %p, size = %d\n", devName, mem, sizeof(*mem));
    for (i = 0; i < numStations; i++)
    {
        station[i].downBuffer = (char*)&mem->buf_dwn[i];
        station[i].downBufferSize = sizeof(mem->buf_dwn[i]);
        station[i].upBuffer = (char*)&mem->buf_up[i];
        station[i].upBufferSize = sizeof(mem->buf_up[i]);
        station[i].upMutex = mem->semID_up[i] = epicsMutexMustCreate();
        station[i].downMutex = mem->semID_dwn[i] = epicsMutexMustCreate();
        station[i].connStatus = &mem->com_sta.status[H_ADR_COC_STATUS + i];
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
        station[i].swapBytes = bigEndian;
#elif (__BYTE_ORDER == __BIG_ENDIAN)
        station[i].swapBytes = !bigEndian;
#else
#error Strange byte order on this machine
#endif        
        s7plcDebugLog(1, "station %d at %p\n", i, &station[i]);
        s7plcDebugLog(1, "    downBuffer offset = %d, size = %d words\n",
            station[i].downBuffer - (char*)mem, station[i].downBufferSize/2);
        s7plcDebugLog(1, "    upBuffer   offset = %d, size = %d words\n",
            station[i].upBuffer - (char*)mem, station[i].upBufferSize/2);
        s7plcDebugLog(1, "    connStatus offset = %d\n",
            (char*)station[i].connStatus - (char*)mem);
        s7plcDebugLog(1, "    swapBytes = %d\n",
            station[i].swapBytes);
    }
    (*pdevice)->numStations = numStations;
    (*pdevice)->name = epicsStrDup(devName);
    (*pdevice)->station = station;
    (*pdevice)->mem = mem;

    return S_drv_OK;
}

#if (EPICS_REVISION>=14)
static const iocshArg s7plcConfigureArg0 = { "devName", iocshArgString };
static const iocshArg s7plcConfigureArg1 = { "bigEndian", iocshArgInt };
static const iocshArg * const s7plcConfigureArgs[] = { &s7plcConfigureArg0, &s7plcConfigureArg1 };
static const iocshFuncDef s7plcConfigureDef = { "s7plcConfigure", 2, s7plcConfigureArgs };
static void s7plcConfigureFunc (const iocshArgBuf *args)
{
    s7plcConfigure(args[0].sval, args[1].ival);
}

static void s7plcRegister ()
{
    iocshRegister(&s7plcConfigureDef, s7plcConfigureFunc);
}

epicsExportRegistrar(s7plcRegister);
#endif

s7plcStation *s7plcOpen(
    char *devName,
    unsigned int stationNumber
)
{
    s7plcDevice *device;

    for (device = devices; device; device = device->next)
    {
        if (strcmp(devName, device->name) == 0)
        {
            if (stationNumber >= device->numStations)
            {
                errlogSevPrintf(errlogMajor, "s7plcOpen: device %s has only %d stations\n",
                    devName, device->numStations);
                return NULL;
            }
            return &device->station[stationNumber];
        } 
    }
    errlogSevPrintf(errlogMajor, "s7plcOpen: device %s not found\n", devName);
    return NULL;
}

int s7plcReadArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* data
)
{
    unsigned int elem, i;
    unsigned char byte;
    epicsUInt16 connStatus;

    if (offset+dlen > station->upBufferSize)
    {
       errlogSevPrintf(errlogMajor, "s7plcRead: offset %u out of range\n", offset);
       return S_drv_badParam;
    }
    if (offset+nelem*dlen > station->upBufferSize)
    {
       errlogSevPrintf(errlogMajor, "s7plcRead: too many elements (%u)\n", nelem);
       return S_drv_badParam;
    }
    s7plcDebugLog(4, "s7plcReadArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->upMutex);
    connStatus = *station->connStatus;
    for (elem = 0; elem < nelem; elem++)
    {
        s7plcDebugLog(5, "data in:");
        for (i = 0; i < dlen; i++)
        {
            if (station->swapBytes)
                byte = station->upBuffer[offset + elem*dlen + dlen - 1 - i];
            else
                byte = station->upBuffer[offset + elem*dlen + i];
            ((char*)data)[elem*dlen+i] = byte;
            s7plcDebugLog(5, " %02x", byte);
        }
        s7plcDebugLog(5, "\n");
    }    
    epicsMutexUnlock(station->upMutex);
    if (!connStatus) return S_drv_noConn;
    return S_drv_OK;
}

int s7plcWriteMaskedArray(
    s7plcStation *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* data,
    void* mask
)
{
    unsigned int elem, i;
    unsigned char byte;
    epicsUInt16 connStatus;

    if (offset+dlen > station->downBufferSize)
    {
       errlogSevPrintf(errlogMajor, "s7plcWrite: offset %u out of range\n", offset);
       return ERR;
    }
    if (offset+nelem*dlen > station->downBufferSize)
    {
       errlogSevPrintf(errlogMajor, "s7plcWrite: too many elements (%u)\n", nelem);
       return ERR;
    }
    s7plcDebugLog(4, "s7plcWriteMaskedArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->downMutex);
    connStatus = *station->connStatus;
    for (elem = 0; elem < nelem; elem++)
    {
        s7plcDebugLog(5, "data out:");
        for (i = 0; i < dlen; i++)
        {
            byte = ((unsigned char*)data)[elem*dlen+i];
            if (mask)
            {
                s7plcDebugLog(5, "(%02x & %02x)", byte, ((unsigned char*)mask)[i]);
                byte &= ((unsigned char*)mask)[i];
            }
            if (station->swapBytes)
            {
                if (mask)
                {
                    s7plcDebugLog(5, " | (%02x & %02x) =>",
                        station->downBuffer[offset + elem*dlen + dlen - 1 - i],
                        ~((unsigned char*)mask)[i]);
                    byte |= station->downBuffer[offset + elem*dlen + dlen - 1 - i]
                        & ~((unsigned char*)mask)[i];
                }
                s7plcDebugLog(5, " %02x", byte);
                station->downBuffer[offset + elem*dlen + dlen - 1 - i] = byte;
            }
            else
            {
                if (mask)
                {
                    s7plcDebugLog(5, " | (%02x & %02x) =>",
                        station->downBuffer[offset + elem*dlen + i],
                        ~((unsigned char*)mask)[i]);
                    byte |= station->downBuffer[offset + elem*dlen + i]
                        & ~((unsigned char*)mask)[i];
                }
                s7plcDebugLog(5, " %02x", byte);
                station->downBuffer[offset + elem*dlen + i] = byte;
            }
        }
        s7plcDebugLog(5, "\n");
    }    
    epicsMutexUnlock(station->downMutex);
    if (!connStatus) return S_drv_noConn;
    return S_drv_OK;
}

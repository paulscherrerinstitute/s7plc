#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>

#if defined(vxWorks) || defined(__vxworks)
#include <inetLib.h>
#include <sockLib.h>
#define in_addr_t unsigned long
#define socklen_t int
#define strdup(s) ({ char* __r=(char*)malloc(strlen(s)+1); __r ? strcpy(__r, s) : NULL; })
#else
#include <fcntl.h>
#include <sys/select.h>
#endif

#include <drvSup.h>
#include <devLib.h>
#include <errlog.h>
#include <epicsVersion.h>

#ifdef BASE_VERSION
#define EPICS_3_13
#include "compat3_13.h"
#else
#include <osiSock.h>
#include <dbAccess.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTimer.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <epicsExport.h>
#endif

#include "drvS7plc.h"

#define CONNECT_TIMEOUT   5.0  /* connect timeout [s] */
#define RECONNECT_DELAY  30.0  /* delay before reconnect [s] */

STATIC long s7plcIoReport(int level);
STATIC long s7plcInit();
void s7plcMain();
STATIC void s7plcSendThread(s7plcStation* station);
STATIC void s7plcReceiveThread(s7plcStation* station);
STATIC int s7plcWaitForInput(s7plcStation* station, double timeout);
STATIC int s7plcConnect(s7plcStation* station);
STATIC void s7plcCloseConnection(s7plcStation* station);
STATIC void s7plcSignal(void* event);
s7plcStation* s7plcStationList = NULL;
static epicsTimerQueueId timerqueue = NULL;
static short bigEndianIoc;

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

int s7plcDebug = 0;
epicsExportAddress(int, s7plcDebug);

struct s7plcStation {
    struct s7plcStation* next;
    char* name;
    char* server;
    int serverPort;
    unsigned int inSize;
    unsigned int outSize;
    unsigned char* inBuffer;
    unsigned char* outBuffer;
    int swapBytes;
    volatile int sockFd;
    epicsMutexId mutex;
    epicsMutexId io;
    epicsTimerId timer;
    epicsEventId outTrigger;
    int outputChanged;
    IOSCANPVT inScanPvt;
    IOSCANPVT outScanPvt;
    epicsThreadId sendThread;
    epicsThreadId recvThread;
    float recvTimeout;
    float sendIntervall;
};

#define s7plcErrorLog(fmt, args...) errlogPrintf("%s " fmt, currentTime() , ##args)
#undef s7plcDebugLog
#define s7plcDebugLog(level, fmt, args...) if (level <= s7plcDebug) errlogPrintf("%s " fmt, currentTime() , ##args)

#ifdef BASE_VERSION
/* 3.13 */
STATIC char* currentTime()
{
    static char buffer [40];
    TS_STAMP stamp;
    tsLocalTime(&stamp);
    tsStampToText(&stamp, TS_TEXT_MMDDYY, buffer);
    buffer[21] = 0;
    return buffer;
}
#else
/* 3.14+ */
STATIC char* currentTime()
{
    static char buffer [40];
    epicsTimeStamp stamp;
    epicsTimeGetCurrent(&stamp);
    epicsTimeToStrftime(buffer, sizeof(buffer), "%m/%d/%y %H:%M:%S.%03f", &stamp);
    return buffer;
}
#endif

STATIC void hexdump(unsigned char* data, int size, int ascii)
{
    int offs, x;
    for (offs = 0; offs < size; offs += 16)
    {
        printf("%04x:", offs);
        for (x = 0; x < 16; x++)
            if (offs+x >= size) printf ("   ");
            else printf(" %02x", data[offs+x]);
        if (ascii)
        {
            printf(" | ");
            for (x = 0; x < 16; x++)
                if (offs+x >= size) printf(" ");
                else if (data[offs+x]>=32 && data[offs+x]<127) printf("%c", data[offs+x]);
                else printf (".");
        }
        printf("\n");
    }
}

STATIC long s7plcIoReport(int level)
{
    s7plcStation *station;

    if (!s7plcStationList)
    {
        printf("No PLCs configured\n");
        return 0;
    }
    for (station = s7plcStationList; station;
        station=station->next)
    {
        printf("  %s %s %s:%d\n",
            station->name,
            station->sockFd != -1 ? "connected to" : "disconnected from",
            station->server, station->serverPort);
        if (level < 1) continue;
        printf("    file descriptor %d\n", station->sockFd);
        printf("    swap bytes %s\n",
            station->swapBytes
                ? ( bigEndianIoc ? "ioc:motorola <-> plc:intel" : "ioc:intel <-> plc:motorola" )
                : ( bigEndianIoc ? "no, both motorola" : "no, both intel" ) );
        printf("    receive timeout %g sec\n",
            station->recvTimeout);
        printf("    send intervall  %g sec\n",
            station->sendIntervall);
        printf("    inBuffer  at address %p (%u bytes)\n",
            station->inBuffer,  station->inSize);
        if (level >= 2)
            hexdump(station->inBuffer,  station->inSize, level >= 3);
        printf("    outBuffer at address %p (%u bytes)\n",
            station->outBuffer,  station->outSize);
        if (level >= 2)
            hexdump(station->outBuffer,  station->outSize, level >= 3);
    }
    return 0;
}

STATIC long s7plcInit()
{
    s7plcStation* station;
    char threadname[20];

    if (!s7plcStationList) return 0;

    for (station = s7plcStationList; station; station=station->next)
    {
        sprintf (threadname, "%.15sR", station->name);
        s7plcDebugLog(1,
            "s7plcMain %s: starting recv thread %s\n",
            station->name, threadname);
        station->recvThread = epicsThreadCreate(
            threadname,
            epicsThreadPriorityHigh,
            epicsThreadGetStackSize(epicsThreadStackBig),
            (EPICSTHREADFUNC)s7plcReceiveThread,
            station);
        if (!station->recvThread)
        {
            s7plcErrorLog(
                "s7plcInit %s: FATAL ERROR! could not start recv thread %s\n",
                station->name, threadname);
            return -1;
        }

        if (station->outSize)
        {
            sprintf (threadname, "%.15sS", station->name);
            s7plcDebugLog(1,
                "s7plcMain %s: starting send thread %s\n",
                station->name, threadname);
            station->sendThread = epicsThreadCreate(
                threadname,
                epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackBig),
                (EPICSTHREADFUNC)s7plcSendThread,
                station);
            if (!station->sendThread)
            {
                s7plcErrorLog(
                    "s7plcInit %s: FATAL ERROR! could not start send thread %s\n",
                    station->name, threadname);
                return -1;
            }
        }
    }
    return 0;
}

STATIC void s7plcSignal(void* event)
{
    epicsEventSignal((epicsEventId)event);
}

int s7plcConfigure(char *name, char* IPaddr, unsigned int port, unsigned int inSize, unsigned int outSize, unsigned int bigEndian, unsigned int recvTimeout, unsigned int sendIntervall)
{
    s7plcStation* station;
    s7plcStation** pstation;

    union {short s; char c [sizeof(short)];} u;
    u.s=1;
    bigEndianIoc = !u.c[0];
    if (!name)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: missing name\n");
        return -1;
    }

    if (!IPaddr || !IPaddr[0])
    {
        IPaddr = NULL;
        errlogSevPrintf(errlogInfo,
            "s7plcConfigure: missing IP address or name. Waiting for address set by record.\n");
    }
    else
    if (!port)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: missing IP port\n");
        return -1;
    }

    /* find last station in list */
    for (pstation = &s7plcStationList; *pstation; pstation = &(*pstation)->next);

    station = callocMustSucceed(1,
        sizeof(s7plcStation) + inSize + outSize + strlen(name)+1, "s7plcConfigure");
    station->next = NULL;
    station->serverPort = port;
    station->inSize = inSize;
    station->outSize = outSize;
    station->inBuffer = (unsigned char*)(station+1);
    station->outBuffer = (unsigned char*)(station+1)+inSize;
    station->name = (char*)(station+1)+inSize+outSize;
    strcpy(station->name, name);
    station->server = IPaddr ? strdup(IPaddr) : NULL;
    station->swapBytes = bigEndian ^ bigEndianIoc;
    station->sockFd = -1;
    station->mutex = epicsMutexMustCreate();
    station->io = epicsMutexMustCreate();
    station->outputChanged = 0;
    if (station->outSize)
    {
        station->outTrigger = epicsEventMustCreate(epicsEventEmpty);
        if (!timerqueue)
        {
            timerqueue = epicsTimerQueueAllocate(1, epicsThreadPriorityHigh);
        }
        station->timer = epicsTimerQueueCreateTimer(timerqueue,
            s7plcSignal, station->outTrigger);
    }
    scanIoInit(&station->inScanPvt);
    scanIoInit(&station->outScanPvt);
    station->recvThread = NULL;
    station->sendThread = NULL;
    station->recvTimeout = recvTimeout > 0 ? recvTimeout/1000.0 : 2.0;
    station->sendIntervall = sendIntervall > 0 ? sendIntervall/1000.0 : 1.0;

    /* append station to list */
    *pstation = station;
    pstation = &station->next;

    return 0;
}

#ifndef EPICS_3_13
static const iocshArg s7plcConfigureArg0 = { "name", iocshArgString };
static const iocshArg s7plcConfigureArg1 = { "IPaddr", iocshArgString };
static const iocshArg s7plcConfigureArg2 = { "IPport", iocshArgInt };
static const iocshArg s7plcConfigureArg3 = { "inSize", iocshArgInt };
static const iocshArg s7plcConfigureArg4 = { "outSize", iocshArgInt };
static const iocshArg s7plcConfigureArg5 = { "bigEndian", iocshArgInt };
static const iocshArg s7plcConfigureArg6 = { "recvTimeout", iocshArgInt };
static const iocshArg s7plcConfigureArg7 = { "sendIntervall", iocshArgInt };
static const iocshArg * const s7plcConfigureArgs[] = {
    &s7plcConfigureArg0,
    &s7plcConfigureArg1,
    &s7plcConfigureArg2,
    &s7plcConfigureArg3,
    &s7plcConfigureArg4,
    &s7plcConfigureArg5,
    &s7plcConfigureArg6,
    &s7plcConfigureArg7
};
static const iocshFuncDef s7plcConfigureDef = { "s7plcConfigure", 8, s7plcConfigureArgs };
static void s7plcConfigureFunc (const iocshArgBuf *args)
{
    int status = s7plcConfigure(
        args[0].sval, args[1].sval, args[2].ival,
        args[3].ival, args[4].ival, args[5].ival,
        args[6].ival, args[7].ival);

    if (status) exit(1);
}

static void s7plcRegister()
{
    iocshRegister(&s7plcConfigureDef, s7plcConfigureFunc);
}

epicsExportRegistrar(s7plcRegister);
#endif

s7plcStation *s7plcOpen(char *name)
{
    s7plcStation *station;

    for (station = s7plcStationList; station; station = station->next)
    {
        if (strcmp(name, station->name) == 0)
        {
            return station;
        }
    }
    errlogSevPrintf(errlogFatal,
        "s7plcOpen: station %s not found\n", name);
    return NULL;
}

IOSCANPVT s7plcGetInScanPvt(s7plcStation *station)
{
    return station->inScanPvt;
}

IOSCANPVT s7plcGetOutScanPvt(s7plcStation *station)
{
    return station->outScanPvt;
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

    if (offset+dlen > station->inSize)
    {
       errlogSevPrintf(errlogMajor,
        "s7plcRead %s/%u: offset out of range\n",
        station->name, offset);
       return S_drv_badParam;
    }
    if (offset+nelem*dlen > station->inSize)
    {
       errlogSevPrintf(errlogMajor,
        "s7plcRead %s/%u: too many elements (%u)\n",
        station->name, offset, nelem);
       return S_drv_badParam;
    }
    s7plcDebugLog(4,
        "s7plcReadArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->mutex);
    for (elem = 0; elem < nelem; elem++)
    {
        s7plcDebugLog(5, "data in:");
        for (i = 0; i < dlen; i++)
        {
            if (station->swapBytes)
                byte = station->inBuffer[offset + elem*dlen + dlen - 1 - i];
            else
                byte = station->inBuffer[offset + elem*dlen + i];
            ((char*)data)[elem*dlen+i] = byte;
            s7plcDebugLog(5, " %02x", byte);
        }
        s7plcDebugLog(5, "\n");
    }
    epicsMutexUnlock(station->mutex);
    if (station->sockFd == -1) return S_drv_noConn;
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

    if (offset+dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "s7plcWrite %s/%d: offset out of range\n",
            station->name, offset);
        return -1;
    }
    if (offset+nelem*dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "s7plcWrite %s/%d: too many elements (%u)\n",
            station->name, offset, nelem);
        return -1;
    }
    s7plcDebugLog(4,
        "s7plcWriteMaskedArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->mutex);
    for (elem = 0; elem < nelem; elem++)
    {
        s7plcDebugLog(5, "data out:");
        for (i = 0; i < dlen; i++)
        {
            byte = ((unsigned char*)data)[elem*dlen+i];
            if (mask)
            {
                s7plcDebugLog(5, "(%02x & %02x)",
                    byte, ((unsigned char*)mask)[i]);
                byte &= ((unsigned char*)mask)[i];
            }
            if (station->swapBytes)
            {
                if (mask)
                {
                    s7plcDebugLog(5, " | (%02x & %02x) =>",
                        station->outBuffer[offset + elem*dlen + dlen - 1 - i],
                        ~((unsigned char*)mask)[i]);
                    byte |=
                        station->outBuffer[offset + elem*dlen + dlen - 1 - i]
                        & ~((unsigned char*)mask)[i];
                }
                s7plcDebugLog(5, " %02x", byte);
                station->outBuffer[offset + elem*dlen + dlen - 1 - i] = byte;
            }
            else
            {
                if (mask)
                {
                    s7plcDebugLog(5, " | (%02x & %02x) =>",
                        station->outBuffer[offset + elem*dlen + i],
                        ~((unsigned char*)mask)[i]);
                    byte |= station->outBuffer[offset + elem*dlen + i]
                        & ~((unsigned char*)mask)[i];
                }
                s7plcDebugLog(5, " %02x", byte);
                station->outBuffer[offset + elem*dlen + i] = byte;
            }
        }
        s7plcDebugLog(5, "\n");
        station->outputChanged=1;
    }
    epicsMutexUnlock(station->mutex);
    if (station->sockFd == -1) return S_drv_noConn;
    return S_drv_OK;
}

STATIC void s7plcSendThread(s7plcStation* station)
{
    char*  sendBuf = callocMustSucceed(1, station->outSize, "s7plcSendThread");

    s7plcDebugLog(1, "s7plcSendThread %s: started\n",
            station->name);

    while (1)
    {
        epicsTimerStartDelay(station->timer, station->sendIntervall);
        s7plcDebugLog(2, "s7plcSendThread %s: look for data to send\n",
            station->name);

        if (interruptAccept && station->sockFd != -1)
        {
            if (station->outputChanged)
            {
                epicsMutexMustLock(station->mutex);
                memcpy(sendBuf, station->outBuffer, station->outSize);
                station->outputChanged = 0;
                epicsMutexUnlock(station->mutex);

                if (station->sockFd != -1)
                {
                    int written;
                    s7plcDebugLog(2,
                        "s7plcSendThread %s: sending %d bytes\n",
                        station->name, station->outSize);
                    written=send(station->sockFd, sendBuf, station->outSize, 0);
                    if (written < 0)
                    {
                        s7plcErrorLog(
                            "s7plcSendThread %s: send(%d, ..., %d, 0) failed: %s\n",
                            station->name,
                            station->sockFd, station->outSize, strerror(errno));
                        s7plcCloseConnection(station);
                    }
                    else if ((unsigned int)written < station->outSize)
                    {
                        s7plcErrorLog(
                            "s7plcSendThread %s: send wrote only %d of %d bytes\n",
                            station->name, written, station->outSize);
                    }
                }
            }
            /* notify all "I/O Intr" output records */
            s7plcDebugLog(2,
                "s7plcSendThread %s: send cycle done, notify all output records\n",
                station->name);
            scanIoRequest(station->outScanPvt);
        }
        epicsEventMustWait(station->outTrigger);
    }
}

STATIC void s7plcReceiveThread(s7plcStation* station)
{
    unsigned char* recvBuf = callocMustSucceed(1, station->inSize, "s7plcReceiveThread");

    s7plcDebugLog(1, "s7plcReceiveThread %s: started\n",
            station->name);

    while (1)
    {
        unsigned int input;
        double timeout;
        double waitTime;
        int received;
        int status;
        epicsTimeStamp start, end;

        if (station->sockFd == -1)
        {
            /* not connected */
            if (s7plcConnect(station) < 0)
            {
                s7plcDebugLog(1,
                    "s7plcMain %s: connect to %s:%d failed. Retry in %g seconds\n",
                    station->name, station->server, station->serverPort,
                    (double)RECONNECT_DELAY);
                epicsThreadSleep(RECONNECT_DELAY);
                continue;
            }
        }

        input = 0;
        timeout = station->recvTimeout;
        /* check (with timeout) for data arrival from server */
        while (station->sockFd != -1 && input < station->inSize)
        {
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: waiting for input on fd %d for %g seconds\n",
                station->name, station->sockFd, timeout);
            epicsTimeGetCurrent(&start);
            /* Don't lock here! We need to be able to send while we wait */
            status = s7plcWaitForInput(station, timeout);
            epicsTimeGetCurrent(&end);
            waitTime = epicsTimeDiffInSeconds(&end, &start);
            if (status < 0)
            {
                s7plcErrorLog(
                    "s7plcReceiveThread %s: waiting for input failed: %s\n",
                    station->name,
                    strerror(errno));
                s7plcCloseConnection(station);
                break;
            }
            if (status > 0)
            {
                int receiveSize = station->inSize;

                received = recv(station->sockFd, (void*)recvBuf+input, receiveSize-input, 0);
                if (received == 0)
                {
                    s7plcErrorLog(
                        "s7plcReceiveThread %s: connection closed by %s\n",
                        station->name, station->server);
                    s7plcCloseConnection(station);
                    break;
                }
                if (received < 0)
                {
                    s7plcErrorLog(
                        "s7plcReceiveThread %s: recv(%d, ..., %d, 0) failed: %s\n",
                        station->name,
                        station->sockFd, station->inSize-input,
                        strerror(errno));
                    s7plcCloseConnection(station);
                    break;
                }
                s7plcDebugLog(1,
                    "s7plcReceiveThread %s: received %4d of %4d bytes after %.6f seconds\n",
                    station->name, received, receiveSize-input, waitTime);
                if (s7plcDebug >= 4)
                    hexdump(recvBuf+input, received, 1);
                input += received;
            }
            if (status <= 0 && timeout > 0.0)
            {
                s7plcErrorLog(
                    "s7plcReceiveThread %s: read error after %d of %d bytes: %s\n",
                    station->name,
                    input, station->inSize, strerror(errno));
                s7plcCloseConnection(station);
                break;
            }
        }
        if (station->sockFd != -1)
        {
            epicsMutexMustLock(station->mutex);
            memcpy(station->inBuffer, recvBuf, station->inSize);
            epicsMutexUnlock(station->mutex);
            /* notify all "I/O Intr" input records */
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: receive successful, notify all input records\n",
                station->name);
            scanIoRequest(station->inScanPvt);
        }
        else
        {
            s7plcDebugLog(1,
                "s7plcReceiveThread %s: connection down, sleeping %g seconds\n",
                station->name, CONNECT_TIMEOUT/4);
            /* lost connection. Wait some time */
            epicsThreadSleep(CONNECT_TIMEOUT/4);
        }
    }
}

STATIC int s7plcWaitForInput(s7plcStation* station, double timeout)
{
    static struct timeval to;
    int sockFd;
    int iSelect;
    fd_set socklist;

    sockFd = station->sockFd;
    FD_ZERO(&socklist);
    FD_SET(sockFd, &socklist);
    to.tv_sec=(int)timeout;
    to.tv_usec=(int)((timeout-to.tv_sec)*1000000);
    /* select returns when either the sockFd has data or the timeout elapsed */
    s7plcDebugLog(3,
        "s7plcWaitForInput %s: waiting for %ld.%06ld seconds\n",
        station->name, to.tv_sec, to.tv_usec);

    while ((iSelect=select(sockFd+1,&socklist, 0, 0,&to)) < 0)
    {
        if (station->sockFd == -1) return -1;
        if (errno != EINTR)
        {
            s7plcErrorLog(
                "s7plcWaitForInput %s: select(%d, %g sec) failed: %s\n",
                station->name, station->sockFd, timeout,
                strerror(errno));
            return -1;
        }
        s7plcErrorLog("s7plcWaitForInput %s: interrupted by signal\n",
            station->name);
    }
    if (iSelect==0 && timeout > 0)            /* timed out */
    {
        s7plcErrorLog(
            "s7plcWaitForInput %s: timeout after %g seconds.\n",
            station->name, timeout);
        errno = ETIMEDOUT;
        return -1;
    }
    return iSelect;
}

STATIC int s7plcConnect(s7plcStation* station)
{
    int sockFd;
    struct sockaddr_in serverAddr = {0};
    struct timeval    to;

    s7plcDebugLog(1, "s7plcConnect %s: IP=%s port=%d\n",
        station->name, station->server, station->serverPort);

    if (station->server[0] == '\0') return -1; /* empty host string */

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(station->serverPort);
    if (hostToIPAddr(station->server, &serverAddr.sin_addr) < 0)
    {
        s7plcErrorLog(
            "s7plcConnect %s: hostToIPAddr(%s) failed.\n",
            station->name, station->server);
        return -1;
    }

    if ((sockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        s7plcErrorLog(
            "s7plcConnect %s: creating socket failed: %s\n",
            station->name, strerror(errno));
        return -1;
    }

    /* connect to server */
    to.tv_sec=(int)(CONNECT_TIMEOUT);
    to.tv_usec=(int)(CONNECT_TIMEOUT-to.tv_sec)*1000000;
#if defined(vxWorks) || defined(__vxworks)
    if (connectWithTimeout(sockFd,
        (struct sockaddr *) &serverAddr, sizeof (serverAddr), &to) < 0)
    {
        s7plcErrorLog(
            "s7plcConnect %s: connectWithTimeout(%d, %s:%d, %g sec) failed: %s\n",
            station->name, sockFd, station->server, station->serverPort, CONNECT_TIMEOUT, strerror(errno));
        close(sockFd);
        return -1;
    }
#else
    /* connect in non-blocking mode */
    fcntl(sockFd, F_SETFL, fcntl(sockFd, F_GETFL, NULL) | O_NONBLOCK);
    if (connect(sockFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            /* start timeout */
            int status;
            socklen_t lon = sizeof(status);
            fd_set fdset;

            FD_ZERO(&fdset);
            FD_SET(sockFd, &fdset);
            /* wait for connection */
            while ((status = select(sockFd+1, NULL, &fdset, NULL, &to)) < 0)
            {
                if (errno != EINTR) /* select may be interrupted by signal */
                {
                    s7plcErrorLog(
                        "s7plcConnect %s: select(%d, %f sec) failed: %s\n",
                        station->name, sockFd, CONNECT_TIMEOUT, strerror(errno));
                    close(sockFd);
                    return -1;
                }
            }
            if (status == 0)
            {
                s7plcErrorLog(
                    "s7plcConnect %s: connect to %s:%d timeout after %g seconds\n",
                    station->name, station->server, station->serverPort, CONNECT_TIMEOUT);
                close(sockFd);
                return -1;
            }
            /* get background error status */
            getsockopt(sockFd, SOL_SOCKET, SO_ERROR, &status, &lon);
            if (status)
            {
                s7plcErrorLog(
                    "s7plcConnect %s: background connect to %s:%d failed: %s\n",
                    station->name, station->server, station->serverPort, strerror(status));
                close(sockFd);
                return -1;
            }
        }
        else
        {
            s7plcErrorLog(
                "s7plcConnect %s: connect to %s:%d failed: %s\n",
                station->name, station->server, station->serverPort, strerror(errno));
            close(sockFd);
            return -1;
        }
    }
    /* connected */
    fcntl(sockFd, F_SETFL, fcntl(sockFd, F_GETFL, NULL) & ~O_NONBLOCK);
#endif
    s7plcErrorLog(
        "s7plcConnect %s: connected to %s:%d\n",
        station->name, station->server, station->serverPort);

    station->sockFd = sockFd;
    return 0;
}

STATIC void s7plcCloseConnection(s7plcStation* station)
{
    s7plcErrorLog(
        "s7plcCloseConnection %s\n", station->name);
    epicsMutexMustLock(station->mutex);
    if (station->sockFd>0)
    {
        if (shutdown(station->sockFd, SHUT_RDWR) && errno != ENOTCONN)
        {
            s7plcErrorLog(
                "s7plcCloseConnection %s: shutdown(%d, SHUT_RDWR) failed (ignored): %s\n",
                station->name,
                station->sockFd, strerror(errno));
        }
        if (close(station->sockFd) && errno != ENOTCONN)
        {
            s7plcErrorLog(
                "s7plcCloseConnection %s: close(%d) failed (ignored): %s\n",
                station->name,
                station->sockFd, strerror(errno));
        }
        station->sockFd = -1;
    }
    epicsMutexUnlock(station->mutex);
    /* notify all "I/O Intr" input records */
    scanIoRequest(station->inScanPvt);
}

int s7plcGetAddr(s7plcStation* station, char* addr)
{
    s7plcDebugLog(1, "s7plcGetAddr %s:%d\n", station->server, station->serverPort);
    if (station->server && strlen(station->server) <= 30)
    {
        sprintf(addr, "%.30s:%d", station->server, station->serverPort);
        return 0;
    }
    return -1;
}

int s7plcSetAddr(s7plcStation* station, const char* addr)
{
    char* c;

    s7plcDebugLog(1, "s7plcSetAddr %s\n", addr);
    epicsMutexMustLock(station->mutex);
    s7plcCloseConnection(station);
    free(station->server);
    station->server = strdup(addr);
    c = strchr(station->server, ':');
    if (c)
    {
        station->serverPort = strtol(c+1,NULL,10);
        *c = 0;
    }
    epicsMutexUnlock(station->mutex);
    return 0;
}

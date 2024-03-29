#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#define SOCKFMT "lld"
#define NFDS(fd) 0
#define SOCKERRTYPE DWORD
#define ioctl ioctlsocket
#undef EINPROGRESS
#define EINPROGRESS WSAEWOULDBLOCK
#define SET_TIMEOUT_ERROR WSASetLastError(WSAETIMEDOUT)
#else
#include <netinet/in.h>
#include <unistd.h>
#define SOCKFMT "d"
#define NFDS(fd) fd+1
#define SOCKERRTYPE int
#define SET_TIMEOUT_ERROR errno = ETIMEDOUT
#endif

#ifdef vxWorks
#include <version.h>
#ifndef _WRS_VXWORKS_MAJOR
#include <ioLib.h>
#define ioctl(s,c,o) ioctl(s,c,(int)o)
#endif
#define socklen_t int
#endif

#include <drvSup.h>
#include <devLib.h>
#include <errlog.h>

#include <osiSock.h>
#include <dbAccess.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTimer.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <epicsString.h>
#include <epicsExport.h>

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
STATIC int s7plcCheckConnection(s7plcStation* station);
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
    SOCKET sock;
    epicsMutexId mutex;
    epicsMutexId io;
    epicsTimerId timer;
    epicsEventId outTrigger;
    int outputChanged;
    IOSCANPVT inScanPvt;
    IOSCANPVT outScanPvt;
    epicsThreadId sendThread;
    epicsThreadId recvThread;
    double recvTimeout;
    double sendIntervall;
};

char* s7plcCurrentTime()
{
    static char buffer [40];
    epicsTimeStamp stamp;
    epicsTimeGetCurrent(&stamp);
    epicsTimeToStrftime(buffer, sizeof(buffer), "%m/%d/%y %H:%M:%S.%03f", &stamp);
    return buffer;
}

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
            station->sock != INVALID_SOCKET ? "connected to" : "disconnected from",
            station->server, station->serverPort);
        if (level < 1) continue;
        printf("    file descriptor %" SOCKFMT "\n", station->sock);
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
        /* Create a receiver thread only if there will be any data to receive. */
        if (station->inSize)
        {
            sprintf(threadname, "%.15sR", station->name);
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
        }

        /* Create a sender thread only if there will be any data to send. */
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
    station->server = IPaddr ? epicsStrDup(IPaddr) : NULL;
    station->swapBytes = bigEndian ^ bigEndianIoc;
    station->sock = INVALID_SOCKET;
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
       return S_dev_badArgument;
    }
    if (offset+nelem*dlen > station->inSize)
    {
       errlogSevPrintf(errlogMajor,
        "s7plcRead %s/%u: too many elements (%u)\n",
        station->name, offset, nelem);
       return S_dev_badArgument;
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
    if (station->sock == INVALID_SOCKET) return S_dev_noDevice;
    return S_dev_success;
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
    if (station->sock == INVALID_SOCKET) return S_dev_noDevice;
    return S_dev_success;
}

STATIC void s7plcSendThread(s7plcStation* station)
{
    char* sendBuf = callocMustSucceed(1, station->outSize, "s7plcSendThread");
    char errmsg[100];

    s7plcDebugLog(1, "s7plcSendThread %s: started\n",
            station->name);

    while (1)
    {
        /*
         * Check if the connection is established and establish a new if it isn't - in a
         * thread-safe manner.
         */
        if (s7plcCheckConnection(station) == -1)
        {
            s7plcDebugLog(1,
                "s7plcMain %s: connect to %s:%d failed. Retry in %g seconds\n",
                station->name, station->server, station->serverPort,
                (double)RECONNECT_DELAY);
            epicsThreadSleep(RECONNECT_DELAY);
            continue;
        }

        epicsTimerStartDelay(station->timer, station->sendIntervall);
        s7plcDebugLog(2, "s7plcSendThread %s: look for data to send\n",
            station->name);

        if (interruptAccept && station->sock != INVALID_SOCKET)
        {
            if (station->outputChanged)
            {
                epicsMutexMustLock(station->mutex);
                memcpy(sendBuf, station->outBuffer, station->outSize);
                station->outputChanged = 0;
                epicsMutexUnlock(station->mutex);

                if (station->sock != INVALID_SOCKET)
                {
                    int written;
                    s7plcDebugLog(2,
                        "s7plcSendThread %s: sending %d bytes\n",
                        station->name, station->outSize);
                    written=send(station->sock, sendBuf, station->outSize, 0);
                    if (written < 0)
                    {
                        epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
                        s7plcErrorLog(
                            "s7plcSendThread %s: send(%d, ..., %d, 0) failed: %s\n",
                            station->name,
                            station->sock, station->outSize, errmsg);
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
        char errmsg[100];

        /*
         * Check if the connection is established and establish a new if it isn't - in a
         * thread-safe manner.
         */
        if (s7plcCheckConnection(station) == -1)
        {
            s7plcDebugLog(1,
                "s7plcMain %s: connect to %s:%d failed. Retry in %g seconds\n",
                station->name, station->server, station->serverPort,
                (double)RECONNECT_DELAY);
            epicsThreadSleep(RECONNECT_DELAY);
            continue;
        }

        input = 0;
        timeout = station->recvTimeout;
        /* check (with timeout) for data arrival from server */
        while (station->sock != INVALID_SOCKET && input < station->inSize)
        {
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: waiting for input on fd %d for %g seconds\n",
                station->name, station->sock, timeout);
            epicsTimeGetCurrent(&start);
            /* Don't lock here! We need to be able to send while we wait */
            status = s7plcWaitForInput(station, timeout);
            epicsTimeGetCurrent(&end);
            waitTime = epicsTimeDiffInSeconds(&end, &start);
            if (status < 0)
            {
                epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
                s7plcErrorLog(
                    "s7plcReceiveThread %s: waiting for input failed: %s\n",
                    station->name, errmsg);
                s7plcCloseConnection(station);
                break;
            }
            if (status > 0)
            {
                int receiveSize = station->inSize;

                received = recv(station->sock, (void*)(recvBuf+input), receiveSize-input, 0);
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
                    epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
                    s7plcErrorLog(
                        "s7plcReceiveThread %s: recv(%d, ..., %d, 0) failed: %s\n",
                        station->name,
                        station->sock, station->inSize-input, errmsg);
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
                epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
                s7plcErrorLog(
                    "s7plcReceiveThread %s: read error after %d of %d bytes: %s\n",
                    station->name,
                    input, station->inSize, errmsg);
                s7plcCloseConnection(station);
                break;
            }
        }
        if (station->sock != INVALID_SOCKET)
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
    SOCKET sock;
    int iSelect;
    fd_set socklist;
    char errmsg[100];

    sock = station->sock;
    FD_ZERO(&socklist);
    FD_SET(sock, &socklist);
    to.tv_sec=(int)timeout;
    to.tv_usec=(int)((timeout-to.tv_sec)*1000000);
    /* select returns when either the sock has data or the timeout elapsed */
    s7plcDebugLog(3,
        "s7plcWaitForInput %s: waiting for %ld.%06ld seconds\n",
        station->name, to.tv_sec, to.tv_usec);

    while ((iSelect=select(NFDS(sock),&socklist, 0, 0,&to)) < 0)
    {
        if (station->sock == INVALID_SOCKET) return -1;
        if (SOCKERRNO != EINTR)
        {
            epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
            s7plcErrorLog(
                "s7plcWaitForInput %s: select(%" SOCKFMT ", %g sec) failed: %s\n",
                station->name, station->sock, timeout, errmsg);
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
        SET_TIMEOUT_ERROR;
        return -1;
    }
    return iSelect;
}

/**
 * Checks if the connection with the PLC is established, and establishes a new connection if
 * it isn't - in a thread-safe manner.
 *
 * Returns 0 if the existing connection is OK (or the connection was successfully established
 * after it not being valid).
 * Returns -1 if the connection was not OK, and a new one couldn't be established.
 */
STATIC int s7plcCheckConnection(s7plcStation* station)
{
    /* 0 = connection is OK, -1 = connection could not be established */
    int connectionOk = 0;
    /*
     * Use a critical section around the socket file descriptor to avoid potential race
     * conditions between the sending and receiving thread, when checking the state of the
     * connection and potential (re)establishing of the connection.
     */
    epicsMutexMustLock(station->mutex);
    if (station->sock == INVALID_SOCKET)
    {
        /* not connected */
        if (s7plcConnect(station) < 0)
        {
            connectionOk = -1;
        }
    }
    epicsMutexUnlock(station->mutex);
    return connectionOk;
}

STATIC int s7plcConnect(s7plcStation* station)
{
    SOCKET sock;
    struct sockaddr_in serverAddr = {0};
    struct timeval to;
    int nonblocking;
    char errmsg[100];

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

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
        s7plcErrorLog(
            "s7plcConnect %s: creating socket failed: %s\n",
            station->name, errmsg);
        return -1;
    }

    /* connect to server */
    to.tv_sec=(int)(CONNECT_TIMEOUT);
    to.tv_usec=(int)(CONNECT_TIMEOUT-to.tv_sec)*1000000;
    /* connect in non-blocking mode to use select with timeout */
    nonblocking = 1;
    ioctl(sock, FIONBIO, &nonblocking);
    if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        if (SOCKERRNO == EINPROGRESS)
        {
            /* start timeout */
            int status;
            SOCKERRTYPE sockerr;
            socklen_t len = sizeof(sockerr);
            fd_set fdset;

            FD_ZERO(&fdset);
            FD_SET(sock, &fdset);
            /* wait for connection */
            while ((status = select(NFDS(sock), NULL, &fdset, NULL, &to)) < 0)
            {
                if (SOCKERRNO != EINTR) /* select may be interrupted by signal */
                {
                    epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
                    s7plcErrorLog(
                        "s7plcConnect %s: select(%d, %f sec) failed: %s\n",
                        station->name, sock, CONNECT_TIMEOUT, errmsg);
                    epicsSocketDestroy(sock);
                    return -1;
                }
            }
            if (status == 0)
            {
                s7plcErrorLog(
                    "s7plcConnect %s: connect to %s:%d timeout after %g seconds\n",
                    station->name, station->server, station->serverPort, CONNECT_TIMEOUT);
                epicsSocketDestroy(sock);
                return -1;
            }
            /* get background error status */
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)&sockerr, &len);
            if (sockerr)
            {
                epicsSocketConvertErrorToString(errmsg, sizeof(errmsg), sockerr);
                s7plcErrorLog(
                    "s7plcConnect %s: background connect to %s:%d failed: %s\n",
                    station->name, station->server, station->serverPort, errmsg);
                epicsSocketDestroy(sock);
                return -1;
            }
        }
        else
        {
            epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
            s7plcErrorLog(
                "s7plcConnect %s: connect to %s:%d failed: %s\n",
                station->name, station->server, station->serverPort, errmsg);
            epicsSocketDestroy(sock);
            return -1;
        }
    }
    /* connected */
    nonblocking = 0;
    ioctl(sock, FIONBIO, &nonblocking);
    s7plcErrorLog(
        "s7plcConnect %s: connected to %s:%d\n",
        station->name, station->server, station->serverPort);

    station->sock = sock;
    return 0;
}

STATIC void s7plcCloseConnection(s7plcStation* station)
{
    char errmsg[100];

    s7plcErrorLog(
        "s7plcCloseConnection %s\n", station->name);
    epicsMutexMustLock(station->mutex);
    if (station->sock>0)
    {
        if (shutdown(station->sock, SHUT_RDWR) && SOCKERRNO != ENOTCONN)
        {
            epicsSocketConvertErrnoToString(errmsg, sizeof(errmsg));
            s7plcErrorLog(
                "s7plcCloseConnection %s: shutdown(%d, SHUT_RDWR) failed (ignored): %s\n",
                station->name,
                station->sock, errmsg);
        }
        epicsSocketDestroy(station->sock);
        station->sock = INVALID_SOCKET;
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
    station->server = epicsStrDup(addr);
    c = strchr(station->server, ':');
    if (c)
    {
        station->serverPort = strtol(c+1,NULL,10);
        *c = 0;
    }
    epicsMutexUnlock(station->mutex);
    return 0;
}

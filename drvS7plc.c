/* $Author: zimoch $ */
/* $Date: 2013/01/16 10:17:33 $ */
/* $Id: drvS7plc.c,v 1.17 2013/01/16 10:17:33 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.17 $ */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(vxWorks) || defined(__vxworks)
#include <sockLib.h>
#include <taskLib.h>
#include <selectLib.h>
#include <taskHookLib.h>
#define in_addr_t unsigned long
#else
#include <fcntl.h>
#endif

#ifdef __rtems__
#include <sys/select.h>
#endif

#include <drvSup.h>
#include <devLib.h>
#include <errlog.h>
#include <epicsVersion.h>

#include "drvS7plc.h"

#if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)
/* R3.14 */
#include <dbAccess.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTimer.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#else
/* R3.13 */
#include "compat3_13.h"
#endif

#define CONNECT_TIMEOUT   5.0  /* connect timeout [s] */
#define RECONNECT_DELAY  10.0  /* delay before reconnect [s] */

static char cvsid[] __attribute__((unused)) =
"$Id: drvS7plc.c,v 1.17 2013/01/16 10:17:33 zimoch Exp $";

STATIC long s7plcIoReport(int level); 
STATIC long s7plcInit();
void s7plcMain ();
STATIC void s7plcSendThread(s7plcStation* station);
STATIC void s7plcReceiveThread(s7plcStation* station);
STATIC int s7plcWaitForInput(s7plcStation* station, double timeout);
STATIC int s7plcEstablishConnection(s7plcStation* station);
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
    char serverIP[20];
    int serverPort;
    int inSize;
    int outSize;
    char* inBuffer;
    char* outBuffer;
    int swapBytes;
    int connStatus;
    volatile int socket;
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

void s7plcDebugLog(int level, const char *fmt, ...)
{
    va_list args;
    
    if (level > s7plcDebug) return;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

STATIC long s7plcIoReport(int level)
{
    s7plcStation *station;

    printf("%s\n", cvsid);
    if (level == 1)
    {
        printf("S7plc stations:\n");
        for (station = s7plcStationList; station;
            station=station->next)
        {
            printf("  Station %s ", station->name);
            if (station->connStatus)
            {
                printf("connected via file descriptor %d to\n",
                    station->socket);
            }
            else
            {
                printf("disconnected from\n");
            }
            printf("  plc with address %s on port %d\n",
                station->serverIP, station->serverPort);
            printf("    inBuffer  at address %p (%d bytes)\n",
                station->inBuffer,  station->inSize);
            printf("    outBuffer at address %p (%d bytes)\n",
                station->outBuffer,  station->outSize);
            printf("    swap bytes %s\n",
                station->swapBytes
                    ? ( bigEndianIoc ? "ioc:motorola <-> plc:intel" : "ioc:intel <-> plc:motorola" )
                    : ( bigEndianIoc ? "no, both motorola" : "no, both intel" ) );
            printf("    receive timeout %g sec\n",
                station->recvTimeout);
            printf("    send intervall  %g sec\n",
                station->sendIntervall);
        }
    }
    return 0;
}

STATIC long s7plcInit()
{
    if (!s7plcStationList) {
        errlogSevPrintf(errlogInfo,
            "s7plcInit: no stations configured\n");
        return 0;
    }
    s7plcDebugLog(1, "s7plcInit: starting main thread\n");
    epicsThreadCreate(
        "s7plcMain",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC)s7plcMain,
        NULL);
    return 0;
}

STATIC void s7plcSignal(void* event)
{
    epicsEventSignal((epicsEventId)event);
}

int s7plcConfigure(char *name, char* IPaddr, int port, int inSize, int outSize, int bigEndian, int recvTimeout, int sendIntervall)
{
    s7plcStation* station;
    s7plcStation** pstation;
    in_addr_t ip;
    
    union {short s; char c [sizeof(short)];} u;
    u.s=1;
    bigEndianIoc = !u.c[0];
    if (!name)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: missing name\n");
        return -1;
    }
    if (!IPaddr)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: missing IP address\n");
        return -1;
    }
    if (!port)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: missing IP port\n");
        return -1;
    }
    ip = inet_addr(IPaddr);
    if (ip == INADDR_NONE)
    {
        errlogSevPrintf(errlogFatal,
            "s7plcConfigure: invalid IP address %s\n", IPaddr);
        return -1;
    }
    ip = ntohl(ip);
        
    /* find last station in list */
    for (pstation = &s7plcStationList; *pstation; pstation = &(*pstation)->next);
    
    station = callocMustSucceed(1,
        sizeof(s7plcStation) + inSize + outSize + strlen(name)+1 , "s7plcConfigure");
    station->next = NULL;
    sprintf(station->serverIP, "%d.%d.%d.%d",
        (int)((ip>>24)&0xff), (int)((ip>>16)&0xff), (int)((ip>>8)&0xff), ((int)ip&0xff));
    station->serverPort = port;
    station->inSize = inSize;
    station->outSize = outSize;
    station->inBuffer = (char*)(station+1);
    station->outBuffer = (char*)(station+1)+inSize;
    station->name = (char*)(station+1)+inSize+outSize;
    strcpy(station->name, name);
    station->swapBytes = bigEndian ^ bigEndianIoc;
    station->connStatus = 0;
    station->socket = -1;
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

#if (EPICS_REVISION>=14)
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

static void s7plcRegister ()
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
    epicsUInt16 connStatus;

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
    connStatus = station->connStatus;
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
    connStatus = station->connStatus;
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
    if (!connStatus) return S_drv_noConn;
    return S_drv_OK;
}

void s7plcMain ()
{
    s7plcStation* station;
    char threadname[20];
    
    s7plcDebugLog(1, "s7plcMain: main thread started\n");
    
    while (1)
    {   /* watch loop to restart dead threads and reopen sockets*/
        for (station = s7plcStationList; station; station=station->next)
        {            
            /* establish connection with server */
            epicsMutexMustLock(station->io);
            if (station->socket == -1)
            {
                /* create station socket */
                if ((station->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! socket(AF_INET, SOCK_STREAM, 0) failed: %s\n",
                        station->name, strerror(errno));
                    abort();
                }
                s7plcDebugLog(1,
                    "s7plcMain %s: Connect to %s:%d on socket %d\n",
                    station->name,
                    station->serverIP, station->serverPort, station->socket);
                if (s7plcEstablishConnection(station) < 0)
                {
                    s7plcDebugLog(1,
                        "s7plcMain %s: connect(%d, %s:%d) failed: %s. Retry in %g seconds\n",
                        station->name,
                        station->socket, station->serverIP, station->serverPort,
                        strerror(errno), (double)RECONNECT_DELAY);
                    if (close(station->socket) && errno != ENOTCONN)
                    {
                        s7plcDebugLog(1,
                            "s7plcMain %s: close(%d) failed (ignored): %s\n",
                            station->name, station->socket, strerror(errno));
                    }
                    station->socket=-1;
                }
            }
            epicsMutexUnlock(station->io);

            /* check whether station threads are running */
            
            sprintf (threadname, "%.15sS", station->name);
            if (station->sendThread && epicsThreadIsSuspended(station->sendThread))
            {    /* if suspended delete it */
                s7plcDebugLog(0,
                    "s7plcMain %s: send thread %s %p is dead\n",
                    station->name, threadname, station->sendThread);
                /* maybe we should cleanup the semaphores ? */
                s7plcCloseConnection(station);
                station->sendThread = 0;
            }
            if (!station->sendThread && station->outSize)
            {
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
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! could not start send thread %s\n",
                        station->name, threadname);
                    abort();
                }
            }
            
            sprintf (threadname, "%.15sR", station->name);
            if (station->recvThread && epicsThreadIsSuspended(station->recvThread))
            {    /* if suspended delete it */
                s7plcDebugLog(0,
                    "s7plcMain %s: recv thread %s %p is dead\n",
                    station->name, threadname, station->recvThread);
                /* maybe we should cleanup the semaphores ? */
                s7plcCloseConnection(station);
                station->recvThread = 0;
            }
            if (!station->recvThread && station->inSize)
            {
                s7plcDebugLog(1,
                    "s7plcMain %s: starting recv thread %s\n",
                    station->name, threadname);
                station->recvThread = epicsThreadCreate(
                    threadname,
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackBig),
                    (EPICSTHREADFUNC)s7plcReceiveThread,
                    station);
                if (!station->recvThread)
                {
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! could not start recv thread %s\n",
                        station->name, threadname);
                    abort();
                }
            }
            
        }
        epicsThreadSleep(RECONNECT_DELAY);
    }        
}

STATIC void s7plcSendThread (s7plcStation* station)
{
    char*  sendBuf = callocMustSucceed(1, station->outSize, "s7plcSendThread ");

    s7plcDebugLog(1, "s7plcSendThread %s: started\n",
            station->name);

    while (1)
    {
        epicsTimerStartDelay(station->timer, station->sendIntervall);
        s7plcDebugLog(2, "s7plcSendThread %s: look for data to send\n",
            station->name);

        if (interruptAccept && station->socket != -1)
        {
            if (station->outputChanged)
            {
                epicsMutexMustLock(station->mutex);
                memcpy(sendBuf, station->outBuffer, station->outSize);
                station->outputChanged = 0; 
                epicsMutexUnlock(station->mutex);

                s7plcDebugLog(4, "send %d bytes\n", station->outSize);
                epicsMutexMustLock(station->io);
                if (station->socket != -1)
                {
                    int written;
                    s7plcDebugLog(2,
                        "s7plcSendThread %s: sending %d bytes\n",
                        station->name, station->outSize);
                    written=send(station->socket, sendBuf, station->outSize, 0);
                    if (written < 0)
                    {
                        s7plcDebugLog(0,
                            "s7plcSendThread %s: send(%d, ..., %d, 0) failed: %s\n",
                            station->name,
                            station->socket, station->outSize, strerror(errno));
                        s7plcCloseConnection(station);
                    }
                }
                epicsMutexUnlock(station->io);
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

STATIC void s7plcReceiveThread (s7plcStation* station)
{
    char*  recvBuf = callocMustSucceed(2, station->inSize, "s7plcReceiveThread");

    s7plcDebugLog(1, "s7plcReceiveThread %s: started\n",
            station->name);

    while (1)
    {
        int input;
        double timeout;
        int received;
        int status;

        input = 0;
        timeout = station->recvTimeout;
        /* check (with timeout) for data arrival from server */
        while (station->socket != -1 && input < station->inSize)
        {
            /* Don't lock here! We need to be able to send while we wait */
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: waiting for input for %g seconds\n",
                station->name, timeout);
            status = s7plcWaitForInput(station, timeout);
            epicsMutexMustLock(station->io);
            if (status > 0)
            {
                /* data available; read data from server plc */
                received = recv(station->socket, recvBuf+input, station->inSize-input, 0);
                s7plcDebugLog(3,
                    "s7plcReceiveThread %s: received %d bytes\n",
                    station->name, received);
                if (received <= 0)
                {
                    s7plcDebugLog(0,
                        "s7plcReceiveThread %s: recv(%d, ..., %d, 0) failed: %s\n",
                        station->name,
                        station->socket, station->inSize-input,
                        strerror(errno));
                    s7plcCloseConnection(station);
                    epicsMutexUnlock(station->io);
                    break;
                }
                input += received;
            }
            if (input > station->inSize)
            {
                /* input complete, check for excess bytes */
                if (status > 0)
                {
                    s7plcDebugLog(0,
                        "s7plcReceiveThread %s: %d bytes excess data received\n",
                        station->name, input - station->inSize);
                    s7plcCloseConnection(station);
                }
                epicsMutexUnlock(station->io);
                break;
            }
            if (status <= 0 && timeout > 0.0)
            {
                s7plcDebugLog(0,
                    "s7plcReceiveThread %s: read error after %d of %d bytes: %s\n",
                    station->name,
                    input, station->inSize, strerror(errno));
                s7plcCloseConnection(station);
                epicsMutexUnlock(station->io);
                break;
            }
            epicsMutexUnlock(station->io);
/*            timeout = (input < station->inSize)? 0.1 : 0.0; */
        }
        if (station->socket != -1)
        {
            epicsMutexMustLock(station->mutex);
            memcpy(station->inBuffer, recvBuf, station->inSize);
            station->connStatus = 1;
            epicsMutexUnlock(station->mutex);
            /* notify all "I/O Intr" input records */
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: receive successful, notify all input records\n",
                station->name);
            scanIoRequest(station->inScanPvt);
        }
        else
        {
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: connection down, sleeping %g seconds\n",
                station->name, station->recvTimeout/4);
            /* lost connection. Wait some time */
            epicsThreadSleep(station->recvTimeout/4);
        }
    }
}

STATIC int s7plcWaitForInput(s7plcStation* station, double timeout)
{
    static struct timeval to;
    int socket;
    int iSelect;
    fd_set socklist;

    socket = station->socket;
    FD_ZERO(&socklist);
    FD_SET(socket, &socklist);
    to.tv_sec=(int)timeout;
    to.tv_usec=(int)((timeout-to.tv_sec)*1000000);
    /* select returns when either the socket has data or the timeout elapsed */
    errno = 0;
    while ((iSelect=select(socket+1,&socklist, 0, 0,&to)) < 0)
    {
        if (errno != EINTR)
        {
            s7plcDebugLog(0,
                "s7plcWaitForInput %s: select(%d, %f sec) failed: %s\n",
                station->name, station->socket, timeout,
                strerror(errno));
            return -1;
        }
    }
    if (iSelect==0 && timeout > 0)            /* timed out */
    {
        s7plcDebugLog(0,
            "s7plcWaitForInput %s: select(%d, %f sec) timed out\n",
            station->name, station->socket, timeout);
        errno = ETIMEDOUT;
    }
    return iSelect;
}

STATIC int s7plcEstablishConnection(s7plcStation* station)
{
    struct sockaddr_in    serverAddr;    /* server socket address */
    struct timeval    to;
#if (!defined(vxWorks)) && (!defined(__vxworks))
    long opt;
#endif

    s7plcDebugLog(1, "s7plcEstablishConnection %s: fd=%d, IP=%s port=%d\n",
        station->name,
        station->socket, station->serverIP, station->serverPort);

    /* build server socket address */
    memset((char *) &serverAddr, 0, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(station->serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(station->serverIP);

    /* connect to server */
    to.tv_sec=(int)(CONNECT_TIMEOUT);
    to.tv_usec=(int)(CONNECT_TIMEOUT-to.tv_sec)*1000000;
#if defined(vxWorks) || defined(__vxworks)
    if (connectWithTimeout(station->socket,
        (struct sockaddr *) &serverAddr, sizeof (serverAddr), &to) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: connectWithTimeout(%d,...,%g sec) failed: %s\n",
            station->name, station->socket, CONNECT_TIMEOUT, strerror(errno));
        return -1;
    }
#else
    /* connect in non-blocking mode */
    if((opt = fcntl(station->socket, F_GETFL, NULL)) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_GETFL, NULL) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
    opt |= O_NONBLOCK;
    if(fcntl(station->socket, F_SETFL, opt) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_SETFL, O_NONBLOCK) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
    if (connect(station->socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            /* start timeout */
            int status;
            socklen_t lon = sizeof(status);
            fd_set fdset;

            FD_ZERO(&fdset);
            FD_SET(station->socket, &fdset);
            /* wait for connection */
            while ((status = select(station->socket+1, NULL, &fdset, NULL, &to)) < 0)
            {
                if (errno != EINTR)
                {
                    s7plcDebugLog(0,
                        "s7plcEstablishConnection %s: select(%d, %f sec) failed: %s\n",
                        station->name, station->socket, CONNECT_TIMEOUT, strerror(errno));
                    return -1;
                }
            }
            if (status == 0)
            {
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: select(%d, %f sec) timed out\n",
                    station->name, station->socket, CONNECT_TIMEOUT);
                errno = ETIMEDOUT;
                return -1;
            }
            /* get background error status */
            if (getsockopt(station->socket, SOL_SOCKET, SO_ERROR, &status, &lon) < 0)
            {
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: getsockopt(%d,...) failed: %s\n",
                    station->name,
                    station->socket, strerror(errno));
                return -1;
            }
            if (status)
            {
                errno = status;
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: background connect(%d,...) failed: %s\n",
                    station->name,
                    station->socket, strerror(errno));
                return -1;
            }
        }
        else
        {
            s7plcDebugLog(0,
                "s7plcEstablishConnection %s: connect(%d,...) failed: %s\n",
                station->name,
                station->socket, strerror(errno));
            return -1;
        }
    }
    /* connected */
    opt &= ~O_NONBLOCK;
    if(fcntl(station->socket, F_SETFL, opt) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_SETFL, ~O_NONBLOCK) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
#endif
    return 0;
}

STATIC void s7plcCloseConnection(s7plcStation* station)
{
    station->connStatus = 0;
    if (station->socket>0)
    {
        if (shutdown(station->socket, 2) < 0)
        {
            s7plcDebugLog(0,
                "s7plcCloseConnection %s: shutdown(%d, 2) failed (ignored): %s\n",
                station->name,
                station->socket, strerror(errno));
        }
        if (close(station->socket) && errno != ENOTCONN)
        {
            s7plcDebugLog(0,
                "s7plcCloseConnection %s: close(%d) failed (ignored): %s\n",
                station->name,
                station->socket, strerror(errno));
        }
        station->socket = -1;
    }
    /* notify all "I/O Intr" input records */
    scanIoRequest(station->inScanPvt);
}

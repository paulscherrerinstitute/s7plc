/*
###############################################################################
#                                                                             #
# Copyright (C) 2003 PSI                                                      #
#                                                                             #
# This program is free software; you can redistribute it and/or               #
# modify it under the terms of the GNU General Public License                 #
# as published by the Free Software Foundation; either version 2              #
# of the License, or (at your option) any later version.                      #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
# but WITHOUT ANY WARRANTY; without even the implied warranty of              #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               #
# GNU General Public License for more details.                                #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program; if not, write to the Free Software                 #
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. #
#                                                                             #
###############################################################################
*/
/*=============================================================================
//
// COMPANY:            Paul Scherrer Institut
// PROGRAMMERS:    Martin Emmenegger EM93
//
// DATE:                30.05.2000
//
//-----------------------------------------------------------------------------
//=============================================================================
//
// COMPANY:            Zuehlke Engineering AG, Wiesenstr. 10a, CH-8952 Schlieren
// PROGRAMMERS:    Edgar Hasler (eh)
//                        Joerg Bisang (jb)
// DATE:                26.05.2000
//
//-----------------------------------------------------------------------------
//
// MODULE:
//
// FILENAME:        ticp.cpp
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:     This program provides the communication between EPICS(IOC) and
//                        until 16  S7 programmable controllers.
//                        It is configured through the headerinformation of the S7- headstation
//===========================================================================
// modification
// history:
//       18.09.03    Korobov      bug fix (from Keith Baker). Version
//                  with/without remote header is selected by
//                  argument of TICP_start.
//                  DESY version 1.10.5
//       27.07.03    Korobov      bug fix in configuration file parser.
//        24.04.03    Korobov      provide run-time switching on/off debugging
//                  print out. TICP_debug-1 is the client number
//                  to print out  (DESY version 1.10.2)
//        28.02.03    Korobov      inserted 5 sec delays between client start
//                  and before client stop because of 'socket
//                  error'. (DESY version 1.10.1)
//        25.07.02    Korobov      data buffer in Memory Table is the char array;
//                  copying from recvBuf to SM is done with
//                  bcopyBytes() routine to avoid alignment
//                  problems for short, long and float.
//                  Added ICP_version routine including CVS data.
//                  (DESY version 1.10)
//        15.05.02    Korobov      data in header are in a binary encoding;
//                  don't eliminate a header in order to keep
//                  the same offsets in PLC and in data base.
//                  (DESY version 1.9).
//        30.04.02    Korobov      data in header are in BCD encoding, so it"s
//                  necessary to confert it to/from BCD;
//                  add bcd2d & d2bcd routines to convert header
//                  information; (perhaps it's temporary solushion).
//        26.04.02    Korobov      enabled REMOTE_HEADER, but its structure is
//                  changed: 10 words for all stations; used for
//                  both directions:
//                    1-st word - PLC ID (last part of IP address)
//                    2-nd word - byte counter
//                    3-10 - reserved.
//                  (DESY version 1.9).
//        23.04.02    Korobov      all stations are independent; ConfCode
//                  reflects 0 station also (0 bit) (DESY version 1.8)
//        17.04.02    Korobov      size of read/write buffers is set in
//                  the PLC configuration file (DESY version 1.7)
//        17.03.02    Korobov      SwapByte argument added to TICP task (DESY version 1.6)
//        15.03.02    Korobov      bug fix: don't use remote header (DESY version 1.6)
//        28.02.02    Korobov      added ICP_start routine (DESY version 1.5)
//        31.01.02    Korobov      default file name for config file (DESY version 1.5)
//        15.01.02    Korobov      changed config file format (DESY version 1.4)
//        11.12.01    Korobov      added mutual exclusion semaphores (DESY version 1.3)
//        20.11.01    Korobov      use PPC board + TPMC880 card (DESY version 1.2)
//        12.10.01    Korobov      provide watch dog of tcp clients; in case
//                  of client suspending delete client task and
//                  then restart corresponding client.
//        10.10.01    Korobov      fixed bug, which caused impossibility
//                  of connection restoring after break.
//        28.09.01    Korobov      added alive counter for all clients
//
//        27.09.01    Korobov      added more elaborated shutdown mechanizm
//                  because initial one didn't work properly.
//
//        26.09.01    Korobov      changed cardNum argument to devName.
//
//        28.08.01    Korobov      attaching of a separate interface is done
//                  after config file reading with IP addresses
//                  of S7 stations; necessary arguments for
//                  the interface attachment should be passed to
//                  the ticp task;
//                  Corrected cleanup hook;
//        12.07.01    Korobov      modified to use in one processor configuration;
//                  with EPICS staff. Shared Memory is allocated
//                  from memory pool. IP addresses of S7 stations
//                  can be taken from config file which name is
//                  defined in ICP_IP_ADDR_FILE_NAME env var.
//
//        14.06.01    Korobov      modified to use in slave CPU;
//                  printf are replaced with logMsg
//
//       08.8.00       EM93  -) - message handler extension:
//                                            - channel,report level and pace chooseable from head station
//                                            - to get consistent message blocks: chooseable pace
//                                                requires block synchronisation
//                                   - expand diagnostic
//                                    - data receive at maximum rate; date send at 1s rate
//
//       07.8.00       EM93  -) test mode: when selected generates test pattern
//
//       06.8.00       EM93  -) reorganisation of shared memory:
//                                        - fix layout suitable for maximum block size
//                                        - data-,status-,up-,down-,general status - seperation
//
//       31.7.00       EM93  -) solution without ringbuffers (renounce data consistency)
//
//       19.6.00       EM93    -) expand to max 16 stations
//                                -) data exchange between ICP and IOC through ringbuffer,access routines
//                                -) include s7- header info (stationlist, data ID, buffer length)
//                                -) introduce report level 0..4 (0:none ... 4: all)
//
//       26.05.2000  Edgar Hasler (eh) ,Joerg Bisang (jb) Zuehlke
//                                 basic communication from SPS S7 to ICP and IOC (one SPS station)
//
//
//
// DESCRIPTION:   example: invoke with:
// taskSpawn "ticpstart",100,0,20000,ticp,"MyDev",3,0,0,"ei","131.169.113.240",0xfffffe00
//                        name            ticpstart
//                        priority         100
//                        options               none
//                        stack size         20000 byte
//                        entry point          ticp
//                        stationlist        0
//                           device name        "MyDev"
//                           Report Level        3
//                        test mode         0
//                        swap bytes        0
//                        interface name        ei
//                        interface address    131.169.113.240
//                        interface mask        0xfffffe00
//===========================================================================*/

/* $Author: zimoch $ */
/* $Date: 2005/03/01 17:33:00 $ */
/* $Id: tiCP.c,v 1.11 2005/03/01 17:33:00 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.11 $ */


/*=============================================================================
// Section:        INCLUDES
// Description: List the include files needed.
//===========================================================================*/

#include <unistd.h>
#include <errlog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#ifdef __vxworks
#include <sockLib.h>
#include <taskLib.h>
#include <taskHookLib.h>
#else
#include <fcntl.h>
#endif

#define BOOL int
#define STATUS int

#include "ticp.h"


typedef struct {
    unsigned ConfCode;
    sm_layout* pSM;
} ticpArgs;

typedef struct TcpClient {
    struct TcpClient* next; /* filled by lineParse */
    int nr_coc;                    /* filled by lineParse */
    char serverIP[20];             /* filled by lineParse */
    int readSize;                  /* filled by lineParse */
    int writeSize;                 /* filled by lineParse */
    char taskName[20];
    int serverPort;
    sm_layout* pSM;
    int sock;
    epicsThreadId tid;
    int restart;
    int sendData;
    int readIndex;
} TcpClient;

TcpClient* firstClient = NULL;

/* run-time debugging printout switch */

int TICP_debug = 0;

/*------------------------------------------*/
/*=============================================================================
// Section:        DATAS
// Description: Definition of global variables.
//===========================================================================*/

static char cvsid[]="$Id: tiCP.c,v 1.11 2005/03/01 17:33:00 zimoch Exp $";

static char rcvBlock[MAX_COC][2*MAX_UP_BUF_SIZE];/* block with assembled rcv data */       

/* delay ticks */
#define DelayTicks10ms  (0.01)         /* calculated Ticks for a wait of 10 [ms]*/
#define DelayTicks50ms  (0.05)         /* calculated Ticks for a wait of 50 [ms]*/
#define DelayTicks60ms  (0.06)         /* calculated Ticks for a wait of 60 [ms]*/
#define DelayTicks500ms (0.50)         /* calculated Ticks for a wait of 500 [ms]*/
#define DelayTicks990ms (0.99)         /* calculated Ticks for a wait of 990 [ms]*/
#define DelayTicks1s    (1.00)         /* calculated Ticks for a wait of 1 [s]*/
#define DelayTicks5s    (5.00)         /* calculated Ticks for a wait of 5 [s]*/

/* message information*/
static BOOL sta_msg_out;                /*  status message output */
static BOOL cmd_msg_out;                /*  command: send new message block */
int     TICP_RepLevel = 4;              /*  report level */
int     TICP_RepStation = 0;            /*  report channel */

/*=============================================================================
// Section:        PROTOTYPES
// Description: Definition of local prototypes.
//===========================================================================*/
void ICP_version();
void ticp (ticpArgs* args);
STATUS tcpClient (TcpClient* client);
BOOL   copyToRcvBlock(TcpClient* client, char * pRcvBuffer, sm_layout* pSM, int iSize);
int    wfTimeoutOrRxdata(int sock, int nr_coc);
int    establishConnection(int sock, char * serverIP, int setverPort);
#ifdef __vxworks
int  cleanup(WIND_TCB *pTcb);
#endif

BOOL   sm_Init(sm_layout* pSM);                                   /* initialize up/dwn data buffer */
BOOL   sm_BufGet(TcpClient* client, char* sendBuf, sm_layout* pSM);/* get block from down buffer */
BOOL   sm_BufPut(TcpClient* client, char* recvBuf, sm_layout* pSM);/* put block to up buffer */
void   ReadDownstream(sm_layout* pSM);

void   UpstreamHeartbeat(sm_layout* pSM);

BOOL   chk_msg_sta(int bsync, int nr_coc, short int rep_lev);        /* check: message output required */
int    lineParse(char *lineBuf, int line);
/*=============================================================================
// Section:        FUNCTIONS
// Description: Implementation of functions.
//===========================================================================*/
/*=============================================================================
// Function:    ICP_version
// Description: prints on console the current version of ICP and CVS data
//===========================================================================*/
void ICP_version()
{
    printf("%s ", cvsid);
    printf("without Remote Header\n");
}

/*=============================================================================
// Function:    ICP_start
// Description: spawns the ICP task with predefined task arguments
//===========================================================================*/
void ICP_start(sm_layout* pSM)        /* allocated memory table */
{
    static ticpArgs args;
    
    args.ConfCode = 0;
    args.pSM = pSM;

    epicsThreadCreate(
        ICP_TASK_NAME,    /* task name */
        MED_PRI,        /* Priority */
        STACK_SIZE,        /* Stack size */
        (EPICSTHREADFUNC)ticp,         /* Entry point */
        &args);
}

/*=============================================================================
// Function:    lineParse
// Description: parsers a line of config file and fills in corresponding element
//        of the client description table.
// Expected line:
// <number>    <IP_address>    <readBlockSize>    [<writeBlockSize>]
//  where: <number> is a station number >= 0, < MAX_COC;
//       <IP_address> - PLC IP address as a string;
//       <readBlockSize> - read (from PLC) block size in bytes;
//       <writeBlockSize> - write (to PLC) block size in bytes (optional).
// Returns:    0 if check OK and fills corresponding element of client
//        description table
//===========================================================================*/
int lineParse(char *lineBuf, int line)
{
    char *rest = NULL, *pIPaddr;
    int indx, IP_part, i, leng, rdSize = 0, wrSize = 0;
    char IPaddr[16];
    TcpClient *client, **pclient; 

    if (TICP_debug)
        errlogPrintf("ticp: lineParse: line='%s'\n", lineBuf);

/* First check station number */

     indx = (int)strtol(lineBuf, &rest, 0);        /* convert 1-st element into int */
    if ((indx < 0) || (indx >= MAX_COC)) {
        errlogPrintf("lineParse: line %d: Invalid station number\n", line);
        return -1;    /* Parse error */
    }
    i = strlen(rest);

    while (i) {
        if (!isalnum((unsigned char)*rest)) {
            rest++;        /* find start of IP address */
            i--;
        }
        else break;
    }
    if (!i) {
        errlogPrintf("lineParse: line %d: No IP address\n", line);
        return -1;    /* Parse error */
    }

    /* Check an IP address validity  */

    pIPaddr = rest;        /* store the IP address pointer */
    for (i=0; i<4; i++) {
        IP_part = (int)strtol(rest, &rest, 0);
        if ((IP_part < 0) || (IP_part > 255)) {
            errlogPrintf("lineParse: line %d: Invalid IP address\n", line);
            return -1;    /* Parse error */
        }
        if (i == 3) {    /* last address element passed */
            leng = (int) (rest - pIPaddr);    /* calculate IP address length */
            if (leng < 16) {

            /* IP address is valid */

                strncpy(IPaddr, pIPaddr, leng);    /* copy IP addr locally */
                IPaddr[leng] = '\0';            /* terminate string */

                if (TICP_debug)
                    errlogPrintf("ticp: lineParse: local IPaddr = '%s'\n",
                        IPaddr);

            }
            else {
                errlogPrintf("lineParse: line %d: Invalid IP address\n", line);
                return -1;            /* too long IP address */
            }
        }
        else {
            if (*rest == '.') rest++;    /* must be '.' separator */
            else {
                errlogPrintf("lineParse: line %d: Invalid IP address\n", line);
                return -1;        /* otherwise parse error */
            }
        }
    }

    /* Check the read/write blocks size */

    rdSize = (int)strtol(rest,&rest, 0);        /* get read Block Size */

    if ((rdSize <= 0) || (rdSize > MAX_UP_BUF_SIZE * 2)) {
        errlogPrintf("ticp: lineParse: invalid rdBlockSize = %d\n", rdSize);
        return -1;
    }

    wrSize = (int)strtol(rest,&rest, 0);
    if ((wrSize < 0) || (wrSize > MAX_DWN_BUF_SIZE * 2)) {
        errlogPrintf("ticp: lineParse: invalid wrBlockSize = %d\n", wrSize);
        return -1;
    }

    if (TICP_debug)
        errlogPrintf("ticp: lineParse: rdSize = %d wrSize = %d\n",
            rdSize, wrSize);

    /* Parsered OK, fill in the client table element */

    client = (TcpClient*)
        callocMustSucceed(1, sizeof(TcpClient), "lineParse");
    
    client->next = NULL;
    client->nr_coc = indx;
    strcpy(client->serverIP, IPaddr);  /* copy PLC IP address */
    client->readSize = rdSize;         /* set read block size */
    client->writeSize = wrSize;        /* set write block size */
    for (pclient = &firstClient; *pclient; pclient = &(*pclient)->next);
    *pclient = client;
    
    if (TICP_debug)
        errlogPrintf("lineParse: set: IP='%s' rdSize=%d wrSize=%d\n",
            client->serverIP,client->readSize, client->writeSize);

    return 0;
}

/*=============================================================================
// Function:    ticp
// Description: main task, allocate shared memory, invokes tasks
//===========================================================================*/

void ticp (ticpArgs* args)
{
    sm_layout* pSM;            /* ptr to shared memory */
    int        idx;
    int        tc;

    char *IPAddrEnv, *ptr;
    static char FileName[80];
    FILE *fdConf;

    char lineBuf[80];
    int IPind, line, nok;
    TcpClient* client;

    /* Try to get stations IP addresses from config file which name is
       defined in ICP_IP_ADDR_FILE_NAME Environment Variable */

    IPAddrEnv = getenv(CONF_FILE_ENV);    /* Search for variable */
    if ((!IPAddrEnv) || (!(*IPAddrEnv)))
       strcpy(FileName, CONF_FILE_NAME);        /* If no or file name is empty use default */
    else
       strcpy(FileName, IPAddrEnv);

    fdConf = fopen(FileName, "r");
    line = 1;
    if (fdConf) {            /* Parse the config file */

        IPind = 0;
        while ((ptr = fgets(lineBuf, 80, fdConf)) != NULL) {
            if (lineBuf[0] == '#') {
                line++;    /* Increment line counter */
                continue;    /* Ignore comment lines */
            }
            nok = lineParse(lineBuf, line);
            if (nok)
                errlogPrintf("ticp: line %d is ignored\n", line);
            else IPind++;        /* Increment IP index */
            line++;        /* Increment line counter */
            if (IPind >= MAX_COC) break;    /* break loop if more
                            than MAX_COC IP addresses */
        }
        fclose(fdConf);
    }
    else {
        errlogPrintf("TICP: can't open config file = '%s': %s\n", FileName, strerror(errno));
        errlogPrintf("TICP: task is stopped\n");
        exit(1);
    }

    if (!firstClient) {
        errlogPrintf("\nTICP: no valid PLC IP found in the configfile; task is stopped\n");
        exit(1);
    }

    sta_msg_out     = 0;
    cmd_msg_out     = 1;
    
    /* initialize shared memory */
    pSM=args->pSM;        /* ptr to shared memory allocated */

    /* initialize shared memory structure */
    for (idx =0;idx <= MAX_COC;idx++) {
        pSM->com_sta.status[H_ADR_ALIVE_CTR + idx] = 0;    /* clear all alive ctr */
    }

    sm_Init(pSM);

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("\nStarting tiCP\n");

    /* check station_list and install communication tasks */
    for (client = firstClient; client; client=client->next)
    {
        client->sendData = (client->writeSize > 0);

        idx = client->nr_coc;
        sprintf (client->taskName, "TcpClient %d", client->nr_coc);
        client->serverPort = SERVER_PORT_NUM;
        client->pSM = pSM;
        client->sock = 0;
        client->tid = epicsThreadCreate(
                                client->taskName,
                                MED_PRI,
                                STACK_SIZE,
                                (EPICSTHREADFUNC)tcpClient,
                                client);
        pSM->com_sta.status[H_ADR_ALIVE_CTR + client->nr_coc + 1] = 1;    /* set alive flag */

        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP: task %s started.\n", client->taskName);

        epicsThreadSleep(DelayTicks5s);        /* delay for 5 sec */
    }

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP: all tasks started.\n");

    while (1)
    {   /* loop forever */
        cmd_msg_out  = 1;
        for(tc = 0;tc < 5;tc++)
        {
            UpstreamHeartbeat(pSM);
            for (client = firstClient; client; client=client->next)
            {
                if (client->writeSize)
                { /* Set 1 only for those stations that
                    have send down data */
                    client->sendData = 1;
                }
            }
            epicsThreadSleep(DelayTicks500ms);
        }

        /* check whether all clients are running */

        for (client = firstClient; client; client=client->next)
        {
            if (client->tid)
            {    /* only for existing clients */
                if (TICP_debug)
                    errlogPrintf("ICP: idx=%d IP='%s'\n",
                        client->nr_coc, client->serverIP);

                if (epicsThreadIsSuspended(client->tid))
                {    /* if suspended delete it */
                    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    {
                        errlogPrintf("ICP: task '%s' is suspended, try to delete it.\n",
                            client->taskName);
                        epicsThreadSleep(DelayTicks50ms);
                    }

                    /* Mark the appropriate connection NOK & clear alive flag */

                    pSM->com_sta.status[H_ADR_COC_STATUS + client->nr_coc] = CONNECTION_NOK; /* mark connection status NOK */
                    pSM->com_sta.status[H_ADR_ALIVE_CTR + client->nr_coc + 1] = 0;    /* clear alive flag */

#ifdef __vxworks
                    if (taskDelete((int)client->tid))
                    {
                        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ERR))
                            errlogPrintf("ICP: suspended '%s' cannot be deleted.\n",
                                client->taskName);

                    }
                    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    {
                        errlogPrintf("ICP: task '%s' is deleted try to restart it.\n",
                            client->taskName);
                        epicsThreadSleep(DelayTicks50ms);
                    }
#endif
                    client->restart = 1;
                    client->tid = 0;        /* clear client tid */
                }
            }
        }

        /* check is there clients to restart */

        for (client = firstClient; client; client=client->next)
        {    /* restart the clients having restart flag set */
            if (client->restart)
            {
            /* !!!! It should be changed if several ticp can be started */

                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP: restarting '%s' task: IP='%s'.\n",
                        client->taskName, client->serverIP);
                client->tid = epicsThreadCreate(
                    client->taskName,
                    MED_PRI,
                    STACK_SIZE,
                    (EPICSTHREADFUNC)tcpClient,
                    client);

                pSM->com_sta.status[H_ADR_ALIVE_CTR + client->nr_coc + 1] = 1;  /* set alive flag */

                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP: task %d restarted.\n", client->nr_coc);

                client->restart = 0;    /* reset restart flag */
            }
        }
    }
}
/*=============================================================================
// Function:    tcpClient
// Description: handle the client part of the socket data transmission with the
// Siemens S7 PLC
//===========================================================================*/

STATUS tcpClient (TcpClient* client)
{
    char   sendBuf[2*(MAX_DWN_BUF_SIZE+1)];    /* send buffer */
    struct sockaddr_in clientAddr;        /* clients socket address */
    char   recvBuf[2*(MAX_UP_BUF_SIZE+1)];        /* receive buffer */
    int    idx;

    char *serverIP   = client->serverIP;
    int serverPort = client->serverPort;
    sm_layout* pSM   = client->pSM;
    int nr_coc       = client->nr_coc;

    client->sock=0;
    for (idx=0;idx<2*MAX_DWN_BUF_SIZE;idx++) {
        sendBuf[idx]=0x00;
    }

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("\nICP %d: Starting TcpClient. Server: %s:%i\n",
            nr_coc, serverIP, serverPort);
    if (serverPort==0)
        serverPort=SERVER_PORT_NUM;            /* default */
    else
        serverPort=serverPort;
    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP %d: using Server Port %i\n", nr_coc, serverPort);

#ifdef __vxworks
    taskDeleteHookAdd(cleanup);            /* install task shutdown cleanup routine */
#endif

    while (1)                                        /* forever */
    {
        int connectionOk = 0;
        int iConnectLoop = 0;
        int len = sizeof (struct sockaddr_in);
        int iCommLoop = 0;

        /* establish connection with server */
        client->readIndex = 0;
        do
        {
            /* create client socket */
            if ((client->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                errlogPrintf("ICP %d: FATAL ERROR! socket(AF_INET, SOCK_STREAM, 0) failed: %s\n",
                    nr_coc, strerror(errno));
                client->sock = 0;
                return -1;
            }
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP %d: Connect to %s:%d on socket %d # %i...\n",
                    nr_coc, serverIP, serverPort, client->sock, iConnectLoop);
            if (establishConnection(client->sock, serverIP, serverPort) < 0)
            {
                errlogPrintf("ICP %d: connect(%d, %s:%d) failed: %s. Retry in %d seconds\n",
                    nr_coc, client->sock, serverIP, serverPort, strerror(errno), RECONNECT_DELAY);
                if (close(client->sock))
                    errlogPrintf("ICP %d: close(%d) failed (ignored): %s\n",
                        nr_coc, client->sock, strerror(errno));
                client->sock=0;
                epicsThreadSleep(RECONNECT_DELAY);
            }
            else
            {
                connectionOk=1;
                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP %d: connected fd %d to %s:%d\n",
                        nr_coc, client->sock, serverIP, serverPort);
            }
            iConnectLoop++;
        } while (!connectionOk);

        /***** connection with server established; do data transfer *****/

        if (getsockname(client->sock, (struct sockaddr *)&clientAddr, &len) < 0)
        {
            errlogPrintf("ICP %d: getsockname(%d,...) failed: %s\n",
                nr_coc, client->sock, strerror(errno));
        }
        else
        {
            if(chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP %d: connection established. Client port: %i\n",
                    nr_coc, ntohs(clientAddr.sin_port));
            pSM->com_sta.status[H_ADR_COC_STATUS + nr_coc] = CONNECTION_OK;
        }

        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
            errlogPrintf("tcpClient %d: before 1-st read: IP='%s' port=%d\n",
                nr_coc, serverIP, serverPort);

        while (1)                            /* send/receive loop */
        {
            int iNumRead;
            int iNumWritten;
            int iRC;

            /* check: time for send new down data block ? */
            if (client->sendData)
            {

                client->sendData = 0;

                /* block the corresponding area of SM */
                epicsMutexMustLock(pSM->semID_dwn[nr_coc]);

                sm_BufGet(client, sendBuf, pSM);

                /* unblock the corresponding area of SM */
                epicsMutexUnlock(pSM->semID_dwn[nr_coc]);

                /* send data to server (S7) */
                if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                    errlogPrintf("ICP %d: Message #%i to send: \n",
                        nr_coc, iCommLoop);

                if ((iNumWritten=send(client->sock, sendBuf, (client->writeSize), 0)) < 0)
                {
                    errlogPrintf("ICP %d: send(%d, ..., %d, 0) failed: %s\n",
                        nr_coc, client->sock, client->writeSize, strerror(errno));
                    break;            /* exit the send/receive loop */
                }
                if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                    errlogPrintf("ICP %d: Message of length %i sent\n",
                        nr_coc, iNumWritten);
            }

            /* check (with timeout) for data arrival from server */

            iRC=wfTimeoutOrRxdata(client->sock, nr_coc);
            if (iRC <= 0)
            { /* error or no data within timeout timeframe */
                errlogPrintf("ICP %d: wfTimeoutOrRxdata failed: %s\n",
                    nr_coc, strerror(errno));
                break;            /* exit the send/receive loop */
            }
            /* data available; read data from server (S7) */

            if ((iNumRead=recv(client->sock, recvBuf, client->readSize, 0)) < 0)
            {
                errlogPrintf("ICP %d: recv(%d, ..., %d, 0) failed: %s\n",
                    nr_coc, client->sock,
                    client->readSize, strerror(errno));
                break;            /* exit the send/receive loop */
            }
            if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                errlogPrintf ("ICP %d: Message received (length %i)\n",
                    nr_coc, iNumRead);

            if (iNumRead > 0) {        /* some data (>0 bytes) received */

                /* block the corresponding area of SM */
                epicsMutexMustLock(pSM->semID_up[nr_coc]);

                /* copy received data to data block */

                if (copyToRcvBlock(client, recvBuf, pSM, iNumRead))
                {
                    /* block end reached ? sync end message block */
                    if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                        if (chk_msg_sta(RBE, nr_coc, REPORT_DIAG))
                            errlogPrintf("ICP %d: <<<<<<<<<<<<< \n", nr_coc);
                    iCommLoop++;
                    /* sync begin new message block */
                    if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                        if (chk_msg_sta(RBB, nr_coc, REPORT_DIAG))
                            errlogPrintf("ICP %d: >>>>>>>>>>>>> Loop: %d\n", nr_coc, iCommLoop);
                }

                /* unblock the corresponding area of SM */
                epicsMutexUnlock(pSM->semID_up[nr_coc]);
            }

            /* wait some time (this time determines the xmit/rcv frequency) */

            epicsThreadSleep(DelayTicks10ms);


        }/* end send/receive loop */

        /* connection not ok; shut it down */

        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ERR))
            errlogPrintf("ICP %d: connection not ok; shut it down and retry in %d seconds\n",
                nr_coc, RECONNECT_DELAY);
        pSM->com_sta.status[H_ADR_COC_STATUS + nr_coc] = CONNECTION_NOK;
        if (shutdown(client->sock, 2) < 0)
            errlogPrintf("ICP %d: shutdown(%d, 2) failed (ignored): %s\n",
                nr_coc, client->sock, strerror(errno));
        if (close (client->sock) < 0)
            errlogPrintf("ICP %d: close(%d) failed (ignored): %s\n",
                nr_coc, client->sock, strerror(errno));
        client->sock=0;

        /* wait some time (allow PLC to disconnect and get ready for new connection) */
        /* PLC drops connection after approx. 10 sec, so wait some longer time here */
        epicsThreadSleep(RECONNECT_DELAY);
        /* connection is shut down; try to re-establish */
    }/* forever */

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP %d: Correctly exiting TcpClient application.\n",
            nr_coc);
    exit(0);    /* Korobov: return OK; causes Illegal instruction
                exeption */
}

/*=============================================================================
// Function:    copyToRcvBlock
// Description: copy an entire data block from the internal buffer to the receive block
// returncode:  none
//===========================================================================*/
BOOL copyToRcvBlock(TcpClient* client, char * pRcvBuffer, sm_layout* pSM, int iSize)
{
    int i;
    BOOL block_finished = 0;

    /* copy all received data in block */
    for (i=0; i<iSize; i++) {
        /* copy to shared memory */
        rcvBlock[client->nr_coc][client->readIndex]=*pRcvBuffer;
        pRcvBuffer++;                        /* next received byte */
        client->readIndex=(client->readIndex+1) % (client->readSize);    /* next entry */
        if (client->readIndex==0)
        {        /* block end reached */
            while (!sm_BufPut(client, rcvBlock[client->nr_coc], pSM));
            block_finished = 1;
        }
    }
    return block_finished;
}

/*=============================================================================
// Function:    wfTimeoutOrRxdata
// Description: wait for Rx data on socket or timeout
// returncode:  0 if timeout; <0 if error; >0 if data
//===========================================================================*/
int wfTimeoutOrRxdata(int sock, int nr_coc)
{
    static struct timeval    to;
    int    iSelect;
    fd_set socklist;

    FD_ZERO(&socklist);
    FD_SET(sock,&socklist);
    to.tv_sec=RECV_TIMEOUT;
    to.tv_usec=0;
    /* select returns when either the socket has data or the timeout elapsed */
    while ((iSelect=select(sock+1,&socklist, 0, 0,&to)) < 0)
    {
        if (errno != EINTR)
        {
            errlogPrintf("ICP %d: select() failed in wfTimeoutOrRxdata: %s\n",
                nr_coc, strerror(errno));
            return -1;
        }
    }
    if (iSelect==0)            /* timed out */
    {
        if(chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))
            errlogPrintf("ICP %d: select() timed out.\n", nr_coc);
    }
    return iSelect;
}

/*=============================================================================
// Function:    establishConnection
// Description: establish permanent (TCP) connection with Server
// returncode:  0 if ok; -1 if error
//===========================================================================*/
int establishConnection(int sock, char * serverIP, int serverPort)
{
    struct sockaddr_in    serverAddr;    /* server socket address */
    /* set the size of the socket level receive buffer */
    int rbuflen = SOCK_RECV_BUFFER_SIZE;
    struct timeval    to;
#ifndef __vxworks
    long opt;
#endif

    if (TICP_debug)
        errlogPrintf("establishConnection: fd=%d, IP=%s port=%d\n", sock, serverIP, serverPort);

      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rbuflen, sizeof(rbuflen)) < 0)
    {
        perror("setsockopt(..., SO_RCVBUF) failed; ignore.");
    }
    /* build server socket address */
    bzero((char *) &serverAddr, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);

    /* connect to server */
    to.tv_sec=CONNECT_TIMEOUT;       /* specify timeout */
    to.tv_usec=0;
#ifdef __vxworks
    if (connectWithTimeout(sock,
        (struct sockaddr *) &serverAddr, sizeof (serverAddr), &to) < 0)
    {
        errlogPrintf("ICP: connectWithTimeout(%d,...) failed: %s\n",
            sock, strerror(errno));
        return -1;
    }
#else
    /* emulate connectWithTimeout (D.Z) */

    /* connect in non-blocking mode */
    if((opt = fcntl(sock, F_GETFL, NULL)) < 0)
    {
        errlogPrintf("ICP: cntl(%d, F_GETFL, NULL) failed: %s\n",
            sock, strerror(errno));
        return -1;
    }
    opt |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opt) < 0)
    {
        errlogPrintf("ICP: fcntl(%d, F_SETFL, O_NONBLOCK) failed: %s\n",
            sock, strerror(errno));
        return -1;
    }
    if (connect(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            /* start timeout */
            long status;
            socklen_t lon = sizeof(status);
            fd_set fdset;

            FD_ZERO(&fdset);
            FD_SET(sock, &fdset);
            /* wait for connection */
            while ((status = select(sock+1, NULL, &fdset, NULL, &to)) < 0)
            {
                if (errno != EINTR)
                {
                    errlogPrintf("ICP: select() failed in connectWithTimeout: %s\n",
                        strerror(errno));
                    return -1;
                }
            }
            if (status == 0)
            {
                errlogPrintf("ICP: select() timed out in connectWithTimeout\n");
                errno = ETIMEDOUT;
                return -1;
            }
            /* get background error status */
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &status, &lon) < 0)
            {
                errlogPrintf("ICP: getsockopt(%d,...) failed in connectWithTimeout: %s\n",
                    sock, strerror(errno));
                return -1;
            }
            if (status)
            {
                errno = status;
                errlogPrintf("ICP: background connect(%d,...) failed in connectWithTimeout: %s\n",
                    sock, strerror(errno));
                return -1;
            }
        }
        else
        {
            errlogPrintf("ICP: connect(%d,...) failed in connectWithTimeout: %s\n",
                sock, strerror(errno));
            return -1;
        }
    }
    /* connected */
    opt &= ~O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opt) < 0)
    {
        errlogPrintf("ICP: fcntl(%d, F_SETFL, ~O_NONBLOCK) failed: %s\n",
            sock, strerror(errno));
        return -1;
    }
#endif
    return 0;
}

/*=============================================================================
// Function:    cleanup
// Description: cleanup at task deletion
// (socket is not closed automatically when the task is killed; therefore do it explicitely here)
// returncode:  none
//===========================================================================*/
#ifdef __vxworks
int cleanup(WIND_TCB *pTcb)
{
    TcpClient* client;

    /* close the connection if any */
    for (client = firstClient; client; client=client->next)
    {
        if (pTcb == (WIND_TCB *)client->tid)
        {
            if (TICP_debug)
                errlogPrintf("cleanup: tid = %p nr_coc = %d\n",
                    pTcb, client->nr_coc);

            client->tid = 0;        /* clear saved tid */
            client->restart = 1;    /* set "restart client" flag */
            if (client->sock)
                close(client->sock);    /* close connection */
            break;
        }
    }
    return 0;
}
#endif

/*=============================================================================
// Function:        sm_Init
// Description:     initialize up and down buffer
//===========================================================================*/

BOOL sm_Init(sm_layout* pSM)
{
    int idx;
    int nr_coc;

    for (nr_coc =0;nr_coc < MAX_COC;nr_coc++) {
        /* create test pattern */
        for (idx = 0; idx < 2*MAX_DWN_BUF_SIZE; idx++) {
            pSM->buf_dwn[nr_coc].data[idx] = (int)0;
        }
        for (idx = 0; idx < 2*MAX_UP_BUF_SIZE; idx++) {
            pSM->buf_up[nr_coc].data[idx] = (int)0;
        }

    }
    return 1;
}

/*=============================================================================
// Function:        sm_BufGet
// Description:     get block from down buffer
//===========================================================================*/

BOOL sm_BufGet(TcpClient* client, char* sendBuf, sm_layout* pSM)
{
    int nr_coc = client->nr_coc;

    /* copy to A32 slave memory */
/*  bcopyBytes((char *)pSM->buf_dwn[nr_coc].data, sendBuf,(block_length_dwn[nr_coc])); */
    memcpy(sendBuf, pSM->buf_dwn[nr_coc].data, client->writeSize);
    if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP %d: DWN: length: %d\n",
                nr_coc, client->writeSize);

    if ((TICP_debug) && (TICP_debug == (nr_coc+1))) {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP %d: DWN: data:\n", nr_coc);
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                pSM->buf_dwn[nr_coc].data[0],
                pSM->buf_dwn[nr_coc].data[1],
                pSM->buf_dwn[nr_coc].data[2],
                pSM->buf_dwn[nr_coc].data[3],
                pSM->buf_dwn[nr_coc].data[4],
                pSM->buf_dwn[nr_coc].data[5]);
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                pSM->buf_dwn[nr_coc].data[6],
                pSM->buf_dwn[nr_coc].data[7],
                pSM->buf_dwn[nr_coc].data[8],
                pSM->buf_dwn[nr_coc].data[9],
                pSM->buf_dwn[nr_coc].data[10],
                pSM->buf_dwn[nr_coc].data[11]);
    }

    return 1;
}

/*=============================================================================
// Function:        sm_BufPut
// Description:     put block to up buffer
//===========================================================================*/
BOOL sm_BufPut(TcpClient* client, char* recvBuf, sm_layout* pSM)
{
    int nr_coc = client->nr_coc;

    /* copy to A32 slave memory */

/*  bcopyBytes(recvBuf,(char *)pSM->buf_up[nr_coc].data, block_length_up[nr_coc]); */
    memcpy(pSM->buf_up[nr_coc].data, recvBuf, client->readSize);

    if ((TICP_debug) && (TICP_debug == (nr_coc+1))) {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP %d: UP: length: %d\n",
        nr_coc, client->readSize);

        if (chk_msg_sta(RBN, nr_coc, REPORT_DIAG))
            errlogPrintf("ICP %d: sm_BufPut::com_status::alive_ctr: %4d\n",
                nr_coc, pSM->com_sta.status[H_ADR_ALIVE_CTR]);

        if (chk_msg_sta(RBN, nr_coc, REPORT_DIAG))
            errlogPrintf("ICP %d: sm_BufPut::com_status::alive_ctr[%d]: %4d\n",
                nr_coc, nr_coc, pSM->com_sta.status[H_ADR_ALIVE_CTR+nr_coc+1]);

        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                pSM->buf_up[nr_coc].data[0],
                pSM->buf_up[nr_coc].data[1],
                pSM->buf_up[nr_coc].data[2],
                pSM->buf_up[nr_coc].data[3],
                pSM->buf_up[nr_coc].data[4],
                pSM->buf_up[nr_coc].data[5]);
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                pSM->buf_up[nr_coc].data[6],
                pSM->buf_up[nr_coc].data[7],
                pSM->buf_up[nr_coc].data[8],
                pSM->buf_up[nr_coc].data[9],
                pSM->buf_up[nr_coc].data[10],
                pSM->buf_up[nr_coc].data[11]);
    }
    return 1;
}

/*=============================================================================
// Function:    UpstreamHeartbeat
// Description: increment upstream heartbeat ctr (tell partner that I am alive)
//===========================================================================*/
void UpstreamHeartbeat(sm_layout* pSM)
{
    short localUheartbeat;

#if 0
    bcopy((char*)(&((*pSM).com_sta.status[H_ADR_ALIVE_CTR])),(char*)&localUheartbeat, sizeof(short)); /* get current heartbeat ctr */
#endif
    localUheartbeat = pSM->com_sta.status[H_ADR_ALIVE_CTR];
    localUheartbeat++;

#if 0
    bcopy((char*)&localUheartbeat,(char*)(&((*pSM).com_sta.status[H_ADR_ALIVE_CTR])), sizeof(short)); /* set current heartbeat ctr */
#endif
    pSM->com_sta.status[H_ADR_ALIVE_CTR] = localUheartbeat;

}

/*=============================================================================
// Function:        ReadDownstream
// Description:     Read and display Downstream
//===========================================================================*/
void    ReadDownstream(sm_layout* pSM)
{
    int nr_coc;
    int idx;

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))printf("ICP: ReadDownstream:: \n");

    for(nr_coc =0;nr_coc <MAX_COC;nr_coc++) {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))printf("ICP%2d> DWN : data ", nr_coc);
        for (idx =0;idx < 16;idx ++) {
                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))printf(" %4X", pSM->buf_dwn[nr_coc].data[idx]);
        }
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))printf("\n");
    }
}
/*=============================================================================
// Function:        check_msg_sta
// Description:     check if message out required
//===========================================================================*/
BOOL chk_msg_sta(int bsync, int nr_coc, short int rep_lev)
{
    if ((nr_coc == REP_ALL_COC)&&(TICP_RepLevel >= rep_lev)) return 1;

    if ((bsync == RBB) && (cmd_msg_out)&& (nr_coc == TICP_RepStation)) {
        cmd_msg_out  = 0;
        sta_msg_out  = 1;
    }

    if (!sta_msg_out) return 0;

    if ((bsync == RBE) && (sta_msg_out)&& (nr_coc == TICP_RepStation))sta_msg_out  = 0;

    if ((nr_coc != TICP_RepStation)&&(nr_coc != REP_ALL_COC)) return 0;
    if (TICP_RepLevel < rep_lev) return 0;
    return 1;
}

int ticpReport()
{    
    TcpClient* client;
    
    for (client = firstClient; client; client=client->next)
    {
        printf ("  %d: server=%s:%d, fd=%d\n",
            client->nr_coc, client->serverIP, client->serverPort, client->sock);
    }
    return 0;
}

/*=============================================================================
// ======================== Bottom of C-File ==================================
//===========================================================================*/

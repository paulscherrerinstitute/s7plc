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
/* $Date: 2005/02/14 13:27:41 $ */
/* $Id: tiCP.c,v 1.2 2005/02/14 13:27:41 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.2 $ */


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
#endif

#define BOOL int
#define STATUS int

#include "ticp.h"

#define TRUE 1
#define FALSE 0

typedef struct {
    unsigned ConfCode;
    sm_layout* pSM;
    unsigned GlbRepLev;
    unsigned GlbTestMode;
    unsigned GlbSwapBytes;
} ticpArgs;

typedef struct {
    char *serverIP;
    int serverPortin;
    sm_layout* pSM;
    int nr_coc;
} tcpClientArgs;

/* run-time debugging printout switch */

int TICP_debug = 0;

static char RemoteHeader = FALSE;    /* default version is without remote header */

/*------------------------------------------*/
/*=============================================================================
// Section:        DATAS
// Description: Definition of global variables.
//===========================================================================*/

static char cvsid[]="$Id: tiCP.c,v 1.2 2005/02/14 13:27:41 zimoch Exp $";

static int    sFd[16];                        /* socket File descriptors */

static char rcvBlock[MAX_COC][2*MAX_UP_BUF_SIZE];/* block with assembled rcv data */       
static int  iNextrcvBlockEntry[MAX_COC];        /* where to write next in block */         
static int  iRcvBlockCtr[MAX_COC];                /* received application data blocks */   
static int  TestMode;                    /* create test patterns */                        
static BOOL finalize;                    /* Flag: finalize ticp */                         
/*static int  dummy = 0;  DZ */  /* because anyhow (?) when touched cmd_send_dwn it
                            changed flag finalize */
static BOOL cmd_send_dwn[MAX_COC];    /*  command: send new data block down*/
/* delay ticks */
#define DelayTicks10ms  (0.01)         /* calculated Ticks for a wait of 10 [ms]*/
#define DelayTicks50ms  (0.05)         /* calculated Ticks for a wait of 50 [ms]*/
#define DelayTicks60ms  (0.06)         /* calculated Ticks for a wait of 60 [ms]*/
#define DelayTicks500ms (0.50)         /* calculated Ticks for a wait of 500 [ms]*/
#define DelayTicks990ms (0.99)         /* calculated Ticks for a wait of 990 [ms]*/
#define DelayTicks1s    (1.00)         /* calculated Ticks for a wait of 1 [s]*/
#define DelayTicks5s    (5.00)         /* calculated Ticks for a wait of 5 [s]*/

/* header information */
static int block_length_up[MAX_COC];
static int block_length_dwn[MAX_COC];
static BOOL SwapByte;
static BOOL XferChangedOnly;
static int StationList;
static BOOL SmInitialized ;
static unsigned  RConfCode;

/* message information*/
static BOOL sta_msg_out;                /*  status message output */
static BOOL cmd_msg_out;                /*  command: send new message block */
static short int     RepLev;            /*  report level */
static short int     RepCoc;            /*  report channel */

/* static epicsThreadId tidICP = 0;    DZ*/       /* tid of ICP itself */
static epicsThreadId tidClient[MAX_COC] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};    /* Array
                                of client tid to be used during shutdown */
static short clientRestart[MAX_COC] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};    /* Array of restart flags for clients */
static dCoC CoC[] = {
        {0,"TcpClient 0","", 0, 0, 0},
        {0,"TcpClient 1","", 0, 0, 0},
        {0,"TcpClient 2","", 0, 0, 0},
        {0,"TcpClient 3","", 0, 0, 0},
        {0,"TcpClient 4","", 0, 0, 0},
        {0,"TcpClient 5","", 0, 0, 0},
        {0,"TcpClient 6","", 0, 0, 0},
        {0,"TcpClient 7","", 0, 0, 0},
        {0,"TcpClient 8","", 0, 0, 0},
        {0,"TcpClient 9","", 0, 0, 0},
        {0,"TcpClient 10","", 0, 0, 0},
        {0,"TcpClient 11","", 0, 0, 0},
        {0,"TcpClient 12","", 0, 0, 0},
        {0,"TcpClient 13","", 0, 0, 0},
        {0,"TcpClient 14","", 0, 0, 0},
        {0,"TcpClient 15","", 0, 0, 0}
};

/*=============================================================================
// Section:        PROTOTYPES
// Description: Definition of local prototypes.
//===========================================================================*/
void ICP_version();
void ticp (ticpArgs* args);
STATUS tcpClient (tcpClientArgs* args);
BOOL   copyToRcvBlock(char * pRcvBuffer, sm_layout* pSM, int iSize, int nr_coc);
int    wfTimeoutOrRxdata(int sockD, int nr_coc);
int    establishConnection(int sockD, char * serverIP, int setverPort);
/*int  cleanup(WIND_TCB *pTcb);*/

BOOL   sm_Init(sm_layout* pSM);                                   /* initialize up/dwn data buffer */
BOOL   sm_BufGet(char* sendBuf, sm_layout* pSM, int nr_coc);/* get block from down buffer */
BOOL   sm_BufPut(char* recvBuf, sm_layout* pSM, int nr_coc);/* put block to up buffer */
void   CreateTestPattern(sm_layout* pSM, int tst_cntr);          /* initialize up data buffer */
void   ReadDownstream(sm_layout* pSM);

void   ByteSwap(char* Buf, int size);                                /* exchange high and low byte */
void   UpstreamHeartbeat(sm_layout* pSM);

BOOL   chk_msg_sta(int bsync, int nr_coc, short int rep_lev);        /* check: message output required */
int    lineParse(char *lineBuf, int line, dCoC *pCoC);
short  bcd2d(short *pbcd, short *pto);        /* convert BCD to decimal */
short  d2bcd(short *pdec, short *pto);        /* convert decimal to BCD */
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
    if (RemoteHeader)
        printf("with Remote Header\n");
    else
        printf("without Remote Header\n");
}

/*=============================================================================
// Function:    bcd2d
// Description: converts short word from BCD encoding to decimal
//===========================================================================*/
short bcd2d(short *pbcd, short *pto)
{
    short local;

    local = *pbcd & 0xF;
    local += ((*pbcd >> 4) & 0xF)*10;
    local += ((*pbcd >> 8) & 0xF)*100;
    *pto = local;
    return local;
}
/*=============================================================================
// Function:    d2bcd
// Description: converts short word from decimal encoding to BCD (only for positive numbers)
//              and writes via pointer
// Returns (-1) if conversion is impossible (e.i. 0 > dec > 999)
//===========================================================================*/
short d2bcd(short *pdec, short *pto)
{
    int local, dec = *pdec, tmp;

    if ((dec < 0) || (dec > 999)) return -1;
    tmp = dec/100;
    dec = dec - tmp*100;    /* store the remainder */
    local = tmp << 8;    /* store a number of 100s */

    tmp = dec/10;    /* get a number of 10s */
    dec = dec-tmp*10;    /* new remainder */
    local = local | (tmp << 4);    /* add a number of 10 */

    local += dec;            /* result */

    *pto = (short)local;
    return (short)local;
}

/*=============================================================================
// Function:    ICP_start
// Description: spawns the ICP task with predefined task arguments
//===========================================================================*/
void ICP_start(sm_layout* pSM,        /* allocated memory table */
               unsigned GlbSwapBytes, /* swap bytes flag */
               unsigned RepLev,       /* report level */
               char RemHead)          /* with/withou remote header */
{
    static ticpArgs args;
    RemoteHeader = (RemHead)? TRUE: FALSE;
    

#if 0
    taskSpawn(ICP_TASK_NAME,    /* task name */
        MED_PRI,        /* Priority */
        0,            /* Option */
        STACK_SIZE,        /* Stack size */
        ticp,        /* Entry point */
        0,            /* Station list (obsolete, replaced with conf file) */
        (int)devName,    /* Device name */
        RepLev,        /* Report Level */
        0,            /* Test mode */
        GlbSwapBytes,    /* Swap bytes flag */
        (int)ifName,    /* Interface name */
        (int)ifAddr,    /* Interface address */
        ifMask,        /* Interface mask */
        0, 0);
#endif
    args.ConfCode = 0;
    args.pSM = pSM;
    args.GlbRepLev = RepLev;
    args.GlbTestMode = 0;
    args.GlbSwapBytes = GlbSwapBytes;

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
int lineParse(char *lineBuf, int line, dCoC *pCoC)
{
    char *rest = NULL, *pIPaddr;
    int indx, IP_part, i, leng, rdSize = 0, wrSize = 0;
    char IPaddr[16];
    short id = -1;
    dCoC *pLoc = pCoC;

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

                id = IP_part,            /* store it as PLC id */
                strncpy(IPaddr, pIPaddr, leng);    /* copy IP addr locally */
                IPaddr[leng] = '\0';            /* terminate string */

                if (TICP_debug)
                    errlogPrintf("ticp: lineParse: local IPaddr = '%s' id = %d\n",
                        IPaddr, id);

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

    if (indx) {                   /* rewrite RConfCode */
        RConfCode |= 1 << indx;
    }
    else RConfCode |= 1;          /* If index = 0 */
    if (indx) {
        while (indx--) pLoc++;
    }
    strcpy(pLoc->ip_adr, IPaddr);  /* copy PLC IP address */
    pLoc->rdBlSize = rdSize;      /* set read block size */
    pLoc->wrBlSize = wrSize;      /* set write block size */
    pLoc->id = id;                /* set PLC ID */

    if (TICP_debug)
        errlogPrintf("lineParse: set: task='%s' IP='%s' rdSize=%d wrSize=%d ID=%d conf=0x%x\n",
            pLoc->task_name, pLoc->ip_adr, pLoc->rdBlSize, pLoc->wrBlSize, pLoc->id, RConfCode);

    return 0;
}

/*=============================================================================
// Function:    ticp
// Description: main task, allocate shared memory, invokes tasks
//===========================================================================*/
void ticp (ticpArgs* args)
{
    sm_layout* pSM;            /* ptr to shared memory */
    unsigned   ConfCodeMask;
    int        idx;
    int        tst_cntr;
    int        tc;

    char *IPAddrEnv, *ptr;
    static char FileName[80];
    FILE *fdConf;

    char lineBuf[80];
    int IPind, line, nok;
    static tcpClientArgs clientArgs [MAX_COC];

    RConfCode = args->ConfCode;    /* if there is config file it override ConfCode */

    /* Try to get stations IP addresses from config file which name is
       defined in ICP_IP_ADDR_FILE_NAME Environment Variable */

    IPAddrEnv = getenv(CONF_FILE_ENV);    /* Search for variable */
    if ((!IPAddrEnv) || (!(*IPAddrEnv)))
       strcpy(FileName, CONF_FILE_NAME);        /* If no or file name is empty use default */
    else
       strcpy(FileName, IPAddrEnv);

    fdConf = fopen(FileName, "r");
    line = 1; RConfCode = 0;    /* RConfCode will be overwritten */
    if (fdConf) {            /* Parse the config file */

        IPind = 0;
        while ((ptr = fgets(lineBuf, 80, fdConf)) != NULL) {
            if (lineBuf[0] == '#') {
                line++;    /* Increment line counter */
                continue;    /* Ignore comment lines */
            }
            nok = lineParse(lineBuf, line, CoC);
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

    if (!RConfCode) {
        errlogPrintf("\nTICP: no valid PLC IP found in the configfile; task is stopped\n");
        exit(1);
    }

    RepLev = args->GlbRepLev;
    TestMode = args->GlbTestMode;

    for (idx =0;idx < MAX_COC;idx++) {
        block_length_up[idx]=(CoC[idx].rdBlSize);    /* set in bytes */
        block_length_dwn[idx]=(CoC[idx].wrBlSize);
    }

    if (!block_length_up[0]) {
        errlogPrintf("\nTICP: block length is 0; task is stopped\n");
        exit(1);
    }

    SwapByte        = args->GlbSwapBytes;
    XferChangedOnly = FALSE;
    StationList     = 0;
    SmInitialized   = FALSE;
    sta_msg_out     = FALSE;
    cmd_msg_out     = TRUE;
    
    for (idx =0;idx < MAX_COC;idx++) {
        if (block_length_dwn[idx])        /* set TRUE only if needed to send down */
        cmd_send_dwn[idx] = TRUE;
    }

    /* initialize shared memory */
    pSM=args->pSM;        /* ptr to shared memory allocated */

    /* initialize shared memory structure */
    for (idx =0;idx <= MAX_COC;idx++) {
        pSM->com_sta.status[H_ADR_ALIVE_CTR + idx] = 0;    /* clear all alive ctr */
    }

    /* Test without s7 stations */
    tst_cntr = 0;
    if (TestMode != 0) {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("\nStarting tiCP T E S T ::TestMode:%X sm_adr:%p\n",
                TestMode, pSM);
        while (tst_cntr < 1) {
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("\n  tiCP T E S T Create Test Pattern ::tst_cntr:%d \n",
                    tst_cntr);
            CreateTestPattern(pSM, tst_cntr);
            ReadDownstream(pSM);
            epicsThreadSleep(DelayTicks1s);      /* wait 1s */
            UpstreamHeartbeat(pSM);
            tst_cntr++;
        }
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("\nEnding tiCP T E S T .\n");
        exit(0);
    }
    else {
        sm_Init(pSM);
    }

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("\nStarting tiCP ConfCode:%X\n", RConfCode);

    /* start communication with master station */

#if 0    /* Korobov: all stations are independent */

    CoC[0].task_id = taskSpawn(CoC[0].task_name,
                    MED_PRI,
                    0,
                    STACK_SIZE,
                    tcpClient,
                    (int)CoC[0].ip_adr,
                    SERVER_PORT_NUM,
                    (int)pSM,
                    0, 0, 0, 0, 0, 0, 0);
    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))errlogPrintf("ICP:> task 0 started.\n");
    tidClient[0] = CoC[0].task_id;
    pSM->com_sta.status[H_ADR_ALIVE_CTR + 1] = TRUE;    /* set alive
                                flag for the client 0 */

    /* wait until first data received */

    msg_sent_once = FALSE;
    while (SmInitialized == FALSE) {
        if (msg_sent_once == FALSE) {

            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:>  wait until shared memory initialized .\n");
            msg_sent_once = TRUE;
        }

        UpstreamHeartbeat(pSM);

        epicsThreadSleep(DelayTicks990ms);
    }
    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP:> shared memory initialized.\n");

#endif

    /* check station_list and install communication tasks */
    ConfCodeMask = 1;
    StationList = RConfCode;
    for (idx = 0;idx < MAX_COC;idx++) {    /* start all client tasks including 0 */
        if ((StationList & ConfCodeMask) != 0) {
/*
            CoC[idx].task_id = taskSpawn(CoC[idx].task_name, MED_PRI, 0, STACK_SIZE, tcpClient,(int)CoC[idx].ip_adr, SERVER_PORT_NUM,(int)pSM, idx, 0, 0, 0, 0, 0, 0);
*/
            clientArgs[idx].serverIP = CoC[idx].ip_adr;
            clientArgs[idx].serverPortin = SERVER_PORT_NUM;
            clientArgs[idx].pSM = pSM;
            clientArgs[idx].nr_coc = idx;
            CoC[idx].task_id = epicsThreadCreate(
                                    CoC[idx].task_name,
                                    MED_PRI,
                                    STACK_SIZE,
                                    (EPICSTHREADFUNC)tcpClient,
                                    &clientArgs[idx]);
            tidClient[idx] = CoC[idx].task_id;
            pSM->com_sta.status[H_ADR_ALIVE_CTR + idx + 1] = TRUE;    /* set alive flag */

            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP:> task %d started.\n", idx);

            epicsThreadSleep(DelayTicks5s);        /* delay for 5 sec */
        }
        ConfCodeMask = ConfCodeMask<<1;
    }

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP:> all tasks started.\n");

    finalize = FALSE;
    while (!finalize) {
        /* wait for shutdown */
        cmd_msg_out  = TRUE;
        for(tc = 0;tc < 5;tc++) {
            UpstreamHeartbeat(pSM);
            for (idx =0;idx < MAX_COC;idx++) {
                if (block_length_dwn[idx]) {    /* Set TRUE only for
                                                those stations that
                                                have send down data */
                    cmd_send_dwn[idx] = TRUE;
                }
            }
            epicsThreadSleep(DelayTicks500ms);
        }

        /* check whether all clients are running */

        for (idx =0;idx < MAX_COC;idx++) {
            if (tidClient[idx]) {    /* only for existing clients */

                if (TICP_debug)
                    errlogPrintf("ICP: idx=%d task '%s' IP='%s'\n",
                        idx, CoC[idx].task_name, CoC[idx].ip_adr);

                if (epicsThreadIsSuspended(tidClient[idx])) {    /* if suspended delete it */
                    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG)) {
                        errlogPrintf("ICP:> task '%s' is suspended, try to delete it.\n",
                            CoC[idx].task_name);
                        epicsThreadSleep(DelayTicks50ms);
                    }

                    /* Mark the appropriate connection NOK & clear alive flag */

                    pSM->com_sta.status[H_ADR_COC_STATUS + idx] = CONNECTION_NOK; /* mark connection status NOK */
                    pSM->com_sta.status[H_ADR_ALIVE_CTR + idx + 1] = FALSE;    /* clear alive flag */

                    if (0/*taskDelete(tidClient[idx])*/) {
                        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ERR))
                            errlogPrintf("ICP:> suspended '%s' cannot be deleted.\n",
                                CoC[idx].task_name);

                    }
                    else {
                        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG)) {
                            errlogPrintf("ICP:> task '%s' is deleted try to restart it.\n",
                                CoC[idx].task_name);
                            epicsThreadSleep(DelayTicks50ms);
                        }
                        clientRestart[idx] = TRUE;
                        tidClient[idx] = 0;        /* clear client tid */
                    }
                }
            }
        }

        /* check is there clients to restart */

        for (idx =0;idx < MAX_COC;idx++) {    /* restart the clients having 'restart flag set */
            if (clientRestart[idx]) {

            /* !!!! It should be changed if several ticp can be started */

                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP:>restarting '%s' task: IP='%s'.\n",
                        CoC[idx].task_name, CoC[idx].ip_adr);
/*
                CoC[idx].task_id = taskSpawn(CoC[idx].task_name, MED_PRI, 0, STACK_SIZE, tcpClient,(int)CoC[idx].ip_adr, SERVER_PORT_NUM,(int)pSM, idx);
*/
                CoC[idx].task_id = epicsThreadCreate(
                    CoC[idx].task_name,
                    MED_PRI,
                    STACK_SIZE,
                    (EPICSTHREADFUNC)tcpClient,
                    &clientArgs[idx]);

                tidClient[idx] = CoC[idx].task_id;
                pSM->com_sta.status[H_ADR_ALIVE_CTR + idx + 1] = TRUE;  /* set alive flag */

                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP:> task %d restarted.\n", idx);

                clientRestart[idx] = FALSE;    /* reset restart flag */
            }
        }
    }

    /* Shutdown, Cleanup */
    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))errlogPrintf("ICP:> start Shutdown.\n");

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))errlogPrintf("ICP:> Shutdown finished.\n");
    exit(0);
}
/*=============================================================================
// Function:    tcpClient
// Description: handle the client part of the socket data transmission with the
// Siemens S7 PLC
//===========================================================================*/

STATUS tcpClient (tcpClientArgs* args)
{
    char   sendBuf[2*(MAX_DWN_BUF_SIZE+1)];    /* send buffer */
    struct sockaddr_in clientAddr;        /* clients socket address */
    char   recvBuf[2*(MAX_UP_BUF_SIZE+1)];        /* receive buffer */
    int    serverPort=SERVER_PORT_NUM;
    int    idx;
    msg_header *psendHeader = (msg_header *)sendBuf;

    char *serverIP   = args->serverIP;
    int serverPortin = args->serverPortin;
    sm_layout* pSM   = args->pSM;
    int nr_coc       = args->nr_coc;

    if (RemoteHeader) {
        /* bcopyWords(&pSM->sta_dwn[nr_coc].status[0],&sendBuf[0],(S_LEN)); */   /* copy the send header */
        memcpy(sendBuf, pSM->sta_dwn[nr_coc].status,(2*S_LEN));    /* copy the send header */
        if (SwapByte)
            ByteSwap(sendBuf, 2 * S_LEN);        /* swap bytes if necessary */
        for (idx=2*S_LEN;idx<2*MAX_DWN_BUF_SIZE;idx++) {
            sendBuf[idx]=0x00;
        }
    } else {
        for (idx=0;idx<2*MAX_DWN_BUF_SIZE;idx++) {
            sendBuf[idx]=0x00;
        }
    }

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("\nICP:%d>  Starting TcpClient. Server: %s:%i\n", nr_coc, serverIP, serverPortin);
    if (serverPortin==0)
        serverPort=SERVER_PORT_NUM;            /* default */
    else
        serverPort=serverPortin;
    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf("ICP:%d>  using Server Port %i\n", nr_coc, serverPort);

    iRcvBlockCtr[nr_coc]=0;
    sFd[nr_coc]=0;

/*   taskDeleteHookAdd(cleanup);    */            /* install task shutdown cleanup routine */

/*    pSM->semID_up[nr_coc] = epicsMutexMustCreate();    moved to driver */  /* create semaphores */

/*     if (block_length_dwn) {    moved to driver  */   /* create semaphor when DWN block exists only */
/*        pSM->semID_dwn[nr_coc] = epicsMutexMustCreate();   */   /* create semaphores */
/*    }     */ 

    while (TRUE)                                        /* forever */
    {
        int connectionOk=FALSE;
        int iConnectLoop=0;
        int len=sizeof (struct sockaddr_in);
        int iCommLoop=0;

        /* establish connection with server */
        iNextrcvBlockEntry[nr_coc]=0;
        do
        {

            /* create client socket */
            if ((sFd[nr_coc] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            {
                perror("socket");

                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ERR))
                    errlogPrintf("ICP:%d>  -1 create socket => Exiting Application.\n",
                        nr_coc);
                epicsThreadSleep(DelayTicks5s);    /* 5 sec delay before termination */
                exit(1);    /* Korobov: return -1; causes
                    Illegal instruction exeption */
            }

            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:%d> Try to connect to %s:%d on fd %d #%i...\n",
                    nr_coc, serverIP, serverPort, sFd[nr_coc], iConnectLoop);

            if (establishConnection(sFd[nr_coc], serverIP, serverPort) == -1)
            {
                errlogPrintf("ICP:%d> connect to %s:%d failed: %s\n",
                    nr_coc, serverIP, serverPort, strerror(errno));
                if (close(sFd[nr_coc]))
                    errlogPrintf("ICP:%d> close fd %d failed: %s\n",
                        nr_coc, sFd[nr_coc], strerror(errno));
                sFd[nr_coc]=0;
                epicsThreadSleep(DelayTicks990ms);
            }
            else
            {
                connectionOk=TRUE;
                if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                    errlogPrintf("ICP:%d> connected to %s:%d on fd %d\n",
                        nr_coc, serverIP, serverPort, sFd[nr_coc]);
            }
            iConnectLoop++;
        }
        while (!connectionOk);

        /***** connection with server established; do data transfer *****/

        if (getsockname(sFd[nr_coc],(struct sockaddr *)&clientAddr,&len) == -1)
            perror("getsockname");
        else {
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))errlogPrintf("ICP:%d> connection established. Client port: %i\n",
                                    nr_coc, ntohs(clientAddr.sin_port));
            pSM->com_sta.status[H_ADR_COC_STATUS + nr_coc] = CONNECTION_OK;
        }

        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
            errlogPrintf("tcpClient %d: before 1-st read: IP='%s' port=%d\n",
                nr_coc, serverIP, serverPort);

        while (TRUE)                            /* send/receive loop */
        {
            int iNumRead;
            int iNumWritten;
            int iRC;
            msg_header *rcvHeader = (msg_header *)recvBuf;

            /* check: time for send new down data block ? */
            if (TRUE == cmd_send_dwn[nr_coc])
            {

                cmd_send_dwn[nr_coc] = FALSE;

                /* block the corresponding area of SM */
                epicsMutexMustLock(pSM->semID_dwn[nr_coc]);

                sm_BufGet(sendBuf, pSM, nr_coc);

                if ((TICP_debug) && (TICP_debug == (nr_coc+1))) {    /**** debugging print out *****/

                    if (RemoteHeader) {
                    psendHeader = (msg_header *)sendBuf;
                    errlogPrintf("ICP:%d> header before send: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                        nr_coc,
                        psendHeader->plcID,
                        psendHeader->byteCnt,
                        psendHeader->resrv[0],
                        psendHeader->resrv[1],
                        psendHeader->resrv[2]);
                    psendHeader = (msg_header *)&sendBuf[20]; /* strange DZ */
                    errlogPrintf("ICP:%d> data before send: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
                        nr_coc,
                        psendHeader->plcID,
                        psendHeader->byteCnt,
                        psendHeader->resrv[0],
                        psendHeader->resrv[1],
                        psendHeader->resrv[2]);
                     }
                }    /***************************/

                /* unblock the corresponding area of SM */
                epicsMutexUnlock(pSM->semID_dwn[nr_coc]);

                /* send data to server (S7) */
                if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                    errlogPrintf ("ICP:%d> Message #%i to send: \n",
                        nr_coc, iCommLoop);

                if ((iNumWritten=send(sFd[nr_coc], sendBuf, (block_length_dwn[nr_coc]), 0)) == -1)
                {
                    errlogPrintf("ICP:%d> send %d bytes to fd % failed: %s\n",
                        block_length_dwn[nr_coc], nr_coc, strerror(errno));
                    break;            /* exit the send/receive loop */
                }
                if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                    errlogPrintf("ICP:%d> Message of length %i sent\n",
                        nr_coc, iNumWritten);
            }

            /* check (with timeout) for data arrival from server */

            iRC=wfTimeoutOrRxdata(sFd[nr_coc], nr_coc);
            if (iRC <= 0) {       /* error or no data within timeout timeframe */
                break;            /* exit the send/receive loop */
            }
            /* data available; read data from server (S7) */

            if ((iNumRead=recv(sFd[nr_coc], recvBuf, block_length_up[nr_coc], 0)) < 0)
            {
                errlogPrintf("ICP:%d> recv: errno = 0x%x\n",
                    nr_coc, errno);
                break;            /* exit the send/receive loop */
            }
            if (chk_msg_sta(RBN, nr_coc, REPORT_ALL))
                errlogPrintf ("ICP:%d>  Message received (length %i)\n",
                    nr_coc, iNumRead);

            if (iNumRead > 0) {        /* some data (>0 bytes) received */

                if (RemoteHeader) {
                    if (iNextrcvBlockEntry[nr_coc] == 0) {    /* New block starts */
                        if (SwapByte)
                            ByteSwap(recvBuf, 2 * S_LEN);        /* swap bytes in header if necessary */

                    /* check whether the PLC ID is correct */


                        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                            errlogPrintf("ICP>%d: Got PLC ID=%d\n", nr_coc, rcvHeader->plcID);
                        if (pSM->sta_up[nr_coc].status[H_ADR_PLC_ID] != rcvHeader->plcID) {    /* is the ID correct? */

                            errlogPrintf("ICP:%d> Wrong PLC ID=%d\n", nr_coc, rcvHeader->plcID);
                            break;        /* ignore the data and shutdown the connections */
                        }

                        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                            errlogPrintf("ICP>%d: Got %d bytes\n", nr_coc, rcvHeader->byteCnt);

                        if (pSM->sta_up[nr_coc].status[H_ADR_BYTE_CNT] != rcvHeader->byteCnt)
                        {

                            if ((rcvHeader->byteCnt > 0) && (rcvHeader->byteCnt < MAX_UP_BUF_SIZE * 2))
                            {
                                pSM->sta_up[nr_coc].status[H_ADR_BYTE_CNT] = rcvHeader->byteCnt;

                                if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                                    errlogPrintf("tcpClient %d: PLC BYTE_CNT changed to %d\n",
                                        nr_coc, pSM->sta_up[nr_coc].status[H_ADR_BYTE_CNT]);

                                block_length_up[nr_coc] = pSM->sta_up[nr_coc].status[H_ADR_BYTE_CNT];
                            }
                            else {        /* Not valid byte counter */
                                errlogPrintf("ICP:%d> BYTE_CNT is not valid = %d\n", nr_coc, rcvHeader->byteCnt);
                                break;        /* shut down the connection */
                            }
                        }
                    }
                }

                /* block the corresponding area of SM */
                epicsMutexMustLock(pSM->semID_up[nr_coc]);

                /* copy received data to data block */

                if (copyToRcvBlock(recvBuf, pSM, iNumRead, nr_coc))
                {
                    /* block end reached ? sync end message block */
                    if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                        if (chk_msg_sta(RBE, nr_coc, REPORT_DIAG))
                            errlogPrintf("ICP:%d> <<<<<<<<<<<<< \n", nr_coc);
                    iCommLoop++;
                    /* sync begin new message block */
                    if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
                        if (chk_msg_sta(RBB, nr_coc, REPORT_DIAG))
                            errlogPrintf("ICP:%d> >>>>>>>>>>>>> Loop: %d\n", nr_coc, iCommLoop);
                }

                /* unblock the corresponding area of SM */
                epicsMutexUnlock(pSM->semID_up[nr_coc]);
            }

            /* wait some time (this time determines the xmit/rcv frequency) */

            epicsThreadSleep(DelayTicks10ms);


        }/* end send/receive loop */

        /* connection not ok; shut it down */

        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ERR))
            errlogPrintf("ICP:%d>  connection not ok; shut it down and retry\n", nr_coc);
        pSM->com_sta.status[H_ADR_COC_STATUS + nr_coc] = CONNECTION_NOK;
        if (shutdown(sFd[nr_coc], 2) == -1)
            errlogPrintf("ICP:%d> shutdown failed: %s\n", nr_coc, strerror(errno));
        if (close (sFd[nr_coc]) == -1)
            errlogPrintf("ICP:%d> close failed: %s\n", nr_coc, strerror(errno));
        sFd[nr_coc]=0;

        /* wait some time (allow PLC to disconnect and get ready for new connection) */
        /* PLC drops connection after approx. 10 sec, so wait some longer time here */
        epicsThreadSleep(RECONNECT_DELAY);
        /* connection is shut down; try to re-establish */
    }/* forever */

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        errlogPrintf ("ICP:%d>  Correctly exiting TcpClient application.\n", nr_coc);
    exit(0);    /* Korobov: return OK; causes Illegal instruction
                exeption */
}

/*=============================================================================
// Function:    copyToRcvBlock
// Description: copy an entire data block from the internal buffer to the receive block
// returncode:  none
//===========================================================================*/
BOOL copyToRcvBlock(char * pRcvBuffer, sm_layout* pSM, int iSize, int nr_coc)
{
    int i;
    BOOL block_finished = FALSE;

    /* copy all received data in block */
    for (i=0; i<iSize; i++) {
        /* copy to shared memory */
        rcvBlock[nr_coc][iNextrcvBlockEntry[nr_coc]]=*pRcvBuffer;
        pRcvBuffer++;                        /* next received byte */
        iNextrcvBlockEntry[nr_coc]=(iNextrcvBlockEntry[nr_coc]+1) % (block_length_up[nr_coc]);    /* next entry */
        if (iNextrcvBlockEntry[nr_coc]==0) {        /* block end reached */
            iRcvBlockCtr[nr_coc]++;
            while (sm_BufPut(rcvBlock[nr_coc], pSM, nr_coc)==FALSE) {};
            block_finished = TRUE;
        }
    }
    return block_finished;
}

/*=============================================================================
// Function:    wfTimeoutOrRxdata
// Description: wait for Rx data on socket or timeout
// returncode:  0 if timeout; <0 if error; >0 if data
//===========================================================================*/
int wfTimeoutOrRxdata(int sockD, int nr_coc)
{
    static struct timeval    to;
    int    iSelect;
    fd_set socklist;

    FD_ZERO(&socklist);
    FD_SET(sockD,&socklist);
    to.tv_sec=RECV_TIMEOUT;
    to.tv_usec=0;
    /* select returns when either the socket has data or the timeout elapsed */
    if ((iSelect=select(sockD+1,&socklist, 0, 0,&to)) < 0)
    {
        errlogPrintf("ICP:%d> select failed: %s\n", nr_coc, strerror(errno));
    }
    if (iSelect==0)            /* timed out */
    {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_ALL))errlogPrintf("ICP:%d>  select timed out.\n", nr_coc);
    }
    return iSelect;
}

/*=============================================================================
// Function:    establishConnection
// Description: establish permanent (TCP) connection with Server
// returncode:  0 if ok; -1 if error
//===========================================================================*/
int establishConnection(int sockD,char * serverIP,int serverPort)
{
    struct sockaddr_in    serverAddr;    /* server socket address */
    /* set the size of the socket level receive buffer */
    int rbuflen = SOCK_RECV_BUFFER_SIZE;
/*    static struct timeval    to; */

    if (TICP_debug)
        errlogPrintf("establishConnection: IP='%s' port=%d\n", serverIP,serverPort);

    if (setsockopt(sockD, SOL_SOCKET, SO_RCVBUF, (char *)&rbuflen, sizeof(rbuflen)) == -1)
    {
        perror("setsockopt(...,SO_RCVBUF) failed; ignore.");
    }
    /* build server socket address */
    bzero((char *) &serverAddr, sizeof (struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);

    /* connect to server */
/*    to.tv_sec=CONNECT_TIMEOUT;     */   /* specify timeout */
/*    to.tv_usec=0;                  */  /* connect has no timeout. Emulate? DZ */
/*    if (connectWithTimeout(sockD, (struct sockaddr *) &serverAddr, sizeof (struct sockaddr_in),&to) == -1) */
    if (connect(sockD, (struct sockaddr *) &serverAddr, sizeof (struct sockaddr_in)) == -1)
        return -1;
    else
        return 0;
}

/*=============================================================================
// Function:    cleanup
// Description: cleanup at task deletion
// (socket is not closed automatically when the task is killed; therefore do it explicitely here)
// returncode:  none
//===========================================================================*/
#if 0
int cleanup(WIND_TCB *pTcb)
{
    int i;

    /* close the connection if any */

    for (i=0; i < MAX_COC; i++) {
        if (pTcb == (WIND_TCB *)tidClient[i]) {
        if (TICP_debug)
            errlogPrintf("cleanup: tid = %x nr_coc = %d\n", pTcb, i);

            tidClient[i] = 0;        /* clear saved tid */
            clientRestart[i] = TRUE;    /* set "restart client" flag */
            if (sFd[i])
                close(sFd[i]);    /* close connection */
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

        if (RemoteHeader) {    /* if remote header is used initialize headers */

            /* downstream headers */
            for (idx = 0; idx < MAX_DWN_STA_SIZE; idx++) {
                if (CoC[nr_coc].id) {    /* for active PLC */
                    if (idx == 0) {
                        pSM->sta_dwn[nr_coc].status[idx] = CoC[nr_coc].id;        /* set 1-st word as ID */
                        continue;
                    }
                    if (idx == 1) {
                        pSM->sta_dwn[nr_coc].status[idx] = CoC[nr_coc].wrBlSize;    /* set 2-nd word as byte counter */
                        continue;
                    }
                    else
                        pSM->sta_dwn[nr_coc].status[idx] = 0;
                }
                else
                    pSM->sta_dwn[nr_coc].status[idx] = 0;
            }

            /* upstream headers */
            for (idx = 0; idx < MAX_UP_STA_SIZE; idx++) {
                if (CoC[nr_coc].id) {    /* for active PLC */
                    if (idx == 0) {
                        pSM->sta_up[nr_coc].status[idx] = CoC[nr_coc].id;        /* set 1-st word as ID */
                        continue;
                    }
                    if (idx == 1) {
                        pSM->sta_up[nr_coc].status[idx] = CoC[nr_coc].rdBlSize;    /* set 2-nd word as byte counter */
                        continue;
                    }
                    else
                        pSM->sta_up[nr_coc].status[idx] = 0;
                }
                else
                    pSM->sta_up[nr_coc].status[idx] = 0;
            }
        }
    }
    return TRUE;
}

/*=============================================================================
// Function:        sm_BufGet
// Description:     get block from down buffer
//===========================================================================*/

BOOL sm_BufGet(char* sendBuf, sm_layout* pSM, int nr_coc)
{

    /* copy to A32 slave memory */

    if (RemoteHeader) {
/*      bcopyBytes((char *)&pSM->buf_dwn[nr_coc].data[2*S_LEN],&sendBuf[2*S_LEN],(block_length_dwn[nr_coc]-2*S_LEN)); */
        memcpy(&sendBuf[2*S_LEN],&pSM->buf_dwn[nr_coc].data[2*S_LEN],(block_length_dwn[nr_coc]-2*S_LEN));
        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:%d>  DWN: id %4d length %d\n",
                    nr_coc, pSM->sta_up[nr_coc].status[H_ADR_PLC_ID], block_length_dwn[nr_coc]);
    } else {
/*      bcopyBytes((char *)pSM->buf_dwn[nr_coc].data, sendBuf,(block_length_dwn[nr_coc])); */
        memcpy(sendBuf, pSM->buf_dwn[nr_coc].data, block_length_dwn[nr_coc]);
        if ((TICP_debug) && (TICP_debug == (nr_coc+1)))
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:%d> DWN: length: %d\n",
                    nr_coc, block_length_dwn[nr_coc]);
    }

    if ((TICP_debug) && (TICP_debug == (nr_coc+1))) {
        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP:%d>  DWN: data:\n", nr_coc);
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

    if (SwapByte) {
        if (RemoteHeader)
            ByteSwap(&sendBuf[2*S_LEN],(block_length_dwn[nr_coc]-2*S_LEN));
        else
            ByteSwap(sendBuf, block_length_dwn[nr_coc]);
    }
    return TRUE;
}

/*=============================================================================
// Function:        sm_BufPut
// Description:     put block to up buffer
//===========================================================================*/
BOOL sm_BufPut(char* recvBuf, sm_layout* pSM, int nr_coc)
{

    /* copy to A32 slave memory */

    if (SwapByte)
        ByteSwap(recvBuf, block_length_up[nr_coc]);

/*  bcopyBytes(recvBuf,(char *)pSM->buf_up[nr_coc].data, block_length_up[nr_coc]); */
    memcpy(pSM->buf_up[nr_coc].data, recvBuf, block_length_up[nr_coc]);

    SmInitialized = TRUE;

    if ((TICP_debug) && (TICP_debug == (nr_coc+1))) {
        if (RemoteHeader) {
            if (chk_msg_sta(RBB, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:%d> PLC ID=%d UP_BYTE_CNT=%d DWN_BYTE_CNT=%d\n",
            nr_coc, pSM->sta_up[nr_coc].status[H_ADR_PLC_ID],
            pSM->sta_up[nr_coc].status[H_ADR_BYTE_CNT],
            pSM->sta_dwn[nr_coc].status[H_ADR_BYTE_CNT]);
        } else {
            if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
                errlogPrintf("ICP:%d> UP: length: %d\n",
            nr_coc, block_length_up[nr_coc]);
        }

        if (chk_msg_sta(RBN, nr_coc, REPORT_DIAG))
            errlogPrintf("ICP:%d>  sm_BufPut::com_status::alive_ctr: %4d\n",
                nr_coc, pSM->com_sta.status[H_ADR_ALIVE_CTR]);

        if (chk_msg_sta(RBN, nr_coc, REPORT_DIAG))
            errlogPrintf("ICP:%d>  sm_BufPut::com_status::alive_ctr[%d]: %4d\n",
                nr_coc, nr_coc, pSM->com_sta.status[H_ADR_ALIVE_CTR+nr_coc+1]);

        if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
            errlogPrintf("ICP:%d>  UP 0x%x: data:\n",
                nr_coc, pSM->sta_up[nr_coc].status[H_ADR_PLC_ID]);
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
    return TRUE;
}

/*=============================================================================
// Function:        ByteSwap
// Description:   exchange high and low byte
//===========================================================================*/
void ByteSwap(char* Buf, int size)
{
    int idx;
    char ctmp;

    for (idx = 0;idx < size; idx=idx+2) {
        ctmp= *(Buf+idx);
        *(Buf+idx)=*(Buf+idx+1);
        *(Buf+idx+1)=ctmp;
    }
    return;
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
// Function:        CreateTestPattern
// Description:     create test pattern for up buffer
//===========================================================================*/
void CreateTestPattern(sm_layout* pSM, int tst_cntr)
{
    int nr_coc;
    int idx;

    if (chk_msg_sta(RBN, REP_ALL_COC, REPORT_DIAG))
        printf("\nICP:Create Test Pattern :: size: %d max_coc:%d up_size:%d dwn_size:%d \n",
            sizeof(sm_layout), MAX_COC, sizeof(sm_data_up), sizeof(sm_data_dwn));
    /* create test pattern */

    /* Korobov: DEBUG */
    errlogPrintf("CreateTestPattern: 1-st addr=%p 2-nd addr=%p\n",
        pSM->sta_up, pSM->buf_up);
    /*--------------------*/

    for (nr_coc = 0; nr_coc < MAX_COC;nr_coc++) {

        for (idx = 0; idx < MAX_UP_STA_SIZE; idx++) {
            pSM->sta_up[nr_coc].status[idx] = 0x3000 + nr_coc;
        }
        for (idx = 0; idx < MAX_UP_BUF_SIZE; idx++) {
            pSM->buf_up[nr_coc].data[idx] = 0x4000+ nr_coc*0x100 +idx % 256;
        }
    }
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
    if ((nr_coc == REP_ALL_COC)&&(RepLev >= rep_lev)) return TRUE;

    if ((bsync == RBB) && (cmd_msg_out)&& (nr_coc == RepCoc)) {
        cmd_msg_out  = FALSE;
        sta_msg_out  = TRUE;
    }

    if (!sta_msg_out) return FALSE;

    if ((bsync == RBE) && (sta_msg_out)&& (nr_coc == RepCoc))sta_msg_out  = FALSE;

    if ((nr_coc != RepCoc)&&(nr_coc != REP_ALL_COC)) return FALSE;
    if (RepLev < rep_lev) return FALSE;
    return TRUE;
}

/*=============================================================================
// ======================== Bottom of C-File ==================================
//===========================================================================*/

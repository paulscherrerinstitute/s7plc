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

/*ticp.h - header used by ticp and tioc
** Revision history:
*        04-09-01        Kor        changed to use in one CPU configuration.
*        12-11-01        Kor        added mutual exclusion semaphores IDs into
*                                the memory table.
*        31-01-02        Kor        default file name for configuration file.
*        17-04-02        Kor        modified dCoC structure definition is moved
*                                into header file.
*        26-04-02        Kor        added id field into dCoC structure and changed
*                                remote header structure.
*        25-07-02        Kor        data type for sm_data_up & sm_data_down is
*                                changed to char. (DESY version 1.10)
*/

/* $Author: zimoch $ */
/* $Date: 2005/02/07 16:06:42 $ */
/* $Id: ticp.h,v 1.1 2005/02/07 16:06:42 zimoch Exp $ */
/* $Name:  $ */
/* $Revision: 1.1 $ */

#ifndef INCticph
#define INCticph

#include <epicsVersion.h>
#if (EPICS_REVISION<14)
/* R3.13 */
#include "compat3_13.h"

#else
/* R3.14 */
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTypes.h>
#endif

/* defines */
#define LOW_PRI          epicsThreadPriorityLow    /* low priority task */
#define MED_PRI          epicsThreadPriorityMedium /* medium priority task */
#define HIGH_PRI         epicsThreadPriorityHigh   /* high priority task */

#define MAX_COC          16                        /* max number of communication channels (S7 stations) */
#define TASK_NAME_LEN    12                        /* nr of characters */
#define IP_ADR_LEN       16                        /* nr of characters */
#define STACK_SIZE       20000                     /* stack size [nr of bytes] */
#define SERVER_PORT_NUM  2000                      /* servers port number */
#define ICP_TASK_NAME    "ticp"                    /* ICP task name */

#define CONF_FILE_ENV    "ICP_IP_ADDR_FILE_NAME"   /* env var to define config file name */
#define CONF_FILE_NAME   "icp_IP_addr.cnf"         /* default file name for
                                                      PLC IP address config file */
/* Korobov: try to use ICP on the slave CPU
   define shared memory start from 0x2e00000
#define SM_MEM_ADRS_PCI                0xc00000        ****/   /* local shared memory base address */
/** Korobov: now try to use ICP & EPICS on the same board
#define SM_MEM_ADRS_PCI                0x2e00000 ***/           /* (local)remote shared memory base address */
#define SM_MEM_ADRS_PCI                0x600000           /* local shared memory base address */
/* !!! word length  must be a multiple of 4 !!! */
#define HEADSTATION_UP_BUF_SIZE        256        /* max size of headstation upstream buffer [word] */
#define HEADSTATION_DWN_BUF_SIZE  16  /* max size of headstation dwnstream buffer [word] */
#define MAX_UP_BUF_SIZE          512  /* max size of upstream buffer [word] */
#define MAX_DWN_BUF_SIZE         128  /* max size of downstream buffer [word] */
#define MAX_UP_STA_SIZE           64  /* max size of upstream status [word] */
#define MAX_DWN_STA_SIZE          64  /* max size of downstream status [word] */
#define MAX_COM_STA_SIZE          64  /* max size of communication status [word] */

#define SOCK_RECV_BUFFER_SIZE   8192  /* socket level receive buffer size [byte] */
/* time constants */
#define POLL_DELAY_10MS           10  /* poll delay [ms]*/
#define POLL_DELAY_60MS           60  /* poll delay [ms]*/
#define POLL_DELAY_500MS         500  /* poll delay [ms]*/
#define POLL_DELAY_990MS         990  /* poll delay [ms]*/
#define POLL_DELAY_1S           1000  /* poll delay [ms]*/
#define RECV_TIMEOUT               5  /* receive timeout [s] */
#define CONNECT_TIMEOUT            5  /* connect timeout [s] */
#define RECONNECT_DELAY           20  /* delay before attempting reconnect [s] */

/* address to header */
#define ADR_HEAD_STATION           0  /* address head station */

#define H_ADR_DATA_ID              0  /* address data id */                                                      
#define H_ADR_STATION_LIST         1  /* address stationlist */                                                  
#define H_ADR_OPT_COM              2  /* address communication optimization */                                   
#define H_ADR_DATA_FORMAT          3  /* address data format */                                                  
#define H_ADR_REP_COC              4  /* address report channel */                                               
#define H_ADR_REP_LEV              5  /* address report level */                                                 
#define H_ADR_BEG_UP_BLEN         11  /* address block begin of: UP data block length, sector station 1..15  */  
#define H_ADR_BEG_DWN_BLEN        31  /* address block begin of: DWN data block length, sector station 1..15  */ 
#define H_LEN                     50  /* address data area ; head station header length */                       
/* Kor: constants for new headers */
#define H_ADR_PLC_ID               0  /* address PLC id */
#define H_ADR_BYTE_CNT             1  /* address of byte counter */

#define H_ADR_ALIVE_CTR            0  /* address alive_ctr */
/* Korobov: added alive counter for all clients + 1 for ticp itself
#define H_ADR_COC_STATUS        0x10 ***/    /* address begin area of communication status 0x10..0x1f */
#define H_ADR_COC_STATUS        0x11  /* address begin area of communication status 0x11..0x1f */

#define S_LEN                     10  /* address data area ; sector station header length  */

#define DATA_FORMAT_STD            0  /* data format no swap */
#define DATA_FORMAT_INTEL          1  /* data format swap */
#define DATA_FORMAT_MOTOROLA       2  /* data format no swap*/
#define XFR_OPT_OFF                0  /* transfer optimization off */
#define XFR_OPT_ON                 1  /* transfer optimization on*/
#define CONNECTION_NOK             0  /* connection broken */
#define CONNECTION_OK              1  /* connection established*/
#define LOOPBACK                   0  /* loopback ICP data*/
#define TESTPATTERN                1  /* generate test pattern*/

#define REP_COC                    0
#define REP_ALL_COC               -1
#define RBB                        1  /* begin report block */
#define RBE                        2  /* emd report block */
#define RBN                        0  /* no report block sync */

/* report level of a certain message
// example: if the global report_level (GlbRepLev) is set to 2
//          all reports less or equal than 2 will be displayed */
#define REPORT_NONE                0  /* no report */
#define REPORT_ERR                 1  /* report errors  */
#define REPORT_DIAG                2  /* report diagnostic info */
#define REPORT_DBG                 3  /* report debugg info */
#define REPORT_ALL                 4  /* report all */

/*  data layout*/
typedef struct s_status_dwn {
    epicsUInt16 status[MAX_DWN_STA_SIZE];  /* status buffer */
} sm_status_dwn;

typedef struct s_status_up {
    epicsUInt16 status[MAX_UP_STA_SIZE];   /* status buffer */
} sm_status_up;

typedef struct s_data_dwn {
    epicsUInt8  data[MAX_DWN_BUF_SIZE*2];  /* byte data buffer */
} sm_data_dwn;

typedef struct s_data_up {
    epicsUInt8  data[MAX_UP_BUF_SIZE*2];   /* byte data buffer */
} sm_data_up;

typedef struct s_com_status {
    epicsUInt16 status[MAX_COM_STA_SIZE];  /* communication status */
} sm_com_status;

typedef struct sm_ly {
    sm_status_dwn   sta_dwn[MAX_COC];   /* EPICS status */
    sm_data_dwn     buf_dwn[MAX_COC];   /* down buffer, from EPICS to S7 */
    sm_status_up    sta_up[MAX_COC];    /* S7 status */
    sm_data_up      buf_up[MAX_COC];    /* up buffer, from S7 to EPICS */
    sm_com_status   com_sta;            /* communication status */
    epicsMutexId    semID_dwn[MAX_COC]; /* down semaphores IDs buffer */
    epicsMutexId    semID_up[MAX_COC];  /* up semaphores IDs buffer */
} sm_layout;

/* Client task description table element */

typedef struct dCoC {
    epicsThreadId task_id; /* Task ID */                             
    char* task_name;       /* Task name */                           
    char ip_adr[16];       /* PLC IP address */                      
    short rdBlSize;        /* Read block size (in bytes) */          
    short wrBlSize;        /* Write block size (in bytes) */         
    short id;              /* PLC ID (last part of PLC IP address)*/
} dCoC;

/* Message header structure (when remote header is used) */

typedef struct msg_header {
    short plcID;           /* PLC ID (the last part of IP address)*/
    short byteCnt;         /* Byte counter for the message */
    short resrv[8];        /* reserved */
} msg_header;

void ICP_start(sm_layout* pSM,        /* allocated memory table */
               unsigned GlbSwapBytes, /* swap bytes flag */
               unsigned RepLev,       /* report level */
               char RemHead);         /* with/withou remote header */
               
#endif /* INCticph */

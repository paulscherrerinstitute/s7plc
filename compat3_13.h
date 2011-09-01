/* EPICS R3.13 compatibility header
*
* Including this header allows (some) EPICS R3.14 code to be compiled
* with EPICS R3.13 for a vxWorks system using a GNU compiler.
*
* It replaces R3.14 headers
*   epicsTypes.h
*   epicsAssert.h
*   epicsString.h
*   epicsExport.h
*   epicsThread.h
*   epicsTimer.h
*   epicsMutex.h
*   epicsEvent.h
*   cantProceed.h
*
* Usage:
*  #include <epicsVersion.h>
*  #if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)
*  #include <cantProceed.h>
*  #include <epicsMutex.h>
*  ...
*  #else
*  #include "compat3_13.h"
*  #endif
*
* It is not complete and only simulates the C interfaces (not C++).
* It uses some gcc extensions and might not work with other compilers.
* It requires vxWorks.
* It might be buggy or produce slightly different behaviour than the
* original R3.14 functions.
*
* Since all functions are simulated as macros, you might not be able to
* take the address of a function. Be careful with arguments as they
* might be evaluated multiple times.
*   This won't work:               fptr = &epicsTimerStartDelay;
*   Don't do this (changing arg):  epicsTimerStartDelay(*ptimer++, delay);
*
* You can take the address of
*   epicsMutexUnlock   
*   epicsMutexDestroy  
*   epicsEventSignal   
*   epicsEventDestroy 
*
* You have been warned.
* Dirk Zimoch, March 2005
*/

#ifndef compat3_13_h
#define compat3_13_h

#include <vxWorks.h>
#include <sysLib.h>
#include <selectLib.h>

/* epicsAssert */
#include <assert.h>

/* epicsString */
#define epicsStrDup(s) strcpy(malloc(strlen(s)+1),(s))

/* epicsExport */
#define epicsExportAddress(a,b) extern void avoid_compiler_warning

/* epicsThread */
#include <taskLib.h>
#define epicsThreadId void*
#define EPICSTHREADFUNC FUNCPTR
#define epicsThreadPriorityLow    10
#define epicsThreadPriorityMedium 50
#define epicsThreadPriorityHigh   90
#define epicsThreadStackBig      20000  /* io thread stack size */

#define epicsThreadCreate(name, pri, stack, func, args) \
    (void*)taskSpawn((name), 255-(pri)*255/99, VX_FP_TASK, (stack), (FUNCPTR)(func), (int)(args), \
    0,0,0,0,0,0,0,0,0)
    
#define epicsThreadSleep(sec) taskDelay((sec)*sysClkRateGet())
#define epicsThreadIsSuspended(t) (taskIdVerify((int)(t)) || taskIsSuspended((int)(t)))
#define epicsThreadGetStackSize(stack) (stack)

/* epicsTimer */
#include <wdLib.h>
#define epicsTimerQueueId char*
#define epicsTimerQueueAllocate(a,b) NULL
typedef void (*epicsTimerCallback) (void* pPrivate);
typedef struct epicsTimerForC {WDOG_ID wd; FUNCPTR cb; int p;} *epicsTimerId;

#define epicsTimerQueueCreateTimer(q, callback, param) __extension__ \
({ \
    epicsTimerId ti = calloc(1,sizeof(struct epicsTimerForC)); \
    ti->wd = wdCreate(); \
    ti->cb = (FUNCPTR)(callback); \
    ti->p = (int)(param); \
    ti; \
})

#define epicsTimerStartDelay(ti,delay) \
    (void) wdStart((ti)->wd, (delay)*sysClkRateGet(), (ti)->cb, (ti)->p)

/* cantProceed */
#define callocMustSucceed(n,s,m) __extension__ \
({ \
    void *mem = calloc((n),(s)); \
    if (!mem) { printErr("FATAL: callocMustSucceed failed in %s line %d %s: \n", \
        __FILE__, __LINE__, (m), strerror(errno)); taskSuspend(0); } \
    mem; \
})

/* epicsMutex */
#include <semLib.h>
#define epicsMutexId SEM_ID

#define epicsMutexMustCreate() __extension__ \
({ \
    SEM_ID m = semMCreate(SEM_Q_FIFO); \
    if (!m) { printErr("FATAL: epicsMutexMustCreate failed in %s line %d: %s\n", \
        __FILE__, __LINE__, strerror(errno)); taskSuspend(0); } \
    m; \
})

#define epicsMutexMustLock(m) \
    (semTake(m, WAIT_FOREVER) && \
    (printErr("FATAL: epicsMutexMustLock failed in %s line %d: %s\n", \
     __FILE__, __LINE__, strerror(errno)), \
     taskSuspend(0), ERROR))

#define epicsMutexUnlock semGive
#define epicsMutexDestroy semDelete

/* epicsEvent */
#define epicsEventId SEM_ID
#define epicsEventEmpty SEM_EMPTY
#define epicsEventFull SEM_FULL

#define epicsEventMustCreate(state) __extension__ \
({ \
    SEM_ID e = semBCreate(SEM_Q_FIFO, state); \
    if (!e) { printErr("FATAL: epicsEventMustCreate failed in %s line %d: %s\n", \
        __FILE__, __LINE__, strerror(errno)); taskSuspend(0); } \
    e; \
})

#define epicsEventMustWait(e) \
    (semTake(e, WAIT_FOREVER) && \
    (printErr("FATAL: epicsEventMustWait failed in %s line %d: %s\n", \
     __FILE__, __LINE__, strerror(errno)), \
     taskSuspend(0), ERROR))

#define epicsEventSignal semGive
#define epicsEventWaitWithTimeout(e,t) semTake((e),(t)*sysClkRateGet())
#define epicsEventWait(e) semTake((e),WAIT_FOREVER)
#define epicsEventTryWait(e) semTake((e),NO_WAIT)
#define epicsEventDestroy semDelete

#endif

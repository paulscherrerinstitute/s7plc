#ifndef compat3_13_h
#define compat3_13_h

#include <vxWorks.h>
#include <semLib.h>
#include <taskLib.h>
#include <sysLib.h>
#include <selectLib.h>
#include <assert.h>

#define epicsMutexId SEM_ID
#define epicsThreadId int
#define EPICSTHREADFUNC FUNCPTR
#define epicsThreadPriorityLow    10
#define epicsThreadPriorityMedium 50
#define epicsThreadPriorityHigh   90
#define epicsUInt8 uint8_t
#define epicsUInt16 uint16_t

#define epicsExportAddress(a,b) extern void avoid_compiler_warning

#define callocMustSucceed(n,s,m) \
    ((void*) (calloc((n),(s)) || \
    (printErr("FATAL: callocMustSucceed failed in %s line %d %s: \n", \
     __FILE__, __LINE__, (m), strerror(errno)), \
     taskSuspend(0), NULL)))
     
#define epicsMutexMustCreate() \
    ((void*) (semMCreate(SEM_Q_FIFO) || \
    (printErr("FATAL: epicsMutexMustCreate failed in %s line %d: %s\n", \
     __FILE__, __LINE__, strerror(errno)), \
     taskSuspend(0), NULL)))

#define epicsMutexMustLock(s) \
    (semTake(s, WAIT_FOREVER) && \
    (printErr("FATAL: epicsMutexMustLock failed in %s line %d: %s\n", \
     __FILE__, __LINE__, strerror(errno)), \
     taskSuspend(0), ERROR))

#define epicsMutexUnlock(s) semGive(s)

#define epicsStrDup(s) strcpy(malloc(strlen(s)+1),(s))

#define epicsThreadCreate(name, pri, stack, func, args) \
    taskSpawn(name, 255-(pri)*255/99, VX_FP_TASK, stack, func, (int)(args), \
    0,0,0,0,0,0,0,0,0)

#define epicsThreadSleep(sec) taskDelay((sec)*sysClkRateGet())

#define epicsThreadIsSuspended(t) (taskIdVerify(t) || taskIsSuspended(t))

#endif
